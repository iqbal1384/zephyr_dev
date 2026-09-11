/*
 * Receives nanopb-encoded LedCommand messages from a Raspberry Pi over the
 * console UART (usart2 / ST-LINK VCP, seen as /dev/ttyACMx on the host) and
 * drives the RGB LED accordingly.
 *
 * Wire format (matches led-ctrl on the Pi side): each message is a varint
 * (LEB128) byte length followed by that many bytes of LedCommand protobuf.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>

#include <pb_decode.h>

#include "src/led_ctrl.pb.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#define RED_NODE   DT_ALIAS(rgb_red)
#define GREEN_NODE DT_ALIAS(rgb_green)
#define BLUE_NODE  DT_ALIAS(rgb_blue)

static const struct gpio_dt_spec red = GPIO_DT_SPEC_GET(RED_NODE, gpios);
static const struct gpio_dt_spec green = GPIO_DT_SPEC_GET(GREEN_NODE, gpios);
static const struct gpio_dt_spec blue = GPIO_DT_SPEC_GET(BLUE_NODE, gpios);

static const struct device *const cmd_uart = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

/* Bytes queued here by the UART ISR and consumed by the decode loop in main(). */
K_MSGQ_DEFINE(uart_rx_msgq, sizeof(uint8_t), 128, 1);

static void uart_rx_isr(const struct device *dev, void *user_data)
{
    ARG_UNUSED(user_data);

    while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
        uint8_t byte;

        if (!uart_irq_rx_ready(dev)) {
            continue;
        }

        while (uart_fifo_read(dev, &byte, 1) == 1) {
            /* Best effort: drop the byte if the queue is ever full. */
            (void)k_msgq_put(&uart_rx_msgq, &byte, K_NO_WAIT);
        }
    }
}

static void apply_led_command(const LedCommand *cmd)
{
    gpio_pin_set_dt(&red, cmd->red_on);
    gpio_pin_set_dt(&green, cmd->green_on);
    gpio_pin_set_dt(&blue, cmd->blue_on);

    LOG_INF("LedCommand applied: R=%d G=%d B=%d", cmd->red_on, cmd->green_on, cmd->blue_on);
}

/* Blocks reading one byte from the UART RX queue. */
static uint8_t recv_byte(void)
{
    uint8_t byte;

    k_msgq_get(&uart_rx_msgq, &byte, K_FOREVER);
    return byte;
}

/* Reads a LEB128 varint length prefix, one byte at a time, off the UART. */
static uint32_t recv_varint_len(void)
{
    uint32_t value = 0;
    uint32_t shift = 0;
    uint8_t byte;

    do {
        byte = recv_byte();
        value |= (uint32_t)(byte & 0x7f) << shift;
        shift += 7;
    } while ((byte & 0x80) && shift < 32);

    return value;
}

/* Reads one framed LedCommand message and applies it. Resyncs on bad frames. */
static void recv_and_apply_command(void)
{
    uint8_t payload[LedCommand_size];
    uint32_t msg_len = recv_varint_len();

    if (msg_len == 0 || msg_len > sizeof(payload)) {
        LOG_WRN("Dropping frame with bad length %u", msg_len);
        return;
    }

    for (uint32_t i = 0; i < msg_len; i++) {
        payload[i] = recv_byte();
    }

    LedCommand cmd = LedCommand_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(payload, msg_len);

    if (pb_decode(&stream, LedCommand_fields, &cmd)) {
        apply_led_command(&cmd);
    } else {
        LOG_ERR("nanopb decode failed: %s", PB_GET_ERROR(&stream));
    }
}

int main(void)
{
    if (!gpio_is_ready_dt(&red) || !gpio_is_ready_dt(&green) || !gpio_is_ready_dt(&blue)) {
        LOG_ERR("One or more RGB GPIO devices are not ready");
        return -ENODEV;
    }

    gpio_pin_configure_dt(&red, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&green, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&blue, GPIO_OUTPUT_INACTIVE);

    if (!device_is_ready(cmd_uart)) {
        LOG_ERR("Command UART device is not ready");
        return -ENODEV;
    }

    uart_irq_callback_user_data_set(cmd_uart, uart_rx_isr, NULL);
    uart_irq_rx_enable(cmd_uart);

    LOG_INF("Waiting for LedCommand frames...");

    while (1) {
        recv_and_apply_command();
    }
}
