cd /home/user/work/zephyr_dev/zephyr_app/stm32/nucleo_g474re/rs485_mcumgr
source /home/user/work/zephyr_dev/zephyr/.venv/bin/activate
export ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb
export GNUARMEMB_TOOLCHAIN_PATH=/opt/arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi

# Build (sysbuild required for MCUboot - not automatic in this west version)
west build -b nucleo_g474re --sysbuild

# Flash - one command flashes both MCUboot and the signed app image, in the
# order recorded in build/domains.yaml (mcuboot, then rs485_mcumgr)
west flash


# For multiple nodes

cd /home/user/work/zephyr_dev/zephyr_app/stm32/nucleo_g474re/rs485_mcumgr
source /home/user/work/zephyr_dev/zephyr/.venv/bin/activate
export ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb
export GNUARMEMB_TOOLCHAIN_PATH=/opt/arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi
west build -b nucleo_g474re --sysbuild -p always


# In the pi

python3 pty_mcumgr_smoketest.py


python3 provision_node.py --dongle /dev/ttyACM1 --new-addr 2

sudo chown -R $(whoami):$(id -gn) application/


---

# nucleo_h723zg rs485_mcumgr

cd /home/user/work/zephyr_dev/zephyr_app/stm32/nucleo_h723zg/rs485_mcumgr
source /home/user/work/zephyr_dev/zephyr/.venv/bin/activate
export ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb
export GNUARMEMB_TOOLCHAIN_PATH=/opt/arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi

# Build (sysbuild required for MCUboot - not automatic in this west version)
west build -b nucleo_h723zg --sysbuild

# Flash - one command flashes both MCUboot and the signed app image
west flash

# RS485 wiring on this board: LPUART1 (Arduino D0/D1 = PB7/PB6) for data,
# DE/RE on PG12 (Arduino D7). Console/printk stays on USART3 (ST-LINK VCP),
# unaffected by the update transport.