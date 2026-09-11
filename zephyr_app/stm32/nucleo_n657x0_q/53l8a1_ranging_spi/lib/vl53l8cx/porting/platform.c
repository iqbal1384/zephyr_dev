/*
 * VL53L8CX platform abstraction layer — Zephyr SPI port
 *
 * ============================================================================
 * SPI REGISTER-ACCESS PROTOCOL (tested on hardware)
 * ============================================================================
 * The VL53L8CA/CX SPI framing uses a 16-bit register address with bit 15
 * as the direction flag:
 *
 *   Read  transaction:  addr | 0x8000  (bit 15 = 1), then receive N bytes
 *   Write transaction:  addr & 0x7FFF  (bit 15 = 0), then transmit N bytes
 *
 * This is the same R/W-in-MSB convention used by many other ST optical
 * ranging sensors (VL53L0X, VL53L3CX, etc.).  Without the 0x8000 bit on
 * reads, the sensor interprets every read command as a write, keeps MISO
 * idle-low, and all reads return 0x00.
 *
 * Other confirmed parameters:
 *   - SPI Mode 0 (CPOL=0, CPHA=0), MSB first, 8-bit words.
 *   - NCS held low across the entire address+payload transfer.
 *   - No dummy/turnaround bytes between address phase and data phase.
 * ============================================================================
 */

#include "platform.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/spi.h>
#include <string.h>

/* 16-bit register address, big-endian. Bit 15 = direction: 1=read, 0=write. */
#define REG_ADDR_LEN  2U
#define RD_FLAG       0x8000U

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
    /* Set bit 15 to signal a read to the sensor. */
    uint16_t rd_addr = RegisterAdress | RD_FLAG;
    uint8_t addr_buf[REG_ADDR_LEN];
    addr_buf[0] = (uint8_t)(rd_addr >> 8);
    addr_buf[1] = (uint8_t)(rd_addr & 0xFF);

    /* Phase 1: clock out the 16-bit address with R flag (discard echoed bytes).
     * Phase 2: clock out don't-care bytes while clocking in `size` bytes.
     * One spi_transceive_dt() call == one NCS-low window covering both. */
    const struct spi_buf tx_bufs[2] = {
        { .buf = addr_buf, .len = REG_ADDR_LEN },
        { .buf = NULL,     .len = size },
    };
    const struct spi_buf rx_bufs[2] = {
        { .buf = NULL,     .len = REG_ADDR_LEN },
        { .buf = p_values, .len = size },
    };
    const struct spi_buf_set tx = { .buffers = tx_bufs, .count = 2U };
    const struct spi_buf_set rx = { .buffers = rx_bufs, .count = 2U };

    int ret = spi_transceive_dt(p_platform->spi, &tx, &rx);
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
    uint8_t addr_buf[REG_ADDR_LEN];
    addr_buf[0] = (uint8_t)(RegisterAdress >> 8);
    addr_buf[1] = (uint8_t)(RegisterAdress & 0xFF);

    /* Register address followed directly by the payload, NCS held low across
     * both — scatter/gather avoids the k_malloc() the I2C port needed to
     * prepend the address to the payload in one contiguous buffer. */
    const struct spi_buf tx_bufs[2] = {
        { .buf = addr_buf,  .len = REG_ADDR_LEN },
        { .buf = p_values,  .len = size },
    };
    const struct spi_buf_set tx = { .buffers = tx_bufs, .count = 2U };

    int ret = spi_transceive_dt(p_platform->spi, &tx, NULL);
    if (ret != 0) {
        printk("WrMulti FAIL reg=0x%04x size=%u ret=%d\n",
               RegisterAdress, size, ret);
    }
    return (ret == 0) ? 0U : 1U;
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
