# NUCLEO-G474RE CAN Control Node

This application runs on `nucleo_g474re` and is controlled by an external CAN master (for example, NVIDIA Jetson Orin).

## Purpose

- Receive command frames from a CAN master.
- Execute local control actions (currently LED0).
- Send response/status frames back to the master.

## CAN protocol (standard 11-bit IDs)

- Master -> Node ID: `0x120`
- Node -> Master ID: `0x121`
- Node ID byte in payload: `0x01`

### Command frame format (`0x120`)

- Byte 0: command
- Byte 1..7: command data

Commands:

- `0x01` (`CMD_SET_LED`): set LED state
  - Byte 1: `0x00` OFF, non-zero ON
- `0x02` (`CMD_GET_STATUS`): request status
- `0x03` (`CMD_PING`): liveness check

### Response frame format (`0x121`)

- Byte 0: node id (`0x01`)
- Byte 1: status (`0x00` ok, `0xEE` unknown command)
- Byte 2: echoed command
- Byte 3: value0 (command specific)
- Byte 4: value1 (command specific)
- Byte 5..7: reserved

## Build

```bash
cd /home/user/work/zephyr_dev/zephyr_app/stm32/nucleo_g474re/can_control
source /home/user/work/zephyr_dev/zephyr/.venv/bin/activate
export ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb
export GNUARMEMB_TOOLCHAIN_PATH=/opt/arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi
west build -p always -b nucleo_g474re -d build .
```

## Flash

```bash
west flash -d build
```

## Jetson Orin quick test (SocketCAN)

On Jetson:

```bash
sudo ip link set can0 down
sudo ip link set can0 type can bitrate 500000
sudo ip link set can0 up
```

Monitor responses:

```bash
candump can0,121:7FF
```

Send commands:

```bash
# ping
cansend can0 120#03

# LED ON
cansend can0 120#0101

# LED OFF
cansend can0 120#0100

# get status
cansend can0 120#02
```
