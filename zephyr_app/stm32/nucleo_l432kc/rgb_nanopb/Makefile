BUILD_DIR = build/zephyr

all:
	west build -b nucleo_l432kc -- -DDTC_OVERLAY_FILE=boards/rgb.overlay
	cp $(BUILD_DIR)/zephyr.elf $(BUILD_DIR)/rgb_nanopb.elf
	cp $(BUILD_DIR)/zephyr.bin $(BUILD_DIR)/rgb_nanopb.bin
	cp $(BUILD_DIR)/zephyr.hex $(BUILD_DIR)/rgb_nanopb.hex

flash:
	west flash

clean:
	west build -t clean
