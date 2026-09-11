#!/usr/bin/env python3
"""Presents a pty to the unmodified `mcumgr` CLI, transparently wrapping its
traffic in the addressed RS485 envelope (see src/rs485_envelope.c for the
canonical format) so mcumgr can keep talking point-to-point SMP-over-serial
while actually running over a shared multi-drop RS485 bus targeting one
specific node.

Run this once per update session, pointed at the real RS485 USB dongle
device and the target node's address; point `mcumgr`/update_firmware.py at
the pty path this script prints on startup instead of the real device.

Before relying on this, run pty_mcumgr_smoketest.py once to confirm your
mcumgr build actually works against a pty at all - see that script's
docstring for why this isn't a given.
"""

import argparse
import os
import struct
import sys
import termios
import threading
import tty

SOF0 = 0x9D
SOF1 = 0x3F

ADDR_BROADCAST = 0x00
ADDR_HOST = 0x01
ADDR_NODE_MIN = 0x02
ADDR_NODE_MAX = 0xFE
ADDR_UNPROVISIONED = 0xFF

KIND_MCUMGR = 0x00

MAX_PAYLOAD = 576  # must match RS485_ENVELOPE_MAX_PAYLOAD in rs485_envelope.h


def crc16_itu_t(seed, data):
    """Bit-for-bit port of Zephyr's crc16_itu_t() (zephyr/subsys/crc/crc16_sw.c) -
    must stay in sync with that implementation, not just "a CRC16"."""
    for byte in data:
        seed = ((seed >> 8) | ((seed << 8) & 0xFFFF)) & 0xFFFF
        seed ^= byte
        seed ^= (seed & 0xFF) >> 4
        seed = (seed ^ (seed << 12)) & 0xFFFF
        seed = (seed ^ ((seed & 0xFF) << 5)) & 0xFFFF
    return seed & 0xFFFF


def build_frame(dest, kind, body):
    payload = bytes([kind]) + body
    header = bytes([dest]) + struct.pack(">H", len(payload))
    crc = crc16_itu_t(0, header + payload)
    return bytes([SOF0, SOF1]) + header + payload + struct.pack(">H", crc)


def open_dongle(path, baud):
    fd = os.open(path, os.O_RDWR | os.O_NOCTTY)
    tty.setraw(fd)
    attrs = termios.tcgetattr(fd)
    baud_const = getattr(termios, f"B{baud}")
    attrs[4] = baud_const
    attrs[5] = baud_const
    # VMIN=1, VTIME=0: block for at least one byte, no inter-byte timeout -
    # simplest correct blocking-read behavior for this proxy's reader thread.
    attrs[6][termios.VMIN] = 1
    attrs[6][termios.VTIME] = 0
    termios.tcsetattr(fd, termios.TCSANOW, attrs)
    return fd


class EnvelopeReceiver:
    """Mirrors the RX state machine in src/rs485_envelope.c."""

    WAIT_SOF1, WAIT_SOF2, WAIT_DEST, WAIT_LEN_HI, WAIT_LEN_LO, PAYLOAD, WAIT_CRC_HI, WAIT_CRC_LO = range(8)

    def __init__(self, on_frame):
        self.state = self.WAIT_SOF1
        self.dest = 0
        self.length = 0
        self.buf = bytearray()
        self.crc_hi = 0
        self.on_frame = on_frame

    def feed(self, byte):
        if self.state == self.WAIT_SOF1:
            if byte == SOF0:
                self.state = self.WAIT_SOF2
        elif self.state == self.WAIT_SOF2:
            if byte == SOF1:
                self.state = self.WAIT_DEST
            elif byte != SOF0:
                self.state = self.WAIT_SOF1
        elif self.state == self.WAIT_DEST:
            self.dest = byte
            self.state = self.WAIT_LEN_HI
        elif self.state == self.WAIT_LEN_HI:
            self.length = byte << 8
            self.state = self.WAIT_LEN_LO
        elif self.state == self.WAIT_LEN_LO:
            self.length |= byte
            self.buf = bytearray()
            if self.length == 0 or self.length > MAX_PAYLOAD:
                self.state = self.WAIT_SOF1
            else:
                self.state = self.PAYLOAD
        elif self.state == self.PAYLOAD:
            self.buf.append(byte)
            if len(self.buf) >= self.length:
                self.state = self.WAIT_CRC_HI
        elif self.state == self.WAIT_CRC_HI:
            self.crc_hi = byte
            self.state = self.WAIT_CRC_LO
        elif self.state == self.WAIT_CRC_LO:
            got_crc = (self.crc_hi << 8) | byte
            header = bytes([self.dest]) + struct.pack(">H", self.length)
            want_crc = crc16_itu_t(0, header + bytes(self.buf))
            if got_crc == want_crc:
                self.on_frame(self.dest, bytes(self.buf))
            self.state = self.WAIT_SOF1


def pty_to_dongle(master_fd, dongle_fd, node_addr):
    """Accumulates pty-master bytes until '\\n' (mcumgr's own line terminator,
    which never appears mid-line - see the design notes in the plan doc),
    wraps each whole line in one envelope frame addressed to node_addr."""
    line = bytearray()
    while True:
        data = os.read(master_fd, 4096)
        if not data:
            return
        for byte in data:
            line.append(byte)
            if byte == 0x0A:  # '\n'
                frame = build_frame(node_addr, KIND_MCUMGR, bytes(line))
                os.write(dongle_fd, frame)
                line.clear()


def dongle_to_pty(dongle_fd, master_fd):
    def on_frame(dest, payload):
        if dest != ADDR_HOST or len(payload) == 0:
            return
        kind, body = payload[0], payload[1:]
        if kind == KIND_MCUMGR:
            os.write(master_fd, body)

    receiver = EnvelopeReceiver(on_frame)
    while True:
        data = os.read(dongle_fd, 4096)
        if not data:
            return
        for byte in data:
            receiver.feed(byte)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--dongle", required=True, help="Real RS485 USB dongle device")
    p.add_argument("--baud", type=int, default=115200)
    p.add_argument("--node-addr", type=int, required=True,
                    help=f"Target node address ({ADDR_NODE_MIN}-{ADDR_NODE_MAX})")
    args = p.parse_args()

    if not (ADDR_NODE_MIN <= args.node_addr <= ADDR_NODE_MAX):
        p.error(f"--node-addr must be in [{ADDR_NODE_MIN}, {ADDR_NODE_MAX}]")

    master_fd, slave_fd = os.openpty()
    tty.setraw(slave_fd)
    slave_path = os.ttyname(slave_fd)

    dongle_fd = open_dongle(args.dongle, args.baud)

    # Printed for the caller (e.g. update_firmware.py) to parse and use as
    # --port; flush immediately since a parent process may be reading this
    # line to know when the proxy is ready.
    print(slave_path, flush=True)

    threads = [
        threading.Thread(target=pty_to_dongle, args=(master_fd, dongle_fd, args.node_addr),
                          daemon=True),
        threading.Thread(target=dongle_to_pty, args=(dongle_fd, master_fd), daemon=True),
    ]
    for t in threads:
        t.start()
    for t in threads:
        t.join()


if __name__ == "__main__":
    sys.exit(main())
