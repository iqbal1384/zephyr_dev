/*
 * Zephyr Blinky for STM32MP257F-DK M33 Core
 * 
 * Blinks LED0 (ORANGE, GPIO H6) every 1 second
 * LED0 not claimed by Linux, available for M33 firmware
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

#define LED0_NODE DT_ALIAS(led0)

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

int main(void)
{
	int ret;

	if (!gpio_is_ready_dt(&led)) {
		printk("LED GPIO device %s is not ready\n", led.port->name);
		return 0;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		printk("Failed to configure LED GPIO pin: %d\n", ret);
		return 0;
	}

	printk("M33 Blinky started - Blinking LED0 (ORANGE) every 1 second\n");

	while (1) {
		ret = gpio_pin_toggle_dt(&led);
		if (ret < 0) {
			printk("Failed to toggle LED: %d\n", ret);
			return 0;
		}
		k_msleep(1000); /* 1 second */
	}

	return 0;
}
