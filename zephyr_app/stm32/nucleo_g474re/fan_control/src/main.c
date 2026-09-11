#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/can.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>

#if !DT_NODE_EXISTS(DT_PATH(zephyr_user)) || !DT_NODE_HAS_PROP(DT_PATH(zephyr_user), io_channels)
#error "No suitable devicetree overlay specified"
#endif

#define MASTER_TO_NODE_ID 0x120
#define NODE_TO_MASTER_ID 0x121

#define CMD_SET_LED       0x01
#define CMD_GET_STATUS    0x02
#define CMD_PING          0x03
#define CMD_SET_FAN_SPEED 0x04
#define CMD_GET_TACH      0x05
#define CMD_GET_POWER     0x06
#define CMD_GET_FAN_SPEED 0x07

#define STATUS_OK          0x00
#define STATUS_BAD_ARG     0xEF
#define STATUS_UNKNOWN_CMD 0xEE

#define NODE_ID 0x01

/* Fan duty-cycle levels selected by CMD_SET_FAN_SPEED byte1 (0..5). */
#define FAN_SPEED_LEVEL_MAX 5
static const uint8_t fan_duty_pct[FAN_SPEED_LEVEL_MAX + 1] = {0, 20, 40, 60, 80, 100};

/*
 * Most 12V/4-wire fans emit 2 tach pulses per revolution. Check your fan's
 * datasheet and adjust if it differs.
 */
#define TACH_PULSES_PER_REV 2

/*
 * ACS712 sensitivity in mV per amp. Confirm the exact part printed on the
 * module (ACS712ELCTR-05B / -20A / -30A) and update to match:
 *   5A variant  -> 185
 *   20A variant -> 100
 *   30A variant -> 66
 * Defaulted to the 5A variant since a small 12V fan draws well under 1A and
 * that gives the best resolution; change it if your module is a different
 * part.
 */
#define ACS712_MV_PER_A 185

/* Passive resistive-divider voltage sensor module, silkscreened "VCC<25V", ratio 5:1. */
#define VOLTAGE_DIVIDER_RATIO 5

const struct device *const can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct pwm_dt_spec fan_pwm = PWM_DT_SPEC_GET(DT_PATH(zephyr_user));
static const struct gpio_dt_spec fan_tach =
	GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), fan_tach_gpios);

#define DT_SPEC_AND_COMMA_FOR_INPUTS(node_id, prop, idx)                                         \
	COND_CODE_1(DT_PHA_HAS_CELL_AT_IDX(node_id, prop, idx, input),                           \
		    (ADC_DT_SPEC_GET_BY_IDX(node_id, idx),), ())

/* io-channels = <&adc1 1>, <&adc1 2>; -> index 0 = voltage sensor, index 1 = ACS712 */
static const struct adc_dt_spec adc_channels[] = {
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), io_channels, DT_SPEC_AND_COMMA_FOR_INPUTS)};

#define ADC_IDX_VOLTAGE 0
#define ADC_IDX_CURRENT 1

CAN_MSGQ_DEFINE(rx_msgq, 16);

static struct gpio_callback tach_cb_data;
static atomic_t tach_pulse_count;

static struct k_mutex sensor_lock;
static uint16_t last_rpm;
static int32_t last_voltage_mv;
static int32_t last_current_ma;
static int32_t acs712_zero_offset_mv = 2500;
static uint8_t fan_speed_level;

K_THREAD_STACK_DEFINE(sensor_thread_stack, 1024);
static struct k_thread sensor_thread_data;

static void tach_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);

	atomic_inc(&tach_pulse_count);
}

static int read_adc_mv(size_t idx, int32_t *out_mv)
{
	uint32_t buf = 0;
	struct adc_sequence sequence = {
		.buffer = &buf,
		.buffer_size = sizeof(buf),
	};
	int ret;

	(void)adc_sequence_init_dt(&adc_channels[idx], &sequence);

	ret = adc_read_dt(&adc_channels[idx], &sequence);
	if (ret != 0) {
		return ret;
	}

	int32_t val_mv = (int32_t)buf;

	ret = adc_raw_to_millivolts_dt(&adc_channels[idx], &val_mv);
	if (ret != 0) {
		return ret;
	}

	*out_mv = val_mv;
	return 0;
}

static void calibrate_acs712_zero(void)
{
	enum { CAL_SAMPLES = 32 };
	int64_t sum = 0;
	int good = 0;

	for (int i = 0; i < CAL_SAMPLES; i++) {
		int32_t mv;

		if (read_adc_mv(ADC_IDX_CURRENT, &mv) == 0) {
			sum += mv;
			good++;
		}
		k_msleep(2);
	}

	if (good > 0) {
		acs712_zero_offset_mv = (int32_t)(sum / good);
	}

	printf("ACS712 zero-current offset calibrated: %d mV\n", acs712_zero_offset_mv);
}

static void sensor_thread_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		k_sleep(K_SECONDS(1));

		unsigned int pulses = (unsigned int)atomic_set(&tach_pulse_count, 0);
		uint16_t rpm = (uint16_t)((pulses * 60U) / TACH_PULSES_PER_REV);

		int32_t voltage_mv = 0;
		int32_t current_mv = 0;
		int ret_v = read_adc_mv(ADC_IDX_VOLTAGE, &voltage_mv);
		int ret_c = read_adc_mv(ADC_IDX_CURRENT, &current_mv);

		k_mutex_lock(&sensor_lock, K_FOREVER);
		last_rpm = rpm;
		if (ret_v == 0) {
			last_voltage_mv = voltage_mv * VOLTAGE_DIVIDER_RATIO;
		}
		if (ret_c == 0) {
			last_current_ma =
				((current_mv - acs712_zero_offset_mv) * 1000) / ACS712_MV_PER_A;
		}
		k_mutex_unlock(&sensor_lock);
	}
}

static int send_response(uint8_t status, uint8_t cmd, const uint8_t *payload, size_t payload_len)
{
	struct can_frame tx_frame = {
		.flags = 0,
		.id = NODE_TO_MASTER_ID,
		.dlc = 8,
		.data = {NODE_ID, status, cmd, 0, 0, 0, 0, 0},
	};

	payload_len = MIN(payload_len, sizeof(tx_frame.data) - 3);
	memcpy(&tx_frame.data[3], payload, payload_len);

	int ret = can_send(can_dev, &tx_frame, K_MSEC(100), NULL, NULL);
	if (ret != 0) {
		printf("CAN tx failed: %d\n", ret);
	}

	return ret;
}

static int send_status(uint8_t status, uint8_t cmd, uint8_t value0, uint8_t value1)
{
	uint8_t payload[2] = {value0, value1};

	return send_response(status, cmd, payload, sizeof(payload));
}

static int set_fan_speed(uint8_t level)
{
	if (level > FAN_SPEED_LEVEL_MAX) {
		return -EINVAL;
	}

	uint32_t pulse_ns = (uint32_t)(((uint64_t)fan_pwm.period * fan_duty_pct[level]) / 100U);

	int ret = pwm_set_pulse_dt(&fan_pwm, pulse_ns);
	if (ret == 0) 
	{
		fan_speed_level = level;
	}

	return ret;
}

static void dump_can_frame(const struct can_frame *frame)
{
	printf("CAN RX id=0x%03X dlc=%u data=", frame->id, frame->dlc);

	for (uint8_t i = 0U; i < frame->dlc && i < sizeof(frame->data); i++) {
		printf("%02X", frame->data[i]);
		if (i + 1U < frame->dlc && i + 1U < sizeof(frame->data)) {
			printf(" ");
		}
	}

	printf("\n");
}

int main(void)
{
	const struct can_filter cmd_filter = {
		.flags = 0U,
		.id = MASTER_TO_NODE_ID,
		.mask = CAN_STD_ID_MASK,
	};
	struct can_frame rx_frame;
	uint8_t heartbeat = 0;
	int filter_id;
	int ret;

	if (!device_is_ready(can_dev)) {
		printf("CAN device not ready: %s\n", can_dev->name);
		return 0;
	}
	printf("CAN device is ready\r\n");

	if (!gpio_is_ready_dt(&led)) {
		printf("LED GPIO not ready\n");
		return 0;
	}

	if (!pwm_is_ready_dt(&fan_pwm)) {
		printf("Fan PWM not ready\n");
		return 0;
	}

	if (!gpio_is_ready_dt(&fan_tach)) {
		printf("Fan tach GPIO not ready\n");
		return 0;
	}

	for (size_t i = 0U; i < ARRAY_SIZE(adc_channels); i++) {
		if (!adc_is_ready_dt(&adc_channels[i])) {
			printf("ADC controller %s not ready\n", adc_channels[i].dev->name);
			return 0;
		}

		ret = adc_channel_setup_dt(&adc_channels[i]);
		if (ret < 0) {
			printf("ADC channel #%zu setup failed: %d\n", i, ret);
			return 0;
		}
	}

	printf("ADC Initialized...\r\n");

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		printf("LED configure failed: %d\n", ret);
		return 0;
	}

	ret = gpio_pin_configure_dt(&fan_tach, GPIO_INPUT);
	if (ret < 0) {
		printf("Fan tach configure failed: %d\n", ret);
		return 0;
	}

	ret = gpio_pin_interrupt_configure_dt(&fan_tach, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret < 0) {
		printf("Fan tach interrupt configure failed: %d\n", ret);
		return 0;
	}

	gpio_init_callback(&tach_cb_data, tach_isr, BIT(fan_tach.pin));
	gpio_add_callback(fan_tach.port, &tach_cb_data);

	k_mutex_init(&sensor_lock);

	printf("Fan control node starting...\n");
	/* Fan is off (0% duty) at this point; safe moment to zero the current sensor. */
	set_fan_speed(0);
	calibrate_acs712_zero();

	k_thread_create(&sensor_thread_data, sensor_thread_stack,
			 K_THREAD_STACK_SIZEOF(sensor_thread_stack), sensor_thread_fn, NULL, NULL,
			 NULL, K_PRIO_PREEMPT(7), 0, K_NO_WAIT);

	ret = can_start(can_dev);
	if (ret != 0) {
		printf("CAN start failed: %d\n", ret);
		return 0;
	}

	filter_id = can_add_rx_filter_msgq(can_dev, &rx_msgq, &cmd_filter);
	if (filter_id < 0) {
		printf("CAN add filter failed: %d\n", filter_id);
		return 0;
	}

	printf("Fan control node ready. RX id=0x%03X TX id=0x%03X\n", MASTER_TO_NODE_ID,
	       NODE_TO_MASTER_ID);
	send_status(STATUS_OK, CMD_PING, 0xAA, 0x55);

	while (1) {
		ret = k_msgq_get(&rx_msgq, &rx_frame, K_FOREVER);
		if (ret != 0) {
			continue;
		}

		dump_can_frame(&rx_frame);

		if (rx_frame.dlc < 1U) {
			send_status(STATUS_UNKNOWN_CMD, 0x00, 0, 0);
			continue;
		}

		switch (rx_frame.data[0]) {
		case CMD_SET_LED: {
			if (rx_frame.dlc < 2U) {
				send_status(STATUS_UNKNOWN_CMD, CMD_SET_LED, 0, 0);
				break;
			}

			uint8_t led_on = rx_frame.data[1] ? 1U : 0U;
			ret = gpio_pin_set_dt(&led, led_on);
			if (ret < 0) {
				send_status((uint8_t)(-ret), CMD_SET_LED, led_on, 0);
				break;
			}

			send_status(STATUS_OK, CMD_SET_LED, led_on, 0);
			break;
		}

		case CMD_GET_STATUS:
			heartbeat++;
			send_status(STATUS_OK, CMD_GET_STATUS, heartbeat,
				    (uint8_t)gpio_pin_get_dt(&led));
			break;

		case CMD_PING:
			send_status(STATUS_OK, CMD_PING, 0x50, 0x4F);
			break;

		case CMD_SET_FAN_SPEED: {
			if (rx_frame.dlc < 2U) {
				send_status(STATUS_UNKNOWN_CMD, CMD_SET_FAN_SPEED, 0, 0);
				break;
			}

			uint8_t level = rx_frame.data[1];

			ret = set_fan_speed(level);
			
			if (ret < 0) {
				send_status(STATUS_BAD_ARG, CMD_SET_FAN_SPEED, level, 0);
				break;
			}

			send_status(STATUS_OK, CMD_SET_FAN_SPEED, level, fan_duty_pct[level]);
			break;
		}

		case CMD_GET_FAN_SPEED:
			send_status(STATUS_OK, CMD_GET_FAN_SPEED, fan_speed_level,
				    fan_duty_pct[fan_speed_level]);
			break;

		case CMD_GET_TACH: {
			k_mutex_lock(&sensor_lock, K_FOREVER);
			uint16_t rpm = last_rpm;
			k_mutex_unlock(&sensor_lock);

			uint8_t payload[2] = {(uint8_t)(rpm >> 8), (uint8_t)(rpm & 0xFF)};

			send_response(STATUS_OK, CMD_GET_TACH, payload, sizeof(payload));
			break;
		}

		case CMD_GET_POWER: {
			k_mutex_lock(&sensor_lock, K_FOREVER);
			int32_t voltage_mv = last_voltage_mv;
			int32_t current_ma = last_current_ma;
			k_mutex_unlock(&sensor_lock);

			uint16_t voltage_u16 = (uint16_t)CLAMP(voltage_mv, 0, UINT16_MAX);
			int16_t current_i16 = (int16_t)CLAMP(current_ma, INT16_MIN, INT16_MAX);

			uint8_t payload[4] = {
				(uint8_t)(voltage_u16 >> 8),
				(uint8_t)(voltage_u16 & 0xFF),
				(uint8_t)(((uint16_t)current_i16) >> 8),
				(uint8_t)(((uint16_t)current_i16) & 0xFF),
			};

			send_response(STATUS_OK, CMD_GET_POWER, payload, sizeof(payload));
			break;
		}

		default:
			send_status(STATUS_UNKNOWN_CMD, rx_frame.data[0], 0, 0);
			break;
		}
	}

	return 0;
}
