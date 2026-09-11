#ifndef GDEP073E01_H_
#define GDEP073E01_H_

#include <stdint.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>

#define GDEP073E01_WIDTH  800
#define GDEP073E01_HEIGHT 480

/* 4-bit color codes as programmed into the panel's RAM (2 px per byte) */
enum gdep073e01_color {
	GDEP073E01_BLACK  = 0x0,
	GDEP073E01_WHITE  = 0x1,
	GDEP073E01_YELLOW = 0x2,
	GDEP073E01_RED    = 0x3,
	GDEP073E01_BLUE   = 0x5,
	GDEP073E01_GREEN  = 0x6,
};

struct gdep073e01_dev {
	const struct device *spi;
	struct spi_config spi_cfg;
	struct gpio_dt_spec cs;
	struct gpio_dt_spec dc;
	struct gpio_dt_spec rst;
	struct gpio_dt_spec busy;
};

/* Resets the panel, runs the SSD1677-family power-up/register sequence. */
int gdep073e01_init(struct gdep073e01_dev *dev);

/* Streams a solid color to the whole panel and triggers a full refresh. */
int gdep073e01_fill(struct gdep073e01_dev *dev, enum gdep073e01_color color);

/* Streams a 6-color vertical bar test pattern and triggers a full refresh. */
int gdep073e01_display_color_bars(struct gdep073e01_dev *dev);

/* Powers off and enters deep sleep. A hw_reset (gdep073e01_init) is
 * required to wake it back up.
 */
int gdep073e01_sleep(struct gdep073e01_dev *dev);

#endif /* GDEP073E01_H_ */
