/*
 * Multi-drop RS485 addressing layer - see rs485_envelope.h.
 *
 * Frame layout on the wire:
 *
 *   offset  size  field    notes
 *   0       2     SOF      resync marker (RS485_SOF0/1), excluded from CRC
 *   2       1     DEST     destination address
 *   3       2     LEN      big-endian, length of PAYLOAD in bytes
 *   5       LEN   PAYLOAD  byte 0 = KIND, rest is KIND-specific body
 *   5+LEN   2     CRC16    big-endian, crc16_itu_t(0, DEST..end-of-PAYLOAD)
 *
 * On CRC failure (or an implausible LEN), the parser never trusts the
 * claimed length - it falls back to scanning byte-by-byte for the next SOF,
 * so a corrupted address/length byte can't misdeliver a frame or desync the
 * receiver for long.
 *
 * A node addressed by DEST buffers+validates the frame; everyone else just
 * counts down LEN+2 bytes and discards them with zero buffering, and never
 * transmits anything - required since RS485 is a shared broadcast medium
 * and two nodes replying at once is real bus contention.
 */

#include "rs485_envelope.h"
#include "rs485_uart.h"

#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/crc.h>

#define RS485_SOF0 0x9D
#define RS485_SOF1 0x3F

#define KIND_MCUMGR       0x00
#define KIND_PROVISIONING 0x01

#define PROV_SUBCMD_SET_ADDRESS     0x01
#define PROV_SUBCMD_SET_ADDRESS_ACK 0x02

/* Guards both the addressed-buffering and bystander-skip paths of the state
 * machine below - independent of, and in addition to, rs485_uart.c's own
 * line-accumulator inactivity timer, which guards a different layer's
 * partial state. Worst case ~535 bytes at 115200 baud is ~46ms, so 300ms of
 * envelope-level silence is unambiguously an abandoned/corrupted transfer.
 */
#define RS485_ENVELOPE_RX_TIMEOUT_MS 300

/* --- Address provisioning (Settings/NVS, backed by the board's existing
 * storage_partition - no devicetree change needed, settings_nvs.c already
 * falls back to that partition when no zephyr,settings-partition chosen
 * node exists). Sentinel 0xFF matches erased-NOR-flash convention. ---
 */
static uint8_t rs485_my_addr = RS485_ADDR_UNPROVISIONED;

static int rs485_addr_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg)
{
	ARG_UNUSED(name);

	if (len != sizeof(rs485_my_addr)) {
		return -EINVAL;
	}

	ssize_t rc = read_cb(cb_arg, &rs485_my_addr, sizeof(rs485_my_addr));

	return rc < 0 ? (int)rc : 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(rs485_addr, "rs485", NULL, rs485_addr_set, NULL, NULL);

/* --- Low-level framing: build+send one envelope frame. --- */

static void rs485_envelope_send_frame(uint8_t dest, uint8_t kind, const uint8_t *body,
				       uint16_t body_len)
{
	uint16_t payload_len = body_len + 1U; /* +1 for the KIND byte */
	uint8_t sof[2] = {RS485_SOF0, RS485_SOF1};
	uint8_t header[3];
	uint8_t crc_bytes[2];
	uint16_t crc;

	header[0] = dest;
	header[1] = (uint8_t)(payload_len >> 8);
	header[2] = (uint8_t)(payload_len & 0xFF);

	crc = crc16_itu_t(0, header, sizeof(header));
	crc = crc16_itu_t(crc, &kind, 1);
	crc = crc16_itu_t(crc, body, body_len);

	crc_bytes[0] = (uint8_t)(crc >> 8);
	crc_bytes[1] = (uint8_t)(crc & 0xFF);

	rs485_uart_de_assert();
	rs485_uart_raw_send(sof, sizeof(sof));
	rs485_uart_raw_send(header, sizeof(header));
	rs485_uart_raw_send(&kind, 1);
	rs485_uart_raw_send(body, body_len);
	rs485_uart_raw_send(crc_bytes, sizeof(crc_bytes));
	rs485_uart_de_deassert();
}

/* --- TX collect/flush (mcumgr path). --- */

static uint8_t tx_scratch[RS485_ENVELOPE_MAX_PAYLOAD - 1];
static size_t tx_scratch_len;

int rs485_envelope_tx_collect(const void *data, int len)
{
	if (len < 0 || tx_scratch_len + (size_t)len > sizeof(tx_scratch)) {
		return -ENOMEM;
	}

	memcpy(&tx_scratch[tx_scratch_len], data, (size_t)len);
	tx_scratch_len += (size_t)len;

	return 0;
}

int rs485_envelope_tx_flush(uint8_t dest_addr)
{
	rs485_envelope_send_frame(dest_addr, KIND_MCUMGR, tx_scratch, (uint16_t)tx_scratch_len);
	tx_scratch_len = 0;

	return 0;
}

/* --- Provisioning: deferred to a workqueue since it writes flash
 * (settings_save_one() is not ISR-safe), unlike the mcumgr fast path which
 * only ever copies bytes in ISR context.
 * ---
 */

static uint8_t prov_pending_addr;

static void prov_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	uint8_t new_addr = prov_pending_addr;

	if (new_addr < RS485_ADDR_NODE_MIN || new_addr > RS485_ADDR_NODE_MAX) {
		return;
	}

	if (settings_save_one("rs485/addr", &new_addr, sizeof(new_addr)) != 0) {
		return;
	}

	rs485_my_addr = new_addr;

	uint8_t ack_body[2] = {PROV_SUBCMD_SET_ADDRESS_ACK, new_addr};

	rs485_envelope_send_frame(RS485_ADDR_HOST, KIND_PROVISIONING, ack_body,
				   sizeof(ack_body));
}

K_WORK_DEFINE(prov_work, prov_work_handler);

/* --- RX state machine. --- */

enum rx_state {
	RX_WAIT_SOF1,
	RX_WAIT_SOF2,
	RX_WAIT_DEST,
	RX_WAIT_LEN_HI,
	RX_WAIT_LEN_LO,
	RX_PAYLOAD,
	RX_WAIT_CRC_HI,
	RX_WAIT_CRC_LO,
};

static enum rx_state rx_state = RX_WAIT_SOF1;
static uint8_t rx_dest;
static uint16_t rx_len;
static uint16_t rx_count;
static bool rx_buffering;
static uint8_t rx_buf[RS485_ENVELOPE_MAX_PAYLOAD];
static uint8_t rx_crc_hi;

static void rx_reset(void)
{
	rx_state = RX_WAIT_SOF1;
	rx_len = 0;
	rx_count = 0;
	rx_buffering = false;
}

static void rx_timeout_expired(struct k_timer *timer)
{
	ARG_UNUSED(timer);

	unsigned int key = irq_lock();

	rx_reset();

	irq_unlock(key);
}

K_TIMER_DEFINE(rs485_envelope_rx_timer, rx_timeout_expired, NULL);

/* Handles one fully-received, CRC-validated frame addressed to this node.
 * Called with the state machine's irq_lock held (see rs485_envelope_rx_byte()) -
 * the mcumgr replay loop below is bounded (<= RS485_ENVELOPE_MAX_PAYLOAD
 * iterations of simple work), so holding interrupts disabled for it is
 * negligible at 115200 baud and eliminates any race with a new frame
 * starting to overwrite rx_buf before this one is fully replayed.
 */
static void dispatch_frame(uint8_t *buf, uint16_t len)
{
	uint8_t kind = buf[0];
	uint16_t body_len = len - 1U;

	if (kind == KIND_MCUMGR) {
		for (uint16_t i = 0; i < body_len; i++) {
			rs485_uart_feed_payload_byte(buf[1 + i]);
		}
	} else if (kind == KIND_PROVISIONING && body_len == 2 &&
		   buf[1] == PROV_SUBCMD_SET_ADDRESS) {
		prov_pending_addr = buf[2];
		k_work_submit(&prov_work);
	}
}

void rs485_envelope_rx_byte(uint8_t byte)
{
	unsigned int key;

	k_timer_start(&rs485_envelope_rx_timer, K_MSEC(RS485_ENVELOPE_RX_TIMEOUT_MS), K_NO_WAIT);

	key = irq_lock();

	switch (rx_state) {
	case RX_WAIT_SOF1:
		if (byte == RS485_SOF0) {
			rx_state = RX_WAIT_SOF2;
		}
		break;

	case RX_WAIT_SOF2:
		if (byte == RS485_SOF1) {
			rx_state = RX_WAIT_DEST;
		} else if (byte != RS485_SOF0) {
			rx_state = RX_WAIT_SOF1;
		}
		/* else: byte == RS485_SOF0 again - stay here, handles
		 * overlapping marker bytes.
		 */
		break;

	case RX_WAIT_DEST:
		rx_dest = byte;
		rx_state = RX_WAIT_LEN_HI;
		break;

	case RX_WAIT_LEN_HI:
		rx_len = (uint16_t)byte << 8;
		rx_state = RX_WAIT_LEN_LO;
		break;

	case RX_WAIT_LEN_LO:
		rx_len |= byte;
		rx_count = 0;
		if (rx_len == 0 || rx_len > sizeof(rx_buf)) {
			/* Implausible - resync instead of trusting it. */
			rx_reset();
			break;
		}
		rx_buffering = (rx_dest == rs485_my_addr) || (rx_dest == RS485_ADDR_BROADCAST);
		rx_state = RX_PAYLOAD;
		break;

	case RX_PAYLOAD:
		if (rx_buffering) {
			rx_buf[rx_count] = byte;
		}
		rx_count++;
		if (rx_count >= rx_len) {
			rx_state = RX_WAIT_CRC_HI;
		}
		break;

	case RX_WAIT_CRC_HI:
		rx_crc_hi = byte;
		rx_state = RX_WAIT_CRC_LO;
		break;

	case RX_WAIT_CRC_LO:
		if (rx_buffering) {
			uint16_t got_crc = ((uint16_t)rx_crc_hi << 8) | byte;
			uint8_t header[3];
			uint16_t want_crc;

			header[0] = rx_dest;
			header[1] = (uint8_t)(rx_len >> 8);
			header[2] = (uint8_t)(rx_len & 0xFF);
			want_crc = crc16_itu_t(0, header, sizeof(header));
			want_crc = crc16_itu_t(want_crc, rx_buf, rx_len);

			if (got_crc == want_crc) {
				dispatch_frame(rx_buf, rx_len);
			}
			/* CRC mismatch: silently drop, resync below. */
		}
		rx_reset();
		break;
	}

	irq_unlock(key);
}

int rs485_envelope_init(void)
{
	int rc;

	rc = settings_subsys_init();
	if (rc != 0) {
		return rc;
	}

	rc = settings_load();
	if (rc != 0) {
		return rc;
	}

	return rs485_uart_init();
}
