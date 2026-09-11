#include "gdep073e01.h"

#include <errno.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(gdep073e01, LOG_LEVEL_DBG);

#define BUSY_TIMEOUT_MS 30000

static void cs_select(struct gdep073e01_dev *dev)
{
	gpio_pin_set_dt(&dev->cs, 0); /* CS is active LOW */
}

static void cs_deselect(struct gdep073e01_dev *dev)
{
	gpio_pin_set_dt(&dev->cs, 1);
}

static int spi_send(struct gdep073e01_dev *dev, const uint8_t *data, size_t len)
{
	struct spi_buf buf = { .buf = (void *)data, .len = len };
	struct spi_buf_set set = { .buffers = &buf, .count = 1 };

	return spi_write(dev->spi, &dev->spi_cfg, &set);
}

/* Sends a command byte (DC low) followed by optional parameter bytes (DC high),
 * holding CS asserted for the whole transaction.
 */
static int send_cmd(struct gdep073e01_dev *dev, uint8_t cmd, const uint8_t *params, size_t len)
{
	int ret;

	cs_select(dev);

	gpio_pin_set_dt(&dev->dc, 0);
	ret = spi_send(dev, &cmd, 1);
	if (ret) {
		goto out;
	}

	if (len > 0) {
		gpio_pin_set_dt(&dev->dc, 1);
		ret = spi_send(dev, params, len);
	}

out:
	cs_deselect(dev);
	return ret;
}

static int wait_busy(struct gdep073e01_dev *dev)
{
	int64_t start = k_uptime_get();
	int64_t last_log = start;

	LOG_DBG("wait_busy: entry, raw BUSY pin = %d", gpio_pin_get_dt(&dev->busy));

	while (gpio_pin_get_dt(&dev->busy) == 0) { /* LOW = busy */
		int64_t now = k_uptime_get();
		int32_t elapsed = (int32_t)(now - start);

		if (now - last_log >= 500) {
			if (elapsed > 5000) {
				LOG_WRN("wait_busy: still busy after %dms (raw=%d) - longer "
					"than a normal refresh, check BUSY/RST wiring and "
					"panel power",
					elapsed, gpio_pin_get_dt(&dev->busy));
			} else {
				LOG_INF("wait_busy: still busy after %dms (raw=%d)", elapsed,
					gpio_pin_get_dt(&dev->busy));
			}
			last_log = now;
		}

		if (elapsed > BUSY_TIMEOUT_MS) {
			LOG_ERR("Timeout waiting for BUSY to release (raw=%d)",
				gpio_pin_get_dt(&dev->busy));
			return -ETIMEDOUT;
		}
		k_msleep(10);
	}

	LOG_DBG("wait_busy: released after %dms", (int32_t)(k_uptime_get() - start));
	return 0;
}

static void hw_reset(struct gdep073e01_dev *dev)
{
	LOG_DBG("hw_reset: RST high (idle)");
	gpio_pin_set_dt(&dev->rst, 1);
	k_msleep(50);

	LOG_DBG("hw_reset: RST low (asserted), BUSY raw=%d", gpio_pin_get_dt(&dev->busy));
	gpio_pin_set_dt(&dev->rst, 0); /* RST is active LOW */
	k_msleep(20);

	LOG_DBG("hw_reset: RST high (released), BUSY raw=%d", gpio_pin_get_dt(&dev->busy));
	gpio_pin_set_dt(&dev->rst, 1);
	k_msleep(10);

	LOG_DBG("hw_reset: done, BUSY raw=%d", gpio_pin_get_dt(&dev->busy));
}

static int panel_init_registers(struct gdep073e01_dev *dev)
{
	static const uint8_t cmdh[]  = { 0x49, 0x55, 0x20, 0x08, 0x09, 0x18 };
	static const uint8_t pwr[]   = { 0x3F };
	static const uint8_t psr[]   = { 0x5F, 0x69 };
	static const uint8_t pofs[]  = { 0x00, 0x54, 0x00, 0x44 };
	static const uint8_t btst1[] = { 0x40, 0x1F, 0x1F, 0x2C };
	static const uint8_t btst2[] = { 0x6F, 0x1F, 0x17, 0x49 };
	static const uint8_t btst3[] = { 0x6F, 0x1F, 0x1F, 0x22 };
	static const uint8_t pll[]   = { 0x08 };
	static const uint8_t cdi[]   = { 0x3F };
	static const uint8_t tcon[]  = { 0x02, 0x00 };
	static const uint8_t tres[]  = { 0x03, 0x20, 0x01, 0xE0 }; /* 800 x 480 */
	static const uint8_t vdcs[]  = { 0x01 };
	static const uint8_t pws[]   = { 0x2F };
	int ret;

	ret = send_cmd(dev, 0xAA, cmdh, sizeof(cmdh));   /* CMDH */
	if (ret) return ret;
	ret = send_cmd(dev, 0x01, pwr, sizeof(pwr));     /* PWR */
	if (ret) return ret;
	ret = send_cmd(dev, 0x00, psr, sizeof(psr));     /* PSR */
	if (ret) return ret;
	ret = send_cmd(dev, 0x03, pofs, sizeof(pofs));   /* POFS */
	if (ret) return ret;
	ret = send_cmd(dev, 0x05, btst1, sizeof(btst1)); /* BTST1 */
	if (ret) return ret;
	ret = send_cmd(dev, 0x06, btst2, sizeof(btst2)); /* BTST2 */
	if (ret) return ret;
	ret = send_cmd(dev, 0x08, btst3, sizeof(btst3)); /* BTST3 */
	if (ret) return ret;
	ret = send_cmd(dev, 0x30, pll, sizeof(pll));     /* PLL */
	if (ret) return ret;
	ret = send_cmd(dev, 0x50, cdi, sizeof(cdi));     /* CDI */
	if (ret) return ret;
	ret = send_cmd(dev, 0x60, tcon, sizeof(tcon));   /* TCON */
	if (ret) return ret;
	ret = send_cmd(dev, 0x61, tres, sizeof(tres));   /* TRES */
	if (ret) return ret;
	ret = send_cmd(dev, 0x84, vdcs, sizeof(vdcs));   /* T_VDCS */
	if (ret) return ret;
	ret = send_cmd(dev, 0xE3, pws, sizeof(pws));     /* PWS */
	if (ret) return ret;

	ret = send_cmd(dev, 0x04, NULL, 0); /* PON: power on */
	if (ret) return ret;

	return wait_busy(dev);
}

int gdep073e01_init(struct gdep073e01_dev *dev)
{
	if (!device_is_ready(dev->spi)) {
		LOG_ERR("SPI bus not ready");
		return -ENODEV;
	}
	if (!gpio_is_ready_dt(&dev->cs) || !gpio_is_ready_dt(&dev->dc) ||
	    !gpio_is_ready_dt(&dev->rst) || !gpio_is_ready_dt(&dev->busy)) {
		LOG_ERR("One or more control GPIOs not ready");
		return -ENODEV;
	}

	gpio_pin_configure_dt(&dev->cs, GPIO_OUTPUT);
	gpio_pin_set_dt(&dev->cs, 1); /* deselected */
	gpio_pin_configure_dt(&dev->dc, GPIO_OUTPUT);
	gpio_pin_configure_dt(&dev->rst, GPIO_OUTPUT);
	gpio_pin_set_dt(&dev->rst, 1); /* idle, not in reset */
	gpio_pin_configure_dt(&dev->busy, GPIO_INPUT);

	hw_reset(dev);

	int ret = wait_busy(dev);

	if (ret) {
		return ret;
	}

	return panel_init_registers(dev);
}

/* Sends the 0x10 (data transmit) command and leaves CS asserted / DC high
 * so the caller can stream scanlines with repeated spi_send() calls.
 */
static int start_data_stream(struct gdep073e01_dev *dev)
{
	uint8_t cmd = 0x10;
	int ret;

	cs_select(dev);
	gpio_pin_set_dt(&dev->dc, 0);
	ret = spi_send(dev, &cmd, 1);
	if (ret) {
		cs_deselect(dev);
		return ret;
	}
	gpio_pin_set_dt(&dev->dc, 1);
	return 0;
}

static int refresh(struct gdep073e01_dev *dev)
{
	static const uint8_t cdi[] = { 0x3F };
	static const uint8_t drf[] = { 0x00 };
	int ret;

	ret = send_cmd(dev, 0x50, cdi, sizeof(cdi)); /* CDI: full refresh border */
	if (ret) return ret;

	ret = send_cmd(dev, 0x12, drf, sizeof(drf)); /* DRF: display refresh */
	if (ret) return ret;

	k_msleep(50);
	return wait_busy(dev);
}

int gdep073e01_fill(struct gdep073e01_dev *dev, enum gdep073e01_color color)
{
	uint8_t line[GDEP073E01_WIDTH / 2];
	int ret;

	memset(line, (uint8_t)((color << 4) | color), sizeof(line));

	ret = start_data_stream(dev);
	if (ret) {
		return ret;
	}

	for (int y = 0; y < GDEP073E01_HEIGHT; y++) {
		ret = spi_send(dev, line, sizeof(line));
		if (ret) {
			cs_deselect(dev);
			return ret;
		}
	}
	cs_deselect(dev);

	return refresh(dev);
}

int gdep073e01_display_color_bars(struct gdep073e01_dev *dev)
{
	static const enum gdep073e01_color bar_colors[] = {
		GDEP073E01_BLACK, GDEP073E01_WHITE, GDEP073E01_YELLOW,
		GDEP073E01_RED,   GDEP073E01_BLUE,  GDEP073E01_GREEN,
	};
	const int n_bars = ARRAY_SIZE(bar_colors);
	const int bar_width = GDEP073E01_WIDTH / n_bars;
	uint8_t line[GDEP073E01_WIDTH / 2];
	int ret;

	/* Vertical bars: identical on every row, so build the scanline once. */
	for (int x = 0; x < GDEP073E01_WIDTH; x++) {
		int bar = MIN(x / bar_width, n_bars - 1);
		uint8_t px = bar_colors[bar];

		if (x % 2 == 0) {
			line[x / 2] = (uint8_t)(px << 4);
		} else {
			line[x / 2] |= px;
		}
	}

	ret = start_data_stream(dev);
	if (ret) {
		return ret;
	}

	for (int y = 0; y < GDEP073E01_HEIGHT; y++) {
		ret = spi_send(dev, line, sizeof(line));
		if (ret) {
			cs_deselect(dev);
			return ret;
		}
	}
	cs_deselect(dev);

	return refresh(dev);
}

int gdep073e01_sleep(struct gdep073e01_dev *dev)
{
	static const uint8_t dslp[] = { 0xA5 };
	int ret;

	ret = send_cmd(dev, 0x02, NULL, 0); /* POF: power off */
	if (ret) return ret;

	ret = wait_busy(dev);
	if (ret) return ret;

	return send_cmd(dev, 0x07, dslp, sizeof(dslp)); /* DSLP: deep sleep */
}
