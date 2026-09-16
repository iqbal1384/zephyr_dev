# STMicroelectronics VL53L8CX 8x8 ToF Sensor Driver: Nucleo Integration Guide

This document explains how to use the standalone **`st_vl53l8cx`** Zephyr driver module on **any STM32 Nucleo board** (or any custom STM32 hardware) with zero modifications to application code.

---

## 1. Architecture Overview

The `st_vl53l8cx` driver is implemented as an out-of-tree West sensor module following standard Zephyr driver conventions:

```
┌────────────────────────────────────────────────────────────────────────┐
│                   Application Layer (src/main.c)                       │
│    Generic Zephyr Sensor API: DEVICE_DT_GET_ANY(st_vl53l8cx)          │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │
┌───────────────────────────────────▼────────────────────────────────────┐
│            Sensor Subsystem & Driver (modules/sensor/st_vl53l8cx)      │
│    - Automated 32 KB FW upload via I2C                                 │
│    - Power enable (PWR_EN) & low-power control (LPn) sequencing         │
│    - 8x8 (64 zones) & 4x4 (16 zones) multi-zone ranging engine         │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │
┌───────────────────────────────────▼────────────────────────────────────┐
│           Board Devicetree Overlay (boards/<board_name>.overlay)       │
│    Assigns MCU I2C peripheral and GPIO pins                            │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Hardware Connections (X-NUCLEO-53L8A1 Shield)

When using the **X-NUCLEO-53L8A1** expansion shield on standard Arduino Uno V3 Nucleo headers:

| Signal | Arduino Pin | Description | Default STM32 Pin (Nucleo-64) |
| :--- | :--- | :--- | :--- |
| **I2C_SCL** | `D15` / `SCL` | I2C Clock (Fast-mode 400 kHz) | `PB8` (on `&i2c1`) |
| **I2C_SDA** | `D14` / `SDA` | I2C Data | `PB9` (on `&i2c1`) |
| **PWR_EN** | `D11` | Main 2.8V / 1.8V Power Rail Enable | `PA7` (or board equivalent) |
| **LPn** | `A3` | Sensor Chip Enable / Low-Power Pin | `PB0` (or board equivalent) |
| **INT** | `A2` | Data-ready interrupt (optional) | `PA4` (or board equivalent) |

> [!NOTE]
> **SPI Conflict Notice**: On some Nucleo-64 boards (such as NUCLEO-F401RE/G474RE), `PA7` (Arduino `D11`) is also mapped to `SPI1_MOSI`. If using I2C mode with the shield, ensure `&spi1` is disabled in your devicetree overlay so `PA7` operates as a standard GPIO.

---

## 3. The 3 Steps to Add VL53L8CX to Any Nucleo Application

### Step 1: Create the Board Devicetree Overlay (`boards/<board_name>.overlay`)
Create an overlay file in your project's `boards/` folder named `<board_name>.overlay`.

```dts
&i2c1 {
	status = "okay";
	clock-frequency = <I2C_BITRATE_FAST>; /* 400 kHz */

	vl53l8cx: vl53l8cx@29 {
		compatible = "st,vl53l8cx";
		reg = <0x29>;
		pwr-en-gpios = <&gpioa 7 GPIO_ACTIVE_HIGH>;   /* Arduino D11 */
		lpn-gpios    = <&gpiob 0 GPIO_ACTIVE_HIGH>;   /* Arduino A3 */
		resolution = <64>;                           /* 64 for 8x8, 16 for 4x4 */
		ranging-frequency = <10>;                     /* 10 Hz */
		status = "okay";
	};
};

/* Optional: Disable SPI1 if D11 / PA7 is shared with SPI */
&spi1 {
	status = "disabled";
};
```

---

### Step 2: Configure Kconfig (`prj.conf`)
Add the sensor and STM32 I2C requirements to your `prj.conf`:

```ini
# Core Sensor Subsystem & VL53L8CX Driver
CONFIG_SENSOR=y
CONFIG_ST_VL53L8CX=y
CONFIG_I2C=y
CONFIG_GPIO=y

# Memory: Allocate >= 40 KB Heap for 32 KB VL53L8CX firmware upload
CONFIG_HEAP_MEM_POOL_SIZE=40960

# STM32 I2C configuration: Required for chunked multi-byte I2C writes
CONFIG_I2C_STM32_INTERRUPT=y
CONFIG_I2C_STM32_TRANSFER_TIMEOUT_MSEC=5000

# Console & Logging
CONFIG_LOG=y
CONFIG_LOG_MODE_IMMEDIATE=y
CONFIG_PRINTK=y
```

---

### Step 3: Write Generic Application Code (`src/main.c`)

```c
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <vl53l8cx.h>

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

int main(void)
{
	const struct device *const dev = DEVICE_DT_GET_ANY(st_vl53l8cx);

	if (!device_is_ready(dev)) {
		LOG_ERR("VL53L8CX sensor device not ready!");
		return 0;
	}

	LOG_INF("VL53L8CX initialized successfully!");

	struct sensor_value distance;

	while (1) {
		if (sensor_sample_fetch(dev) < 0) {
			LOG_WRN("Sensor sample fetch failed");
			k_msleep(100);
			continue;
		}

		/* Read center zone distance */
		sensor_channel_get(dev, SENSOR_CHAN_DISTANCE, &distance);
		LOG_INF("Center distance: %d.%03d m", distance.val1, distance.val2 / 1000);

		k_msleep(100);
	}
}
```

---

## 4. Pre-Configured Board Overlays Reference

### 1. NUCLEO-F401RE / F411RE / F446RE (`boards/nucleo_f401re.overlay`)
```dts
&i2c1 {
	status = "okay";
	vl53l8cx: vl53l8cx@29 {
		compatible = "st,vl53l8cx";
		reg = <0x29>;
		pwr-en-gpios = <&gpioa 7 GPIO_ACTIVE_HIGH>;   /* Arduino D11 */
		lpn-gpios    = <&gpiob 0 GPIO_ACTIVE_HIGH>;   /* Arduino A3 */
		resolution = <64>;
		ranging-frequency = <10>;
	};
};

&spi1 {
	status = "disabled";
};
```

### 2. NUCLEO-G474RE (`boards/nucleo_g474re.overlay`)
```dts
&i2c1 {
	status = "okay";
	vl53l8cx: vl53l8cx@29 {
		compatible = "st,vl53l8cx";
		reg = <0x29>;
		pwr-en-gpios = <&gpioa 7 GPIO_ACTIVE_HIGH>;   /* Arduino D11 */
		lpn-gpios    = <&gpiob 0 GPIO_ACTIVE_HIGH>;   /* Arduino A3 */
		resolution = <64>;
		ranging-frequency = <10>;
	};
};

&spi1 {
	status = "disabled";
};
```

### 3. NUCLEO-H723ZG / H743ZI (`boards/nucleo_h723zg.overlay`)
```dts
&i2c1 {
	status = "okay";
	vl53l8cx: vl53l8cx@29 {
		compatible = "st,vl53l8cx";
		reg = <0x29>;
		pwr-en-gpios = <&gpioa 7 GPIO_ACTIVE_HIGH>;   /* Arduino D11 */
		lpn-gpios    = <&gpiob 0 GPIO_ACTIVE_HIGH>;   /* Arduino A3 */
		resolution = <64>;
		ranging-frequency = <10>;
	};
};
```

### 4. NUCLEO-N657X0-Q (`boards/nucleo_n657x0_q.overlay`)
```dts
&i2c2 {
	status = "okay";
	vl53l8cx: vl53l8cx@29 {
		compatible = "st,vl53l8cx";
		reg = <0x29>;
		pwr-en-gpios = <&gpioc 1 GPIO_ACTIVE_HIGH>;   /* Pin Header CN7.36 */
		lpn-gpios    = <&gpioa 0 GPIO_ACTIVE_HIGH>;   /* Pin Header CN7.28 */
		resolution = <64>;
		ranging-frequency = <10>;
	};
};
```

### 5. NUCLEO-L432KC / L476RG (`boards/nucleo_l432kc.overlay`)
```dts
&i2c1 {
	status = "okay";
	vl53l8cx: vl53l8cx@29 {
		compatible = "st,vl53l8cx";
		reg = <0x29>;
		pwr-en-gpios = <&gpioa 7 GPIO_ACTIVE_HIGH>;
		lpn-gpios    = <&gpioa 4 GPIO_ACTIVE_HIGH>;
		resolution = <64>;
		ranging-frequency = <10>;
	};
};
```

---

## 5. Building and Flashing

Build and flash for your target board using standard West commands:

```bash
# Set toolchain (if not using Zephyr SDK)
export ZEPHYR_TOOLCHAIN_VARIANT=cross-compile
export CROSS_COMPILE=/opt/arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi/bin/arm-none-eabi-

# Build for NUCLEO-F401RE:
west build -b nucleo_f401re zephyr_app/stm32/nucleo_f401re/vl53l8cx_ranging -p auto

# Flash via ST-Link:
west flash

# View serial console output:
# Linux: /dev/ttyACM* @ 115200 baud
```

---

## 6. Key Configuration Options

| Kconfig Symbol | Default | Recommended | Description |
| :--- | :--- | :--- | :--- |
| `CONFIG_ST_VL53L8CX` | `n` | `y` | Enables the VL53L8CX sensor driver module |
| `CONFIG_HEAP_MEM_POOL_SIZE` | `0` | `40960` | Heap in bytes. 40 KB is required to buffer the 32 KB firmware during power-up boot |
| `CONFIG_I2C_STM32_INTERRUPT` | `n` | `y` | Required for ST I2C peripheral transfers larger than 255 bytes |
| `CONFIG_I2C_STM32_TRANSFER_TIMEOUT_MSEC` | `500` | `5000` | Extends I2C timeout to accommodate the 740 ms firmware burst at 400 kHz |
| `CONFIG_VL53L8CX_POLL_INTERVAL_MS` | `10` | `10` | Polling loop interval when waiting for data-ready flag |

---

## 7. Advanced: Accessing Full 8x8 Ranging Matrix Data

To read all 64 zones (or 16 zones in 4x4 mode), use the custom multizone API provided in `<vl53l8cx.h>`:

```c
#include <vl53l8cx.h>

struct vl53l8cx_ranging_data results;

if (vl53l8cx_get_ranging_data(dev, &results) == 0) {
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            int zone = row * 8 + col;
            int16_t dist_mm = results.distance_mm[zone];
            uint8_t status  = results.target_status[zone];

            if (status == 5 || status == 6 || status == 9) {
                printk("%4d mm ", dist_mm);
            } else {
                printk("  --- mm ");
            }
        }
        printk("\n");
    }
}
```

