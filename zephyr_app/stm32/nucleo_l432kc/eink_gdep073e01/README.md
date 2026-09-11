# GDEP073E01 e-paper (nucleo_l432kc)

Drives a Good Display GDEP073E01 7.3" E Ink Spectra 6 (6-color) panel via a
DESPI-C73 adapter board, from a Nucleo-L432KC. Draws a 6-color vertical bar
test pattern on boot, then puts the panel to deep sleep.

## Wiring

DESPI-C73 header P2 (left to right in silkscreen order) -> Nucleo Arduino-Nano header:

| P2 pin | Signal        | Nucleo pin | MCU pin |
|--------|---------------|------------|---------|
| 1      | 3.3V          | 3V3        | -       |
| 2      | GND           | GND        | -       |
| 3      | SDI (MOSI)    | A6         | PA7     |
| 4      | SCK           | A4         | PA5     |
| 5      | CS            | A3         | PA4     |
| 6      | D/C           | D3         | PB0     |
| 7      | RES (reset)   | D7         | PC14    |
| 8      | BUSY          | D8         | PC15    |

The adapter has no MISO pin - the panel is write-only over SPI, so it isn't wired.

## Build & flash

```sh
make        # west build -b nucleo_l432kc -- -DDTC_OVERLAY_FILE=boards/eink_gdep073e01.overlay
make flash
```

## Notes

- Zephyr has no in-tree driver for this panel (the built-in `ssd16xx` driver
  only covers 1-bit mono/red panels up to 320px wide), so `src/gdep073e01.c`
  is a small self-contained driver: reset, SSD1677-family register init,
  4bpp scanline streaming, full refresh, deep sleep.
- Pixel data is streamed row-by-row (400-byte lines) rather than buffered as
  a full 192 KB frame, since the L432KC only has 64 KB of SRAM.
- Full refresh takes ~15-20s; this is a panel limitation, not a bug.
- Color encoding is 4 bits/pixel: `0x0` black, `0x1` white, `0x2` yellow,
  `0x3` red, `0x5` blue, `0x6` green (values `0x4`/`0x7` are used by the
  7-color E7 variant and don't apply to this 6-color E6 panel).
