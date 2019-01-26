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
#ifndef __APP_AUDIO_H__
#define __APP_AUDIO_H__

#include "app_bt_stream.h"
#include "heap_api.h"
//should solve this problem
//#include "./../../utils/cqueue/cqueue.h"
//#include "cqueue.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __TWS_USE_PLL_SYNC__
#define A2DP_AUDIO_SYNC_WITH_LOCAL (1)
#endif

#ifdef A2DP_AUDIO_SYNC_WITH_LOCAL
#ifdef __TWS_USE_PLL_SYNC__
#define A2DP_AUDIO_SYNC_WITH_LOCAL_STEP (1)
#else
#define A2DP_AUDIO_SYNC_WITH_LOCAL_STEP (2)
#endif

//we care play too fast as buffer is not enougth ,should play slowly more aggressive than dec adust 
#define A2DP_AUDIO_SYNC_WITH_LOCAL_SAMPLERATE_STEP    (0.002f)
//if buffer enougth, just play a little fast than common
#define A2DP_AUDIO_SYNC_WITH_LOCAL_SAMPLERATE_DEC_STEP    (0.0005f)

#define A2DPPLAY_SYNC_STATUS_SET (0x01)
#define A2DPPLAY_SYNC_STATUS_RESET (0x02)
#define A2DPPLAY_SYNC_STATUS_PROC (0x03)

#define A2DPPLAY_SYNC_ADJ_INC (0x01)
#define A2DPPLAY_SYNC_ADJ_IDLE (0x02)
#define A2DPPLAY_SYNC_ADJ_DEC (0x03)
//for tws shift feild can't exceed 0x07, because ,only bit2:bit0 for this value
#define A2DPPLAY_SYNC_TWS_MAX_SHIFT (0x07)
//limited play fast max shift avoid play too fast.
#define A2DPPLAY_SYNC_TWS_DEC_MAX_SHIFT (0x01)
//should keep dma trigger timer > 300ms can cal with audio dma irq 
//for send sbc timeout is 300ms ,so the delay number * dma irq > 300ms
//A2DPPLAY_SYNC_DELAY_NUMBER should not less than TWS_PLL_SYNC_THRESHOLD + 6(controller tx buffer size)
#define A2DPPLAY_SYNC_DELAY_NUMBER (15)
//should large than A2DPPLAY_SYNC_DELAY_NUMBER.  
//this para set adjust mineral interval between two action event.
//less value means action more actively
#define A2DPPLAY_SYNC_MONITOR_IRQ_CIRCLE_NUM (1)
#define A2DPPLAY_SYNC_MONITOR_ADJ_IRQ_NUM (10)
#define A2DPPLAY_SYNC_ADJ_MINI_INTERVAL_NUMBER ((A2DPPLAY_SYNC_MONITOR_IRQ_CIRCLE_NUM * A2DPPLAY_SYNC_MONITOR_ADJ_IRQ_NUM )  \
                                                                                                >A2DPPLAY_SYNC_DELAY_NUMBER?(A2DPPLAY_SYNC_MONITOR_IRQ_CIRCLE_NUM * A2DPPLAY_SYNC_MONITOR_ADJ_IRQ_NUM):A2DPPLAY_SYNC_DELAY_NUMBER + 2) 

#define A2DPPLAY_SYNC_SINGLE_MONITOR_IRQ_CIRCLE_NUM (1)
#define A2DPPLAY_SYNC_SINGLE_MONITOR_ADJ_IRQ_NUM (10)
#define A2DPPLAY_SYNC_SINGLE_ADJ_MINI_INTERVAL_NUMBER (A2DPPLAY_SYNC_SINGLE_MONITOR_IRQ_CIRCLE_NUM * A2DPPLAY_SYNC_SINGLE_MONITOR_ADJ_IRQ_NUM + 2)

//master slave will check send threshold to keep the two sides will operation at the same time
#define TWS_PLL_SYNC_THRESHOLD (6)   
#endif

#define MAX_AUDIO_BUF_LIST 5
enum APP_AUDIO_CACHE_T {
    APP_AUDIO_CACHE_CACHEING= 0,
    APP_AUDIO_CACHE_OK,
    APP_AUDIO_CACHE_QTY,
};


typedef void (*APP_AUDIO_CALLBACK_T)(uint32_t status, uint32_t param);


#define APP_AUDIO_SET_MESSAGE(appevt, id, status) (appevt = (((uint32_t)id&0xffff)<<16)|(status&0xffff))
#define APP_AUDIO_GET_ID(appevt, id) (id = (appevt>>16)&0xffff)
#define APP_AUDIO_GET_STATUS(appevt, status) (status = appevt&0xffff)
#define APP_AUDIO_GET_AUD_ID(appevt, aud_id) (aud_id = appevt)
#define APP_AUDIO_GET_FREQ(appevt, freq) (freq = appevt)
#define APP_AUDIO_GET_AUD_PARAM(appevt, param) (param = appevt)
#define APP_AUDIO_GET_AUD_CALLBACK(appevt, callback) (callback = appevt)


#define APP_AUDIO_InitCQueue(a, b, c)   InitCQueue(a, b, c)
#define APP_AUDIO_AvailableOfCQueue(a)     AvailableOfCQueue(a)
#define APP_AUDIO_LengthOfCQueue(a)     LengthOfCQueue(a)
#define APP_AUDIO_PeekCQueue(a, b, c, d, e, f)   PeekCQueue(a, b, c, d, e, f)
#define APP_AUDIO_EnCQueue(a, b, c)     EnCQueue(a, b, c)
#define APP_AUDIO_DeCQueue(a, b, c)     DeCQueue(a, b, c)

#define app_audio_mempool_init  syspool_init

#define app_audio_mempool_noclr_init syspool_init

#define app_audio_mempool_use_mempoolsection_init syspool_init

#define app_audio_mempool_free_buff_size syspool_free_size

#define app_audio_mempool_get_buff syspool_get_buff

void UNLOCK_APP_AUDIO_QUEUE();

void LOCK_APP_AUDIO_QUEUE();

uint32_t app_audio_lr_balance(uint8_t *buf, uint32_t len, int8_t balance);

int app_audio_pcmbuff_init(uint8_t *buff, uint16_t len);

int app_audio_pcmbuff_length(void);

int app_audio_pcmbuff_put(uint8_t *buff, uint16_t len);

int app_audio_pcmbuff_get(uint8_t *buff, uint16_t len);

int app_audio_pcmbuff_discard(uint16_t len);

void app_audio_memcpy_16bit(int16_t *des, int16_t *src, int len);

void app_audio_memset_16bit(int16_t *des, int16_t val, int len);

int app_audio_sendrequest(uint16_t id, uint8_t status, uint32_t param0, uint32_t ptr);

int app_audio_sendrequest_param(uint8_t id, uint8_t status, uint32_t ptr, uint32_t param);

void app_audio_open(void);

void app_audio_close(void);

bool app_audio_list_append(APP_AUDIO_STATUS* aud_status);

bool app_audio_list_rmv_callback(APP_AUDIO_STATUS* status, APP_AUDIO_STATUS* list_status);

void app_audio_list_clear();

void app_audio_switch_flash_flush_req(void);

int app_capture_audio_mempool_init(void);

uint32_t app_capture_audio_mempool_free_buff_size();

int app_capture_audio_mempool_get_buff(uint8_t **buff, uint32_t size);
#ifdef __cplusplus
}
#endif

#endif//__FMDEC_H__
