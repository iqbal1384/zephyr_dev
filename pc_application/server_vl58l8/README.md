# VL53L8A1 Simple TCP Server (C)

Plain C TCP server for receiving and displaying VL53L8A1 ranging frames.

## Build

```bash
cd /home/user/work/zephyr_dev/pc_application/server_vl58l8
make clean
make
```

## Run

```bash
./server 5000
```

Or use:

```bash
make run
```

The server will listen on `0.0.0.0:5000` and display each frame as an 8x8 grid in the terminal.

## Device Configuration

Update the device config to match:

File: `zephyr_app/stm32/nucleo_n657x0_q/53l8a1_ranging_eth/src/server_config.h`

Set:
- `SERVER_IP_ADDR` = your PC's IP on the LAN
- `SERVER_PORT` = 5000 (or your chosen port)

## Frame Format

Expected from device (one line per frame):

```
ts=<ms>,z0=<mm>,z1=<mm>,...,z63=<mm>
```

Value `-1` for a zone means no target detected (shown as `----` in grid).
