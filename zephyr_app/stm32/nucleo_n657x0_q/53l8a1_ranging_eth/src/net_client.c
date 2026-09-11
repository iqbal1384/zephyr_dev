#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/dhcpv4.h>
#include <zephyr/sys/printk.h>

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "net_client.h"
#include "server_config.h"

#define NET_CLIENT_STACK_SIZE 4096
#define NET_CLIENT_PRIORITY   7
#define NET_CLIENT_QUEUE_LEN  8
#define NET_TX_LINE_MAX       1024

K_MSGQ_DEFINE(ranging_msgq, sizeof(struct ranging_frame), NET_CLIENT_QUEUE_LEN, 4);
K_THREAD_STACK_DEFINE(net_client_stack, NET_CLIENT_STACK_SIZE);

static struct k_thread net_client_thread_data;
static k_tid_t net_client_tid;
static bool net_client_started;

static int format_frame_line(const struct ranging_frame *frame, char *buf, size_t len)
{
    int used = snprintf(buf, len, "ts=%lld", frame->timestamp_ms);

    if (used < 0 || (size_t)used >= len) {
        return -ENOMEM;
    }

    for (int i = 0; i < RANGING_ZONE_COUNT; i++) {
        int written = snprintf(&buf[used], len - (size_t)used,
                               ",z%d=%d", i,
                               frame->target_detected[i] ? frame->distance_mm[i] : -1);
        if (written < 0 || (size_t)written >= (len - (size_t)used)) {
            return -ENOMEM;
        }
        used += written;
    }

    if ((size_t)used + 1U >= len) {
        return -ENOMEM;
    }

    buf[used++] = '\n';
    buf[used] = '\0';

    return used;
}

static void print_ipv4_addr(void)
{
    struct net_if *iface = net_if_get_default();
    if (!iface) {
        printk("NET: no default interface\n");
        return;
    }

    struct net_in_addr *addr = net_if_ipv4_get_global_addr(iface,
                                                            NET_ADDR_PREFERRED);
    if (addr) {
        char addr_str[NET_IPV4_ADDR_LEN];
        net_addr_ntop(AF_INET, addr, addr_str, sizeof(addr_str));
        printk("NET: IPv4 = %s\n", addr_str);
        return;
    }

    printk("NET: no preferred IPv4 address assigned yet\n");
}

static int apply_static_ipv4_fallback(struct net_if *iface)
{
    struct net_in_addr ip;
    struct net_in_addr netmask;
    struct net_in_addr gw;

    if (net_addr_pton(AF_INET, DEVICE_IP_ADDR, &ip) < 0 ||
        net_addr_pton(AF_INET, DEVICE_NETMASK, &netmask) < 0 ||
        net_addr_pton(AF_INET, DEVICE_GW_ADDR, &gw) < 0) {
        printk("NET: invalid static fallback config\n");
        return -EINVAL;
    }

#if defined(CONFIG_NET_DHCPV4)
    net_dhcpv4_stop(iface);
#endif

    struct net_if_addr *added = net_if_ipv4_addr_add(iface, &ip,
                                                     NET_ADDR_MANUAL, 0);
    if (added == NULL) {
        printk("NET: failed to add static IPv4 address\n");
        return -EADDRNOTAVAIL;
    }

    (void)net_if_ipv4_set_netmask_by_addr(iface, &ip, &netmask);
    net_if_ipv4_set_gw(iface, &gw);

    printk("NET: static fallback applied (%s/%s gw %s)\n",
           DEVICE_IP_ADDR, DEVICE_NETMASK, DEVICE_GW_ADDR);
    return 0;
}

int net_client_init(void)
{
    printk("NET: waiting for network interface...\n");

    struct net_if *iface = net_if_get_default();
    if (!iface) {
        printk("ERROR: NET: no default interface available\n");
        return -ENODEV;
    }

    printk("NET: interface: %s\n", net_if_get_device(iface)->name);

    if (!net_if_is_admin_up(iface)) {
        int up_ret = net_if_up(iface);
        printk("NET: net_if_up() -> %d\n", up_ret);
    }

#if DEVICE_FORCE_STATIC
    printk("NET: DEVICE_FORCE_STATIC=1, applying static IPv4\n");
    int static_ret = apply_static_ipv4_fallback(iface);
    if (static_ret == 0) {
        print_ipv4_addr();
        printk("NET: interface is up and configured (static)\n");
        return 0;
    }
    printk("NET: static apply failed (%d), continuing with DHCP\n", static_ret);
#endif

#if defined(CONFIG_NET_DHCPV4)
    printk("NET: starting DHCPv4 client\n");
    net_dhcpv4_start(iface);
#endif

    /* Wait for interface to be up and configured (DHCP) */
    int timeout_ms = DEVICE_DHCP_TIMEOUT_MS;
    int poll_interval = 500; /* 500 ms */
    int elapsed = 0;
    int next_log_ms = 0;

    while (elapsed < timeout_ms) {
        bool is_up = net_if_is_up(iface);
        bool carrier_ok = net_if_is_carrier_ok(iface);

        if (elapsed >= next_log_ms) {
            printk("NET: link up=%d carrier=%d\n", is_up, carrier_ok);
            next_log_ms += 5000;
        }

        if (is_up && carrier_ok) {
            struct net_in_addr *addr = net_if_ipv4_get_global_addr(iface,
                                                                    NET_ADDR_PREFERRED);

            if (addr != NULL) {
                print_ipv4_addr();
                printk("NET: interface is up and configured\n");
                return 0;
            }
        }

        k_msleep(poll_interval);
        elapsed += poll_interval;
    }

    printk("WARN: NET: timeout waiting for interface configuration\n");
    print_ipv4_addr();

#if DEVICE_USE_STATIC_FALLBACK
    printk("NET: DHCP timeout, trying static fallback\n");
    int fallback_ret = apply_static_ipv4_fallback(iface);
    if (fallback_ret == 0) {
        print_ipv4_addr();
        return 0;
    }
    printk("NET: static fallback failed (%d)\n", fallback_ret);
#endif

    return -ETIMEDOUT;
}

static int connect_to_server(void)
{
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(SERVER_PORT),
    };

    if (zsock_inet_pton(AF_INET, SERVER_IP_ADDR, &server_addr.sin_addr) != 1) {
        printk("NET: invalid SERVER_IP_ADDR: %s\n", SERVER_IP_ADDR);
        return -EINVAL;
    }

    int sock = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        int err = -errno;
        printk("NET: socket() failed: %d\n", err);
        return err;
    }

    printk("NET: connecting to %s:%d...\n", SERVER_IP_ADDR, SERVER_PORT);

    if (zsock_connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        int err = -errno;
        printk("NET: connect() failed: %d\n", err);
        zsock_close(sock);
        return err;
    }

    printk("NET: connected to %s:%d\n", SERVER_IP_ADDR, SERVER_PORT);
    return sock;
}

static void net_client_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    int sock = -1;
    char tx_line[NET_TX_LINE_MAX];

    while (1) {
        if (sock < 0) {
            sock = connect_to_server();
            if (sock < 0) {
                printk("NET: connect failed, retry in %d ms\n",
                       SERVER_RECONNECT_DELAY_MS);
                k_msleep(SERVER_RECONNECT_DELAY_MS);
                continue;
            }
        }

        struct ranging_frame frame;
        if (k_msgq_get(&ranging_msgq, &frame, K_MSEC(500)) != 0) {
            continue;
        }

        int tx_len = format_frame_line(&frame, tx_line, sizeof(tx_line));
        if (tx_len < 0) {
            printk("NET: frame encode failed (%d)\n", tx_len);
            continue;
        }

        int sent = zsock_send(sock, tx_line, (size_t)tx_len, 0);
        if (sent < 0) {
            printk("NET: send failed (%d), reconnecting\n", -errno);
            zsock_close(sock);
            sock = -1;
        }
    }
}

int net_client_start(void)
{
    if (net_client_started) {
        return 0;
    }

    net_client_tid = k_thread_create(&net_client_thread_data,
                                     net_client_stack,
                                     K_THREAD_STACK_SIZEOF(net_client_stack),
                                     net_client_thread,
                                     NULL, NULL, NULL,
                                     NET_CLIENT_PRIORITY,
                                     0,
                                     K_NO_WAIT);

    if (!net_client_tid) {
        return -ENOMEM;
    }

    k_thread_name_set(net_client_tid, "net_client");
    net_client_started = true;

    return 0;
}

int net_client_submit(const struct ranging_frame *frame)
{
    if (!net_client_started) {
        return -EAGAIN;
    }

    int ret = k_msgq_put(&ranging_msgq, frame, K_NO_WAIT);
    if (ret == 0) {
        return 0;
    }

    /* Keep the stream live by dropping stale frames and enqueueing newest one. */
    k_msgq_purge(&ranging_msgq);
    return k_msgq_put(&ranging_msgq, frame, K_NO_WAIT);
}
