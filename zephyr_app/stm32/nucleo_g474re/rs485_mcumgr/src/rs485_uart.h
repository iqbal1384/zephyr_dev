#ifndef RS485_UART_H_
#define RS485_UART_H_

#include <stdint.h>
#include <zephyr/types.h>

/* Must satisfy RS485_RX_BUF_COUNT * RS485_RX_BUF_SIZE >= CONFIG_MCUMGR_TRANSPORT_NETBUF_SIZE,
 * mirroring the relation Zephyr's own uart_mcumgr driver documents for
 * CONFIG_UART_MCUMGR_RX_BUF_COUNT/CONFIG_UART_MCUMGR_RX_BUF_SIZE.
 */
#define RS485_RX_BUF_SIZE  128
#define RS485_RX_BUF_COUNT 4

/** A single received mcumgr-over-serial fragment (one line, up to and including '\n'). */
struct rs485_rx_buf {
	void *fifo_reserved; /* 1st word reserved for use by k_fifo */
	uint8_t data[RS485_RX_BUF_SIZE];
	int length;
};

/** Called from ISR context when a complete line has been received. */
typedef void rs485_rx_frag_fn(struct rs485_rx_buf *rx_buf);

/**
 * Called (from timer-ISR context) when no byte has arrived for
 * RS485_RX_INACTIVITY_TIMEOUT_MS - lets a higher layer (the SMP transport)
 * discard any of its own in-progress multi-line reassembly state, since the
 * line-level buffer that fed it has just been discarded too.
 */
typedef void rs485_rx_idle_fn(void);

/**
 * Brings up the USART1 device (already pinned to PC4/PC5 = Arduino D0/D1 by
 * the board's default devicetree) and the RS485 DE/RE GPIO (PA8 = Arduino D7),
 * and starts interrupt-driven RX byte capture. Must be called before any
 * other rs485_uart_* function.
 */
int rs485_uart_init(void);

/** Registers the callback invoked (from ISR context) for each completed line. */
void rs485_uart_register_rx_cb(rs485_rx_frag_fn *cb);

/**
 * Feeds one payload byte (already unwrapped/validated by the addressing
 * envelope layer, rs485_envelope.c) into the line accumulator. Invokes the
 * registered rx callback itself once a complete line ('\n'-terminated) has
 * been accumulated. Runs in ISR context (called from the envelope layer's
 * own ISR-context byte handler).
 */
void rs485_uart_feed_payload_byte(uint8_t byte);

/**
 * Registers the callback invoked (from timer-ISR context) when the RX
 * inactivity timeout fires, i.e. a partial/aborted line was just discarded.
 */
void rs485_uart_register_rx_idle_cb(rs485_rx_idle_fn *cb);

/** Returns a completed rx buffer to the pool once its contents have been processed. */
void rs485_uart_free_rx_buf(struct rs485_rx_buf *rx_buf);

/**
 * Raises DE (transmit mode) and waits for the transceiver to settle. Must be
 * paired with a subsequent rs485_uart_de_deassert() call.
 */
void rs485_uart_de_assert(void);

/**
 * Waits for the last transmitted byte to finish shifting out, then drops DE
 * (receive mode).
 */
void rs485_uart_de_deassert(void);

/**
 * Dumb byte pusher with no DE handling of its own - matches the
 * mcumgr_serial_tx_cb signature exactly so it can be passed directly to
 * mcumgr_serial_tx_pkt(). Callers must wrap it with
 * rs485_uart_de_assert()/rs485_uart_de_deassert().
 */
int rs485_uart_raw_send(const void *data, int len);

#endif /* RS485_UART_H_ */
