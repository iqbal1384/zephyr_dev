# M33 Hardware Resource Allocation — STM32MP257f-DK (mp2 workspace)

> Source: decompiled `stm32mp257f-dk-ca35tdcid-ostl-m33-examples-stm32mp25-disco.dtb`
> (Linux/A35 device tree, kernel 6.6.48-stm32mp-r1.2) via the `mp2` workspace build.
> Logged for cross-agent reference on 2026-07-10.

## Remoteproc / Firmware Node
- Node: `/ahb@1/m33@0`, `compatible = "st,stm32mp2-m33-tee"`
- `status = "okay"` (M33 is active/enabled in this build)
- Resets: `mcu_rst`, `hold_boot`
- Firmware currently loaded via `remoteproc0`: `m33_led_ctrl.elf`
  (`echo m33_led_ctrl.elf > /sys/class/remoteproc/remoteproc0/firmware`)

## IPCC (Inter-Processor Communication)
- `ipcc1: mailbox@40490000`, `compatible = "st,stm32mp1-ipcc"`
- `st,proc-id = 0`
- IRQs: rx=0xab, tx=0xac
- `status = "okay"`
- Used for OpenAMP/RPMsg mailboxes (`vq0`, `vq1`, `shutdown`)

## Reserved Memory (CM33-related regions)
| Region | Address | Size | Purpose |
|---|---|---|---|
| `cm33-sram1` | 0x0A041000 | 0x1F000 (124 KB) | CM33 internal SRAM1 |
| `cm33-sram2` | 0x0A060000 | 0x20000 (128 KB) | CM33 internal SRAM2 (also mapped as vring/rpmsg memory-region) |
| `cm33-retram` | 0x0A080000 | 0x1F000 (124 KB) | CM33 retention RAM |
| `cm33-cube-fw` | 0x80100000 | 0x800000 (8 MB) | CM33 CubeMX firmware code region (DDR) |
| `cm33-cube-data` | 0x80A00000 | 0x800000 (8 MB) | CM33 CubeMX firmware data region (DDR) |

## OpenAMP / RPMsg Shared Memory (vdev0, IPC channel)
| Region | Address | Size | Purpose |
|---|---|---|---|
| `ipc-shmem-1` | 0x81200000 | 0xF8000 (992 KB) | Shared DMA pool |
| `vdev0vring0` | 0x812F8000 | 0x1000 (4 KB) | virtio ring 0 |
| `vdev0vring1` | 0x812F9000 | 0x1000 (4 KB) | virtio ring 1 |
| `vdev0buffer` | 0x812FA000 | 0x6000 (24 KB) | virtio rpmsg buffers |

- `m33@0` `memory-region` = `{ ipc-shmem-1, vdev0vring0, vdev0vring1, vdev0buffer, cm33-sram2 }`
- `m33@0` `mboxes` = `vq0, vq1, shutdown` (all via `ipcc1`)
- Exposed to Linux as `/dev/ttyRPMSG0` (virtual UART) — current `m33_led_ctrl` app uses this channel for LED command RPC
- Additional rpmsg child node present but unused by current app: `i2c@2` (`compatible = "rpmsg,i2c-controller"`, `status = "okay"`) — an RPMsg-backed virtual I2C controller exposed by the M33 side

## GPIO / Peripheral Split (LEDs, as currently owned)
- Linux (`gpio-leds`) only claims **one** LED via device tree: `led-blue` (heartbeat trigger, GPIO bank H pin 7 → LED4/BLUE)
- Remaining LEDs on GPIO bank H (LED1 RED=PH4, LED2 GREEN=PH5, LED3 ORANGE=PH6) are **not** claimed in the Linux DT — free for M33 firmware, which currently drives all 4 (`m33_led_ctrl/M33/Core/Src/main.c`)
- GPIO bank H base: `gpio@442b0000` (part of `pinctrl@44240000`)
- Note: a prior note in `git_file/documents/m33_led_ctrl.md` says "we own only LED3 and LED2" — slightly inconsistent with current firmware (which controls LED1–4) and with the DT (which only reserves LED4/blue for Linux). Worth reconciling with the other agent.

## Secondary Co-processor (present but disabled)
- `/ahb@2/m0@0`, `compatible = "st,stm32mp2-m0"`, `status = "disabled"` — Cortex-M0+ core defined in DT but not enabled in this build.

## Current M33 Application (Reference)
- Firmware: BLE-agnostic LED controller (`m33_led_ctrl`), receives `LED[1-4]_ON/OFF/TOGGLE` commands over RPMsg virtual UART, drives BSP LED GPIOs directly.
- Linux-side companion: `m33_led_ctrl/linux_app/led_ctrl.c` (writes to `/dev/ttyRPMSG0`).

---

## Zephyr M33 Projects (mp2_m33 workspace)

### Project 1: Blinky (IN PROGRESS)
- **Location:** `zephyr_app/stm32/mp2_m33/blinky/`
- **Task:** Toggle LED1 (RED, PH4) every 1 second using Zephyr GPIO
- **Status:** Project structure created
  - `CMakeLists.txt` — Zephyr build config
  - `prj.conf` — GPIO + logging enabled
  - `src/main.c` — GPIO initialization + toggle loop
  - `README.md` — Build/deploy instructions
- **Build Command:** `west build -p always -b stm32mp257f_dk/stm32mp257fxx/m33 -d build .`
- **Next Steps:**
  - [ ] Verify board definition exists for stm32mp257f_dk
  - [ ] Compile and test on hardware
  - [ ] Confirm remoteproc loading mechanism

### Planned Projects
- **Project 2:** RPMsg-based multi-LED remote control (Linux ↔ M33)
- **Project 3:** Sensor integration (if available on board)
- **Project 4:** UART communication testing
