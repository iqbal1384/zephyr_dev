# MP2 M33 Dual Task GPIO Project

## Overview
Zephyr-based dual-task GPIO application for the **STM32MP257F-DK** running on the **Cortex-M33** core.

- Task 1: Blinks **LED0 (ORANGE, GPIO H6)** every **1 second**
- Task 2: Toggles **PA1** every **100 microseconds**

## Hardware Details (from mp2.md)
- **GPIO Bank A** (STM32MP2 GPIOA)
- **PA1:** GPIO test output toggled every 100 us by task 2
- **LED1 (RED):** PH4 — **Free for M33** (not claimed by Linux)
- **LED2 (GREEN):** PH5 — Free for M33
- **LED3 (ORANGE):** PH6 — Free for M33
- **LED4 (BLUE):** PH7 — Claimed by Linux (heartbeat trigger)

## Project Structure
```
mp2_m33/
└── blinky_dual_task/
    ├── CMakeLists.txt      # Zephyr build configuration
    ├── prj.conf            # Zephyr kernel config
    ├── src/
   │   └── main.c          # Two independent threads: LED + PA1 toggle
    └── README.md           # This file
```

## Build & Deploy

### Prerequisites
- Zephyr SDK installed
- west tool configured
- Cortex-A35 running OpenSTLinux on MP2 board

### Build
```bash
cd /home/user/work/zephyr_dev/zephyr_app/stm32/mp2_m33/blinky_dual_task
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

3. LED0 (ORANGE) blinks every 1 second and PA1 toggles every 100 us

## Notes
- PA1 toggling uses `k_busy_wait(100)` to target a 100 us interval.
- Verify PA1 on a scope or logic analyzer connected to GPIOA pin 1.
