#!/usr/bin/env python3
"""Smoke test: does the `mcumgr` CLI work at all against a pty instead of a
real serial device?

This gates the whole rs485_proxy.py approach (see the plan doc / design
notes in rs485_proxy.py). A pty slave supports generic termios calls (raw
mode, baud fields) fine, but some serial client libraries issue exotic
serial-only ioctls (e.g. TIOCGSERIAL) that a pty doesn't support - if
mcumgr's Go serial client does that and treats the failure as fatal, the
whole proxy idea needs a different approach before more time is spent on it.

This test doesn't need a real RS485 dongle or any Zephyr device attached -
it just loops the pty back to itself with a trivial stub, so a successful
run here only proves the pty-open/configure/read/write path works, not the
full envelope/addressing logic.

No external tools required (no strace, no apt/opkg) - deliberately, since
this is meant to run on constrained/embedded targets (e.g. a Yocto image
with no package manager available at runtime). Instead of tracing syscalls,
this distinguishes the two failure modes behaviorally: the stub below does
NOT speak real SMP-over-serial framing (that would need a full CBOR
encoder just for a smoke test), so mcumgr will typically never get a reply
it considers valid and will eventually give up - that alone is NOT a
failure. What matters is whether mcumgr got as far as WRITING its request
to the pty before getting stuck waiting for a reply:
  - it wrote data -> proves open/configure/write all worked; it's just
    stuck waiting on a reply our dumb stub can't produce -> PASS.
  - it wrote nothing at all -> consistent with being stuck earlier, e.g.
    inside an ioctl()/open() call against the pty itself -> FAIL, this is
    the real incompatibility being tested for.

Run this on the Raspberry Pi (or wherever `mcumgr` is installed) before
relying on rs485_proxy.py.
"""

import os
import subprocess
import sys
import threading


def echo_stub(master_fd, received):
    """Drains whatever mcumgr writes, recording how many bytes arrived -
    that count is what distinguishes the two failure modes (see module
    docstring). Deliberately does not send back anything mcumgr would
    recognize as a valid SMP reply."""
    try:
        while True:
            data = os.read(master_fd, 4096)
            if not data:
                break
            received.append(data)
    except OSError:
        pass


def main():
    mcumgr_bin = sys.argv[1] if len(sys.argv) > 1 else "mcumgr"
    echo_text = "smoketest"

    master_fd, slave_fd = os.openpty()
    slave_path = os.ttyname(slave_fd)
    print(f"pty slave: {slave_path}")

    received = []
    t = threading.Thread(target=echo_stub, args=(master_fd, received), daemon=True)
    t.start()

    cmd = [
        mcumgr_bin,
        "--conntype", "serial",
        "--connstring", f"dev={slave_path},baud=115200",
        "echo", echo_text,
    ]
    print("running:", " ".join(cmd))

    timed_out = False
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=10)
    except subprocess.TimeoutExpired:
        timed_out = True
        result = None
    except FileNotFoundError:
        print(f"\nFAIL: '{mcumgr_bin}' not found - install mcumgr first.")
        return 1

    bytes_written = sum(len(chunk) for chunk in received)

    if timed_out:
        print("\nmcumgr didn't return within 10s.")
        print(f"Bytes mcumgr wrote to the pty before/during the hang: {bytes_written}")
        if bytes_written > 0:
            print("\nPASS: mcumgr successfully wrote its request to the pty before "
                  "getting stuck waiting for a reply - the open/configure/write path "
                  "works. The hang itself is expected: this stub doesn't speak real "
                  "SMP framing, so mcumgr never gets a reply it considers valid.")
            return 0
        print("\nFAIL: mcumgr never wrote any data at all - consistent with being "
              "stuck earlier (e.g. inside an ioctl()/open() call against the pty "
              "itself). The proxy approach likely needs a different transport than "
              "a pty for this mcumgr build.")
        return 1

    print("--- mcumgr stdout ---")
    print(result.stdout)
    print("--- mcumgr stderr ---")
    print(result.stderr)

    combined = (result.stdout + result.stderr).lower()
    open_time_failures = [
        "no such device", "permission denied", "inappropriate ioctl",
        "not a typewriter", "invalid argument",
    ]
    if any(needle in combined for needle in open_time_failures):
        print("\nFAIL: mcumgr couldn't even open/configure the pty - "
              "see stderr above. The proxy approach needs a different "
              "transport than a pty for this mcumgr build.")
        return 1

    print("\nPASS: mcumgr opened and used the pty without an open/ioctl-time "
          "failure (a protocol/timeout error above, if any, is expected from "
          "this stub and is NOT a failure of this smoke test).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
