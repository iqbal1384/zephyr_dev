# MP2 M33 Blinky Project

## Overview
Zephyr-based blinky application for the **STM32MP257F-DK** running on the **Cortex-M33** core.

Blinks **LED1 (RED, GPIO H4)** every **1 second**.

## Hardware Details (from mp2.md)
- **GPIO Bank H** (base: 0x442b0000)
- **LED1 (RED):** PH4 — **Free for M33** (not claimed by Linux)
- **LED2 (GREEN):** PH5 — Free for M33
- **LED3 (ORANGE):** PH6 — Free for M33
- **LED4 (BLUE):** PH7 — Claimed by Linux (heartbeat trigger)

## Project Structure
```
mp2_m33/
└── blinky/
    ├── CMakeLists.txt      # Zephyr build configuration
    ├── prj.conf            # Zephyr kernel config
    ├── src/
    │   └── main.c          # Main application (GPIO + blinky loop)
    └── README.md           # This file
```

## Build & Deploy

### Prerequisites
- Zephyr SDK installed
- west tool configured
- Cortex-A35 running OpenSTLinux on MP2 board

### Build
```bash
cd /home/user/work/zephyr_dev/zephyr_app/stm32/mp2_m33/blinky
west build -p always -b stm32mp257f_dk/stm32mp257fxx/m33 -d build .
```

### Load & Run
1. Copy firmware to board:
   ```bash
   scp build/zephyr/zephyr.elf root@<board_ip>:/tmp/
   ```

2. Load via remoteproc (on board, in Linux):
   ```bash
   echo /tmp/zephyr.elf > /sys/class/remoteproc/remoteproc0/firmware
   echo start > /sys/class/remoteproc/remoteproc0/state
   ```

3. LED1 (RED) will blink every 1 second

## Future Enhancements
- [ ] Multi-LED control
- [ ] RPMsg communication with Linux for remote LED control
- [ ] PWM-based LED brightness control
- [ ] Sensor integration
