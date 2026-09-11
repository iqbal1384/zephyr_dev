# NUCLEO-G474RE Fan Control Node

This application runs on `nucleo_g474re` and is the enhanced successor to
[`can_control`](../can_control): it keeps LED control and adds closed-loop-ish
speed control of a 12V PWM fan (tach feedback) plus power monitoring
(voltage + current) of the fan supply rail. It is controlled by the same
external CAN master (e.g. NVIDIA Jetson Orin) over the same node/CAN IDs as
`can_control`, so it can be flashed onto that node in place of it.

## Hardware

| Signal                    | MCU pin          | Arduino header | Notes                                    |
|---------------------------|-------------------|-----------------|-------------------------------------------|
| Fan PWM out                | PB6 (TIM4_CH1)    | D10             | 25 kHz, active-high                       |
| Fan tach in                 | PC7               | D9              | Open-collector, internal pull-up enabled  |
| Voltage sensor `S`         | PA0 (ADC1_IN1)    | A0              | 0-25V module, 5:1 divider                 |
| ACS712 `OUT`                | PA1 (ADC1_IN2)    | A1              | Current sensor analog output              |
| LED0                        | PA5               | D13             | Onboard user LED (unchanged from can_control) |

D10 doubles as the default Arduino-header SPI1 chip-select on this board.
SPI1 is unused by this project, so the overlay disables it to free PB6 for
the fan PWM signal.

### Fan (4-wire PWM fan)

- Fan `+`/`-` power leads: to the 12V rail being measured (through the
  ACS712 current-sense path, see below).
- Fan PWM input: PB6/D10. A 3.3V logic PWM signal drives essentially all
  4-wire PC/DC fans directly; no level shifting needed.
- Fan tach output: PC7/D9. Tach outputs on these fans are open-collector, so
  the STM32 internal pull-up (enabled in the overlay) is enough — do **not**
  also tie the tach wire to the fan's own 5V rail through a pull-up, or you
  risk driving >3.3V into the GPIO.
- Firmware assumes **2 tach pulses per revolution** (`TACH_PULSES_PER_REV` in
  `src/main.c`), which is the norm for these fans. Check your fan's datasheet
  and adjust if it differs.

### ACS712 current sensor

Wire the fan's 12V supply **in series** through the ACS712's high-current
screw terminals (IP+ / IP-) so the fan current flows through the sense path.
Power the sensor's own logic side (`VCC`) from the Nucleo's 5V pin (ACS712
needs a 5V rail to bias correctly) and connect `GND` to a common ground with
the STM32. `OUT` goes to PA1/A1.

The sensitivity constant `ACS712_MV_PER_A` in `src/main.c` defaults to the
5A variant (185 mV/A), since a small 12V fan draws well under 1A and that
gives the best resolution. **Check the text printed on your specific module**
(something like `ACS712ELCTR-05B-T`, `-20A`, or `-30A`) and update the
constant if it's not the 5A part:

| Variant | mV per A |
|---------|----------|
| 5A      | 185      |
| 20A     | 100      |
| 30A     | 66       |

The firmware self-calibrates the zero-current offset at boot (fan is forced
to 0% duty first), so no manual zero trim is needed — just make sure nothing
else is drawing current through the sensor at power-up.

### Voltage sensor module

The pictured module is a passive resistive divider rated `VCC<25V` with a
5:1 ratio (`VOLTAGE_DIVIDER_RATIO` in `src/main.c`). Wire the 12V rail across
its screw terminals and `S` to PA0/A0, `GND` to common ground. This keeps the
ADC input safely under 3.3V for any measured voltage up to ~16V — do not use
it to measure rails above that without re-checking the math (`3.3V x 5 =
16.5V` is the ceiling for this wiring).

## CAN protocol (standard 11-bit IDs)

Same node identity as `can_control`:

- Master -> Node ID: `0x120`
- Node -> Master ID: `0x121`
- Node ID byte in payload: `0x01`

### Command frame format (`0x120`)

- Byte 0: command
- Byte 1..7: command data

| Cmd    | Name              | Byte 1                       | Description                          |
|--------|-------------------|-------------------------------|---------------------------------------|
| `0x01` | `CMD_SET_LED`      | `0x00` OFF, non-zero ON        | Set LED state (unchanged)             |
| `0x02` | `CMD_GET_STATUS`   | -                              | Request heartbeat/LED status (unchanged) |
| `0x03` | `CMD_PING`         | -                              | Liveness check (unchanged)            |
| `0x04` | `CMD_SET_FAN_SPEED`| level `0..5`                   | 0=off,1=20%,2=40%,3=60%,4=80%,5=100%  |
| `0x05` | `CMD_GET_TACH`     | -                              | Request fan RPM                       |
| `0x06` | `CMD_GET_POWER`    | -                              | Request rail voltage + fan current    |
| `0x07` | `CMD_GET_FAN_SPEED`| -                              | Request current speed level/duty      |

### Response frame format (`0x121`)

- Byte 0: node id (`0x01`)
- Byte 1: status (`0x00` ok, `0xEE` unknown command, `0xEF` bad argument)
- Byte 2: echoed command
- Byte 3..7: command-specific payload

For `CMD_SET_LED`, `CMD_GET_STATUS`, `CMD_PING`, `CMD_SET_FAN_SPEED`,
`CMD_GET_FAN_SPEED` (unchanged 2-byte style):

- Byte 3: value0 (command specific)
- Byte 4: value1 (command specific)
- Byte 5..7: reserved

For `CMD_GET_TACH`:

- Byte 3..4: RPM, unsigned 16-bit big-endian
- Byte 5..7: reserved

For `CMD_GET_POWER`:

- Byte 3..4: rail voltage in mV, unsigned 16-bit big-endian
- Byte 5..6: fan current in mA, signed 16-bit big-endian (two's complement;
  can read negative if the ACS712 is wired reversed)
- Byte 7: reserved

Power in mW = `voltage_mV * current_mA / 1000` (computed on the master side).

## Build

```bash
cd /home/user/work/zephyr_dev/zephyr_app/stm32/nucleo_g474re/fan_control
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

# LED ON / OFF
cansend can0 120#0101
cansend can0 120#0100

# fan speed: 0=off .. 5=100%
cansend can0 120#0403

# read tach (RPM)
cansend can0 120#05

# read power (voltage_mV, current_mA)
cansend can0 120#06

# read current fan speed level/duty
cansend can0 120#07
```
