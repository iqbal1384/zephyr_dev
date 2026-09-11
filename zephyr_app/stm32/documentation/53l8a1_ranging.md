# VL53L8A1 Ranging — Project Documentation

## Overview

This application runs on the **STM32 NUCLEO-N657X0-Q** board with the
**X-NUCLEO-53L8A1** expansion shield. It continuously measures distances
across an **8×8 grid of zones** using the ST VL53L8CX Time-of-Flight sensor
and prints the results to the serial console.

---

## Hardware

### Board
- **NUCLEO-N657X0-Q** (STM32N6)

### Shield
- **X-NUCLEO-53L8A1** (VL53L8CX multizone ToF sensor)

### Pin Mapping (Arduino header)

| Signal  | Arduino pin | STM32N6 pin | Description                        |
|---------|-------------|-------------|------------------------------------|
| SCL     | D15         | PH9         | I2C clock (i2c1)                   |
| SDA     | D14         | PC1         | I2C data (i2c1)                    |
| PWR_EN  | D11         | PG2         | Shield power enable (active high)  |
| LPn     | A3          | PA12        | Sensor low-power / reset (active high) |

Sensor I2C address: **0x29** (7-bit).

> Note: SPI5 is disabled in the overlay to free PG2 (shared with SPI5_MOSI).

---

## Project Structure

```
53l8a1_ranging/
├── src/
│   └── main.c                          # Application entry point
├── lib/
│   └── vl53l8cx/
│       ├── modules/                    # ST vendor API (unmodified)
│       │   ├── vl53l8cx_api.c/.h
│       │   ├── vl53l8cx_buffers.h
│       │   └── vl53l8cx_plugin_*.c/.h
│       └── porting/                    # Zephyr platform abstraction layer
│           ├── platform.c
│           └── platform.h
├── boards/
│   └── nucleo_n657x0_q.overlay         # Device tree: I2C, GPIO pins
├── prj.conf                            # Zephyr Kconfig options
├── CMakeLists.txt
└── Makefile
```

---

## Application Flow (`main.c`)

1. **Power on** — Assert `PWR_EN` (PG2) to power the shield.
2. **Sensor reset** — Cycle `LPn` (PA12) low → high to force a clean cold boot.
3. **I2C scan** — Walk addresses 0x08–0x77 and confirm ACK at 0x29.
4. **Init** — Call `vl53l8cx_init()`, which uploads the ~32 KB sensor firmware
   over I2C and boots the sensor's internal MCU.
5. **Configure** — Set 8×8 resolution, continuous ranging mode, 10 Hz,
   30 ms timing budget.
6. **Ranging loop** — Poll `data_ready` every 100 ms; when data is available
    call `vl53l8cx_get_ranging_data()` and print an 8×8 distance grid.

---

## Zephyr Platform Layer (`lib/vl53l8cx/porting/`)

The ST vendor API expects a platform abstraction layer that provides I2C read/write
and delay functions. The original layer uses STM32 HAL; this project replaces it
with a Zephyr port.

### `VL53L8CX_Platform` struct

```c
typedef struct {
    const struct device *i2c_dev;  // Zephyr I2C device pointer
    uint16_t             address;  // 7-bit I2C address (0x29)
} VL53L8CX_Platform;
```

### Function mapping

| Vendor function       | Zephyr implementation                        |
|-----------------------|----------------------------------------------|
| `VL53L8CX_RdByte`     | delegates to `VL53L8CX_RdMulti` (1 byte)    |
| `VL53L8CX_WrByte`     | delegates to `VL53L8CX_WrMulti` (1 byte)    |
| `VL53L8CX_RdMulti`    | `i2c_write_read()` (2-byte reg addr + N read) |
| `VL53L8CX_WrMulti`    | `k_malloc()` + `i2c_write()` (2-byte reg addr prepended to payload) |
| `VL53L8CX_WaitMs`     | `k_msleep()`                                 |
| `VL53L8CX_SwapBuffer` | manual 32-bit byte-swap loop                 |

All register addresses are 16-bit and sent big-endian before the payload.

---

## Configuration (`prj.conf`)

```kconfig
CONFIG_I2C=y
CONFIG_GPIO=y
CONFIG_SERIAL=y
CONFIG_CONSOLE=y
CONFIG_UART_CONSOLE=y

# Heap for WrMulti: largest single alloc is 32768+2 bytes (firmware block)
CONFIG_HEAP_MEM_POOL_SIZE=40960

# Interrupt mode required for large (>255 byte) I2C writes
CONFIG_I2C_STM32_INTERRUPT=y

# Raised from default 500 ms — 32 KB upload at 400 kHz takes ~740 ms
CONFIG_I2C_STM32_TRANSFER_TIMEOUT_MSEC=5000
```

---

## Device Tree Overlay (`boards/nucleo_n657x0_q.overlay`)

```dts
&i2c1 {
    status = "okay";
    clock-frequency = <I2C_BITRATE_FAST>;   /* 400 kHz */
    vl53l8a1: vl53l8a1@29 {
        compatible = "i2c-device";
        reg = <0x29>;
        label = "VL53L8A1";
    };
};

&spi5 { status = "disabled"; };  /* free PG2 for PWR_EN */

/ {
    zephyr,user {
        vl53l8a1-pwren-gpios = <&gpiog 2  GPIO_ACTIVE_HIGH>;
        vl53l8a1-lpn-gpios   = <&gpioa 12 GPIO_ACTIVE_HIGH>;
    };
};
```

---

## Build and Flash

```bash
cd ~/work/zephyr_dev/zephyr_app/stm32/nucleo_n657x0_q/53l8a1_ranging

make            # incremental build
make pristine   # full rebuild
```

Flash via **Windows STM32CubeProgrammer** (ST-Link FW on the NUCLEO is V3J17 —
too old for west flash):

| Setting          | Value                                                                 |
|------------------|-----------------------------------------------------------------------|
| Port             | ST-LINK, SWD, Hot Plug, AP=1                                          |
| External loader  | MX25UM51245G_STM32N6570-NUCLEO                                        |
| File             | `...\53l8a1_ranging\build\zephyr\zephyr.signed.bin`                   |
| Start address    | `0x70000000`                                                          |

Jumper settings:
- **Flashing**: JP1 pos 1, JP2 pos 2 (non-printed side)
- **Running**: JP1 pos 1, JP2 pos 1 (printed side) — then power cycle

---

## Known Issue Fixed: I2C Transfer Timeout

During `vl53l8cx_init()` the sensor receives a ~32 KB firmware blob in a single
`i2c_write()` call. The STM32 I2C v2 driver handles it correctly using hardware
RELOAD mode internally (255-byte chunks), but the calling thread waits on a
semaphore with `CONFIG_I2C_STM32_TRANSFER_TIMEOUT_MSEC`.

| Parameter            | Before fix       | After fix          |
|----------------------|------------------|--------------------|
| I2C clock            | 100 kHz (Standard) | 400 kHz (Fast)   |
| 32 KB upload time    | ~2.95 s          | ~0.74 s            |
| Transfer timeout     | 500 ms (default) | 5000 ms            |
| Result               | `-EIO` (-5), sensor never boots | Init OK |
