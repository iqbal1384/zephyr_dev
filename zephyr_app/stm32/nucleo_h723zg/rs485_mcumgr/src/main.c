#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/sys/printk.h>

#include "smp_rs485_transport.h"

#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)
#define WDT_NODE  DT_ALIAS(watchdog0)

/* LED1_TOGGLE_MS is a half-period (toggle interval), not a full cycle, so
 * 500ms here gives a 1Hz on/off blink rate - visual proof a new image (this
 * one) is actually running after an mcumgr update, distinct from led0's
 * faster 200ms heartbeat that both versions share.
 */
#define LED1_TOGGLE_MS 500

static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
static const struct device *const wdt_dev = DEVICE_DT_GET(WDT_NODE);
static int wdt_channel_id = -1;

/*
 * Safety-net fallback: if the Pi never explicitly sends `mcumgr image
 * confirm` (mgmt/mcumgr/grp/img_mgmt/src/zephyr_img_mgmt.c's
 * img_mgmt_write_confirmed() already calls boot_write_img_confirmed() for
 * that path - no device code needed for it), the device confirms itself
 * after this many seconds of continuous, healthy operation, so it doesn't
 * stay in a perpetually-revertible "test" state for its whole operational
 * life. boot_is_img_confirmed() makes this a no-op if the Pi already
 * confirmed first.
 */
static void self_confirm_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	if (!boot_is_img_confirmed()) {
		int rc = boot_write_img_confirmed();

		printk("rs485_mcumgr: self-confirm %s\n", rc == 0 ? "ok" : "failed");
	}
}
K_WORK_DELAYABLE_DEFINE(self_confirm_work, self_confirm_work_handler);

/*
 * Arms the IWDG so a bad update that hangs/crashes gets reset rather than
 * bricking the board: since the new image is never confirmed in that case,
 * MCUboot reverts to the last-known-good image on the next boot.
 * WDT_OPT_PAUSE_HALTED_BY_DBG freezes IWDG only while a debugger has the
 * core halted at a breakpoint - a no-op in production, but removes the
 * "resets while stepping" annoyance during development.
 */
static int watchdog_arm(void)
{
	if (!device_is_ready(wdt_dev)) {
		printk("rs485_mcumgr: watchdog not ready\n");
		return -ENODEV;
	}

	struct wdt_timeout_cfg wdt_cfg = {
		.window = {.min = 0, .max = CONFIG_APP_WDT_TIMEOUT_MS},
		.callback = NULL,
		.flags = WDT_FLAG_RESET_SOC,
	};

	wdt_channel_id = wdt_install_timeout(wdt_dev, &wdt_cfg);
	if (wdt_channel_id < 0) {
		printk("rs485_mcumgr: wdt_install_timeout failed: %d\n", wdt_channel_id);
		return wdt_channel_id;
	}

	return wdt_setup(wdt_dev, WDT_OPT_PAUSE_HALTED_BY_DBG);
}

int main(void)
{
	int ret;

	printk("rs485_mcumgr: starting (blinky v3, led1 1Hz)\n");

	ret = watchdog_arm();
	if (ret != 0) {
		printk("rs485_mcumgr: continuing without watchdog: %d\n", ret);
	}

	if (!gpio_is_ready_dt(&led0)) {
		printk("rs485_mcumgr: led0 not ready\n");
		return 0;
	}

	ret = gpio_pin_configure_dt(&led0, GPIO_OUTPUT_INACTIVE);
	if (ret != 0) {
		printk("rs485_mcumgr: led0 configure failed: %d\n", ret);
		return 0;
	}

	if (!gpio_is_ready_dt(&led1)) {
		printk("rs485_mcumgr: led1 not ready\n");
		return 0;
	}

	ret = gpio_pin_configure_dt(&led1, GPIO_OUTPUT_INACTIVE);
	if (ret != 0) {
		printk("rs485_mcumgr: led1 configure failed: %d\n", ret);
		return 0;
	}

	ret = smp_rs485_transport_init();
	if (ret != 0) {
		printk("rs485_mcumgr: transport init failed: %d\n", ret);
		return 0;
	}

	printk("rs485_mcumgr: ready, mcumgr reachable over RS485\n");

	/*
	 * Do NOT confirm immediately - give the Pi a chance to verify the new
	 * image is actually healthy and confirm it explicitly first. Schedule
	 * the grace-period fallback in case it never gets around to it.
	 */
	k_work_schedule(&self_confirm_work, K_SECONDS(CONFIG_APP_SELF_CONFIRM_GRACE_PERIOD_S));

	int elapsed_ms = 0;

	while (1) {
		gpio_pin_toggle_dt(&led0);
		if (wdt_channel_id >= 0) {
			wdt_feed(wdt_dev, wdt_channel_id);
		}

		elapsed_ms += 200;
		if (elapsed_ms >= LED1_TOGGLE_MS) {
			gpio_pin_toggle_dt(&led1);
			elapsed_ms -= LED1_TOGGLE_MS;
		}

		k_msleep(200);
	}

	return 0;
}
