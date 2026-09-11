#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include "gdep073e01.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#define ZEPHYR_USER_NODE DT_PATH(zephyr_user)

static struct gdep073e01_dev eink = {
	.cs   = GPIO_DT_SPEC_GET(ZEPHYR_USER_NODE, eink_cs_gpios),
	.dc   = GPIO_DT_SPEC_GET(ZEPHYR_USER_NODE, eink_dc_gpios),
	.rst  = GPIO_DT_SPEC_GET(ZEPHYR_USER_NODE, eink_rst_gpios),
	.busy = GPIO_DT_SPEC_GET(ZEPHYR_USER_NODE, eink_busy_gpios),
};

int main(void)
{
	int ret;

	eink.spi = DEVICE_DT_GET(DT_NODELABEL(spi1));
	eink.spi_cfg.frequency = 2000000U;
	eink.spi_cfg.operation = SPI_WORD_SET(8) | SPI_TRANSFER_MSB;
	/* eink.spi_cfg.cs left zero-initialized: CS is driven manually below,
	 * not by the SPI driver.
	 */

	LOG_INF("Initializing GDEP073E01 7.3in Spectra 6 e-paper display...");

	ret = gdep073e01_init(&eink);
	if (ret) {
		LOG_ERR("Display init failed: %d", ret);
		return ret;
	}

	LOG_INF("Drawing color bar test pattern (full refresh takes ~15-20s)...");
	ret = gdep073e01_display_color_bars(&eink);
	if (ret) {
		LOG_ERR("Display refresh failed: %d", ret);
		return ret;
	}

	LOG_INF("Done. Putting panel into deep sleep.");
	gdep073e01_sleep(&eink);

	while (1) {
		k_msleep(1000);
	}

	return 0;
}
