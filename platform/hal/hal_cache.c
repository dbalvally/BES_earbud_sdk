/***************************************************************************
 *
 * Copyright 2015-2019 BES.
 * All rights reserved. All unpublished rights reserved.
 *
 * No part of this work may be used or reproduced in any form or by any
 * means, or stored in a database or retrieval system, without prior written
 * permission of BES.
 *
 * Use of this work is governed by a license granted by BES.
 * This work contains confidential and proprietary information of
 * BES. which is protected by copyright, trade secret,
 * trademark and other intellectual property rights.
 *
 ****************************************************************************/
#include "plat_types.h"
#include "plat_addr_map.h"
#include "hal_cache.h"
#include "hal_location.h"
#if defined(CHIP_BEST2300) || defined(CHIP_BEST1400)
#include "hal_norflash.h"
#include "hal_timer.h"
#endif

/* cache controller */

#if 0
#elif defined(CHIP_BEST1000)
#define CACHE_SIZE                          0x1000
#define CACHE_LINE_SIZE                     0x10
#elif defined(CHIP_BEST2000) || defined(CHIP_BEST3001)
#define CACHE_SIZE                          0x2000
#define CACHE_LINE_SIZE                     0x10
#else
#define CACHE_SIZE                          0x2000
#define CACHE_LINE_SIZE                     0x20
#endif

#define CACHE_SET_ASSOCIATIVITY             4

/* reg value */
#define CACHE_ENABLE_REG_OFFSET             0x00
#define CACHE_INI_CMD_REG_OFFSET            0x04
#define WRITEBUFFER_ENABLE_REG_OFFSET       0x08
#define WRITEBUFFER_FLUSH_REG_OFFSET        0x0C
#define LOCK_UNCACHEABLE_REG_OFFSET         0x10
#define INVALIDATE_ADDRESS_REG_OFFSET       0x14
#define INVALIDATE_SET_CMD_REG_OFFSET       0x18
// Since best2300
#define MONITOR_ENABLE_REG_OFFSET           0x1C
#define MONITOR_CNT_READ_HIT0_REG_OFFSET    0x20
#define MONITOR_CNT_READ_HIT1_REG_OFFSET    0x24
#define MONITOR_CNT_READ_MISS0_REG_OFFSET   0x28
#define MONITOR_CNT_READ_MISS1_REG_OFFSET   0x2C

#define CACHE_EN                            (1 << 0)
// Since best2300
#define WRAP_EN                             (1 << 1)

#define WRITEBUFFER_EN                      (1 << 0)
// Since best2300
#define WRITE_BACK_EN                       (1 << 1)

#define LOCK_UNCACHEABLE                    (1 << 0)

// Since best2300
#define MONITOR_EN                          (1 << 0)

#define CNT_READ_HIT_31_0_SHIFT             (0)
#define CNT_READ_HIT_31_0_MASK              (0xFFFFFFFF << CNT_READ_HIT_31_0_SHIFT)
#define CNT_READ_HIT_31_0(n)                BITFIELD_VAL(CNT_READ_HIT_31_0, n)

#define CNT_READ_HIT_39_32_SHIFT            (0)
#define CNT_READ_HIT_39_32_MASK             (0xFF << CNT_READ_HIT_39_32_SHIFT)
#define CNT_READ_HIT_39_32(n)               BITFIELD_VAL(CNT_READ_HIT_39_32, n)

/* read write */
#define cacheip_write32(v,b,a) \
    (*((volatile uint32_t *)(b+a)) = v)
#define cacheip_read32(b,a) \
    (*((volatile uint32_t *)(b+a)))

static inline void cacheip_enable_cache(uint32_t reg_base, uint32_t v)
{
    uint32_t val;
    val = cacheip_read32(reg_base, CACHE_ENABLE_REG_OFFSET);
    if (v) {
        val |= CACHE_EN;
    } else {
        val &= ~CACHE_EN;
    }
    cacheip_write32(val, reg_base, CACHE_ENABLE_REG_OFFSET);
}
static inline void cacheip_enable_wrap(uint32_t reg_base, uint32_t v)
{
    uint32_t val;
    val = cacheip_read32(reg_base, CACHE_ENABLE_REG_OFFSET);
    if (v) {
        val |= WRAP_EN;
    } else {
        val &= ~WRAP_EN;
    }
    cacheip_write32(val, reg_base, CACHE_ENABLE_REG_OFFSET);
}
static inline void cacheip_init_cache(uint32_t reg_base)
{
    cacheip_write32(1, reg_base, CACHE_INI_CMD_REG_OFFSET);
}
static inline void cacheip_enable_writebuffer(uint32_t reg_base, uint32_t v)
{
    uint32_t val;
    if (v) {
        val = WRITEBUFFER_EN;
    } else {
        val = 0;
    }
    cacheip_write32(val, reg_base, WRITEBUFFER_ENABLE_REG_OFFSET);
}
static inline void cacheip_flush_writebuffer(uint32_t reg_base)
{
    cacheip_write32(1, reg_base, WRITEBUFFER_FLUSH_REG_OFFSET);
}
static inline void cacheip_set_invalidate_address(uint32_t reg_base, uint32_t v)
{
    cacheip_write32(v, reg_base, INVALIDATE_ADDRESS_REG_OFFSET);
}
static inline void cacheip_trigger_invalidate(uint32_t reg_base)
{
    cacheip_write32(1, reg_base, INVALIDATE_SET_CMD_REG_OFFSET);
}
/* cache controller end */

/* hal api */
static inline uint32_t _cache_get_reg_base(enum HAL_CACHE_ID_T id)
{
    uint32_t base;

    if (id == HAL_CACHE_ID_I_CACHE) {
        base = ICACHE_CTRL_BASE;
    } else if (id == HAL_CACHE_ID_D_CACHE) {
#ifdef DCACHE_CTRL_BASE
        base = DCACHE_CTRL_BASE;
#else
        base = ICACHE_CTRL_BASE;
#endif
    } else {
        base = 0;
    }

    return base;
}
uint8_t BOOT_TEXT_FLASH_LOC hal_cache_enable(enum HAL_CACHE_ID_T id, uint32_t val)
{
    uint32_t reg_base = 0;

#ifndef DCACHE_CTRL_BASE
    if (id == HAL_CACHE_ID_D_CACHE) {
        return 0;
    }
#endif

    reg_base = _cache_get_reg_base(id);

    if (reg_base == 0) {
        return 0;
    }

    if (val) {
        cacheip_init_cache(reg_base);
    }

    cacheip_enable_cache(reg_base, val);
    return 0;
}
uint8_t BOOT_TEXT_FLASH_LOC hal_cache_writebuffer_enable(enum HAL_CACHE_ID_T id, uint32_t val)
{
    uint32_t reg_base = 0;
    reg_base = _cache_get_reg_base(id);

    if (reg_base == 0) {
        return 0;
    }

    cacheip_enable_writebuffer(reg_base, val);
    return 0;
}
uint8_t hal_cache_writebuffer_flush(enum HAL_CACHE_ID_T id)
{
    uint32_t reg_base = 0;
    reg_base = _cache_get_reg_base(id);

    if (reg_base == 0) {
        return 0;
    }

    cacheip_flush_writebuffer(reg_base);
    return 0;
}
// Wrap is enabled during flash init
uint8_t BOOT_TEXT_SRAM_LOC hal_cache_wrap_enable(enum HAL_CACHE_ID_T id, uint32_t val)
{
    uint32_t reg_base = 0;

#ifndef DCACHE_CTRL_BASE
    if (id == HAL_CACHE_ID_D_CACHE) {
        return 0;
    }
#endif

    reg_base = _cache_get_reg_base(id);

    if (reg_base == 0) {
        return 0;
    }

    cacheip_enable_wrap(reg_base, val);
    return 0;
}
// Flash timing calibration might need to invalidate cache
uint8_t BOOT_TEXT_SRAM_LOC hal_cache_invalidate(enum HAL_CACHE_ID_T id, uint32_t start_address, uint32_t len)
{
    uint32_t reg_base;
    uint32_t end_address;

    reg_base = _cache_get_reg_base(id);

    if (reg_base == 0) {
        return 0;
    }

#if defined(CHIP_BEST2300) || defined(CHIP_BEST1400)
    uint32_t time;

    time = hal_sys_timer_get();
    while (hal_norflash_busy() && (hal_sys_timer_get() - time) < MS_TO_TICKS(2));
    // Delay for at least 8 cycles till the cache becomes idle
    for (int delay = 0; delay < 8; delay++) {
        asm volatile("nop");
    }
#endif

    if (len >= CACHE_SIZE / 2) {
        cacheip_init_cache(reg_base);
        return 0;
    }

    end_address = start_address + len;
    while (start_address < end_address) {
        cacheip_set_invalidate_address(reg_base, start_address);
        cacheip_trigger_invalidate(reg_base);
        start_address += CACHE_LINE_SIZE;
    }

    return 0;
}

