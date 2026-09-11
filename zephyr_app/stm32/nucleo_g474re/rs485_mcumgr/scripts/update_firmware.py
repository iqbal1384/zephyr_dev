#!/usr/bin/env python3
"""Orchestrates a full firmware update over the RS485 mcumgr transport.

Runs on the Raspberry Pi. Shells out to the `mcumgr` CLI (the same tool and
serial transport already validated by hand: upload -> parse hash -> test ->
reset -> poll for the board coming back -> verify the new image is active and
its mgmt stack is responding -> only then explicitly confirm it.

If anything fails after `reset`, this script deliberately does NOT confirm
the image - the device's own watchdog + grace-period self-confirm fallback
(see src/main.c), or a future reset, is what protects against a bad image in
that case, not this script.
"""

import argparse
import atexit
import os
import re
import subprocess
import sys
import time

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))


def parse_args():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--image", required=True, help="Path to the signed image (zephyr.signed.bin)")
    p.add_argument("--port",
                    help="Serial device to talk mcumgr on directly, e.g. /dev/ttyACM1. "
                         "Omit if using --node-addr/--dongle (rs485_proxy.py is spawned "
                         "and this is set automatically to its pty).")
    p.add_argument("--node-addr", type=int,
                    help="Target node's RS485 address (2-254) - spawns rs485_proxy.py "
                         "against --dongle and points mcumgr at its pty instead of --port.")
    p.add_argument("--dongle", help="Real RS485 USB dongle device, used with --node-addr")
    p.add_argument("--baud", type=int, default=115200)
    p.add_argument("--mcumgr-bin", default="mcumgr")
    p.add_argument("--upload-timeout", type=float, default=300,
                    help="Seconds allowed for the whole image upload")
    p.add_argument("--cmd-timeout", type=float, default=15,
                    help="Seconds allowed for a single quick command")
    p.add_argument("--reboot-wait", type=float, default=3,
                    help="Seconds to wait before polling after reset")
    p.add_argument("--poll-interval", type=float, default=2)
    p.add_argument("--poll-attempts", type=int, default=15)
    p.add_argument("--echo-text", default="HEALTHCHECK",
                    help="Distinctive payload for the post-update liveliness check")
    args = p.parse_args()

    if args.node_addr is not None:
        if not args.dongle:
            p.error("--node-addr requires --dongle")
    elif not args.port:
        p.error("must give either --port, or --node-addr together with --dongle")

    return args


def start_proxy(dongle, node_addr):
    """Spawns rs485_proxy.py against the real dongle, targeting node_addr.
    Returns (process, pty_path). Caller must terminate the process when done
    (this registers an atexit handler for that as a backstop)."""
    proxy_path = os.path.join(SCRIPT_DIR, "rs485_proxy.py")
    proc = subprocess.Popen(
        [sys.executable, proxy_path, "--dongle", dongle, "--node-addr", str(node_addr)],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
    )
    atexit.register(proc.terminate)

    pty_path = proc.stdout.readline().strip()
    if not pty_path:
        proc.terminate()
        stderr = proc.stderr.read()
        raise UpdateFailed(f"rs485_proxy.py failed to start: {stderr}")

    return proc, pty_path


class UpdateFailed(Exception):
    pass


def run_mcumgr(base_cmd, args, timeout):
    cmd = base_cmd + args
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
    except subprocess.TimeoutExpired as exc:
        raise UpdateFailed(f"command timed out after {timeout}s: {' '.join(args)}") from exc

    if result.returncode != 0:
        raise UpdateFailed(
            f"command failed (rc={result.returncode}): {' '.join(args)}\n{result.stderr}"
        )

    return result.stdout


def parse_image_list(text):
    """Parses `mcumgr image list` text output into a list of slot dicts.

    The output is whitespace-sensitive and multi-line per slot (not one
    regex-able block), so track fields as they appear line by line instead.
    """
    slots = []
    cur = None

    for raw_line in text.splitlines():
        line = raw_line.strip()

        m = re.match(r"image=(\d+)\s+slot=(\d+)", line)
        if m:
            cur = {"image": int(m.group(1)), "slot": int(m.group(2)), "flags": "", "hash": None}
            slots.append(cur)
            continue

        if cur is None:
            continue

        if line.startswith("flags:"):
            cur["flags"] = line[len("flags:"):].strip()
        elif line.startswith("hash:"):
            m = re.search(r"[0-9a-fA-F]{64}", line)
            if m:
                cur["hash"] = m.group(0)

    return slots


def find_pending_upload_slot(slots):
    """Returns the slot record that is NOT currently active - the one we just
    uploaded (there should be exactly one non-active bootable slot)."""
    candidates = [s for s in slots if "active" not in s["flags"].split()]
    if len(candidates) != 1:
        raise UpdateFailed(f"expected exactly one non-active slot, found {len(candidates)}: {slots}")
    if candidates[0]["hash"] is None:
        raise UpdateFailed(f"non-active slot has no hash: {candidates[0]}")
    return candidates[0]


def find_slot_by_hash(slots, image_hash):
    for s in slots:
        if s["hash"] == image_hash:
            return s
    return None


def main():
    args = parse_args()

    proxy_proc = None
    if args.node_addr is not None:
        print(f"[0/8] Starting rs485_proxy.py for node {args.node_addr} on {args.dongle} ...")
        proxy_proc, args.port = start_proxy(args.dongle, args.node_addr)
        print(f"      proxy pty: {args.port}")

    try:
        return run_update(args)
    finally:
        if proxy_proc is not None:
            proxy_proc.terminate()


def run_update(args):
    base_cmd = [
        args.mcumgr_bin,
        "--conntype", "serial",
        "--connstring", f"dev={args.port},baud={args.baud}",
    ]

    print(f"[1/8] Uploading {args.image} ...")
    run_mcumgr(base_cmd, ["image", "upload", args.image], timeout=args.upload_timeout)
    print("      upload done")

    print("[2/8] Listing images to find the new slot's hash ...")
    slots = parse_image_list(run_mcumgr(base_cmd, ["image", "list"], timeout=args.cmd_timeout))
    new_slot = find_pending_upload_slot(slots)
    new_hash = new_slot["hash"]
    print(f"      new image hash: {new_hash}")

    print(f"[3/8] Marking {new_hash} for test-boot ...")
    run_mcumgr(base_cmd, ["image", "test", new_hash], timeout=args.cmd_timeout)

    print("[4/8] Resetting the board ...")
    run_mcumgr(base_cmd, ["reset"], timeout=args.cmd_timeout)

    print(f"[5/8] Waiting {args.reboot_wait}s then polling for the board to come back ...")
    time.sleep(args.reboot_wait)

    slots = None
    last_error = None
    for attempt in range(1, args.poll_attempts + 1):
        try:
            slots = parse_image_list(
                run_mcumgr(base_cmd, ["image", "list"], timeout=args.cmd_timeout)
            )
            break
        except UpdateFailed as exc:
            last_error = exc
            print(f"      attempt {attempt}/{args.poll_attempts}: not up yet ({exc})")
            time.sleep(args.poll_interval)

    if slots is None:
        raise UpdateFailed(f"board never responded after reset: {last_error}")

    print("[6/8] Verifying the new image is active ...")
    active_slot = find_slot_by_hash(slots, new_hash)
    if active_slot is None or "active" not in active_slot["flags"].split():
        raise UpdateFailed(
            "new image is not active after reset - MCUboot likely rejected/reverted it "
            f"(image list: {slots})"
        )
    print("      confirmed active")

    print(f"[7/8] Health check: os echo '{args.echo_text}' ...")
    echo_out = run_mcumgr(base_cmd, ["echo", args.echo_text], timeout=args.cmd_timeout)
    if args.echo_text not in echo_out:
        raise UpdateFailed(f"echo check failed, got: {echo_out!r}")
    print("      app mgmt stack is responsive")

    print(f"[8/8] Confirming {new_hash} ...")
    run_mcumgr(base_cmd, ["image", "confirm", new_hash], timeout=args.cmd_timeout)

    slots = parse_image_list(run_mcumgr(base_cmd, ["image", "list"], timeout=args.cmd_timeout))
    confirmed_slot = find_slot_by_hash(slots, new_hash)
    if confirmed_slot is None or "confirmed" not in confirmed_slot["flags"].split():
        raise UpdateFailed(f"confirm did not stick (image list: {slots})")

    print("\nPASS: update installed, verified, and confirmed.")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except UpdateFailed as exc:
        print(f"\nFAIL: {exc}", file=sys.stderr)
        print(
            "NOT confirming - the device will revert to the previous image via its own "
            "watchdog+grace-period fallback, or on the next reset.",
            file=sys.stderr,
        )
        sys.exit(1)
