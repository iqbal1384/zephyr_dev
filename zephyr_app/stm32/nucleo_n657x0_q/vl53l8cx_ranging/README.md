# STMicroelectronics VL53L8CX Ranging Application (STM32 Nucleo-N657X0-Q)

This application demonstrates ranging data acquisition from the **STMicroelectronics VL53L8CX** 8x8 multizone Time-of-Flight sensor on the **STM32 Nucleo-N657X0-Q** board using the external Zephyr sensor driver module located at `modules/sensor/st_vl53l8cx`.

---

## 1. Features
* **Decoupled Architecture**: Consumes the independent `st_vl53l8cx` sensor driver module via `ZEPHYR_EXTRA_MODULES`.
* **Standard Sensor API**: Interacts with the hardware exclusively through `<zephyr/drivers/sensor.h>` and `<zephyr/drivers/sensor/vl53l8cx.h>`.
* **Real-Time 8x8 Grid Display**: Formats and clears the console terminal to display live distance measurements (in mm) and target status for all 64 zones.

---

## 2. Hardware Connections
Target: **NUCLEO-N657X0-Q** with **X-NUCLEO-53L8A1** shield on Arduino headers:
* **I2C1**: SCL on `PH9` (D15), SDA on `PC1` (D14)
* **PWR_EN**: `PG2` (Arduino D11)
* **LPn**: `PA12` (Arduino A3)
* **Shield Jumpers**: Set `J9` to `[2-3]` for I2C communication mode.

---

## 3. How to Build & Flash

### Using Makefile
```bash
cd zephyr_app/stm32/nucleo_n657x0_q/vl53l8cx_ranging
make
make flash
```

### Using West CLI Directly
```bash
west build -b nucleo_n657x0_q zephyr_app/stm32/nucleo_n657x0_q/vl53l8cx_ranging
west flash
```

