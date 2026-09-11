#include "rs485_uart.h"
#include "rs485_envelope.h"

#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>

#define UART_NODE     DT_NODELABEL(lpuart1)
#define RS485_DE_NODE DT_NODELABEL(rs485_de)

#if !DT_NODE_HAS_STATUS(UART_NODE, okay)
#error "lpuart1 is not enabled in devicetree"
#endif

#if !DT_NODE_HAS_STATUS(RS485_DE_NODE, okay)
#error "rs485_de node is not enabled in devicetree"
#endif

static const struct device *const uart_dev = DEVICE_DT_GET(UART_NODE);
static const struct gpio_dt_spec rs485_de = GPIO_DT_SPEC_GET(RS485_DE_NODE, gpios);

/* Fragments (complete lines) waiting to be decoded off the ISR's hands. */
K_MEM_SLAB_DEFINE(rs485_rx_slab, sizeof(struct rs485_rx_buf), RS485_RX_BUF_COUNT, 1);

static rs485_rx_frag_fn *rs485_rx_cb;
static rs485_rx_idle_fn *rs485_rx_idle_cb;
static struct rs485_rx_buf *rs485_cur_rx_buf;
static bool rs485_rx_ignoring;

/* A noise burst or aborted transfer with no trailing '\n' would otherwise
 * leave a partial line sitting forever, corrupting whatever legitimate bytes
 * arrive next. Restarted on every received byte; on expiry, discards any
 * in-progress line and lets a higher layer (the SMP transport) discard its
 * own in-progress multi-line reassembly state too via rs485_rx_idle_cb.
 */
#define RS485_RX_INACTIVITY_TIMEOUT_MS 300

static void rs485_rx_timeout_expired(struct k_timer *timer)
{
	ARG_UNUSED(timer);

	unsigned int key = irq_lock();

	if (rs485_cur_rx_buf != NULL) {
		rs485_uart_free_rx_buf(rs485_cur_rx_buf);
		rs485_cur_rx_buf = NULL;
	}
	rs485_rx_ignoring = false;

	irq_unlock(key);

	if (rs485_rx_idle_cb != NULL) {
		rs485_rx_idle_cb();
	}
}

K_TIMER_DEFINE(rs485_rx_timeout_timer, rs485_rx_timeout_expired, NULL);

static struct rs485_rx_buf *rs485_alloc_rx_buf(void)
{
	struct rs485_rx_buf *rx_buf;
	void *block;

	if (k_mem_slab_alloc(&rs485_rx_slab, &block, K_NO_WAIT) != 0) {
		return NULL;
	}

	rx_buf = block;
	rx_buf->length = 0;
	return rx_buf;
}

void rs485_uart_free_rx_buf(struct rs485_rx_buf *rx_buf)
{
	k_mem_slab_free(&rs485_rx_slab, rx_buf);
}

void rs485_uart_register_rx_cb(rs485_rx_frag_fn *cb)
{
	rs485_rx_cb = cb;
}

void rs485_uart_register_rx_idle_cb(rs485_rx_idle_fn *cb)
{
	rs485_rx_idle_cb = cb;
}

/* Accumulates one incoming (already envelope-unwrapped) payload byte;
 * dispatches to the registered rx callback once a line (terminated by '\n')
 * has been received, mirroring uart_mcumgr_rx_byte().
 *
 * rs485_cur_rx_buf/rs485_rx_ignoring are shared with the RX inactivity timer
 * (rs485_rx_timeout_expired(), a separate interrupt context) and so are
 * protected by irq_lock() here, not just implicitly single-threaded as when
 * only the UART ISR touched them.
 */
void rs485_uart_feed_payload_byte(uint8_t byte)
{
	struct rs485_rx_buf *rx_buf;
	struct rs485_rx_buf *completed = NULL;
	unsigned int key;

	/* Restart the "no byte for N ms" clock on every byte - k_timer_start()
	 * internally re-arms from now even if already running.
	 */
	k_timer_start(&rs485_rx_timeout_timer, K_MSEC(RS485_RX_INACTIVITY_TIMEOUT_MS), K_NO_WAIT);

	key = irq_lock();

	if (!rs485_rx_ignoring) {
		if (rs485_cur_rx_buf == NULL) {
			rs485_cur_rx_buf = rs485_alloc_rx_buf();
			if (rs485_cur_rx_buf == NULL) {
				rs485_rx_ignoring = true;
			}
		}
	}

	rx_buf = rs485_cur_rx_buf;
	if (!rs485_rx_ignoring) {
		if (rx_buf->length >= sizeof(rx_buf->data)) {
			rs485_uart_free_rx_buf(rs485_cur_rx_buf);
			rs485_cur_rx_buf = NULL;
			rs485_rx_ignoring = true;
		} else {
			rx_buf->data[rx_buf->length++] = byte;
		}
	}

	if (byte == '\n') {
		if (rs485_rx_ignoring) {
			rs485_rx_ignoring = false;
		} else {
			rs485_cur_rx_buf = NULL;
			completed = rx_buf;
		}
	}

	irq_unlock(key);

	if (completed != NULL && rs485_rx_cb != NULL) {
		rs485_rx_cb(completed);
	}
}

static void rs485_uart_isr(const struct device *dev, void *user_data)
{
	uint8_t buf[32];
	int chunk_len;

	ARG_UNUSED(user_data);

	while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
		if (!uart_irq_rx_ready(dev)) {
			continue;
		}

		chunk_len = uart_fifo_read(dev, buf, sizeof(buf));
		for (int i = 0; i < chunk_len; i++) {
			rs485_envelope_rx_byte(buf[i]);
		}
	}
}

void rs485_uart_de_assert(void)
{
	gpio_pin_set_dt(&rs485_de, 1);
	k_busy_wait(100);
}

void rs485_uart_de_deassert(void)
{
	/* uart_poll_out() only guarantees the shift register was free for the
	 * PREVIOUS byte, not that the LAST byte finished shifting out onto the
	 * wire - wait comfortably longer than one frame time (10 bits @ 115200
	 * baud = ~87us) before dropping DE, exactly as validated in the rs485
	 * test project.
	 */
	k_busy_wait(200);
	k_busy_wait(20);
	gpio_pin_set_dt(&rs485_de, 0);
}

int rs485_uart_raw_send(const void *data, int len)
{
	const uint8_t *p = data;

	while (len--) {
		uart_poll_out(uart_dev, *p++);
	}

	return 0;
}

int rs485_uart_init(void)
{
	if (!device_is_ready(uart_dev)) {
		return -ENODEV;
	}

	if (!gpio_is_ready_dt(&rs485_de)) {
		return -ENODEV;
	}

	int ret = gpio_pin_configure_dt(&rs485_de, GPIO_OUTPUT_INACTIVE);

	if (ret != 0) {
		return ret;
	}

	uart_irq_rx_disable(uart_dev);
	uart_irq_tx_disable(uart_dev);
	uart_irq_callback_set(uart_dev, rs485_uart_isr);
	uart_irq_rx_enable(uart_dev);

	return 0;
}
