# NUCLEO-H723ZG TMC5160A Stepper Driver

Drives a Trinamic/Analog Devices TMC5160A stepper driver over SPI using
Zephyr's built-in stepper subsystem (`adi,tmc51xx` driver -- covers the
TMC5130/TMC5160/TMC5161 family, all sharing the same SPI register map). No
custom protocol here: the app brings the device up and exposes it through
Zephyr's `stepper` / `stepper_ctrl` shell commands over the console UART, so
you can enable/move/stop the motor interactively.

Hardware has not been wired up yet -- this is the pin plan to build to.

## Hardware

| Signal                  | MCU pin | Arduino header | Notes                                    |
|--------------------------|---------|-----------------|--------------------------------------------|
| SPI1 SCK                | PA5     | D13             | Board default SPI1 pinout                  |
| SPI1 MISO (SDO on chip)  | PA6     | D12             |                                             |
| SPI1 MOSI (SDI on chip)  | PB5     | D11             |                                             |
| SPI1 CS (CSN on chip)    | PD14    | D10             | `cs-gpios` already set on `&spi1` in the board's default DTS |
| ENN (driver hw enable)   | PD15    | D9              | Active-high in this overlay: GPIO high = enabled, low = disabled |

Console/shell is on `usart3` (ST-Link VCP) at 115200 8N1, per this board's
default devicetree -- no change needed for that.

Not covered by this overlay -- wire per the TMC5160A datasheet before
applying power:

- **CLK**: tie to GND to use the chip's internal ~12 MHz oscillator (matches
  `clock-frequency = <12000000>` in the overlay). If you instead drive CLK
  from an external clock, update `clock-frequency` to match or all velocity/
  position math will be off.
- **VM / motor supply, coil outputs (A1/A2/B1/B2)**: motor and driver-stage
  power, sized for your motor's voltage/current rating.
- **VCC_IO / 5VOUT**: logic supply per datasheet.
- **Sense resistors**: `ihold`/`irun` in the overlay are raw 0-31 register
  values (31 = 32/32 scale), not amps -- the actual current also depends on
  your sense resistor value. Start low (current defaults: `irun=16`,
  `ihold=8`) and raise once you've checked the math for your resistors and
  motor rating.
- **DIAG0**: not wired/used in this overlay. Add a `diag0-gpios` property on
  the `tmc5160` node if you want interrupt-driven stall/position-reached
  events instead of polling.

## Shell usage

Build/flash, then open the serial console (`usart3`, 115200 8N1) and use
the `stepper` (driver-level) and `stepper_ctrl` (motion-controller) shell
commands. Device names below are the devicetree node labels used in
`boards/nucleo_h723zg.overlay`.

```
uart:~$ stepper enable tmc5160_stepper_driver
uart:~$ stepper_ctrl move_by tmc5160_motion_controller 3200
uart:~$ stepper_ctrl move_to tmc5160_motion_controller 0
uart:~$ stepper_ctrl run tmc5160_motion_controller 1
uart:~$ stepper_ctrl stop tmc5160_motion_controller
uart:~$ stepper disable tmc5160_stepper_driver
```

`move_by`/`move_to` take microsteps (at `micro-step-res = <16>`, that's
16 microsteps per full step -- 3200 microsteps = one full revolution on a
typical 200-full-step/rev motor). Run `stepper` / `stepper_ctrl` with no
args, or `<cmd> -h`, to list all subcommands (`info`, `control_info`,
`set_micro_step_res`, `configure_ramp`, `get_actual_position`, etc.).

Boot log also prints the exact device names to copy-paste, in case they
drift from this README.

## Ramp / current tuning

`boards/nucleo_h723zg.overlay` sets conservative bring-up values
(`vmax`, `amax`, `ihold`, `irun`, ...) on the `tmc5160-motion-controller`
node. Retune these once a real motor is wired, following the TMC5160A
datasheet's ramp generator and current scaling sections -- `stallguard2-*`
and `activate-stallguard2` properties are also available on the same node
if you want stall detection.

## Build

```bash
cd /home/user/work/zephyr_dev/zephyr_app/stm32/nucleo_h723zg/stepper_tmc5160
source /home/user/work/zephyr_dev/zephyr/.venv/bin/activate
export ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb
export GNUARMEMB_TOOLCHAIN_PATH=/opt/arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi
west build -p always -b nucleo_h723zg -d build .
```

## Flash

```bash
west flash -d build
```
