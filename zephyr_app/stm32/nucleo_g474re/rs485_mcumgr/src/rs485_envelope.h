#ifndef RS485_ENVELOPE_H_
#define RS485_ENVELOPE_H_

#include <stdint.h>

/*
 * Multi-drop RS485 addressing layer. Every frame on the bus carries a
 * destination address; a node only ever buffers/acts on/responds to frames
 * addressed to it (or the broadcast address), and silently, cheaply
 * discards everything else without ever transmitting - required since
 * RS485 is a shared broadcast medium and two nodes replying at once is
 * real electrical bus contention, not just a logical mixup.
 *
 * This is a general-purpose foundation: KIND_MCUMGR carries the existing
 * mcumgr/SMP transport traffic, KIND_PROVISIONING carries the one-off
 * address-assignment command, and further KIND values are reserved for a
 * future addressed application protocol on this same bus.
 */

/* Reserved/special addresses. 0x02-0xFE are provisioned node addresses. */
#define RS485_ADDR_BROADCAST     0x00
#define RS485_ADDR_HOST          0x01
#define RS485_ADDR_NODE_MIN      0x02
#define RS485_ADDR_NODE_MAX      0xFE
#define RS485_ADDR_UNPROVISIONED 0xFF

/*
 * Shared by the TX scratch buffer, the RX payload buffer, and the
 * "implausible LEN" sanity bound, so the check and the actual capacity can
 * never drift apart. Covers the worst case whole mcumgr transmission at the
 * configured MTU: simulating mcumgr_serial_tx_pkt()'s output for
 * CONFIG_MCUMGR_TRANSPORT_NETBUF_SIZE=384 gives 5 lines / 535 wire bytes
 * (MCUMGR_SERIAL_MAX_FRAME=127 per line); this leaves ~7% margin.
 */
#define RS485_ENVELOPE_MAX_PAYLOAD 576

/**
 * Initializes settings-backed address provisioning (reads this node's
 * address from flash, defaulting to RS485_ADDR_UNPROVISIONED if never set),
 * then brings up the RS485 UART/DE hardware. Must be called before
 * registering an SMP transport's rx callbacks on top of this layer.
 */
int rs485_envelope_init(void);

/**
 * Called once, from the UART ISR, per raw received byte. Runs the envelope
 * parser (resync/address/length/CRC). A frame addressed to this node (or
 * broadcast) has its payload dispatched by KIND; everything else is
 * discarded without ever transmitting a response.
 */
void rs485_envelope_rx_byte(uint8_t byte);

/**
 * Collects one chunk of outgoing mcumgr data - matches the
 * mcumgr_serial_tx_cb signature so it can be passed directly to
 * mcumgr_serial_tx_pkt(). Must be followed by exactly one
 * rs485_envelope_tx_flush() call once the whole packet has been collected.
 */
int rs485_envelope_tx_collect(const void *data, int len);

/**
 * Wraps everything collected since the last flush into one addressed
 * (KIND_MCUMGR) envelope frame destined for dest_addr and transmits it,
 * DE-guarded internally. Resets the collector for the next packet
 * regardless of outcome.
 */
int rs485_envelope_tx_flush(uint8_t dest_addr);

#endif /* RS485_ENVELOPE_H_ */
