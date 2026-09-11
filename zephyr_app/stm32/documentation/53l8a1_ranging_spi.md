# VL53L8A1 Ranging — SPI Port (Project Documentation)

## Status: SPI protocol confirmed on hardware — see "Open Items" for remaining validation

This is an SPI-based sibling of [`53l8a1_ranging`](53l8a1_ranging.md) (which
talks I2C and is known-good). It targets the same **NUCLEO-N657X0-Q** board
and **X-NUCLEO-53L8A1 / VL53L8CX** sensor, but drives the sensor over SPI
instead of I2C.

It was written from scratch with **no SPI reference available** — no ST
example, no datasheet section, no vendored driver covers SPI for this part —
so both the wiring and the on-wire register protocol below are best-effort
and need validation against real hardware. See "Open Items".

---

## Hardware — a board change is required

### Why the shield's Arduino connector can't be used as-is

The X-NUCLEO-53L8A1 schematic shows the sensor's data/clock lines as combined
signals — `MOSI_SDA` and `MCLK_SCL` — literally the same physical wires used
for I2C `SDA`/`SCL`, switched between modes by the sensor's `SPI_I2C_N` pin
(LOW = I2C, HIGH = SPI). On this shield, those wires run through the Arduino
header to **D14/D15**, which land on STM32N6 pins **PC1/PH9**.

Checking `stm32n657x0hxq-pinctrl.dtsi`: **PC1 and PH9 have no SPI alternate
function** on the STM32N657 (only I2C1 / I3C1 / UART / etc). A hardware SPI
peripheral cannot drive or sample those lines — so SPI to this sensor through
the shield's Arduino connector, as wired, is not possible on this host board.

### The fix: direct-wire to `arduino_spi` (`spi5`)

The board exposes a real SPI bus on the Arduino header (`arduino_spi = &spi5`,
pins D10–D13). This project bypasses the shield connector and wires the
sensor's SPI signals directly to those pins:

| Sensor signal     | STM32N6 pin | Arduino pin | `spi5` function  |
|-------------------|-------------|-------------|------------------|
| `MCLK_SCL` (SCK)  | PE15        | D13         | `spi5_sck_pe15`  |
| `MOSI_SDA` (MOSI) | PG2         | D11         | `spi5_mosi_pg2`  |
| `MISO`            | PG1         | D12         | `spi5_miso_pg1`  |
| `NCS`             | PA3         | D10         | `spi5_nss_pa3` (hardware NSS) |

**Required physical changes:**
1. Run jumper wires from the X-NUCLEO-53L8A1 satellite connector (or sensor
   breakout) signals `MCLK_SCL` / `MOSI_SDA` / `MISO` / `NCS` to Nucleo pins
   PE15 / PG2 / PG1 / PA3.
2. Move the shield's `SPI_I2C_N` jumper (**J9**, silkscreened "one jumper for
   all devices") to its **SPI** position, so the sensor's `SPI_I2C_N` pin is
   driven HIGH. This is a physical board jumper — firmware cannot toggle it.
3. `PWR_EN` was on PG2 in the I2C variant (`spi5_mosi_pg2` — direct conflict
   with MOSI). It's relocated here to **PD7 (Arduino D9)**, which is unused
   by `spi5`/`i2c1`. Run a jumper wire from the shield's `PWR_EN` pin to PD7.
4. `LPn` keeps its original assignment, **PA12 (Arduino A3)** — no conflict.

---

## Project Structure

```
53l8a1_ranging_spi/
├── src/
│   └── main.c                          # Application entry point (SPI bring-up)
├── lib/
│   └── vl53l8cx/
│       ├── modules/                    # ST vendor API (unmodified, copied from ../53l8a1_ranging)
│       └── porting/                    # Zephyr platform abstraction layer — SPI
│           ├── platform.c              # spi_transceive_dt()-based register access
│           └── platform.h              # VL53L8CX_Platform now wraps a spi_dt_spec
├── dts/bindings/sensor/
│   └── st,vl53l8a1-spi.yaml            # Minimal out-of-tree binding (see note below)
├── boards/
│   └── nucleo_n657x0_q.overlay         # Device tree: spi5, GPIO pins, wiring notes
├── prj.conf                            # Zephyr Kconfig options (CONFIG_SPI=y, ...)
├── CMakeLists.txt
└── Makefile
```

> **Why a custom binding file is needed**: unlike `compatible = "i2c-device"` (a real,
> directly-matchable binding the I2C variant uses), `"spi-device"` is **not** a
> matchable compatible in mainline Zephyr — `spi-device.yaml` is a fields-only
> fragment meant to be `include`d by a vendor binding (see e.g.
> `dts/bindings/test/vnd,spi-device.yaml`). Using `compatible = "spi-device"`
> directly leaves the devicetree node bindingless: required properties like
> `spi-max-frequency` get parsed as plain values but none of `spi-device.yaml`'s
> *generated* properties (`duplex`, `frame-format`, `spi-interframe-delay-ns`, …)
> exist, and `SPI_DT_SPEC_GET()` fails to compile with "undeclared" errors on
> those `DT_PROP()` lookups. The fix: ship a one-line out-of-tree binding,
> `dts/bindings/sensor/st,vl53l8a1-spi.yaml`, declaring
> `compatible: "st,vl53l8a1-spi"` and `include: spi-device.yaml`. Zephyr's
> `DTS_ROOT` always includes the application directory, so it's auto-discovered
> — no `CMakeLists.txt` changes needed. The overlay's node now uses
> `compatible = "st,vl53l8a1-spi"` to match it.

Everything outside `porting/` and the overlay is an unmodified copy of the
I2C project — the ST ULD driver (`vl53l8cx_api.c` and friends) only ever calls
`VL53L8CX_RdByte/WrByte/RdMulti/WrMulti`, so the bus swap is contained
entirely to the platform layer plus `main.c`'s device binding.

---

## Platform Layer (`lib/vl53l8cx/porting/`) — protocol assumptions

`platform.c` carries a large banner comment with the full rationale; summary:

- **16-bit register address, big-endian, sent before the payload.**
- **Bit 15 = direction flag: read = `addr | 0x8000`, write = `addr & 0x7FFF`.**
  Without this bit the sensor interprets every read as a write, holds MISO
  idle-low, and all reads return 0x00 — which is exactly the failure mode
  observed on first hardware test (`poll_for_answer TIMEOUT, got=0x00`).
  This convention is consistent with other ST optical ranging sensors
  (VL53L0X, VL53L3CX, etc.).
- **NCS held low across the whole address+payload transfer**: each
  `RdMulti`/`WrMulti` is a single `spi_transceive_dt()` call with a
  two-entry scatter/gather buffer set (`[address][payload]`) — Zephyr's
  STM32 SPI driver asserts the hardware NSS pin for the duration of one such
  call.
- **SPI Mode 0** (CPOL=0, CPHA=0), MSB-first, 8-bit words — set via the
  devicetree node (`spi-cpol`/`spi-cpha` both omitted ⇒ default to 0) and
  `SPI_DT_SPEC_GET()`'s `operation` argument in `main.c`.
- **No dummy/turnaround bytes** between address phase and data phase.

### `VL53L8CX_Platform` struct

```c
typedef struct {
    const struct spi_dt_spec *spi;   // Zephyr SPI device spec (bus + NCS + mode)
    uint16_t                  address; // unused on SPI — see note below
} VL53L8CX_Platform;
```

The `address` field is dead weight on SPI, but it has to stay: the vendor ULD
driver's `vl53l8cx_set_i2c_address()` (in `vl53l8cx_api.c`, compiled
unconditionally — it's not behind any I2C `#ifdef`) directly writes
`p_dev->platform.address = i2c_address`. Without the field the build fails with
`'VL53L8CX_Platform' has no member named 'address'`. This SPI port simply never
calls that function.

### Function mapping

| Vendor function       | Zephyr implementation                                          |
|-----------------------|-----------------------------------------------------------------|
| `VL53L8CX_RdByte`     | delegates to `VL53L8CX_RdMulti` (1 byte)                       |
| `VL53L8CX_WrByte`     | delegates to `VL53L8CX_WrMulti` (1 byte)                       |
| `VL53L8CX_RdMulti`    | one `spi_transceive_dt()`, 2-buffer scatter/gather: `[addr (tx, discard rx)][dummy tx, real rx]` |
| `VL53L8CX_WrMulti`    | one `spi_transceive_dt()`, 2-buffer scatter/gather: `[addr][payload]`, no rx |
| `VL53L8CX_WaitMs`     | `k_msleep()`                                                   |
| `VL53L8CX_SwapBuffer` | manual 32-bit byte-swap loop (unchanged from I2C variant)      |

Note the SPI `WrMulti` needs **no heap allocation** (unlike the I2C variant's
`k_malloc()` to prepend the address to the payload) — `spi_buf_set` scatter/
gather sends the address and payload as separate buffers within one
NCS-low transaction. `CONFIG_HEAP_MEM_POOL_SIZE` was dropped from `prj.conf`
accordingly.

---

## Build and Flash

Same as the I2C variant — see [`53l8a1_ranging.md`](53l8a1_ranging.md#build-and-flash)
for the full STM32CubeProgrammer / jumper procedure, just from this folder:

```bash
cd ~/work/zephyr_dev/zephyr_app/stm32/nucleo_n657x0_q/53l8a1_ranging_spi

make            # incremental build
make pristine   # full rebuild
```

---

## Open Items

1. **Wiring**: confirm the satellite connector pinout against the physical
   sensor board you're using and complete the jumper-wire changes above.
2. **`SPI_I2C_N` jumper (J9)**: must be physically moved to SPI; the sensor
   stays in I2C mode (and won't respond on SPI at all) until it is.
3. **SPI clock speed**: `spi-max-frequency = <2000000>` (2 MHz) in the overlay
   is a conservative placeholder. Once `vl53l8cx_init()` passes, increase it
   toward the sensor's rated maximum (check the datasheet) to reduce the
   firmware-upload time.
4. ~~Register-access protocol~~ — confirmed on hardware: `addr | 0x8000` for
   reads, `addr & 0x7FFF` for writes, Mode 0, no dummy bytes.
5. ~~SPI mode~~ — confirmed: CPOL=0, CPHA=0 (Mode 0).
