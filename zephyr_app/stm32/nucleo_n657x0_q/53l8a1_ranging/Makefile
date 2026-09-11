BOARD       = nucleo_n657x0_q
BUILD_DIR   = build
ZEPHYR_VENV = /home/user/work/zephyr_dev/zephyr/.venv/bin/activate
TOOLCHAIN   = /opt/arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi
SIGNING_BIN = /opt/st/stm32cubeclt_1.21.0/STM32CubeProgrammer/bin

WEST = . $(ZEPHYR_VENV) && PATH=$(SIGNING_BIN):$$PATH west

all:
	$(WEST) build -b $(BOARD) -d $(BUILD_DIR) . -- \
	    -DGNUARMEMB_TOOLCHAIN_PATH=$(TOOLCHAIN) \
	    -DZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb

flash:
	$(WEST) flash -d $(BUILD_DIR)

pristine:
	$(WEST) build -p always -b $(BOARD) -d $(BUILD_DIR) . -- \
	    -DGNUARMEMB_TOOLCHAIN_PATH=$(TOOLCHAIN) \
	    -DZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb

clean:
	$(WEST) build -d $(BUILD_DIR) -t clean
