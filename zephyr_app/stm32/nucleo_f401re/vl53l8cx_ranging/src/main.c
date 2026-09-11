/*
 * STMicroelectronics VL53L8CX Multizone Ranging Application
 *
 * Target: STM32 Nucleo-F401RE with X-NUCLEO-53L8A1 Shield
 * Integrates with external Zephyr sensor driver module (modules/sensor/st_vl53l8cx).
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/sensor/vl53l8cx.h>

#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET_OR(LED0_NODE, gpios, {0});

static void display_matrix_results(const struct vl53l8cx_ranging_data *data,
                                  const struct sensor_value *center_dist)
{
	uint8_t zones_per_side = (data->nb_zones == 64) ? 8 : 4;

	/* ANSI clear screen and home cursor */
	printf("\033[2J\033[H");
	printf("=================================================================\n");
	printf("    STMicroelectronics VL53L8CX 8x8 Multizone Ranging Demo      \n");
	printf("    Target: Nucleo-F401RE | Sensor Subsystem Driver Module      \n");
	printf("=================================================================\n\n");

	printf("Center Distance: %d.%03d m | Grid: %dx%d (%d zones)\n\n",
	       center_dist->val1, center_dist->val2 / 1000,
	       zones_per_side, zones_per_side, data->nb_zones);

	/* Top border */
	printf("+");
	for (int col = 0; col < zones_per_side; col++) {
		printf("---------+");
	}
	printf("\n");

	/* Display 2D Matrix (row by row, mirrored columns for natural POV) */
	for (int row = 0; row < zones_per_side; row++) {
		printf("|");
		for (int col = zones_per_side - 1; col >= 0; col--) {
			int zone = row * zones_per_side + col;
			int16_t dist = data->distance_mm[zone];

			if (data->nb_targets[zone] > 0 && dist > 0) {
				printf(" %4d mm |", dist);
			} else {
				printf("   ----  |");
			}
		}
		printf("\n");

		/* Status row */
		printf("|");
		for (int col = zones_per_side - 1; col >= 0; col--) {
			int zone = row * zones_per_side + col;
			if (data->nb_targets[zone] > 0) {
				printf("  [st:%2d] |", data->target_status[zone]);
			} else {
				printf("  [none] |");
			}
		}
		printf("\n+");
		for (int col = 0; col < zones_per_side; col++) {
			printf("---------+");
		}
		printf("\n");
	}

	printf("\nSampling active... (Press reset on Nucleo to restart)\n");
	fflush(stdout);
}

static void scan_i2c_bus(const struct device *i2c_dev)
{
	if (!device_is_ready(i2c_dev)) {
		printf("[i2c_scan] ERROR: I2C bus device not ready\n");
		return;
	}

	printf("[i2c_scan] Scanning I2C bus %s...\n", i2c_dev->name);
	bool found = false;
	for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
		uint8_t dummy = 0;
		if (i2c_write(i2c_dev, &dummy, 0, addr) == 0) {
			printf("[i2c_scan]   -> ACK at address 0x%02x\n", addr);
			found = true;
		}
	}
	if (!found) {
		printf("[i2c_scan]   -> No devices answered (Check J9 jumper on shield: must be [2-3] for I2C)\n");
	}
	fflush(stdout);
}

int main(void)
{
	struct sensor_value center_dist;
	struct vl53l8cx_ranging_data matrix_data;
	int ret;

	if (gpio_is_ready_dt(&led)) {
		gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	}

	printf("\n\n==================================================\n");
	printf("*** STM32F401RE VL53L8CX Sensor Application ***\n");
	printf("==================================================\n");
	fflush(stdout);

	const struct device *const dev = DEVICE_DT_GET_ANY(st_vl53l8cx);
	const struct device *const i2c1_dev = DEVICE_DT_GET(DT_NODELABEL(i2c1));

	if (!dev) {
		printf("FATAL: No st,vl53l8cx device in devicetree!\n");
		while (1) {
			if (gpio_is_ready_dt(&led)) { gpio_pin_toggle_dt(&led); }
			k_msleep(200);
		}
	}

	while (!device_is_ready(dev)) {
		printf("\n[app] Device '%s' not ready (init failed during boot).\n", dev->name);
		scan_i2c_bus(i2c1_dev);
		printf("[app] Retrying in 2 seconds... (Press reset on board once shield jumper is verified)\n");
		fflush(stdout);

		if (gpio_is_ready_dt(&led)) { gpio_pin_toggle_dt(&led); }
		k_msleep(2000);
	}

	printf("\nSUCCESS: Found ready VL53L8CX sensor: %s\n", dev->name);
	fflush(stdout);

	while (1) {
		/* Fetch latest ranging sample from sensor DSP */
		ret = sensor_sample_fetch(dev);
		if (ret == 0) {
			if (gpio_is_ready_dt(&led)) { gpio_pin_toggle_dt(&led); }

			/* Read standard center distance */
			sensor_channel_get(dev, SENSOR_CHAN_DISTANCE, &center_dist);

			/* Read full multizone 8x8 depth matrix */
			sensor_channel_get(dev, (enum sensor_channel)SENSOR_CHAN_VL53L8CX_DISTANCE_MATRIX,
			                   (struct sensor_value *)&matrix_data);

			/* Render visual terminal grid */
			display_matrix_results(&matrix_data, &center_dist);
		} else if (ret == -EBUSY) {
			/* Data not ready yet */
		} else {
			printf("WARN: sensor_sample_fetch error %d\n", ret);
			fflush(stdout);
		}

		k_msleep(100);
	}

	return 0;
}
