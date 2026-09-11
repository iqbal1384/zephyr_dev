#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* Bind to our alias nodes from the overlay */
#define RED_NODE    DT_ALIAS(rgb_red)
#define GREEN1_NODE DT_ALIAS(rgb_green1)
#define GREEN2_NODE DT_ALIAS(rgb_green2)
#define BLUE_NODE   DT_ALIAS(rgb_blue)

static const struct gpio_dt_spec red    = GPIO_DT_SPEC_GET(RED_NODE, gpios);
static const struct gpio_dt_spec green1 = GPIO_DT_SPEC_GET(GREEN1_NODE, gpios);
static const struct gpio_dt_spec green2 = GPIO_DT_SPEC_GET(GREEN2_NODE, gpios);
static const struct gpio_dt_spec blue   = GPIO_DT_SPEC_GET(BLUE_NODE, gpios);

struct led_task_cfg {
    const struct gpio_dt_spec *led;
    const char *name;
    uint32_t delay_ms;
};

/* LED thread entry function */
void led_thread_entry(void *p1, void *p2, void *p3)
{
    struct led_task_cfg *cfg = p1;

    gpio_pin_configure_dt(cfg->led, GPIO_OUTPUT_INACTIVE);
    LOG_INF("Thread started for %s", cfg->name);

    while (1) {
        gpio_pin_toggle_dt(cfg->led);
        k_msleep(cfg->delay_ms);
    }
}

/* Thread stacks + control blocks */
#define STACK_SIZE 512
#define PRIORITY 5

K_THREAD_STACK_DEFINE(red_stack,    STACK_SIZE);
K_THREAD_STACK_DEFINE(green1_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(green2_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(blue_stack,   STACK_SIZE);

static struct k_thread red_thread;
static struct k_thread green1_thread;
static struct k_thread green2_thread;
static struct k_thread blue_thread;

/* Config per LED */
static struct led_task_cfg red_cfg = {
    .led = &red,
    .name = "red_led",
    .delay_ms = 200
};

static struct led_task_cfg green1_cfg = {
    .led = &green1,
    .name = "green1_led",
    .delay_ms = 350
};

static struct led_task_cfg green2_cfg = {
    .led = &green2,
    .name = "green2_led",
    .delay_ms = 500
};

static struct led_task_cfg blue_cfg = {
    .led = &blue,
    .name = "blue_led",
    .delay_ms = 700
};

int main(void)
{
    LOG_INF("Starting RGB tasks…");

    /* Create threads dynamically */
    k_thread_create(&red_thread, red_stack, STACK_SIZE,
                    led_thread_entry, &red_cfg, NULL, NULL,
                    PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(&red_thread, "red_led_thread");

    k_thread_create(&green1_thread, green1_stack, STACK_SIZE,
                    led_thread_entry, &green1_cfg, NULL, NULL,
                    PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(&green1_thread, "green1_led_thread");

    k_thread_create(&green2_thread, green2_stack, STACK_SIZE,
                    led_thread_entry, &green2_cfg, NULL, NULL,
                    PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(&green2_thread, "green2_led_thread");

    k_thread_create(&blue_thread, blue_stack, STACK_SIZE,
                    led_thread_entry, &blue_cfg, NULL, NULL,
                    PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(&blue_thread, "blue_led_thread");

    /* Main thread just sleeps */
    while (1) {
        k_msleep(1000);
    }
}