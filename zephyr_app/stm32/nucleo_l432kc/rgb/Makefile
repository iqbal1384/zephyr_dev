BUILD_DIR = build/zephyr

all:
	west build -b nucleo_l432kc -- -DDTC_OVERLAY_FILE=boards/rgb.overlay
	cp $(BUILD_DIR)/zephyr.elf $(BUILD_DIR)/rgb.elf
	cp $(BUILD_DIR)/zephyr.bin $(BUILD_DIR)/rgb.bin
	cp $(BUILD_DIR)/zephyr.hex $(BUILD_DIR)/rgb.hex

flash:
	west flash

clean:
	west build -t clean
