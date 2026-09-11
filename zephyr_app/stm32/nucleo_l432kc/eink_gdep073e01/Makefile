BUILD_DIR = build/zephyr

all:
	west build -b nucleo_l432kc -- -DDTC_OVERLAY_FILE=boards/eink_gdep073e01.overlay
	cp $(BUILD_DIR)/zephyr.elf $(BUILD_DIR)/eink_gdep073e01.elf
	cp $(BUILD_DIR)/zephyr.bin $(BUILD_DIR)/eink_gdep073e01.bin
	cp $(BUILD_DIR)/zephyr.hex $(BUILD_DIR)/eink_gdep073e01.hex

flash:
	west flash

clean:
	west build -t clean
