#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ev_motors, LOG_LEVEL_INF);

#define SPEC(lbl) GPIO_DT_SPEC_GET(DT_NODELABEL(lbl), gpios)

/* Arduino → Nucleo mapping and shield usage (confirmed from sketch)
 *
 *   E1 = D3   -> PB3   (PWM)
 *   E2 = D11  -> PA7   (PWM)
 *   E3 = D5   -> PB4   (PWM)
 *   E4 = D6   -> PB10  (PWM)
 *   M1 = D4   -> PB5   (DIR)
 *   M2 = D12  -> PA6   (DIR)
 *   M3 = D8   -> PA9   (DIR)
 *   M4 = D7   -> PA8   (DIR)
 *
 * Devicetree overlay provides node labels:
 *   m1_pwm, m1_dir, m2_pwm, m2_dir, m3_pwm, m3_dir, m4_pwm, m4_dir
 */

static const struct gpio_dt_spec m1_pwm = SPEC(m1_pwm);
static const struct gpio_dt_spec m1_dir = SPEC(m1_dir);

static const struct gpio_dt_spec m2_pwm = SPEC(m2_pwm);
static const struct gpio_dt_spec m2_dir = SPEC(m2_dir);

static const struct gpio_dt_spec m3_pwm = SPEC(m3_pwm);
static const struct gpio_dt_spec m3_dir = SPEC(m3_dir);

static const struct gpio_dt_spec m4_pwm = SPEC(m4_pwm);
static const struct gpio_dt_spec m4_dir = SPEC(m4_dir);

static void motor_pins_init(const struct gpio_dt_spec *pwm,
                            const struct gpio_dt_spec *dir)
{
    int ret;

    ret = gpio_pin_configure_dt(pwm, GPIO_OUTPUT_INACTIVE);
    if (ret) {
        LOG_ERR("Failed to config PWM pin %s:%d (%d)",
                pwm->port->name, pwm->pin, ret);
    }

    ret = gpio_pin_configure_dt(dir, GPIO_OUTPUT_INACTIVE);
    if (ret) {
        LOG_ERR("Failed to config DIR pin %s:%d (%d)",
                dir->port->name, dir->pin, ret);
    }
}

static void motor_set(const char *name,
                      const struct gpio_dt_spec *pwm,
                      const struct gpio_dt_spec *dir,
                      bool dir_level, bool enable)
{
    gpio_pin_set_dt(dir, dir_level);
    gpio_pin_set_dt(pwm, enable ? 1 : 0);

    LOG_DBG("%s: DIR=%d, PWM=%d", name, dir_level, enable ? 1 : 0);
}

int main(void)
{
    LOG_INF("Motor demo start: 4 motors, 1s forward / 1s reverse");

    /* Configure all PWM + DIR pins as outputs */
    motor_pins_init(&m1_pwm, &m1_dir);
    motor_pins_init(&m2_pwm, &m2_dir);
    motor_pins_init(&m3_pwm, &m3_dir);
    motor_pins_init(&m4_pwm, &m4_dir);

    LOG_INF("GPIO init done");

    while (1) {
        /* ---------- FORWARD (same as *_advance() on Arduino) ---------- */
        LOG_INF("MOTORS: FORWARD");
        // M1_advance: M1 = LOW
        motor_set("M1", &m1_pwm, &m1_dir, 0, true);
        k_sleep(K_SECONDS(2));

        LOG_INF("MOTORS: STOPPED");
        motor_set("M1", &m1_pwm, &m1_dir, 0, false);
        
        k_sleep(K_SECONDS(10));

        /* ---------- REVERSE (same as *_back() on Arduino) ---------- */
        LOG_INF("MOTORS: REVERSE");
        // M1_back: M1 = HIGH
        motor_set("M1", &m1_pwm, &m1_dir, 1, true);

        k_sleep(K_SECONDS(1));
        motor_set("M1", &m1_pwm, &m1_dir, 0, false);
        
        k_sleep(K_SECONDS(5));
    }
}
