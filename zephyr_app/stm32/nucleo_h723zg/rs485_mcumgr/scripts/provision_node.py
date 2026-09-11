#!/usr/bin/env python3
"""One-off tool to assign a fresh (unprovisioned) node its RS485 bus address.

Talks directly to the real RS485 dongle (NOT through rs485_proxy.py - that
proxy and `mcumgr` only understand KIND_MCUMGR traffic; provisioning is a
separate, simpler protocol handled entirely inside rs485_envelope.c on the
device, independent of mcumgr).

IMPORTANT: only one unprovisioned board may be connected to the bus at a
time when running this - there is no protocol-level way to disambiguate two
unprovisioned boards both listening on the same reserved address (see the
design notes in rs485_envelope.c).
"""

import argparse
import os
import select
import sys
import time

from rs485_proxy import (
    ADDR_HOST, ADDR_NODE_MAX, ADDR_NODE_MIN, ADDR_UNPROVISIONED,
    EnvelopeReceiver, build_frame, open_dongle,
)

KIND_PROVISIONING = 0x01
SUBCMD_SET_ADDRESS = 0x01
SUBCMD_SET_ADDRESS_ACK = 0x02


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--dongle", required=True, help="Real RS485 USB dongle device")
    p.add_argument("--baud", type=int, default=115200)
    p.add_argument("--new-addr", type=int, required=True,
                    help=f"Address to assign ({ADDR_NODE_MIN}-{ADDR_NODE_MAX})")
    p.add_argument("--timeout", type=float, default=3.0, help="Seconds to wait for the ACK")
    args = p.parse_args()

    if not (ADDR_NODE_MIN <= args.new_addr <= ADDR_NODE_MAX):
        p.error(f"--new-addr must be in [{ADDR_NODE_MIN}, {ADDR_NODE_MAX}]")

    fd = open_dongle(args.dongle, args.baud)

    frame = build_frame(ADDR_UNPROVISIONED, KIND_PROVISIONING,
                         bytes([SUBCMD_SET_ADDRESS, args.new_addr]))
    os.write(fd, frame)
    print(f"Sent SET_ADDRESS({args.new_addr}) to any unprovisioned node, waiting for ACK...")

    acked = []

    def on_frame(dest, payload):
        if (dest == ADDR_HOST and len(payload) >= 3
                and payload[0] == KIND_PROVISIONING
                and payload[1] == SUBCMD_SET_ADDRESS_ACK):
            acked.append(payload[2])

    receiver = EnvelopeReceiver(on_frame)
    deadline = time.time() + args.timeout

    while time.time() < deadline and not acked:
        remaining = deadline - time.time()
        ready, _, _ = select.select([fd], [], [], max(remaining, 0))
        if not ready:
            break
        data = os.read(fd, 256)
        for byte in data:
            receiver.feed(byte)

    if not acked:
        print("FAIL: no ACK received. Check that exactly one unprovisioned board is "
              "connected and powered, and that the dongle/wiring is correct.")
        return 1

    print(f"PASS: node acknowledged new address {acked[0]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
