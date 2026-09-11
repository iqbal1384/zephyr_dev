# STMicroelectronics VL53L8CX Ranging Application (STM32 Nucleo-F401RE)

Port of `zephyr_app/stm32/nucleo_n657x0_q/vl53l8cx_ranging` to the **STM32 Nucleo-F401RE**,
proving that the `st_vl53l8cx` driver module (`modules/sensor/st_vl53l8cx`) is portable
across STM32 families with no driver or application code changes — only board-specific
devicetree wiring.

`src/main.c` is unchanged (aside from the banner text) from the N657X0-Q version: it talks
to the sensor exclusively through the standard Zephyr `sensor.h` API, so it has no
board-specific code at all.

---

## 1. Hardware Setup

* **Base Board**: `nucleo_f401re`
* **Shield**: `X-NUCLEO-53L8A1` on the Arduino UNO V3 headers
* **Shield jumper J9**: `[2-3]` (I2C mode)
* **Shield jumper J7/J8**: `[1-2]` (I2C mode — enables the I2C pull-ups; see project notes)

## 2. Pin Mapping (Arduino Header)

| Signal | Arduino pin | STM32F401RE pin | Description                         |
|--------|-------------|------------------|--------------------------------------|
| SCL    | D15         | PB8 (`i2c1`)     | I2C clock (already Fast-mode by board default) |
| SDA    | D14         | PB9 (`i2c1`)     | I2C data                             |
| PWR_EN | D11         | PA7              | Shield power enable (active high)    |
| LPn    | A3          | PB0              | Sensor low-power / reset (active high) |

> Note: `&spi1` is disabled in the overlay to free `PA7` (`SPI1_MOSI`) for `PWR_EN` — the
> same header conflict the N6 app has with `&spi5`/`PG2`. `&i2c1` is already enabled at
> 400 kHz by the board's own `.dts`, so the overlay only adds the sensor child node.

## 3. Build & Flash

```bash
cd zephyr_app/stm32/nucleo_f401re/vl53l8cx_ranging
make
make flash
```

Unlike the N6 board, the F401RE has no external QSPI flash — the app runs from internal
flash, so `west flash` (via `stm32cubeprogrammer`) needs no external loader or download
address.
