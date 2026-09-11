#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#define RED_NODE   DT_ALIAS(rgb_red)
#define GREEN_NODE DT_ALIAS(rgb_green)
#define BLUE_NODE  DT_ALIAS(rgb_blue)

static const struct gpio_dt_spec red = GPIO_DT_SPEC_GET(RED_NODE, gpios);
static const struct gpio_dt_spec green = GPIO_DT_SPEC_GET(GREEN_NODE, gpios);
static const struct gpio_dt_spec blue = GPIO_DT_SPEC_GET(BLUE_NODE, gpios);

struct rgb_task_cfg {
    uint32_t on_ms;
    uint32_t off_ms;
};

static struct rgb_task_cfg rgb_cfg = {
    .on_ms = 500,
    .off_ms = 1000,
};

static void blink_once(const struct gpio_dt_spec *led, uint32_t on_ms, uint32_t off_ms)
{
    gpio_pin_set_dt(led, 1);
    k_msleep(on_ms);
    gpio_pin_set_dt(led, 0);
    k_msleep(off_ms);
}

static void rgb_sequence_task(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    struct rgb_task_cfg *cfg = p1;

    while (1) {
        blink_once(&red, cfg->on_ms, cfg->off_ms);
        blink_once(&green, cfg->on_ms, cfg->off_ms);
        blink_once(&blue, cfg->on_ms, cfg->off_ms);
    }
}

#define STACK_SIZE 768
#define PRIORITY 5

K_THREAD_STACK_DEFINE(rgb_stack, STACK_SIZE);
static struct k_thread rgb_thread;

int main(void)
{
    if (!gpio_is_ready_dt(&red) || !gpio_is_ready_dt(&green) || !gpio_is_ready_dt(&blue)) {
        LOG_ERR("One or more RGB GPIO devices are not ready");
        return -ENODEV;
    }

    gpio_pin_configure_dt(&red, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&green, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&blue, GPIO_OUTPUT_INACTIVE);

    LOG_INF("Starting RGB sequence task...");

    k_thread_create(&rgb_thread, rgb_stack, STACK_SIZE,
                    rgb_sequence_task, &rgb_cfg, NULL, NULL,
                    PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(&rgb_thread, "rgb_sequence_task");

    while (1) 
    {
        k_msleep(1000);
    }
}
