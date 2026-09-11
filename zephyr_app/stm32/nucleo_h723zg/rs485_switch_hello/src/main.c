/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * rs485_switch_hello — NUCLEO-H723ZG side.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>
#include <string.h>

#define LPUART1_NODE   DT_NODELABEL(lpuart1)
#define CONN1_OUT_NODE DT_ALIAS(conn1_out)
#define RS485_DE_NODE  DT_ALIAS(rs485_de)
#define IRQ_IN_NODE    DT_ALIAS(irq_in)

static const struct device *const uart_dev = DEVICE_DT_GET(LPUART1_NODE);

static const struct gpio_dt_spec conn1_out =
	GPIO_DT_SPEC_GET(CONN1_OUT_NODE, gpios);
static const struct gpio_dt_spec rs485_de =
	GPIO_DT_SPEC_GET(RS485_DE_NODE, gpios);
static const struct gpio_dt_spec irq_in =
	GPIO_DT_SPEC_GET(IRQ_IN_NODE, gpios);

static const char rs485_msg[] = "Hello testing spring from nucleo\r\n";

#define DE_SETTLE_US   10
#define DE_TAIL_US    200
#define CONN1_HOLD_MS 200
#define SWITCH_SETTLE_MS 1

static struct k_work trigger_work;
static volatile uint32_t irq_count = 0;

static void rs485_send(const char *msg, size_t len)
{
	gpio_pin_set_dt(&rs485_de, 1);
	k_busy_wait(DE_SETTLE_US);

	for (size_t i = 0; i < len; i++) {
		uart_poll_out(uart_dev, msg[i]);
	}

	k_busy_wait(DE_TAIL_US);
	gpio_pin_set_dt(&rs485_de, 0);
}

static void do_trigger(struct k_work *work)
{
	ARG_UNUSED(work);

	uint32_t count = irq_count;
	int val = gpio_pin_get_dt(&irq_in);
	printk("\n▶ [IRQ #%u] Trigger event on PD11 (current pin level = %d)\n", count, val);

	/* 1. Drive Conn_1 (PE3) HIGH */
	gpio_pin_set_dt(&conn1_out, 1);
	printk("  Conn_1 (PE3) → HIGH [RS-485 channel 1 enabled]\n");

	k_msleep(SWITCH_SETTLE_MS);

	/* 2. Send RS-485 message */
	printk("  Sending RS-485: \"%s\"", rs485_msg);
	rs485_send(rs485_msg, sizeof(rs485_msg) - 1);
	printk("  RS-485 transmission complete.\n");

	/* 3. Hold Conn_1 HIGH then release */
	k_msleep(CONN1_HOLD_MS);
	gpio_pin_set_dt(&conn1_out, 0);
	printk("  Conn_1 (PE3) → LOW [cycle complete]\n\n");
}

static struct gpio_callback irq_cb_data;

static void trigger_irq_handler(const struct device *dev,
				 struct gpio_callback *cb,
				 uint32_t pins)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);

	irq_count++;
	k_work_submit(&trigger_work);
}

int main(void)
{
	int rc;

	printk("\n==================================================\n");
	printk("=== rs485_switch_hello (NUCLEO-H723ZG) ===\n");
	printk("==================================================\n");
	printk("Waiting for interrupt edge on PD11 (from Curie PJ12).\n");
	printk("On trigger: PE3 HIGH (Conn_1) → RS-485 msg → PE3 LOW.\n");
	printk("Sniffer: waveshare_rs485 on /dev/ttyACM0 at 115200.\n\n");

	if (!device_is_ready(uart_dev)) {
		printk("ERROR: LPUART1 not ready\n");
		return 0;
	}

	if (!gpio_is_ready_dt(&conn1_out)) {
		printk("ERROR: conn1_out (PE3) not ready\n");
		return 0;
	}
	rc = gpio_pin_configure_dt(&conn1_out, GPIO_OUTPUT_INACTIVE);
	if (rc < 0) {
		printk("ERROR: conn1_out configure: %d\n", rc);
		return 0;
	}

	if (!gpio_is_ready_dt(&rs485_de)) {
		printk("ERROR: rs485_de (PG12) not ready\n");
		return 0;
	}
	rc = gpio_pin_configure_dt(&rs485_de, GPIO_OUTPUT_INACTIVE);
	if (rc < 0) {
		printk("ERROR: rs485_de configure: %d\n", rc);
		return 0;
	}

	/*
	 * Configure PD11 with PULL_UP (for open-collector Curie output)
	 * and trigger on BOTH edges so both rising and falling edges wake it.
	 */
	if (!gpio_is_ready_dt(&irq_in)) {
		printk("ERROR: irq_in (PD11) not ready\n");
		return 0;
	}
	rc = gpio_pin_configure_dt(&irq_in, GPIO_INPUT | GPIO_PULL_UP);
	if (rc < 0) {
		printk("ERROR: irq_in configure: %d\n", rc);
		return 0;
	}
	rc = gpio_pin_interrupt_configure_dt(&irq_in, GPIO_INT_EDGE_BOTH);
	if (rc < 0) {
		printk("ERROR: irq_in interrupt configure: %d\n", rc);
		return 0;
	}

	k_work_init(&trigger_work, do_trigger);

	gpio_init_callback(&irq_cb_data, trigger_irq_handler, BIT(irq_in.pin));
	rc = gpio_add_callback(irq_in.port, &irq_cb_data);
	if (rc < 0) {
		printk("ERROR: gpio_add_callback: %d\n", rc);
		return 0;
	}

	int initial_val = gpio_pin_get_dt(&irq_in);
	printk("All GPIOs configured. IRQ armed on PD11 (EDGE_BOTH, PULL_UP).\n");
	printk("Initial PD11 level = %d\n", initial_val);
	printk("Listening...\n\n");

	while (1) {
		k_sleep(K_SECONDS(5));
		int cur_val = gpio_pin_get_dt(&irq_in);
		printk("[Status] PD11 = %d, total triggers handled = %u\n", cur_val, irq_count);
	}

	return 0;
}
