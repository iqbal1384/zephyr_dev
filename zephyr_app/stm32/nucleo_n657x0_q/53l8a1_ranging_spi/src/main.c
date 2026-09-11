/*
 * VL53L8A1 Simple Ranging — Zephyr SPI port for nucleo_n657x0_q
 *
 * Ported from STM32CubeMX X-CUBE-TOF1 example, then re-ported from this
 * project's own I2C variant (../53l8a1_ranging) onto SPI.
 * HAL calls replaced with Zephyr APIs:
 *   - SPI       : zephyr/drivers/spi.h
 *   - GPIO      : zephyr/drivers/gpio.h
 *   - Delay     : k_msleep()
 *   - Print     : printk()
 *
 * *** Requires the hardware change described in boards/nucleo_n657x0_q.overlay
 *     — the sensor's SPI lines must be jumper-wired directly to the spi5
 *     pins, and the shield's SPI_I2C_N jumper (J9) moved to SPI. ***
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>

#include "vl53l8cx_api.h"

/* --------------------------------------------------------------------------
 * Board wiring (X-NUCLEO-53L8A1 sensor, direct-wired to nucleo_n657x0_q):
 *   PWR_EN   -> PD7  (Arduino D9  — relocated, see overlay comment)
 *   LPn      -> PA12 (Arduino A3)
 *   SPI      -> spi5 / arduino_spi: SCK=PE15(D13) MOSI=PG2(D11)
 *               MISO=PG1(D12) NCS=PA3(D10, hardware NSS)
 * -------------------------------------------------------------------------- */
#define VL53L8A1_SPI_NODE   DT_NODELABEL(vl53l8a1)

/* SPI word size/bit order are fixed here; mode (CPOL/CPHA) and max frequency
 * come from the devicetree node (spi-cpol/spi-cpha/spi-max-frequency) — see
 * the overlay and lib/vl53l8cx/porting/platform.c for the assumptions behind
 * these settings and what to check if the sensor doesn't respond. */
static const struct spi_dt_spec vl53l8a1_spi = SPI_DT_SPEC_GET(
    VL53L8A1_SPI_NODE,
    SPI_OP_MODE_MASTER | SPI_WORD_SET(8) | SPI_TRANSFER_MSB,
    0);

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
    /* --- Check SPI bus + device are ready --- */
    if (!spi_is_ready_dt(&vl53l8a1_spi)) {
        printk("ERROR: SPI device not ready\n");
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

    /* --- Initialise platform struct --- */
    static VL53L8CX_Configuration sensor;
    memset(&sensor, 0, sizeof(sensor));
    sensor.platform.spi = &vl53l8a1_spi;

    /* --- Hardware reset: cycle LPn to ensure sensor starts from clean state --- */
#if DT_NODE_HAS_PROP(DT_PATH(zephyr_user), vl53l8a1_lpn_gpios)
    gpio_pin_set_dt(&lpn, 0);   /* LPn LOW  → sensor enters low-power/reset */
    k_msleep(10);
    gpio_pin_set_dt(&lpn, 1);   /* LPn HIGH → sensor boots from ROM */
    k_msleep(100);              /* allow full cold boot before any SPI access */
    printk("LPn cycled — sensor cold boot\n");
#endif

    /* --- SPI link diagnostics ---
     * (a) Print CS GPIO info to confirm software CS is configured.
     * (b) Write-verify: write 0xBE to 0x7FFF (page-select / safe scratchpad),
     *     immediately read it back.  If the SPI round-trip works the read
     *     must return 0xBE regardless of what the register means; if we get
     *     0x00 the link is broken at the hardware level.
     * (c) Read reg 0x0006 — boot-status register, expected 0x00 at cold boot. */
    {
        const struct spi_cs_control *cs = &vl53l8a1_spi.config.cs;
        if (cs->gpio.port) {
            printk("SPI CS GPIO: port=%s pin=%d active=%s\n",
                   cs->gpio.port->name, cs->gpio.pin,
                   (cs->gpio.dt_flags & GPIO_ACTIVE_LOW) ? "low" : "high");
        } else {
            printk("SPI CS GPIO: NOT configured (hardware NSS)\n");
        }
    }
    {
        uint8_t rd = 0xFF;
        VL53L8CX_WrByte(&sensor.platform, 0x7FFF, 0xBE);
        VL53L8CX_RdMulti(&sensor.platform, 0x7FFF, &rd, 1);
        printk("WR-verify 0x7FFF: wrote=0xBE read=0x%02x (%s)\n",
               rd, (rd == 0xBE) ? "MATCH-link OK" : "MISMATCH-link broken");
    }
    {
        uint8_t buf[4] = {0};
        VL53L8CX_RdMulti(&sensor.platform, 0x0000, buf, 4);
        printk("rd[0x0000..3]: %02x %02x %02x %02x\n",
               buf[0], buf[1], buf[2], buf[3]);
    }
    {
        uint8_t reg_val = 0xFF;
        VL53L8CX_RdMulti(&sensor.platform, 0x0006, &reg_val, 1);
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
