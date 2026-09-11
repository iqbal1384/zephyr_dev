/*
 * Custom mcumgr SMP transport carrying SMP-over-serial (base64 + CRC16)
 * framing over the RS485 link, reusing Zephyr's own shared encode/decode
 * helpers (mcumgr_serial_tx_pkt/mcumgr_serial_process_frag - the same ones
 * zephyr/subsys/mgmt/mcumgr/transport/src/smp_uart.c uses) instead of
 * CONFIG_MCUMGR_TRANSPORT_UART, since that built-in transport's raw TX path
 * (uart_mcumgr_send_raw() in zephyr/drivers/console/uart_mcumgr.c) is a plain
 * uart_poll_out() loop with no hook for RS485 DE control, and it's tied to a
 * fixed zephyr,uart-mcumgr chosen device we'd rather not repurpose.
 *
 * This file mirrors smp_uart.c's fifo+work deferred-decode structure almost
 * exactly; the only real differences are the underlying UART device/ISR
 * (rs485_uart.c, not the vendored uart_mcumgr driver) and wrapping the whole
 * mcumgr_serial_tx_pkt() call with RS485 DE assert/deassert.
 *
 * smp_rx_req()/smp_packet_free() come from
 * mgmt/mcumgr/transport/smp_internal.h, which lives outside include/zephyr/
 * and is not a documented stable API - it's the same internal header
 * smp_uart.c itself relies on, and there's no public alternative for this.
 * Worth re-checking against this file on future Zephyr version bumps.
 */

#include "smp_rs485_transport.h"
#include "rs485_envelope.h"
#include "rs485_uart.h"

#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/net_buf.h>
#include <zephyr/mgmt/mcumgr/smp/smp.h>
#include <zephyr/mgmt/mcumgr/transport/smp.h>
#include <zephyr/mgmt/mcumgr/transport/serial.h>

#include <mgmt/mcumgr/transport/smp_internal.h>

static void smp_rs485_process_rx_queue(struct k_work *work);

K_FIFO_DEFINE(smp_rs485_rx_fifo);
K_WORK_DEFINE(smp_rs485_work, smp_rs485_process_rx_queue);

static struct mcumgr_serial_rx_ctxt smp_rs485_rx_ctxt;
static struct smp_transport smp_rs485_transport;

/** Processes a single completed line (fragment) off the deferred work queue. */
static void smp_rs485_process_frag(struct rs485_rx_buf *rx_buf)
{
	struct net_buf *nb;

	nb = mcumgr_serial_process_frag(&smp_rs485_rx_ctxt, rx_buf->data, rx_buf->length);

	rs485_uart_free_rx_buf(rx_buf);

	if (nb != NULL) {
		smp_rx_req(&smp_rs485_transport, nb);
	}
}

static void smp_rs485_process_rx_queue(struct k_work *work)
{
	ARG_UNUSED(work);

	struct rs485_rx_buf *rx_buf;

	while ((rx_buf = k_fifo_get(&smp_rs485_rx_fifo, K_NO_WAIT)) != NULL) {
		smp_rs485_process_frag(rx_buf);
	}
}

/** Enqueues a received fragment for later processing. Runs in ISR context. */
static void smp_rs485_rx_frag(struct rs485_rx_buf *rx_buf)
{
	k_fifo_put(&smp_rs485_rx_fifo, rx_buf);
	k_work_submit(&smp_rs485_work);
}

/*
 * mcumgr_serial_process_frag() reassembles a whole SMP packet across
 * multiple lines (a PKT line plus FRAG continuation lines) into
 * smp_rs485_rx_ctxt.nb - MCUMGR_SERIAL_MAX_FRAME is only 127 bytes vs our
 * 384-byte MTU, so this routinely spans multiple lines. If the RS485 line
 * buffer feeding it was just discarded by rs485_uart's inactivity timeout
 * (a dropped/corrupted continuation line), this in-progress net_buf would
 * otherwise leak forever, eventually exhausting the whole
 * CONFIG_MCUMGR_TRANSPORT_NETBUF_COUNT pool shared by all of mcumgr. Runs
 * in timer-ISR context; smp_packet_free() is net_buf-unref based and
 * ISR-safe, so no k_work deferral is needed here.
 */
static void smp_rs485_rx_idle(void)
{
	if (smp_rs485_rx_ctxt.nb != NULL) {
		smp_packet_free(smp_rs485_rx_ctxt.nb);
		smp_rs485_rx_ctxt.nb = NULL;
	}
}

static uint16_t smp_rs485_get_mtu(const struct net_buf *nb)
{
	ARG_UNUSED(nb);

	return CONFIG_MCUMGR_TRANSPORT_NETBUF_SIZE;
}

static int smp_rs485_tx_pkt(struct net_buf *nb)
{
	int rc;

	/* mcumgr_serial_tx_pkt() invokes its raw callback many times per
	 * packet (once per ~3-4 base64-encoded bytes) - rs485_envelope_tx_collect()
	 * just buffers each chunk; rs485_envelope_tx_flush() wraps the WHOLE
	 * collected packet in one addressed envelope frame and sends it
	 * DE-guarded as a single contiguous write, once the total length is
	 * known. Toggling DE per-callback instead would fragment one
	 * continuous transmission into dozens of DE toggles per packet.
	 */
	rc = mcumgr_serial_tx_pkt(nb->data, nb->len, rs485_envelope_tx_collect);
	rs485_envelope_tx_flush(RS485_ADDR_HOST);

	smp_packet_free(nb);

	return rc;
}

int smp_rs485_transport_init(void)
{
	int rc;

	rc = rs485_envelope_init();
	if (rc != 0) {
		return rc;
	}

	smp_rs485_transport.functions.output = smp_rs485_tx_pkt;
	smp_rs485_transport.functions.get_mtu = smp_rs485_get_mtu;

	rc = smp_transport_init(&smp_rs485_transport);
	if (rc != 0) {
		return rc;
	}

	rs485_uart_register_rx_cb(smp_rs485_rx_frag);
	rs485_uart_register_rx_idle_cb(smp_rs485_rx_idle);

	return 0;
}
