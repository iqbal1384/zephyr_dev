#ifndef NET_CLIENT_H_
#define NET_CLIENT_H_

#include <zephyr/kernel.h>
#include <stdint.h>

#define RANGING_ZONE_COUNT 64

struct ranging_frame {
    int64_t timestamp_ms;
    int16_t distance_mm[RANGING_ZONE_COUNT];
    uint8_t target_detected[RANGING_ZONE_COUNT];
};

int net_client_init(void);
int net_client_start(void);
int net_client_submit(const struct ranging_frame *frame);

#endif /* NET_CLIENT_H_ */
