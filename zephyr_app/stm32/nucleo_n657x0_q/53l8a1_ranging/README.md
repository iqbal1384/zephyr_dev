# X-NUCLEO-53L8A1 (VL53L8CX) 8x8 Multizone ToF Ranging on STM32N6 (nucleo_n657x0_q)

A self-contained, standalone Zephyr application for the **STMicroelectronics VL53L8CX Direct Time-of-Flight (dToF) 8x8 multizone sensor** running on the **STM32N657X0 Nucleo board (`nucleo_n657x0_q`)**.

---

## Hardware Setup

* **Base Board**: `nucleo_n657x0_q` (STM32N6570-DK / Nucleo-N657X0-Q)
* **Shield**: `X-NUCLEO-53L8A1` mounted onto the standard Arduino UNO V3 headers of the Nucleo.
* **Jumper Configuration on X-NUCLEO-53L8A1**:
  * **J9**: Set to **`[2-3]` (I2C mode)**. *(Setting [1-2] selects SPI mode).*

---

## Pin Mapping (Arduino Header)

| Signal | X-NUCLEO-53L8A1 Pin | STM32N6 Pin | Description |
| :--- | :--- | :--- | :--- |
| **I2C SCL** | Arduino `D15` | `PH9` (`i2c1`) | I2C Clock ($400\text{ kHz}$ Fast Mode) |
| **I2C SDA** | Arduino `D14` | `PC1` (`i2c1`) | I2C Data |
| **PWR_EN**  | Arduino `D11` | `PG2` (`gpiog 2`) | Power stage enable (Active HIGH) |
| **LPn**     | Arduino `A3`  | `PA12` (`gpioa 12`)| Low-power / hardware reset (Active LOW) |

> [!NOTE]
> `&spi5` is disabled in [boards/nucleo_n657x0_q.overlay](file:///home/user/work/zephyr_dev/zephyr_app/stm32/nucleo_n657x0_q/53l8a1_ranging/boards/nucleo_n657x0_q.overlay) to free pin `PG2` (`SPI5_MOSI`) for `PWR_EN`.

---

## Key Zephyr Configurations (`prj.conf`)

* **Heap Memory**: `CONFIG_HEAP_MEM_POOL_SIZE=40960` (40 KB heap allocated for uploading the $32\text{ KB}$ sensor firmware blob over I2C).
* **Interrupt-Driven I2C**: `CONFIG_I2C_STM32_INTERRUPT=y` (Required on STM32N6 to prevent the 255-byte hardware reload boundary corruption on large mailbox transfers).
* **Extended Transfer Timeout**: `CONFIG_I2C_STM32_TRANSFER_TIMEOUT_MSEC=5000` (Allows the full $\approx 740\text{ ms}$ continuous firmware upload).

---

## Build & Flash Commands

### Using `make`:
```bash
# Build the application
make

# Flash to the Nucleo-N6 board
make flash

# Clean build artifacts
make clean
```

### Using `west`:
```bash
west build -b nucleo_n657x0_q zephyr_app/stm32/nucleo_n657x0_q/53l8a1_ranging -d build
west flash -d build
```

---

## Output Display
Once booted, the application uploads the sensor firmware, performs calibration, and streams an ANSI-cleared $8\times8$ 64-zone distance heatmap directly to the serial terminal ($115200\text{ baud}$):

```
VL53L8A1 — distance results (8x8)

|  485 mm |  490 mm |  488 mm |  492 mm |  495 mm |  491 mm |  487 mm |  485 mm |
|  486 mm |  491 mm |  489 mm |  493 mm |  494 mm |  490 mm |  486 mm |  484 mm |
|  487 mm |  492 mm |  490 mm |  494 mm |  495 mm |  491 mm |  487 mm |  485 mm |
|  488 mm |  493 mm |  491 mm |  495 mm |  496 mm |  492 mm |  488 mm |  486 mm |
|  487 mm |  492 mm |  490 mm |  494 mm |  495 mm |  491 mm |  487 mm |  485 mm |
|  486 mm |  491 mm |  489 mm |  493 mm |  494 mm |  490 mm |  486 mm |  484 mm |
|  485 mm |  490 mm |  488 mm |  492 mm |  493 mm |  489 mm |  485 mm |  483 mm |
|  484 mm |  489 mm |  487 mm |  491 mm |  492 mm |  488 mm |  484 mm |  482 mm |
```

