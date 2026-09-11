

# Zephyr Build Reference

## First-time workspace setup

python3 -m venv .venv
source .venv/bin/activate
pip install west

# From the workspace root (~/work)
west init -l zephyr
west update zephyr hal_stm32 cmsis

pip install -r scripts/requirements-base.txt \
            -r scripts/requirements-build.txt \
            -r scripts/requirements-run.txt


---

## nucleo_f401re / nucleo_g474re

# Toolchain (GCC 13.3 from STM32CubeCLT 1.18.0)
export ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb
export GNUARMEMB_TOOLCHAIN_PATH=/opt/st/stm32cubeclt_1.18.0/GNU-tools-for-STM32

# Build
west build -b nucleo_f401re samples/basic/blinky

# Flash
west flash


---

## nucleo_n657x0_q (STM32N657)

# Toolchain — must use ARM GNU 15.2, NOT the GCC 13.3 from CubeCLT (unsupported on N6)
/opt/arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi/bin/

# Signing tool — from STM32CubeCLT 1.21.0 (not present in 1.18.0)
export PATH=/opt/st/stm32cubeclt_1.21.0/STM32CubeProgrammer/bin:$PATH

# If HAL is out of sync after a Zephyr pull (pinctrl DTS conflict), run:
west update hal_stm32

# Build — run from the application folder directly
cd ~/work/zephyr_app/stm32/nucleo_n657x0_q/blinky

source ~/work/zephyr/.venv/bin/activate
export PATH=/opt/st/stm32cubeclt_1.21.0/STM32CubeProgrammer/bin:$PATH

west build -b nucleo_n657x0_q -d build . -- \
    -DGNUARMEMB_TOOLCHAIN_PATH=/opt/arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi \
    -DZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb

# Or use the Makefile (same thing, one command):
make            # incremental build
make pristine   # full rebuild

## Flash — via Windows STM32CubeProgrammer
# (west flash blocked: ST-Link FW V3J17M10 too old for STM32N6 Debug Authentication)
# Needs V3J20+ — update via STM32CubeIDE ST-Link upgrade tool

# Jumper settings FOR FLASHING (SWD to external XSPI flash):
#   JP1 position 1  (BOOT0=0, printed side)
#   JP2 position 2  (BOOT1=1, non-printed side)

# Windows STM32CubeProgrammer settings:
#   Port:            ST-LINK, SWD, Hot Plug, AP=1
#   External loader: MX25UM51245G_STM32N6570-NUCLEO
#   File:            \\wsl$\Ubuntu\home\user\work\zephyr_app\stm32\nucleo_n657x0_q\blinky\build\zephyr\zephyr.signed.bin
#   Start address:   0x70000000

# Jumper settings TO RUN after flashing:
#   JP1 position 1  (BOOT0=0)
#   JP2 position 1  (BOOT1=0, printed side)
#   Power cycle the board.
