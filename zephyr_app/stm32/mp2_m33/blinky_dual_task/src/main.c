/*
 * Zephyr dual-task GPIO demo for STM32MP257F-DK M33 Core
 *
 * Task 1: Blink LED0 (GPIO H6) every 1 second.
 * Task 2: Toggle PA1 every 10 microseconds, paced by TIM4's free-running
 *         hardware counter read directly from its register block (classic
 *         delay_us() pattern), not through Zephyr's PWM/counter subsystem.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <soc.h>

#define LED0_NODE DT_ALIAS(led0)
#define GPIOA_NODE DT_NODELABEL(gpioa)

#define LED_THREAD_STACK_SIZE 512
#define PA1_THREAD_STACK_SIZE 512
#define LED_THREAD_PRIORITY 7
#define PA1_THREAD_PRIORITY 7

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec pa1 = {
	.port = DEVICE_DT_GET(GPIOA_NODE),
	.pin = 1,
	.dt_flags = GPIO_ACTIVE_HIGH,
};

/*
 * TIM4 / RCC non-secure register blocks, cast directly from the known
 * physical address (matches this SoC's Linux devicetree and the address
 * pwm_stm32.c itself uses via DT_REG_ADDR()) rather than the ambiguous
 * secure/non-secure `TIM4`/`RCC` CMSIS macros, which resolve differently
 * depending on CORTEX_IN_SECURE_STATE.
 */
#define TIM4_REGS ((TIM_TypeDef *)0x40020000UL)
#define RCC_REGS  ((RCC_TypeDef *)0x44200000UL)

K_THREAD_STACK_DEFINE(led_stack, LED_THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(pa1_stack, PA1_THREAD_STACK_SIZE);

static struct k_thread led_thread_data;
static struct k_thread pa1_thread_data;

static void led_task(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	while (1) {
		(void)gpio_pin_toggle_dt(&led);
		k_msleep(1000);
	}
}

/*
 * ck_ker_tim4 = 200 MHz (confirmed live via
 * /sys/kernel/debug/clk/clk_summary on the STM32MP257F-DK). Prescaler 199
 * divides that down to 1 MHz, so TIM4->CNT increments once per microsecond
 * and can be read directly as a microsecond tick count.
 */
static void tim4_delay_init(void)
{
	RCC_REGS->TIM4CFGR |= RCC_TIM4CFGR_TIM4EN;

	TIM4_REGS->CR1 &= ~TIM_CR1_CEN;
	TIM4_REGS->PSC = 199U;
	TIM4_REGS->ARR = 0xFFFFU;
	TIM4_REGS->EGR = TIM_EGR_UG; /* force PSC/ARR reload immediately */
	TIM4_REGS->CR1 |= TIM_CR1_CEN;
}

static void delay_us(uint32_t us)
{
	TIM4_REGS->CNT = 0;
	while (TIM4_REGS->CNT < us) {
		/* busy-wait on the hardware counter */
	}
}

static void pa1_task(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	while (1) {
		(void)gpio_pin_toggle_dt(&pa1);
		delay_us(10);
	}
}

int main(void)
{
	int ret;

	if (!gpio_is_ready_dt(&led)) {
		printk("LED GPIO device %s is not ready\n", led.port->name);
		return 0;
	}

	if (!gpio_is_ready_dt(&pa1)) {
		printk("PA1 GPIO device %s is not ready\n", pa1.port->name);
		return 0;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		printk("Failed to configure LED GPIO pin: %d\n", ret);
		return 0;
	}

	ret = gpio_pin_configure_dt(&pa1, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		printk("Failed to configure PA1 GPIO pin: %d\n", ret);
		return 0;
	}

	tim4_delay_init();

	printk("M33 dual task started: LED0 blink + PA1 toggle @ 10us (TIM4 hw counter)\n");

	k_thread_create(&led_thread_data,
			led_stack,
			K_THREAD_STACK_SIZEOF(led_stack),
			led_task,
			NULL,
			NULL,
			NULL,
			LED_THREAD_PRIORITY,
			0,
			K_NO_WAIT);

	k_thread_create(&pa1_thread_data,
			pa1_stack,
			K_THREAD_STACK_SIZEOF(pa1_stack),
			pa1_task,
			NULL,
			NULL,
			NULL,
			PA1_THREAD_PRIORITY,
			0,
			K_NO_WAIT);

	while (1) {
		k_sleep(K_SECONDS(60));
	}

	return 0;
}
