/*
 * VL53L8A1 Simple Ranging — Zephyr port for nucleo_n657x0_q
 *
 * Ported from STM32CubeMX X-CUBE-TOF1 example.
 * HAL calls replaced with Zephyr APIs:
 *   - I2C       : zephyr/drivers/i2c.h
 *   - GPIO      : zephyr/drivers/gpio.h
 *   - Delay     : k_msleep()
 *   - Print     : printk()
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>

#include "vl53l8cx_api.h"

/* --------------------------------------------------------------------------
 * Board wiring (X-NUCLEO-53L8A1 on Arduino header, nucleo_n657x0_q):
 *   PWR_EN -> PG2  (Arduino D11)
 *   LPn    -> PA12 (Arduino A3)
 *   I2C    -> PH9/PC1 (SCL/SDA, Arduino D15/D14, i2c1)
 * -------------------------------------------------------------------------- */
#define VL53L8A1_I2C_NODE   DT_NODELABEL(vl53l8a1)
#define VL53L8A1_I2C_ADDR   0x29U   /* 7-bit (0x52 >> 1) */

/* --------------------------------------------------------------------------
 * Ranging configuration
 * -------------------------------------------------------------------------- */
#define TIMING_BUDGET       30U                    /* ms  — must be 5..100 ms */
#define RANGING_FREQUENCY   10U                    /* Hz  — must be consistent with budget */
#define RANGING_RESOLUTION  VL53L8CX_RESOLUTION_8X8
#define ZONES_PER_LINE      8U

/* --------------------------------------------------------------------------
 * Helpers
 * -------------------------------------------------------------------------- */
static void print_result(VL53L8CX_ResultsData *results, uint8_t zones_per_line)
{
    printk("\033[2J\033[H");   /* clear terminal */
    printk("VL53L8A1 — distance results (%dx%d)\n\n",
           zones_per_line, zones_per_line);

    for (int row = 0; row < zones_per_line; row++) {
        for (int col = zones_per_line - 1; col >= 0; col--) {
            int zone = row * zones_per_line + col;
            if (results->nb_target_detected[zone] > 0) {
                printk("| %4d mm ", results->distance_mm[zone * VL53L8CX_NB_TARGET_PER_ZONE]);
            } else {
                printk("|    ---- ");
            }
        }
        printk("|\n");
    }
    printk("\n");
}

/* --------------------------------------------------------------------------
 * Main
 * -------------------------------------------------------------------------- */
int main(void)
{
    /* --- Get I2C bus device from DTS --- */
    const struct device *i2c_dev = DEVICE_DT_GET(DT_BUS(VL53L8A1_I2C_NODE));

    if (!device_is_ready(i2c_dev)) {
        printk("ERROR: I2C device not ready\n");
        return -1;
    }

    /* --- Optional GPIO control (PWR_EN, LPn) --- */
#if DT_NODE_HAS_PROP(DT_PATH(zephyr_user), vl53l8a1_pwren_gpios)
    const struct gpio_dt_spec pwr_en =
        GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), vl53l8a1_pwren_gpios);
    if (!device_is_ready(pwr_en.port)) 
    {
        printk("WARN: PWR_EN GPIO port not ready\n");
    } 
    else 
    {
        gpio_pin_configure_dt(&pwr_en, GPIO_OUTPUT_INACTIVE);
        k_msleep(10);
        gpio_pin_set_dt(&pwr_en, 1);
        k_msleep(10);
        printk("PWR_EN asserted (port=%s pin=%d)\n", pwr_en.port->name, pwr_en.pin);
    }
#else
    printk("WARN: PWR_EN not defined in DTS\n");
#endif

#if DT_NODE_HAS_PROP(DT_PATH(zephyr_user), vl53l8a1_lpn_gpios)
    const struct gpio_dt_spec lpn =
        GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), vl53l8a1_lpn_gpios);
    if (!device_is_ready(lpn.port)) 
    {
        printk("WARN: LPn GPIO port not ready\n");
    } 
    else {
        gpio_pin_configure_dt(&lpn, GPIO_OUTPUT_INACTIVE);
        k_msleep(2);
        gpio_pin_set_dt(&lpn, 1);
        k_msleep(10);
        printk("LPn asserted (port=%s pin=%d)\n", lpn.port->name, lpn.pin);
    }
#else
    printk("WARN: LPn not defined in DTS\n");
#endif

    /* --- Full I2C bus scan --- */
    printk("I2C bus scan:\n");
    bool found_any = false;
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) 
    {
        uint8_t dummy = 0;
        if (i2c_write(i2c_dev, &dummy, 0, addr) == 0) 
        {
            printk("  ACK at 0x%02x\n", addr);
            found_any = true;
        }
    }
    if (!found_any) 
    {
        printk("  No devices found — check J9 jumper on shield (must be [2-3] for I2C)\n");
    }

    /* --- Initialise platform struct --- */
    static VL53L8CX_Configuration sensor;
    memset(&sensor, 0, sizeof(sensor));
    sensor.platform.i2c_dev  = i2c_dev;
    sensor.platform.address = VL53L8A1_I2C_ADDR;

    /* --- Hardware reset: cycle LPn to ensure sensor starts from clean state --- */
#if DT_NODE_HAS_PROP(DT_PATH(zephyr_user), vl53l8a1_lpn_gpios)
    gpio_pin_set_dt(&lpn, 0);   /* LPn LOW  → sensor enters low-power/reset */
    k_msleep(10);
    gpio_pin_set_dt(&lpn, 1);   /* LPn HIGH → sensor boots from ROM */
    k_msleep(100);              /* allow full cold boot before any I2C */
    printk("LPn cycled — sensor cold boot\n");
#endif

    /* --- Read reg 0x06 to check sensor state before init --- */
    {
        uint8_t reg_addr[2] = {0x00, 0x06};
        uint8_t reg_val = 0xFF;
        i2c_write_read(i2c_dev, VL53L8A1_I2C_ADDR, reg_addr, 2, &reg_val, 1);
        printk("Pre-init reg 0x0006 = 0x%02x\n", reg_val);
    }

    /* --- Init sensor --- */
    int64_t t0 = k_uptime_get();
    printk("VL53L8A1 Simple Ranging — initialising...\n");
    uint8_t status = vl53l8cx_init(&sensor);
    printk("vl53l8cx_init took %lld ms\n", k_uptime_get() - t0);
    if (status != VL53L8CX_STATUS_OK) 
    {
        printk("ERROR: vl53l8cx_init failed (status=%d)\n", status);
        return -1;
    }
    printk("Sensor init OK\n");

    /* --- Configure profile: 8x8 continuous --- */
    vl53l8cx_set_resolution(&sensor, RANGING_RESOLUTION);
    vl53l8cx_set_ranging_mode(&sensor, VL53L8CX_RANGING_MODE_CONTINUOUS);
    vl53l8cx_set_ranging_frequency_hz(&sensor, RANGING_FREQUENCY);
    vl53l8cx_set_integration_time_ms(&sensor, TIMING_BUDGET);

    /* --- Start ranging --- */
    status = vl53l8cx_start_ranging(&sensor);
    if (status != VL53L8CX_STATUS_OK) 
    {
        printk("ERROR: vl53l8cx_start_ranging failed (status=%d)\n", status);
        return -1;
    }
    printk("Ranging started — polling every 100 ms\n");

    static VL53L8CX_ResultsData results;
    uint8_t data_ready;

    while (1) 
    {
        vl53l8cx_check_data_ready(&sensor, &data_ready);
        if (data_ready) 
        {
            vl53l8cx_get_ranging_data(&sensor, &results);
            print_result(&results, ZONES_PER_LINE);
        }
        k_msleep(100);
    }

    return 0;
}
