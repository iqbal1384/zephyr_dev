/*
 * VL53L8CX platform header — Zephyr port
 *
 * Replaces the original function-pointer based platform struct with
 * a Zephyr i2c_dt_spec-based struct. The sensor API (vl53l8cx_api.c)
 * only includes this header via VL53L8CX_Platform, so the swap is transparent.
 */

#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#include <stdint.h>
#include <string.h>
#include <zephyr/drivers/i2c.h>

typedef struct {
    const struct device *i2c_dev;  /* Zephyr I2C device pointer */
    uint16_t             address;  /* 7-bit I2C address (e.g. 0x29) — named 'address' to match vl53l8cx_api.c */
} VL53L8CX_Platform;

#ifndef VL53L8CX_NB_TARGET_PER_ZONE
#define VL53L8CX_NB_TARGET_PER_ZONE  (1U)
#endif

/* #define VL53L8CX_USE_RAW_FORMAT */
/* #define VL53L8CX_DISABLE_AMBIENT_PER_SPAD      */
/* #define VL53L8CX_DISABLE_NB_SPADS_ENABLED       */
/* #define VL53L8CX_DISABLE_NB_TARGET_DETECTED     */
/* #define VL53L8CX_DISABLE_SIGNAL_PER_SPAD        */
/* #define VL53L8CX_DISABLE_RANGE_SIGMA_MM         */
/* #define VL53L8CX_DISABLE_DISTANCE_MM            */
/* #define VL53L8CX_DISABLE_REFLECTANCE_PERCENT    */
/* #define VL53L8CX_DISABLE_TARGET_STATUS          */
/* #define VL53L8CX_DISABLE_MOTION_INDICATOR       */

uint8_t VL53L8CX_RdByte(VL53L8CX_Platform *p_platform,
                         uint16_t RegisterAdress, uint8_t *p_value);
uint8_t VL53L8CX_WrByte(VL53L8CX_Platform *p_platform,
                         uint16_t RegisterAdress, uint8_t value);
uint8_t VL53L8CX_RdMulti(VL53L8CX_Platform *p_platform,
                          uint16_t RegisterAdress, uint8_t *p_values,
                          uint32_t size);
uint8_t VL53L8CX_WrMulti(VL53L8CX_Platform *p_platform,
                          uint16_t RegisterAdress, uint8_t *p_values,
                          uint32_t size);
void    VL53L8CX_SwapBuffer(uint8_t *buffer, uint16_t size);
uint8_t VL53L8CX_WaitMs(VL53L8CX_Platform *p_platform, uint32_t TimeMs);

#endif /* _PLATFORM_H_ */
