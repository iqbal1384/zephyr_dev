/*
 * VL53L8CX platform abstraction layer — Zephyr port
 *
 * Replaces the STM32 HAL I2C calls with Zephyr I2C API.
 * The VL53L8CX_Platform struct is extended in platform.h to carry
 * a Zephyr i2c_dt_spec pointer instead of HAL function pointers.
 */

#include "platform.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <string.h>

/* The VL53L8CX uses 16-bit register addresses sent big-endian before payload. */
#define REG_ADDR_LEN 2U

/* Define VL53L8CX_I2C_CHUNK_SIZE to a value ≤255 to work around STM32 I2C
 * NBYTES/RELOAD issues for large transfers. Each chunk re-sends the register
 * address incremented by the bytes already written.
 * Leave undefined (default) to send the full transfer in one i2c_write call.
 * Try this only if CONFIG_I2C_STM32_INTERRUPT=y still fails.           */
/* #define VL53L8CX_I2C_CHUNK_SIZE 255U */

uint8_t VL53L8CX_RdByte(VL53L8CX_Platform *p_platform,
                         uint16_t RegisterAdress,
                         uint8_t *p_value)
{
    return VL53L8CX_RdMulti(p_platform, RegisterAdress, p_value, 1U);
}

uint8_t VL53L8CX_WrByte(VL53L8CX_Platform *p_platform,
                         uint16_t RegisterAdress,
                         uint8_t value)
{
    return VL53L8CX_WrMulti(p_platform, RegisterAdress, &value, 1U);
}

uint8_t VL53L8CX_RdMulti(VL53L8CX_Platform *p_platform,
                          uint16_t RegisterAdress,
                          uint8_t *p_values,
                          uint32_t size)
{
    uint8_t reg_buf[REG_ADDR_LEN];
    reg_buf[0] = (uint8_t)(RegisterAdress >> 8);
    reg_buf[1] = (uint8_t)(RegisterAdress & 0xFF);

    int ret = i2c_write_read(p_platform->i2c_dev,
                             p_platform->address,
                             reg_buf, REG_ADDR_LEN,
                             p_values, size);
    if (ret != 0) {
        printk("RdMulti FAIL reg=0x%04x size=%u ret=%d\n",
               RegisterAdress, size, ret);
    }
    return (ret == 0) ? 0U : 1U;
}

uint8_t VL53L8CX_WrMulti(VL53L8CX_Platform *p_platform,
                          uint16_t RegisterAdress,
                          uint8_t *p_values,
                          uint32_t size)
{
#ifdef VL53L8CX_I2C_CHUNK_SIZE
    /* Chunked path: break the write into segments of ≤VL53L8CX_I2C_CHUNK_SIZE
     * data bytes, each prefixed with an incremented register address.        */
    uint8_t txbuf[REG_ADDR_LEN + VL53L8CX_I2C_CHUNK_SIZE];
    uint32_t offset = 0;
    while (offset < size) {
        uint32_t chunk = size - offset;
        if (chunk > VL53L8CX_I2C_CHUNK_SIZE) {
            chunk = VL53L8CX_I2C_CHUNK_SIZE;
        }
        uint16_t reg = (uint16_t)(RegisterAdress + offset);
        txbuf[0] = (uint8_t)(reg >> 8);
        txbuf[1] = (uint8_t)(reg & 0xFF);
        memcpy(txbuf + REG_ADDR_LEN, p_values + offset, chunk);
        int ret = i2c_write(p_platform->i2c_dev, txbuf, REG_ADDR_LEN + chunk,
                            p_platform->address);
        if (ret != 0) {
            printk("WrMulti FAIL reg=0x%04x offset=%u chunk=%u ret=%d\n",
                   RegisterAdress, offset, chunk, ret);
            return 1U;
        }
        offset += chunk;
    }
    return 0U;
#else
    /* Default path: send register address + all data in one i2c_write call.
     * Requires CONFIG_I2C_STM32_INTERRUPT=y so the driver uses interrupt/DMA
     * and handles large transfers (>255 bytes) without NBYTES/RELOAD issues. */
    uint8_t *txbuf = k_malloc(REG_ADDR_LEN + size);
    if (!txbuf) {
        printk("WrMulti: k_malloc(%u) failed\n", REG_ADDR_LEN + size);
        return 1U;
    }
    txbuf[0] = (uint8_t)(RegisterAdress >> 8);
    txbuf[1] = (uint8_t)(RegisterAdress & 0xFF);
    memcpy(txbuf + REG_ADDR_LEN, p_values, size);
    int ret = i2c_write(p_platform->i2c_dev, txbuf, REG_ADDR_LEN + size,
                        p_platform->address);
    k_free(txbuf);
    if (ret != 0) {
        printk("WrMulti FAIL reg=0x%04x size=%u ret=%d\n",
               RegisterAdress, size, ret);
    }
    return (ret == 0) ? 0U : 1U;
#endif /* VL53L8CX_I2C_CHUNK_SIZE */
}

void VL53L8CX_SwapBuffer(uint8_t *buffer, uint16_t size)
{
    uint32_t tmp;
    for (uint16_t i = 0; i < size; i += 4) {
        tmp = ((uint32_t)buffer[i]     << 24)
            | ((uint32_t)buffer[i + 1] << 16)
            | ((uint32_t)buffer[i + 2] <<  8)
            | ((uint32_t)buffer[i + 3]);
        memcpy(&buffer[i], &tmp, 4);
    }
}

uint8_t VL53L8CX_WaitMs(VL53L8CX_Platform *p_platform, uint32_t TimeMs)
{
    ARG_UNUSED(p_platform);
    k_msleep(TimeMs);
    return 0U;
}
