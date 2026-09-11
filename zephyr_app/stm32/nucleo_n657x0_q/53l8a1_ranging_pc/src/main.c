/*
 * VL53L8A1 PC-Streaming Ranging — Zephyr port for nucleo_n657x0_q
 *
 * Outputs one CSV line per measurement frame on the serial console:
 *   FRAME:d0,d1,...,d63\n
 * where d[i] is the distance in mm for zone i (row-major order, 0 = no target).
 * The PC application parses these lines to reconstruct the 8x8 grid.
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>

#include "vl53l8cx_api.h"

#define VL53L8A1_I2C_NODE   DT_NODELABEL(vl53l8a1)
#define VL53L8A1_I2C_ADDR   0x29U

#define TIMING_BUDGET       30U
#define RANGING_FREQUENCY   10U
#define RANGING_RESOLUTION  VL53L8CX_RESOLUTION_8X8
#define NB_ZONES            64U

/* Output one CSV frame line — zone order: row-major [0..63]. */
static void print_frame_csv(VL53L8CX_ResultsData *results)
{
    printk("FRAME:");
    for (int i = 0; i < NB_ZONES; i++) {
        uint16_t d = 0;
        if (results->nb_target_detected[i] > 0) {
            int32_t raw = results->distance_mm[i * VL53L8CX_NB_TARGET_PER_ZONE];
            if (raw > 0 && raw < 65535)
                d = (uint16_t)raw;
        }
        if (i > 0)
            printk(",");
        printk("%u", d);
    }
    printk("\n");
}

int main(void)
{
    const struct device *i2c_dev = DEVICE_DT_GET(DT_BUS(VL53L8A1_I2C_NODE));

    if (!device_is_ready(i2c_dev)) {
        printk("ERROR: I2C device not ready\n");
        return -1;
    }

#if DT_NODE_HAS_PROP(DT_PATH(zephyr_user), vl53l8a1_pwren_gpios)
    const struct gpio_dt_spec pwr_en =
        GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), vl53l8a1_pwren_gpios);
    if (device_is_ready(pwr_en.port)) {
        gpio_pin_configure_dt(&pwr_en, GPIO_OUTPUT_INACTIVE);
        k_msleep(10);
        gpio_pin_set_dt(&pwr_en, 1);
        k_msleep(10);
        printk("PWR_EN asserted\n");
    }
#endif

#if DT_NODE_HAS_PROP(DT_PATH(zephyr_user), vl53l8a1_lpn_gpios)
    const struct gpio_dt_spec lpn =
        GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), vl53l8a1_lpn_gpios);
    if (device_is_ready(lpn.port)) {
        gpio_pin_configure_dt(&lpn, GPIO_OUTPUT_INACTIVE);
        k_msleep(2);
        gpio_pin_set_dt(&lpn, 1);
        k_msleep(10);
        printk("LPn asserted\n");
    }
#endif

#if DT_NODE_HAS_PROP(DT_PATH(zephyr_user), vl53l8a1_lpn_gpios)
    /* Cycle LPn to guarantee a clean sensor cold boot. */
    gpio_pin_set_dt(&lpn, 0);
    k_msleep(10);
    gpio_pin_set_dt(&lpn, 1);
    k_msleep(100);
    printk("LPn cycled — sensor cold boot\n");
#endif

    static VL53L8CX_Configuration sensor;
    memset(&sensor, 0, sizeof(sensor));
    sensor.platform.i2c_dev = i2c_dev;
    sensor.platform.address = VL53L8A1_I2C_ADDR;

    printk("VL53L8A1 PC streaming — initialising...\n");
    uint8_t status = vl53l8cx_init(&sensor);
    if (status != VL53L8CX_STATUS_OK) {
        printk("ERROR: vl53l8cx_init failed (status=%d)\n", status);
        return -1;
    }
    printk("Sensor init OK\n");

    vl53l8cx_set_resolution(&sensor, RANGING_RESOLUTION);
    vl53l8cx_set_ranging_mode(&sensor, VL53L8CX_RANGING_MODE_CONTINUOUS);
    vl53l8cx_set_ranging_frequency_hz(&sensor, RANGING_FREQUENCY);
    vl53l8cx_set_integration_time_ms(&sensor, TIMING_BUDGET);

    status = vl53l8cx_start_ranging(&sensor);
    if (status != VL53L8CX_STATUS_OK) {
        printk("ERROR: vl53l8cx_start_ranging failed (status=%d)\n", status);
        return -1;
    }
    printk("Streaming started — FRAME:<64 distances in mm>\n");

    static VL53L8CX_ResultsData results;
    uint8_t data_ready;

    while (1) {
        vl53l8cx_check_data_ready(&sensor, &data_ready);
        if (data_ready) {
            vl53l8cx_get_ranging_data(&sensor, &results);
            print_frame_csv(&results);
        }
        k_msleep(100);
    }

    return 0;
}
