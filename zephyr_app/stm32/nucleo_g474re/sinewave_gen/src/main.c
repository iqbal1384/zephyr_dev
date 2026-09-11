/*
 * STM32G474RE Nucleo Sine Wave Generator via DAC1 Channel 1 (PA4)
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/dac.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/counter.h>
#include <math.h>

#define ZEPHYR_USER_NODE DT_PATH(zephyr_user)

#if (DT_NODE_HAS_PROP(ZEPHYR_USER_NODE, dac) && \
	DT_NODE_HAS_PROP(ZEPHYR_USER_NODE, dac_channel_id) && \
	DT_NODE_HAS_PROP(ZEPHYR_USER_NODE, dac_resolution))
#define DAC_NODE DT_PHANDLE(ZEPHYR_USER_NODE, dac)
#define DAC_CHANNEL_ID DT_PROP(ZEPHYR_USER_NODE, dac_channel_id)
#define DAC_RESOLUTION DT_PROP(ZEPHYR_USER_NODE, dac_resolution)
#else
#error "Unsupported board: check /zephyr,user node in app.overlay"
#endif

/* User LED (LD2) */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios, {0});

static const struct device *const dac_dev = DEVICE_DT_GET(DAC_NODE);
static const struct device *const pacer_dev =
	DEVICE_DT_GET(DT_CHILD(DT_NODELABEL(timers6), counter));

static const struct dac_channel_cfg dac_ch_cfg = {
	.channel_id  = DAC_CHANNEL_ID,
	.resolution  = DAC_RESOLUTION,
	.buffered    = true,
};

#define SINE_POINTS 128
#define TARGET_FREQ_HZ 500

static uint16_t sine_lut[SINE_POINTS];
static volatile uint32_t lut_index;

static void init_sine_table(void)
{
	/* 12-bit DAC: 0 to 4095. Center at 2048, amplitude 1800 (ranges from ~248 to ~3848) */
	const float center = 2048.0f;
	const float amplitude = 1800.0f;
	const float pi = 3.14159265358979323846f;

	for (int i = 0; i < SINE_POINTS; i++) {
		float angle = 2.0f * pi * ((float)i / (float)SINE_POINTS);
		float val = center + amplitude * sinf(angle);
		sine_lut[i] = (uint16_t)val;
	}
}

/* Runs in the TIM6 ISR at a fixed rate (TARGET_FREQ_HZ * SINE_POINTS): just
 * a table lookup and a DAC register write, so the sample period stays
 * deterministic regardless of what the console/USB stack is doing on the
 * main thread. The previous version paced samples with k_busy_wait() in a
 * plain loop, which drifts under IRQ load -> irregular period on a scope. */
static void pacer_tick(const struct device *dev, void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(user_data);

	dac_write_value(dac_dev, DAC_CHANNEL_ID, sine_lut[lut_index]);
	lut_index = (lut_index + 1) % SINE_POINTS;
}

static void led_blink_handler(struct k_timer *timer_id)
{
	ARG_UNUSED(timer_id);
	if (led.port) {
		gpio_pin_toggle_dt(&led);
	}
}

K_TIMER_DEFINE(led_blink_timer, led_blink_handler, NULL);

int main(void)
{
	printk("=========================================\n");
	printk(" STM32G474RE Sine Wave Generator Starting \n");
	printk(" DAC: %s, Channel: %d (PA4 / Pin A2)    \n", dac_dev->name, DAC_CHANNEL_ID);
	printk(" Resolution: %d-bit, LUT Points: %d    \n", DAC_RESOLUTION, SINE_POINTS);
	printk("=========================================\n");

	if (led.port && gpio_is_ready_dt(&led)) {
		gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	}

	if (!device_is_ready(dac_dev)) {
		printk("Error: DAC device %s is not ready!\n", dac_dev->name);
		return 0;
	}

	if (!device_is_ready(pacer_dev)) {
		printk("Error: pacer timer device is not ready!\n");
		return 0;
	}

	int ret = dac_channel_setup(dac_dev, &dac_ch_cfg);
	if (ret != 0) {
		printk("Error: Setting up DAC channel failed with code %d\n", ret);
		return 0;
	}

	init_sine_table();

	uint32_t counter_freq = counter_get_frequency(pacer_dev);
	uint32_t sample_rate_hz = TARGET_FREQ_HZ * SINE_POINTS;
	uint32_t top_ticks = counter_freq / sample_rate_hz;

	printk("Pacer timer freq: %u Hz, sample rate: %u Hz (%u Hz sine), top=%u ticks\n",
		counter_freq, sample_rate_hz, TARGET_FREQ_HZ, top_ticks);

	struct counter_top_cfg top_cfg = {
		.ticks = top_ticks,
		.callback = pacer_tick,
		.user_data = NULL,
		.flags = 0,
	};

	ret = counter_set_top_value(pacer_dev, &top_cfg);
	if (ret != 0) {
		printk("Error: counter_set_top_value failed with code %d\n", ret);
		return 0;
	}

	ret = counter_start(pacer_dev);
	if (ret != 0) {
		printk("Error: counter_start failed with code %d\n", ret);
		return 0;
	}

	printk("Generating continuous %u Hz sine wave, ISR-paced.\n", TARGET_FREQ_HZ);

	k_timer_start(&led_blink_timer, K_SECONDS(1), K_SECONDS(1));

	while (1) {
		k_sleep(K_FOREVER);
	}

	return 0;
}
