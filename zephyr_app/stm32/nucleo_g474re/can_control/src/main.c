#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <zephyr/device.h>
#include <zephyr/drivers/can.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#define MASTER_TO_NODE_ID 0x120
#define NODE_TO_MASTER_ID 0x121

#define CMD_SET_LED 0x01
#define CMD_GET_STATUS 0x02
#define CMD_PING 0x03

#define STATUS_OK 0x00
#define STATUS_UNKNOWN_CMD 0xEE

#define NODE_ID 0x01

const struct device *const can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

CAN_MSGQ_DEFINE(rx_msgq, 16);

static int send_status(uint8_t status, uint8_t cmd, uint8_t value0, uint8_t value1)
{
	struct can_frame tx_frame = {
		.flags = 0,
		.id = NODE_TO_MASTER_ID,
		.dlc = 8,
		.data = {NODE_ID, status, cmd, value0, value1, 0, 0, 0},
	};

	int ret = can_send(can_dev, &tx_frame, K_MSEC(100), NULL, NULL);
	if (ret != 0) {
		printf("CAN tx failed: %d\n", ret);
	}

	return ret;
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

	if (!gpio_is_ready_dt(&led)) {
		printf("LED GPIO not ready\n");
		return 0;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		printf("LED configure failed: %d\n", ret);
		return 0;
	}

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

	printf("CAN control node ready. RX id=0x%03X TX id=0x%03X\n", MASTER_TO_NODE_ID,
	       NODE_TO_MASTER_ID);
	send_status(STATUS_OK, CMD_PING, 0xAA, 0x55);

	while (1) {
		ret = k_msgq_get(&rx_msgq, &rx_frame, K_FOREVER);
		if (ret != 0) {
			continue;
		}

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

		default:
			send_status(STATUS_UNKNOWN_CMD, rx_frame.data[0], 0, 0);
			break;
		}
	}

	return 0;
}
