#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <stdio.h>
#include <string.h>

#define UART_NODE DT_NODELABEL(usart1)
#define RS485_DE_NODE DT_NODELABEL(rs485_de)

#if !DT_NODE_HAS_STATUS(UART_NODE, okay)
#error "usart1 is not enabled in devicetree"
#endif

#if !DT_NODE_HAS_STATUS(RS485_DE_NODE, okay)
#error "rs485_de node is not enabled in devicetree"
#endif

static const struct device *const uart_dev = DEVICE_DT_GET(UART_NODE);
static const struct gpio_dt_spec rs485_de = GPIO_DT_SPEC_GET(RS485_DE_NODE, gpios);
static const struct uart_config uart_cfg = {
    .baudrate = 115200,
    .parity = UART_CFG_PARITY_NONE,
    .stop_bits = UART_CFG_STOP_BITS_1,
    .data_bits = UART_CFG_DATA_BITS_8,
    .flow_ctrl = UART_CFG_FLOW_CTRL_NONE,
};

static void rs485_send(const char *msg);

static void rs485_send_ack(const char *line)
{
    char tx_buf[80];
    int len;

    len = snprintf(tx_buf, sizeof(tx_buf), "ACK: %s\r\n", line);
    if (len > 0) {
        rs485_send(tx_buf);
    }
}

static void rs485_set_rx(void)
{
    gpio_pin_set_dt(&rs485_de, 0);
}

static void handle_received_char(char c, char *line, size_t *line_len)
{
    if (c == '\r' || c == '\n') {
        if (*line_len > 0U) {
            line[*line_len] = '\0';
            rs485_send_ack(line);
            *line_len = 0;
        }
    } else if (*line_len < 63U) {
        line[(*line_len)++] = c;
    } else {
        *line_len = 0;
        rs485_send("ERR: line too long\r\n");
    }
}

static void rs485_send(const char *msg)
{
    size_t i;

    gpio_pin_set_dt(&rs485_de, 1);
    k_busy_wait(100);

    for (i = 0; i < strlen(msg); i++) {
        uart_poll_out(uart_dev, (unsigned char)msg[i]);
    }

    /* uart_poll_out() only waits for the shift register to be free before
     * loading the next byte, so after the loop the last byte can still be
     * sitting in the data register, not yet finished shifting onto the
     * wire. Wait two frame times (10 bits @ 115200 baud each) plus margin
     * so DE isn't dropped mid-transmission.
     */
    k_busy_wait(200);

    k_busy_wait(20);
    gpio_pin_set_dt(&rs485_de, 0);
}

int main(void)
{
    int ret;
    char c;
    char line[64];
    size_t line_len = 0;

    printk("rs485 debug: booting\n");

    if (!device_is_ready(uart_dev)) {
        printk("rs485 debug: uart_dev not ready\n");
        return 0;
    }

    if (!gpio_is_ready_dt(&rs485_de)) {
        printk("rs485 debug: rs485_de gpio not ready\n");
        return 0;
    }

    ret = uart_configure(uart_dev, &uart_cfg);
    if (ret != 0) {
        printk("rs485 debug: uart_configure failed: %d\n", ret);
        return 0;
    }

    ret = gpio_pin_configure_dt(&rs485_de, GPIO_OUTPUT_INACTIVE);
    if (ret != 0) {
        printk("rs485 debug: gpio_pin_configure_dt failed: %d\n", ret);
        return 0;
    }

    printk("rs485 debug: init ok, sending banner\n");
    rs485_set_rx();
    rs485_send("RS485 STM32 ready @115200\r\n");
    rs485_send("Send text ending with ENTER\r\n");
    printk("rs485 debug: banner sent, entering rx loop\n");

    while (1) {
        bool received = false;

        while (uart_poll_in(uart_dev, &c) == 0) {
            received = true;
            handle_received_char(c, line, &line_len);
        }

        if (!received) {
            k_busy_wait(50);
        }
    }

    return 0;
}
