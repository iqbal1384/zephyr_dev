#ifndef SERVER_CONFIG_H_
#define SERVER_CONFIG_H_

/* Update these for your PC server. */
#define SERVER_IP_ADDR            "192.168.1.100"
#define SERVER_PORT               5000

/* Network bring-up mode:
 * 0 = try DHCP first, then static fallback after timeout
 * 1 = force static immediately (skip DHCP)
 */
#define DEVICE_FORCE_STATIC        1
#define DEVICE_DHCP_TIMEOUT_MS     20000

/*
 * If DHCP does not provide an address, enable static fallback below.
 * Keep DEVICE_IP_ADDR in the same subnet as SERVER_IP_ADDR.
 */
#define DEVICE_USE_STATIC_FALLBACK 1
#define DEVICE_IP_ADDR             "192.168.1.50"
#define DEVICE_NETMASK             "255.255.255.0"
#define DEVICE_GW_ADDR             "192.168.1.1"

/* Retry delay while server is not reachable. */
#define SERVER_RECONNECT_DELAY_MS 2000

#endif /* SERVER_CONFIG_H_ */
