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
#ifndef __HAL_TRACE_H__
#define __HAL_TRACE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "plat_types.h"

// #define AUDIO_DEBUG
// #define AUDIO_DEBUG_V0_1_0
// #define HAL_TRACE_RX_ENABLE

enum HAL_TRACE_MODUAL {
    HAL_TRACE_LEVEL_0         = 1<<0,
    HAL_TRACE_LEVEL_ENV       = 1<<1,
    HAL_TRACE_LEVEL_INTERSYS  = 1<<2,
    HAL_TRACE_LEVEL_BTSTACK   = 1<<3,
    HAL_TRACE_LEVEL_TWS       = 1<<4,
    HAL_TRACE_LEVEL_XTALSYNC  = 1<<5,
    HAL_TRACE_LEVEL_ROLESWITCH= 1<<6,
    HAL_TRACE_LEVEL_APPS      = 1<<7,
    HAL_TRACE_LEVEL_TO_DUMP   = 1<<8,
    HAL_TRACE_LEVEL_A2DP      = 1<<9,
    HAL_TRACE_LEVEL_HAL       = 1<<10,
    HAL_TRACE_LEVEL_ANC       = 1<<11,
    HAL_TRACE_LEVEL_BLE       = 1<<12,
    HAL_TRACE_LEVEL_FP        = 1<<13,
    HAL_TRACE_LEVEL_HFP       = 1<<14,
    HAL_TRACE_LEVEL_AVRCP     = 1<<15,
    HAL_TRACE_LEVEL_SPP       = 1<<16,
    HAL_TRACE_LEVEL_VOICE_CAP_AUD      = 1<<17,
    HAL_TRACE_LEVEL_VOICE_CALL_AUD     = 1<<18,
    HAL_TRACE_LEVEL_NVREC              = 1<<19,
    HAL_TRACE_LEVEL_BT_CONN_MGR        = 1<<20,
    HAL_TRACE_LEVEL_BT_LINK_MGR        = 1<<21,
    HAL_TRACE_LEVEL_BT_DRIVER          = 1<<22,
    HAL_TRACE_LEVEL_MEDIA_MANAGER      = 1<<23,
    HAL_TRACE_LEVEL_BT_STREAM          = 1<<24,
    HAL_TRACE_LEVEL_DMA_RESYNC        = 1<<25,
    HAL_TRACE_LEVEL_26        = 1<<26,
    HAL_TRACE_LEVEL_27        = 1<<27,
    HAL_TRACE_LEVEL_28        = 1<<28,
    HAL_TRACE_LEVEL_29        = 1<<29,
    HAL_TRACE_LEVEL_30        = 1<<30,
    HAL_TRACE_LEVEL_31        = 1<<31,
    HAL_TRACE_LEVEL_ALL       = 0XFFFFFFFF,
};

typedef enum {
    LOG_LV_NONE,
    LOG_LV_FATAL,
    LOG_LV_ERROR,
    LOG_LV_WARN,
    LOG_LV_INFO,
    LOG_LV_DEBUG,
}LOG_LEVEL_T;

typedef enum {
    LOG_MOD_NONE,
    LOG_MOD_OS,
    LOG_MOD_BT_STACK,
    LOG_MOD_SYS,
    LOG_MOD_MEDIA,
    LOG_MOD_APP,
}LOG_MODULE_T;

#if defined(_AUTO_TEST_)
#define HAL_TRACE_RX_ENABLE
extern int auto_test_send(char *resp);
extern void auto_test_init(void);
#define AUTO_TEST_SEND(str)         do{auto_test_send((char*)str);}while(0)

#endif

#define TRACE_HIGHPRIO(str, ...)    	hal_trace_printf_imm(HAL_TRACE_LEVEL_TO_DUMP, str, ##__VA_ARGS__)
#define TRACE_HIGHPRIO_IMM(str, ...)    hal_trace_printf_imm(HAL_TRACE_LEVEL_TO_DUMP, str, ##__VA_ARGS__)
#define TRACE_HIGHPRIO_NOCRLF(str, ...) hal_trace_printf_without_crlf_imm(HAL_TRACE_LEVEL_TO_DUMP, str, ##__VA_ARGS__)

#if defined(DEBUG) && (!defined(AUDIO_DEBUG) || defined(AUDIO_DEBUG_V0_1_0))
#define TRACE_LVL(lvl, str, ...)    hal_trace_printf(lvl, str, ##__VA_ARGS__)
#define TRACE(str, ...)             hal_trace_printf(HAL_TRACE_LEVEL_0, str, ##__VA_ARGS__)
#define TRACE_NOCRLF(str, ...)      hal_trace_printf_without_crlf(HAL_TRACE_LEVEL_0, str, ##__VA_ARGS__)
#define TRACE_IMM(str, ...)         hal_trace_printf_imm(HAL_TRACE_LEVEL_0, str, ##__VA_ARGS__)
#define TRACE_OUTPUT(str, len)      hal_trace_output(str, len)
#define FUNC_ENTRY_TRACE()          hal_trace_printf(HAL_TRACE_LEVEL_0, __FUNCTION__)
#define DUMP8(str, buf, cnt)        hal_trace_dump(HAL_TRACE_LEVEL_0, str, sizeof(uint8_t), cnt, buf)
#define DUMP16(str, buf, cnt)       hal_trace_dump(HAL_TRACE_LEVEL_0, str, sizeof(uint16_t), cnt, buf)
#define DUMP32(str, buf, cnt)       hal_trace_dump(HAL_TRACE_LEVEL_0, str, sizeof(uint32_t), cnt, buf)
#define DUMP8_LVL(lvl, str, buf, cnt)        hal_trace_dump((lvl), (str), sizeof(uint8_t), cnt, buf)
#define DUMP16_LVL(lvl, str, buf, cnt)       hal_trace_dump((lvl), (str), sizeof(uint16_t), cnt, buf)
#define DUMP32_LVL(lvl, str, buf, cnt)       hal_trace_dump((lvl), (str), sizeof(uint32_t), cnt, buf)
#define TRACE_DUMP(lvl, str, ...)   hal_trace_printf(HAL_TRACE_LEVEL_TO_DUMP, str, ##__VA_ARGS__)
#else
// To avoid warnings on unused variables
#define TRACE_LVL(lvl, str, ...)    hal_trace_dummy(str, ##__VA_ARGS__)
#define TRACE(str, ...)             hal_trace_dummy(str, ##__VA_ARGS__)
#define TRACE_NOCRLF(str, ...)      hal_trace_dummy(str, ##__VA_ARGS__)
#define TRACE_IMM(str, ...)         hal_trace_dummy(str, ##__VA_ARGS__)
#define TRACE_OUTPUT(str, len)      hal_trace_dummy(str, len)
#define FUNC_ENTRY_TRACE()          hal_trace_dummy(NULL)
#define DUMP8(str, buf, cnt)        hal_trace_dummy(str, buf, cnt)
#define DUMP16(str, buf, cnt)       hal_trace_dummy(str, buf, cnt)
#define DUMP32(str, buf, cnt)       hal_trace_dummy(str, buf, cnt)
#define DUMP8_LVL(lvl, str, buf, cnt)        hal_trace_dummy((const char *)(lvl), (str), (buf), (cnt))
#define DUMP16_LVL(lvl, str, buf, cnt)       hal_trace_dummy((const char *)(lvl), (str), (buf), (cnt))
#define DUMP32_LVL(lvl, str, buf, cnt)       hal_trace_dummy((const char *)(lvl), (str), (buf), (cnt))
#define TRACE_DUMP(lvl, str, ...)   hal_trace_printf(HAL_TRACE_LEVEL_TO_DUMP, str, ##__VA_ARGS__)
#endif

#if defined(DEBUG) && defined(ASSERT_SHOW_FILE_FUNC)
#define ASSERT(cond, str, ...)      { if (!(cond)) { hal_trace_assert_dump(__FILE__, __FUNCTION__, __LINE__, str, ##__VA_ARGS__); } }
#elif defined(DEBUG) && defined(ASSERT_SHOW_FILE)
#define ASSERT(cond, str, ...)      { if (!(cond)) { hal_trace_assert_dump(__FILE__, __LINE__, str, ##__VA_ARGS__); } }
#elif defined(DEBUG) && defined(ASSERT_SHOW_FUNC)
#define ASSERT(cond, str, ...)      { if (!(cond)) { hal_trace_assert_dump(__FUNCTION__, __LINE__, str, ##__VA_ARGS__); } }
#else
#define ASSERT(cond, str, ...)      { if (!(cond)) { hal_trace_assert_dump(str, ##__VA_ARGS__); } }
#endif

#define ASSERT_NODUMP(cond)         { if (!(cond)) { SAFE_PROGRAM_STOP(); } }

#ifdef CHIP_BEST1000
// Avoid CPU instruction fetch blocking the system bus on BEST1000
#define SAFE_PROGRAM_STOP()         do { asm volatile("nop \n nop \n nop \n nop"); } while (1)
#else
#define SAFE_PROGRAM_STOP()         do { } while (1)
#endif
#define ASSERT_SIMPLIFIED(cond)		{ if (!(cond)) { ASSERT(false, "Encounter an assert at line %d func %s!!!", __LINE__, __FUNCTION__);  } }

#define LOG_PRINT_ENV(s,...) 	TRACE_LVL(HAL_TRACE_LEVEL_ENV, s, ##__VA_ARGS__)
#define LOG_PRINT_ENV_DUMP8(str, buf, cnt) 	DUMP8_LVL(HAL_TRACE_LEVEL_ENV, str, buf, cnt)

#define LOG_PRINT_A2DP(s,...) 	TRACE_LVL(HAL_TRACE_LEVEL_A2DP, s, ##__VA_ARGS__)
#define LOG_PRINT_A2DP_DUMP8(str, buf, cnt) DUMP8_LVL(HAL_TRACE_LEVEL_A2DP, str, buf, cnt)
#define LOG_PRINT_HAL(s,...) 	TRACE_LVL(HAL_TRACE_LEVEL_HAL, s, ##__VA_ARGS__)
#define LOG_PRINT_ANC(s,...) 	TRACE_LVL(HAL_TRACE_LEVEL_ANC, s, ##__VA_ARGS__)

#define LOG_PRINT_BLE(s,...) 	TRACE_LVL(HAL_TRACE_LEVEL_BLE, s, ##__VA_ARGS__)
#define LOG_PRINT_BLE_DUMP8(str, buf, cnt) 	DUMP8_LVL(HAL_TRACE_LEVEL_BLE, str, buf, cnt)

#define LOG_PRINT_FP(s,...) 	TRACE_LVL(HAL_TRACE_LEVEL_FP, s, ##__VA_ARGS__)
#define LOG_PRINT_FP_DUMP8(str, buf, cnt) 	DUMP8_LVL(HAL_TRACE_LEVEL_FP, str, buf, cnt)

#define LOG_PRINT_HFP(s,...) 	TRACE_LVL(HAL_TRACE_LEVEL_HFP, s, ##__VA_ARGS__)
#define LOG_PRINT_AVRCP(s,...) 	TRACE_LVL(HAL_TRACE_LEVEL_AVRCP, s, ##__VA_ARGS__)
#define LOG_PRINT_AVRCP_DUMP8(str, buf, cnt) DUMP8_LVL(HAL_TRACE_LEVEL_AVRCP, str, buf, cnt)

#define LOG_PRINT_SPP(s,...) 	TRACE_LVL(HAL_TRACE_LEVEL_SPP, s, ##__VA_ARGS__)
#define LOG_PRINT_SPP_DUMP8(str, buf, cnt) DUMP8_LVL(HAL_TRACE_LEVEL_SPP, str, buf, cnt)

#define LOG_PRINT_VOICE_CAP_AUD(s,...) 	TRACE_LVL(HAL_TRACE_LEVEL_VOICE_CAP_AUD, s, ##__VA_ARGS__)

#define LOG_PRINT_VOICE_CALL_AUD(s,...) 	TRACE_LVL(HAL_TRACE_LEVEL_VOICE_CALL_AUD, s, ##__VA_ARGS__)
#define LOG_PRINT_VOICE_CALL_AUD_DUMP8(str, buf, cnt) DUMP8_LVL(HAL_TRACE_LEVEL_VOICE_CALL_AUD, str, buf, cnt)

#define LOG_PRINT_TWS(s,...) TRACE_LVL(HAL_TRACE_LEVEL_TWS, s, ##__VA_ARGS__)
#define LOG_PRINT_TWS_DUMP8(str, buf, cnt) DUMP8_LVL(HAL_TRACE_LEVEL_TWS, str, buf, cnt)

#define LOG_PRINT_NVREC(s,...) TRACE_LVL(HAL_TRACE_LEVEL_NVREC, s, ##__VA_ARGS__)
#define LOG_PRINT_NVREC_DUMP8(str, buf, cnt) DUMP8_LVL(HAL_TRACE_LEVEL_NVREC, str, buf, cnt)

#define LOG_PRINT_BT_CONN_MGR(s,...) TRACE_LVL(HAL_TRACE_LEVEL_BT_CONN_MGR, s, ##__VA_ARGS__)
#define LOG_PRINT_BT_CONN_MGR_DUMP8(str, buf, cnt) DUMP8_LVL(HAL_TRACE_LEVEL_BT_CONN_MGR, str, buf, cnt)

#define LOG_PRINT_BT_LINK_MGR(s,...) TRACE_LVL(HAL_TRACE_LEVEL_BT_LINK_MGR, s, ##__VA_ARGS__)

#define LOG_PRINT_INTERSYS(s,...) TRACE_LVL(HAL_TRACE_LEVEL_INTERSYS, s, ##__VA_ARGS__)
#define LOG_PRINT_INTERSYS_DUMP8(str, buf, cnt) DUMP8_LVL(HAL_TRACE_LEVEL_INTERSYS, str, buf, cnt)

#define LOG_PRINT_BT_DRIVER(s,...) TRACE_LVL(HAL_TRACE_LEVEL_BT_DRIVER, s, ##__VA_ARGS__)
#define LOG_PRINT_BT_DRIVER_DUMP8(str, buf, cnt) DUMP8_LVL(HAL_TRACE_LEVEL_BT_DRIVER, str, buf, cnt)

#define LOG_PRINT_APPS(s,...) TRACE_LVL(HAL_TRACE_LEVEL_APPS, s, ##__VA_ARGS__)
#define LOG_PRINT_APPS_DUMP8(str, buf, cnt) DUMP8_LVL(HAL_TRACE_LEVEL_APPS, str, buf, cnt)

#define LOG_PRINT_MEDIA_MGR(s,...) TRACE_LVL(HAL_TRACE_LEVEL_MEDIA_MANAGER, s, ##__VA_ARGS__)
#define LOG_PRINT_MEDIA_MGR_DUMP8(str, buf, cnt) DUMP8_LVL(HAL_TRACE_LEVEL_MEDIA_MANAGER, str, buf, cnt)

#define LOG_PRINT_BT_STREAM(s,...) TRACE_LVL(HAL_TRACE_LEVEL_BT_STREAM, s, ##__VA_ARGS__)

#define LOG_PRINT_TWS_ROLE_SWITCH(str, ...)         TRACE_LVL(HAL_TRACE_LEVEL_ROLESWITCH, str, ##__VA_ARGS__)
#define LOG_PRINT_TWS_ROLE_SWITCH_DUMP8(str, buf, cnt)    DUMP8_LVL(HAL_TRACE_LEVEL_ROLESWITCH, str, buf, cnt)

#define LOG_PRINT_DMA_RESYNC(str, ...)         TRACE_LVL(HAL_TRACE_LEVEL_DMA_RESYNC, str, ##__VA_ARGS__)

enum HAL_TRACE_TRANSPORT_T {
#ifdef CHIP_HAS_USB
    HAL_TRACE_TRANSPORT_USB,
#endif
    HAL_TRACE_TRANSPORT_UART0,
#if (CHIP_HAS_UART > 1)
    HAL_TRACE_TRANSPORT_UART1,
#endif

    HAL_TRACE_TRANSPORT_QTY
};

typedef void (*HAL_TRACE_CRASH_DUMP_CB_T)(void);

int hal_trace_switch(enum HAL_TRACE_TRANSPORT_T transport);

int hal_trace_open(enum HAL_TRACE_TRANSPORT_T transport);

int hal_trace_close(void);

int hal_trace_output(const unsigned char *buf, unsigned int buf_len);

int hal_trace_printf(uint32_t lvl, const char *fmt, ...);

int hal_trace_printf_without_crlf(uint32_t lvl, const char *fmt, ...);

int hal_trace_printf_with_tag(uint32_t lvl, const char *tag, const char *fmt, ...);

void hal_trace_printf_imm(uint32_t lvl, const char *fmt, ...);

void hal_trace_printf_without_crlf_imm(uint32_t lvl, const char *fmt, ...);

int hal_trace_printf_without_crlf_fix_arg(uint32_t lvl, const char *fmt);

int hal_trace_dump(uint32_t lvl, const char *fmt, unsigned int size,  unsigned int count, const void *buffer);

int hal_trace_busy(void);

void hal_trace_idle_send(void);

int hal_trace_crash_dump_register(HAL_TRACE_CRASH_DUMP_CB_T cb);

int hal_trace_crash_printf(const char *fmt, ...);

int hal_trace_crash_printf_without_crlf(const char *fmt, ...);

void hal_trace_flash_flush(void);

int hal_trace_with_log_dump(const unsigned char *buf, unsigned int buf_len);

void hal_trace_print_backtrace(uint32_t addr, uint32_t search_cnt, uint32_t print_cnt);

static inline void hal_trace_dummy(const char *fmt, ...) { }

int hal_trace_tportopen(void);

int hal_trace_tportset(int port);

int hal_trace_tportclr(int port);

#if defined(DEBUG) && defined(ASSERT_SHOW_FILE_FUNC)
#define ASSERT_DUMP_ARGS    const char *file, const char *func, unsigned int line, const char *fmt, ...
#elif defined(DEBUG) && (defined(ASSERT_SHOW_FILE) || defined(ASSERT_SHOW_FUNC))
#define ASSERT_DUMP_ARGS    const char *scope, unsigned int line, const char *fmt, ...
#else
#define ASSERT_DUMP_ARGS    const char *fmt, ...
#endif
void NORETURN hal_trace_assert_dump(ASSERT_DUMP_ARGS);

int hal_trace_address_writable(uint32_t addr);

int hal_trace_address_executable(uint32_t addr);

int hal_trace_address_readable(uint32_t addr);


//==============================================================================
// AUDIO_DEBUG
//==============================================================================

#ifdef AUDIO_DEBUG
#define AUDIO_DEBUG_TRACE(str, ...)         hal_trace_printf(str, ##__VA_ARGS__)
#define AUDIO_DEBUG_DUMP(buf, cnt)          hal_trace_output(buf, cnt)
#endif


//==============================================================================
// TRACE RX
//==============================================================================

#ifdef HAL_TRACE_RX_ENABLE

#include "stdio.h"

#define hal_trace_rx_parser(buf, str, ...)  sscanf(buf, str, ##__VA_ARGS__)

typedef unsigned int (*HAL_TRACE_RX_CALLBACK_T)(unsigned char *buf, unsigned int  len);

int hal_trace_rx_register(char *name, HAL_TRACE_RX_CALLBACK_T callback);
int hal_trace_rx_deregister(char *name);

#endif

int hal_trace_pause(void);

int hal_trace_continue(void);

#ifdef __cplusplus
}
#endif

#endif
