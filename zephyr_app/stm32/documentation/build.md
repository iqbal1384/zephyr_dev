
# To build the specific project

cd ~/work/zephyr_dev/zephyr_app/stm32/nucleo_f401re/blinky

# Activate the viartual environment
source ~/work/zephyr_dev/zephyr/.venv/bin/activate


# Import the toolchain
export ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb
export GNUARMEMB_TOOLCHAIN_PATH=/opt/arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi


# Build the target
west build -b nucleo_f401re -d build-f401 .

# Build with the additinal overlays( device tree file )

west build -b nucleo_f401re -- -DDTC_OVERLAY_FILE=boards/rgb.overlay


# Flash the target
west flash -d build-f401


---

# nucleo_n657x0_q

## Toolchain
# Requires ARM GNU Toolchain 15.2 (NOT the GCC 13.3 bundled with STM32CubeCLT)
/opt/arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi/bin/

# Signing tool (from STM32CubeCLT 1.21.0)
/opt/st/stm32cubeclt_1.21.0/STM32CubeProgrammer/bin/STM32_SigningTool_CLI

## Build

cd ~/work/zephyr_dev/zephyr_app/stm32/nucleo_n657x0_q/blinky
source ~/work/zephyr_dev/zephyr/.venv/bin/activate
export PATH=/opt/st/stm32cubeclt_1.21.0/STM32CubeProgrammer/bin:$PATH

west build -b nucleo_n657x0_q -d build . -- \
    -DGNUARMEMB_TOOLCHAIN_PATH=/opt/arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi \
    -DZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb

# Or simply:
make            # incremental build
make pristine   # full rebuild

## Flash (via Windows STM32CubeProgrammer - west flash blocked by ST-Link FW V3J17)

Jumper settings for flashing (SWD to external XSPI flash):
  JP1 position 1 (BOOT0=0)
  JP2 position 2 (BOOT1=1)  <-- non-printed side

STM32CubeProgrammer settings:
  Port:            ST-LINK, SWD, Hot Plug, AP=1
  External loader: MX25UM51245G_STM32N6570-NUCLEO
  File:            \\wsl$\Ubuntu\home\user\work\zephyr_dev\zephyr_app\stm32\nucleo_n657x0_q\blinky\build\zephyr\zephyr.signed.bin
  Start address:   0x70000000

Jumper settings to run after flashing:
  JP1 position 1 (BOOT0=0)
  JP2 position 1 (BOOT1=0)  <-- printed side
  Power cycle the board.

## Notes
- ST-Link FW on the NUCLEO is V3J17M10 - too old for STM32N6 Debug Authentication.
  west flash from WSL does not work until ST-Link FW is updated to V3J20+.
  Use Windows STM32CubeProgrammer as workaround.
- west update hal_stm32 required after pulling new Zephyr commits that bump the HAL
  (pinctrl DTSI path changed in commit 1d38a4ac04a).


  # For building L432KC

  cd /home/user/work/zephyr_dev/zephyr_app/stm32/nucleo_l432kc/rgb && source activate 
  
  export ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb
  export GNUARMEMB_TOOLCHAIN_PATH=/opt/arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi && west build -p always -b nucleo_l432kc -- -DDTC_OVERLAY_FILE=boards/rgb.overlay
  
  cd /home/user/work/zephyr_dev/zephyr_app/stm32/nucleo_l432kc/rgb && source activate

  cd /home/user/work/zephyr_dev/zephyr_app/stm32/nucleo_l432kc/rgb
source /home/user/work/zephyr_dev/zephyr/.venv/bin/activate
export ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb
export GNUARMEMB_TOOLCHAIN_PATH=/opt/arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi
west build -p always -b nucleo_l432kc -- -DDTC_OVERLAY_FILE=boards/rgb.overlay


---

# For building G474RE

cd /home/user/work/zephyr_dev/zephyr_app/stm32/nucleo_g474re/blinky
source /home/user/work/zephyr_dev/zephyr/.venv/bin/activate

export ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb
export GNUARMEMB_TOOLCHAIN_PATH=/opt/arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi

# Clean rebuild
west build -p always -b nucleo_g474re -d build .

# Incremental rebuild
west build -b nucleo_g474re -d build .

# Flash
west flash -d build


---

# For building G474RE CAN control node

cd /home/user/work/zephyr_dev/zephyr_app/stm32/nucleo_g474re/can_control
source /home/user/work/zephyr_dev/zephyr/.venv/bin/activate

export ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb
export GNUARMEMB_TOOLCHAIN_PATH=/opt/arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi

west build -p always -b nucleo_g474re -d build .
west flash -d build


# MP2

cd /home/user/work/zephyr_dev/zephyr_app/stm32/mp2_m33/blinky_dual_task
source /home/user/work/zephyr_dev/zephyr/.venv/bin/activate
west build -p always -b stm32mp257f_dk/stm32mp257fxx/m33 -d build .


# H723ZG

west build -b nucleo_h723zg zephyr_app/stm32/nucleo_h723zg/blinky

west build -b nucleo_h723zg -d build .


# For building H723ZG TMC5160A stepper driver

cd /home/user/work/zephyr_dev/zephyr_app/stm32/nucleo_h723zg/stepper_tmc5160
source /home/user/work/zephyr_dev/zephyr/.venv/bin/activate

export ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb
export GNUARMEMB_TOOLCHAIN_PATH=/opt/arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi

west build -p always -b nucleo_h723zg -d build .
west flash -d build


west build -p always -b nucleo_h723zg -d build .
west build -b nucleo_h723zg --sysbuild -d build

west build -p always -b nucleo_h723zg --sysbuild -d build

west flash -d build


# Curie

source /home/user/work/zephyr_dev/zephyr/.venv/bin/activate

export ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb
export GNUARMEMB_TOOLCHAIN_PATH=/opt/arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi

cd zephyr_app/stm32/curie4.0/j14_test
west build -b curie4_0 . -d build
west flash -d build
west flash -d build --runner stm32cubeprogrammer

