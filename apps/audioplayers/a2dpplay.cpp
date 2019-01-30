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
#ifdef MBED
#include "mbed.h"
#endif
// Standard C Included Files
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include "rtos.h"
#include "cmsis.h"
#include "tgt_hardware.h"
#ifdef MBED
#include "SDFileSystem.h"
#endif
#include "hal_uart.h"
#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_codec.h"
#include "audioflinger.h"
#include "cqueue.h"
#include "app_audio.h"
#include "analog.h"
#include "a2dp_api.h"
#if defined(A2DP_LHDC_ON)
#include "lhdcUtil_ext.h"
#endif


#if defined(__AUDIO_RESAMPLE__) && defined(SW_PLAYBACK_RESAMPLE)
#include "hal_sysfreq.h"
#include "hal_chipid.h"
#include "audio_resample_ex.h"
#endif

//#if defined(A2DP_AAC_ON)
#include "btapp.h"
//#endif

enum A2DPPLAY_STRTEAM_T {
    A2DPPLAY_STRTEAM_PUT= 0,
    A2DPPLAY_STRTEAM_GET,
    A2DPPLAY_STRTEAM_QTY,
};

/* sbc decoder */
static bool need_init_decoder = true;
static btif_sbc_decoder_t* sbc_decoder = NULL;

#if !defined( __TWS__)  && (FPGA==0)
#define A2DP_AUDIO_SYNC_WITH_LOCAL (1)
#endif

#define A2DP_AUDIO_SYNC_TRACE(s,...)
//LOG_PRINT_A2DP(s, ##__VA_ARGS__)

#ifndef __TWS_USE_PLL_SYNC__
#ifdef A2DP_AUDIO_SYNC_WITH_LOCAL
#define A2DP_AUDIO_SYNC_WITH_LOCAL_SAMPLERATE_DEFAULT (0)
#define A2DP_AUDIO_SYNC_WITH_LOCAL_SAMPLERATE_INC     (2)
#define A2DP_AUDIO_SYNC_WITH_LOCAL_SAMPLERATE_DEC     (-2)
#define A2DP_AUDIO_SYNC_WITH_LOCAL_SAMPLERATE_STEP    (0.00005f)
#define A2DPPLAY_SYNC_STATUS_SET (0x01)
#define A2DPPLAY_SYNC_STATUS_RESET (0x02)
#define A2DPPLAY_SYNC_STATUS_PROC (0x04)

enum A2DP_AUDIO_SYNC_STATUS {
    A2DP_AUDIO_SYNC_STATUS_SAMPLERATE_DEC,
    A2DP_AUDIO_SYNC_STATUS_SAMPLERATE_INC,
    A2DP_AUDIO_SYNC_STATUS_SAMPLERATE_DEFAULT,
};
#endif
#endif
#define A2DPPLAY_CACHE_OK_THRESHOLD (sbc_frame_size<<6)

static float sbc_eq_band_gain[CFG_HW_AUD_EQ_NUM_BANDS];

/* sbc queue */
#define SBC_TEMP_BUFFER_SIZE 128
#define SBC_QUEUE_SIZE_DEFAULT (SBC_TEMP_BUFFER_SIZE*64)
#define SBC_QUEUE_SIZE (1024*11)
static CQueue sbc_queue;

static uint16_t g_sbc_queue_size = SBC_QUEUE_SIZE_DEFAULT;
static int16_t sbc_frame_size = SBC_TEMP_BUFFER_SIZE;
static uint16_t sbc_frame_rev_len = 0;
static uint16_t sbc_frame_num = 0;

static enum APP_AUDIO_CACHE_T a2dp_cache_status = APP_AUDIO_CACHE_QTY;

#define A2DP_SYNC_WITH_GET_MUTUX_TIMEROUT_CNT (5)
#define A2DP_SYNC_WITH_GET_MUTUX_TIMEROUT_MS (20)
static osThreadId a2dp_put_thread_tid = NULL;
static bool a2dp_get_need_sync = false;

#if defined(__AUDIO_RESAMPLE__) && defined(SW_PLAYBACK_RESAMPLE)
extern enum AUD_SAMPRATE_T a2dp_sample_rate;
#ifdef CHIP_BEST1000
static bool allow_resample = false;
#else
static const bool allow_resample = true;
#endif
#endif

extern int a2dp_timestamp_parser_needsync(void);

#define A2DP_SYNC_WITH_GET_MUTUX_ALLOC() do{ \
                                                if (a2dp_put_thread_tid == NULL){ \
                                                    a2dp_put_thread_tid = osThreadGetId(); \
                                                } \
                                           }while(0)

#define A2DP_SYNC_WITH_GET_MUTUX_FREE() do{ \
                                                a2dp_put_thread_tid = NULL; \
                                          }while(0)

#define A2DP_SYNC_WITH_GET_MUTUX_WAIT() do{ \
                                                a2dp_get_need_sync = true; \
                                                if (a2dp_put_thread_tid){ \
                                                    osSignalClear(a2dp_put_thread_tid, 0x80); \
                                                    osSignalWait(0x80, A2DP_SYNC_WITH_GET_MUTUX_TIMEROUT_MS); \
                                                }\
                                          }while(0)

#define A2DP_SYNC_WITH_GET_MUTUX_SET() do{ \
                                            if (a2dp_get_need_sync){ \
                                                a2dp_get_need_sync = false; \
                                                if (a2dp_put_thread_tid){ \
                                                    osSignalSet(a2dp_put_thread_tid, 0x80); \
                                                } \
                                            } \
                                      }while(0)

//#define A2DP_SYNC_WITH_PUT_MUTUX (1)
#define A2DP_SYNC_WITH_PUT_MUTUX_TIMEROUT_CNT (1)
#define A2DP_SYNC_WITH_PUT_MUTUX_TIMEROUT_MS (3)
static osThreadId a2dp_get_thread_tid = NULL;
static bool a2dp_get_put_sync = false;

#define A2DP_SYNC_WITH_PUT_MUTUX_ALLOC() do{ \
                                                if (a2dp_get_thread_tid == NULL){ \
                                                    a2dp_get_thread_tid = osThreadGetId(); \
                                                } \
                                           }while(0)

#define A2DP_SYNC_WITH_PUT_MUTUX_FREE() do{ \
                                                a2dp_get_thread_tid = NULL; \
                                          }while(0)

#define A2DP_SYNC_WITH_PUT_MUTUX_WAIT() do{ \
                                                a2dp_get_put_sync = true; \
                                                if (a2dp_get_thread_tid){\
                                                    osSignalClear(a2dp_get_thread_tid, 0x80); \
                                                    osSignalWait(0x5, A2DP_SYNC_WITH_PUT_MUTUX_TIMEROUT_MS); \
                                                }\
                                          }while(0)

#define A2DP_SYNC_WITH_PUT_MUTUX_SET() do{ \
                                                if (a2dp_get_put_sync){ \
                                                    a2dp_get_put_sync = false; \
                                                    if (a2dp_get_thread_tid){\
                                                        osSignalSet(a2dp_get_thread_tid, 0x80); \
                                                    }\
                                                } \
                                          }while(0)


int a2dp_audio_sbc_set_frame_info(int rcv_len, int frame_num)
{
    if (sbc_frame_rev_len != rcv_len || sbc_frame_num != frame_num){
        sbc_frame_rev_len = rcv_len;
        sbc_frame_num = frame_num;
        sbc_frame_size = rcv_len/frame_num;
    }

    return 0;
}

#ifndef __TWS_USE_PLL_SYNC__
#ifdef A2DP_AUDIO_SYNC_WITH_LOCAL
static enum A2DP_AUDIO_SYNC_STATUS sync_status = A2DP_AUDIO_SYNC_STATUS_SAMPLERATE_DEFAULT;

static int a2dp_audio_sync_proc(uint8_t status, int shift)
{
#if !(defined(__AUDIO_RESAMPLE__) && defined(SW_PLAYBACK_RESAMPLE))
    struct AF_STREAM_CONFIG_T *cfg;

    bool need_shift = false;
    static int cur_shift = 0;
    static int dest_shift = 0;

    LOCK_APP_AUDIO_QUEUE();
    if (status & A2DPPLAY_SYNC_STATUS_RESET){
        sync_status = A2DP_AUDIO_SYNC_STATUS_SAMPLERATE_DEFAULT;
        cur_shift = 0;
        dest_shift = 0;
    }
    if (status & A2DPPLAY_SYNC_STATUS_SET){
        dest_shift = shift;
    }
    if (cur_shift > dest_shift){
        cur_shift--;
        need_shift = true;
    }
    if (cur_shift<dest_shift){
        cur_shift++;
        need_shift = true;
    }
    if (need_shift){
        A2DP_AUDIO_SYNC_TRACE("a2dp_audio_sync_proc shift:%d\n", cur_shift);
        af_stream_get_cfg(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK, &cfg, false);
        hal_codec_playback_sample_rate_trim(HAL_CODEC_ID_0, cfg->sample_rate, A2DP_AUDIO_SYNC_WITH_LOCAL_SAMPLERATE_STEP*cur_shift);
    }
    UNLOCK_APP_AUDIO_QUEUE();
#endif
    return 0;
}
#endif
#endif

static bool a2dp_audio_isrunning(enum A2DPPLAY_STRTEAM_T stream, bool run)
{
    static bool stream_running[A2DPPLAY_STRTEAM_QTY] = {false, false};

    if (stream >= A2DPPLAY_STRTEAM_QTY)
        return false;

    stream_running[stream] = run;

    if (stream_running[A2DPPLAY_STRTEAM_PUT] && stream_running[A2DPPLAY_STRTEAM_GET])
        return true;
    else
        return false;
}

static int store_sbc_buffer(unsigned char *buf, unsigned int len) __attribute__ ((__unused__));
static int store_sbc_buffer(unsigned char *buf, unsigned int len)
{
    int size;
    int cnt = 0;
    int nRet = 0;

    if (!a2dp_audio_isrunning(A2DPPLAY_STRTEAM_PUT, true)){
        LOG_PRINT_A2DP("%s not ready:%d cache_status:%d",__func__, len, a2dp_cache_status);
    }

    switch (a2dp_cache_status) {
        case APP_AUDIO_CACHE_CACHEING:
            LOCK_APP_AUDIO_QUEUE();
            nRet = APP_AUDIO_EnCQueue(&sbc_queue, buf, len);
            size = APP_AUDIO_LengthOfCQueue(&sbc_queue);
            UNLOCK_APP_AUDIO_QUEUE();
            if (size >= (g_sbc_queue_size-(sbc_frame_size*20))){
                LOG_PRINT_A2DP("cache ok:%d\n", size);
                a2dp_cache_status = APP_AUDIO_CACHE_OK;
            }else if (sbc_frame_size && (size>=A2DPPLAY_CACHE_OK_THRESHOLD)){
                LOG_PRINT_A2DP("cache ok:%d\n", size);
                a2dp_cache_status = APP_AUDIO_CACHE_OK;
            }else{
                LOG_PRINT_A2DP("cache add:%d\n", len);
            }
            if (CQ_ERR == nRet){
                LOG_PRINT_A2DP("cache add overflow\n");
                a2dp_cache_status = APP_AUDIO_CACHE_OK;
            }
            if (a2dp_cache_status == APP_AUDIO_CACHE_OK){
#ifdef __LOCK_AUDIO_THREAD__
                af_unlock_thread();
#endif
                A2DP_SYNC_WITH_GET_MUTUX_ALLOC();
                A2DP_SYNC_WITH_GET_MUTUX_WAIT();
#ifdef __LOCK_AUDIO_THREAD__
                af_lock_thread();
#endif
            }
            break;
        case APP_AUDIO_CACHE_OK:
            do{
                LOCK_APP_AUDIO_QUEUE();
                nRet = APP_AUDIO_EnCQueue(&sbc_queue, buf, len);
                UNLOCK_APP_AUDIO_QUEUE();
                if (CQ_OK == nRet){
                    nRet = 0;
                    break;
                }else{
#ifndef __TWS_USE_PLL_SYNC__                
#ifdef A2DP_AUDIO_SYNC_WITH_LOCAL
                    a2dp_audio_sync_proc(A2DPPLAY_SYNC_STATUS_SET, 0);
#endif
#endif
                    nRet = -1;
#ifdef __LOCK_AUDIO_THREAD__
                    af_unlock_thread();
#endif
                    A2DP_SYNC_WITH_GET_MUTUX_ALLOC();
                    A2DP_SYNC_WITH_GET_MUTUX_WAIT();
#ifdef __LOCK_AUDIO_THREAD__
                    af_lock_thread();
#endif
                }
            }while(cnt++<A2DP_SYNC_WITH_GET_MUTUX_TIMEROUT_CNT);
            break;
        case APP_AUDIO_CACHE_QTY:
        default:
            break;
    }
#ifdef A2DP_SYNC_WITH_PUT_MUTUX
    A2DP_SYNC_WITH_PUT_MUTUX_SET();
#endif

    if (nRet){
        LOG_PRINT_A2DP("cache overflow\n");
    }
    return 0;
}

#if 0
#if defined(A2DP_AAC_ON)
#include "aacdec.h"

#define AAC_READBUF_SIZE    (AAC_MAINBUF_SIZE)  /* pick something big enough to hold a bunch of frames */
unsigned char aac_input_mid_buf[AAC_READBUF_SIZE];
HAACDecoder *hAACDecoder = NULL;
AACFrameInfo aacFrameInfo;
int decode_aac_frame(unsigned char *pcm_buffer, unsigned int pcm_len)
{
    int r = 0;
    unsigned char *e1 = NULL, *e2 = NULL, *input_buf = NULL;
    unsigned int len1 = 0, len2 = 0;
    int bytesLeft, err, outOfData, eofReached;
#if 1
    if (hAACDecoder == NULL) {
        hAACDecoder = (HAACDecoder *)AACInitDecoder();
        AACSetLOASFormat(hAACDecoder);
    }

get_aac_again:
    LOCK_APP_AUDIO_QUEUE();
    len1 = len2 = 0;
    e1 = e2 = 0;
    r = PeekCQueue(&sbc_queue, AAC_READBUF_SIZE, &e1, &len1, &e2, &len2);
    UNLOCK_APP_AUDIO_QUEUE();
    if(r == CQ_ERR) {
        //osDelay(2);
        //goto get_aac_again;
        return 0;
    }
    else {
        // normal
        if (e1) {
            memcpy(aac_input_mid_buf, e1, len1);
        }
        if (e2) {
            memcpy(aac_input_mid_buf + len1, e2, len2);
        }
    }

    bytesLeft = len1 + len2;
    input_buf = (unsigned char *)&aac_input_mid_buf;
    err = AACDecode(hAACDecoder, &input_buf, &bytesLeft, (short *)pcm_buffer);

    if (err) {
        switch (err) {
            case ERR_AAC_INDATA_UNDERFLOW:
                if (eofReached || bytesLeft == AAC_READBUF_SIZE)
                    outOfData = 1;
                break;
            default:
                outOfData = 1;
                break;
        }
    }
    AACGetLastFrameInfo(hAACDecoder, &aacFrameInfo);

    //LOG_PRINT_A2DP("aacdec err:%d bytesLeft %d, outputsamples %d, pcm_len %d\n", err,bytesLeft, aacFrameInfo.outputSamps, pcm_len);

    if (bytesLeft != (len1 + len2)) {
        LOCK_APP_AUDIO_QUEUE();
        DeCQueue(&sbc_queue, 0, len1 + len2 - bytesLeft);
        UNLOCK_APP_AUDIO_QUEUE();
    }
#endif
    return pcm_len;
}
#endif
#endif

#if 0
int decode_sbc_frame(unsigned char *pcm_buffer, unsigned int pcm_len)
{
    static uint32_t g_preIrqTime = 0;
    uint8_t underflow = 0;
    int32_t stime,etime;

    unsigned char retry = 0;
    int r = 0;
    unsigned char *e1 = NULL, *e2 = NULL;
    unsigned int len1 = 0, len2 = 0;

    static SbcPcmData pcm_data;
    XaStatus ret = XA_STATUS_SUCCESS;
    unsigned short byte_decode = 0;


    stime = hal_sys_timer_get();
    pcm_data.data = (unsigned char*)pcm_buffer;

    LOCK_APP_AUDIO_QUEUE();
again:
    if(need_init_decoder) {
        pcm_data.data = (unsigned char*)pcm_buffer;
        pcm_data.dataLen = 0;
        SBC_InitDecoder(&sbc_decoder);
    }

get_again:
    len1 = len2 = 0;
    r = APP_AUDIO_PeekCQueue(&sbc_queue, SBC_TEMP_BUFFER_SIZE, &e1, &len1, &e2, &len2);

    if(r == CQ_ERR || len1 == 0) {
        int size;
        UNLOCK_APP_AUDIO_QUEUE();
        osDelay(2);
        LOCK_APP_AUDIO_QUEUE();
        if (retry++<8){
            goto get_again;
        }else{
            need_init_decoder = true;
            size = APP_AUDIO_LengthOfCQueue(&sbc_queue);
            underflow = 1;
//            a2dp_cache_status = APP_AUDIO_CACHE_CACHEING;
            LOG_PRINT_A2DP("cache underflow size:%d retry:%d\n", size, retry);
            goto exit;
        }
    }
    {
        uint32_t lock = int_lock();
        ret = SBC_DecodeFrames(&sbc_decoder, (unsigned char *)e1, len1, &byte_decode,
        &pcm_data, pcm_len, sbc_eq_band_gain);
        int_unlock(lock);
    }
    if(ret == XA_STATUS_CONTINUE) {
        need_init_decoder = false;
        APP_AUDIO_DeCQueue(&sbc_queue, 0, len1);
        goto again;

        /* back again */
    }
    else if(ret == XA_STATUS_SUCCESS) {
        need_init_decoder = false;
        r = pcm_data.dataLen;
        pcm_data.dataLen = 0;

        APP_AUDIO_DeCQueue(&sbc_queue, 0, byte_decode);

        //LOG_PRINT_A2DP("p %d\n", pcm_data.sampleFreq);

        /* leave */
    }
    else if(ret == XA_STATUS_FAILED) {
        need_init_decoder = true;
        r = pcm_data.dataLen;
        LOG_PRINT_A2DP("err\n");

        APP_AUDIO_DeCQueue(&sbc_queue, 0, byte_decode);

        /* leave */
    }
    else if(ret == XA_STATUS_NO_RESOURCES) {
        need_init_decoder = false;

        LOG_PRINT_A2DP("no\n");

        /* leav */
        r = 0;
    }

exit:
    etime = hal_sys_timer_get();
    if (underflow || need_init_decoder){
        int32_t a,b;
        a = TICKS_TO_MS(stime - g_preIrqTime);
        b = TICKS_TO_MS(etime - stime);
        LOG_PRINT_A2DP( "sbc_decoder irqDur:%d Decode:%d diff:%d size:%d retry:%d\n ", a, b, (a-b), APP_AUDIO_LengthOfCQueue(&sbc_queue),retry);
    }
    g_preIrqTime = stime;
    UNLOCK_APP_AUDIO_QUEUE();
    return r;
}

#else
static int decode_sbc_frame(unsigned char *pcm_buffer, unsigned int pcm_len)
{
    static uint32_t g_preIrqTime = 0;
    uint8_t underflow = 0;
    int32_t stime,etime;

    static unsigned int sbc_next_frame_size;
    unsigned char retry = 0;
    int r = 0;
    unsigned char *e1 = NULL, *e2 = NULL;
    unsigned int len1 = 0, len2 = 0;

    unsigned int pcm_offset = 0;
    unsigned int pcm_processed = 0;

    static btif_sbc_pcm_data_t pcm_data;
    bt_status_t  ret = BT_STS_SUCCESS;
    unsigned short byte_decode = 0;
#ifndef __TWS_USE_PLL_SYNC__
#ifdef A2DP_AUDIO_SYNC_WITH_LOCAL
    int q_usedlen = 0;
#endif
#endif
    stime = hal_sys_timer_get();

    pcm_data.data = (unsigned char*)pcm_buffer;

    if (!sbc_next_frame_size){
        sbc_next_frame_size = sbc_frame_size;
    }

reinit:
    if(need_init_decoder) {
        sbc_next_frame_size = sbc_frame_size;
        pcm_offset = 0;
        pcm_data.data = (unsigned char*)(pcm_buffer+pcm_offset);
        pcm_data.dataLen = 0;
        btif_sbc_init_decoder(sbc_decoder);
    }

get_again:
    LOCK_APP_AUDIO_QUEUE();
    len1 = len2 = 0;
    r = PeekCQueue(&sbc_queue, sbc_next_frame_size, &e1, &len1, &e2, &len2);
    UNLOCK_APP_AUDIO_QUEUE();
#ifdef  __LOCK_AUDIO_THREAD__
    if(r == CQ_ERR) {
        pcm_processed = pcm_data.dataLen;
        LOG_PRINT_A2DP("cache underflow retry:%d\n", retry);
        goto exit;
    }
#elif defined (A2DP_SYNC_WITH_PUT_MUTUX)
    if(r == CQ_ERR) {
        int size;
        A2DP_SYNC_WITH_PUT_MUTUX_ALLOC();
        A2DP_SYNC_WITH_PUT_MUTUX_WAIT();
        if (retry++<A2DP_SYNC_WITH_PUT_MUTUX_TIMEROUT_CNT){
            goto get_again;
        }else{
            LOCK_APP_AUDIO_QUEUE();
            size = LengthOfCQueue(&sbc_queue);
            UNLOCK_APP_AUDIO_QUEUE();
            pcm_processed = pcm_data.dataLen;
            underflow = 1;
            LOG_PRINT_A2DP("cache underflow size:%d retry:%d\n", size, retry);
            goto exit;
        }
    }else{
        retry = 0;
    }
#else
#if 1
    if(r == CQ_ERR) {
        pcm_processed = pcm_data.dataLen;
        LOG_PRINT_A2DP("cache underflow retry:%d\n", retry);
        goto exit;
    }
#else
    if(r == CQ_ERR) {
        int size;
        osDelay(2);
        LOCK_APP_AUDIO_QUEUE();
        size = LengthOfCQueue(&sbc_queue);
        UNLOCK_APP_AUDIO_QUEUE();
        if (retry++<12){
            goto get_again;
        }else{
            pcm_processed = pcm_data.dataLen;
            underflow = 1;
//            a2dp_cache_status = APP_AUDIO_CACHE_CACHEING;
            LOG_PRINT_A2DP("cache underflow size:%d retry:%d\n", size, retry);
            goto exit;
        }
    }
#endif
#endif
    if (!len1){
        LOG_PRINT_A2DP("len1 underflow %d/%d\n", len1, len2);
        goto get_again;
    }
    {
        uint32_t lock = int_lock();
        ret =  btif_sbc_decode_frames(sbc_decoder, (unsigned char *)e1, len1,
                                    &byte_decode,
                                    &pcm_data,
                                    pcm_len - pcm_offset,
                                    sbc_eq_band_gain);
        int_unlock(lock);
    }
    if(ret == BT_STS_CONTINUE) {
        need_init_decoder = false;
        LOCK_APP_AUDIO_QUEUE();
        DeCQueue(&sbc_queue, 0, byte_decode);
        UNLOCK_APP_AUDIO_QUEUE();
#ifndef __TWS_USE_PLL_SYNC__
#ifdef A2DP_AUDIO_SYNC_WITH_LOCAL
        q_usedlen += byte_decode;
#endif
#endif
        sbc_next_frame_size = (sbc_frame_size-byte_decode)>0?(sbc_frame_size-byte_decode):sbc_frame_size;

        goto get_again;

        /* back again */
    }
    else if(ret == BT_STS_SUCCESS) {
        need_init_decoder = false;
        pcm_processed = pcm_data.dataLen;
        pcm_data.dataLen = 0;

        LOCK_APP_AUDIO_QUEUE();
        DeCQueue(&sbc_queue, 0, byte_decode);
        UNLOCK_APP_AUDIO_QUEUE();
#ifndef __TWS_USE_PLL_SYNC__
#ifdef A2DP_AUDIO_SYNC_WITH_LOCAL
        q_usedlen += byte_decode;
#endif
#endif

        sbc_next_frame_size = sbc_frame_size;
        //LOG_PRINT_A2DP("p %d\n", pcm_data.sampleFreq);

        /* leave */
    }
    else if(ret == BT_STS_FAILED) {
        need_init_decoder = true;
        pcm_processed = pcm_data.dataLen;
        pcm_offset += pcm_data.dataLen;
        LOG_PRINT_A2DP("err mutelen:%d\n",pcm_processed);

        LOCK_APP_AUDIO_QUEUE();
        DeCQueue(&sbc_queue, 0, byte_decode);
        UNLOCK_APP_AUDIO_QUEUE();

        sbc_next_frame_size = (sbc_frame_size-byte_decode)>0?(sbc_frame_size-byte_decode):sbc_frame_size;

        if (pcm_offset<pcm_len)
            goto reinit;
        /* leave */
    }
    else if(ret == BT_STS_NO_RESOURCES) {
        need_init_decoder = false;
        pcm_processed = pcm_data.dataLen;
        pcm_offset += pcm_data.dataLen;
        LOG_PRINT_A2DP("no_res mutelen:%d\n",pcm_processed);

        LOCK_APP_AUDIO_QUEUE();
        DeCQueue(&sbc_queue, 0, byte_decode);
        UNLOCK_APP_AUDIO_QUEUE();

        sbc_next_frame_size = (sbc_frame_size-byte_decode)>0?(sbc_frame_size-byte_decode):sbc_frame_size;
    }
exit:

    etime = hal_sys_timer_get();
    if (underflow || need_init_decoder)
        LOG_PRINT_A2DP( "sbc_decoder %d/%d %d\n ", TICKS_TO_MS(stime - g_preIrqTime),TICKS_TO_MS(etime - stime),retry);
    g_preIrqTime = stime;
#ifndef __TWS_USE_PLL_SYNC__
#ifdef A2DP_AUDIO_SYNC_WITH_LOCAL
    if(pcm_len == pcm_processed){
        int size;
        LOCK_APP_AUDIO_QUEUE();
        size = LengthOfCQueue(&sbc_queue);
        UNLOCK_APP_AUDIO_QUEUE();
//        LOG_PRINT_A2DP("decode_sbc_frame Queue remain size:%d frame_size:%d\n",size, sbc_frame_size);
        A2DP_AUDIO_SYNC_TRACE("sync status:%d qsize:%d/%d fsize:%d, thr:%d used:%d", sync_status,size, g_sbc_queue_size, sbc_frame_size, A2DPPLAY_CACHE_OK_THRESHOLD, q_usedlen);
/*
nor:
    if (size>7488+1170*2)
        goto inc
    if (size<7488-1170)
        goto dec
dec:
    if (size>=7488+1170)
        goto nor
inc:
    if (size<=7488)
        goto nor
*/
        switch (sync_status) {
            case A2DP_AUDIO_SYNC_STATUS_SAMPLERATE_DEC:
                if (size >= (A2DPPLAY_CACHE_OK_THRESHOLD + q_usedlen)){
                    a2dp_audio_sync_proc(A2DPPLAY_SYNC_STATUS_SET, A2DP_AUDIO_SYNC_WITH_LOCAL_SAMPLERATE_DEFAULT);
                    sync_status = A2DP_AUDIO_SYNC_STATUS_SAMPLERATE_DEFAULT;
                    A2DP_AUDIO_SYNC_TRACE( "default pll freq");
                }else{
                    a2dp_audio_sync_proc(A2DPPLAY_SYNC_STATUS_PROC, 0);
                }
                break;
            case A2DP_AUDIO_SYNC_STATUS_SAMPLERATE_INC:
                if (size <= (A2DPPLAY_CACHE_OK_THRESHOLD)){
                    a2dp_audio_sync_proc(A2DPPLAY_SYNC_STATUS_SET, A2DP_AUDIO_SYNC_WITH_LOCAL_SAMPLERATE_DEFAULT);
                    sync_status = A2DP_AUDIO_SYNC_STATUS_SAMPLERATE_DEFAULT;
                    A2DP_AUDIO_SYNC_TRACE( "default pll freq");
                }else{
                    a2dp_audio_sync_proc(A2DPPLAY_SYNC_STATUS_PROC, 0);
                }
                break;
            case A2DP_AUDIO_SYNC_STATUS_SAMPLERATE_DEFAULT:
            default:
                if (size > (A2DPPLAY_CACHE_OK_THRESHOLD + (q_usedlen<<1))){
                    a2dp_audio_sync_proc(A2DPPLAY_SYNC_STATUS_SET, A2DP_AUDIO_SYNC_WITH_LOCAL_SAMPLERATE_INC);
                    sync_status = A2DP_AUDIO_SYNC_STATUS_SAMPLERATE_INC;
                    A2DP_AUDIO_SYNC_TRACE( "inc pll freq");
                }else if (size < (A2DPPLAY_CACHE_OK_THRESHOLD - q_usedlen)){
                    a2dp_audio_sync_proc(A2DPPLAY_SYNC_STATUS_SET, A2DP_AUDIO_SYNC_WITH_LOCAL_SAMPLERATE_DEC);
                    sync_status = A2DP_AUDIO_SYNC_STATUS_SAMPLERATE_DEC;
                    A2DP_AUDIO_SYNC_TRACE( "dec pll freq");
                }else{
                    a2dp_audio_sync_proc(A2DPPLAY_SYNC_STATUS_PROC, 0);
                }
                break;
        }
    }
#endif
#endif
    return pcm_processed;
}
#endif

#if defined(__AUDIO_RESAMPLE__) && defined(SW_PLAYBACK_RESAMPLE)
#define AAC_RESAMPLE_ITER_LEN               (1024 * 2 * 2)
#define SBC_RESAMPLE_ITER_LEN               (128 * 2 * 2)

static struct APP_RESAMPLE_T *a2dp_resample;

#if 0
#if defined(A2DP_AAC_ON)
static int a2dp_resample_iter_aac(uint8_t *buf, uint32_t len)
{
    decode_aac_frame(buf, len);
    return 0;
}
#endif
#endif

static int a2dp_resample_iter_sbc(uint8_t *buf, uint32_t len)
{
    uint32_t l = decode_sbc_frame(buf, len);
    if (l != len) {
        LOG_PRINT_A2DP( "a2dp_audio_more_data decode err %d/%d", l, len);
        a2dp_cache_status = APP_AUDIO_CACHE_CACHEING;
        LOG_PRINT_A2DP( "set to APP_AUDIO_CACHE_CACHEING");
        a2dp_timestamp_parser_needsync();
        return 1;
    }
    return 0;
}

#endif

uint32_t a2dp_audio_more_data(uint8_t *buf, uint32_t len)
{
    uint32_t l = 0;

    if (!a2dp_audio_isrunning(A2DPPLAY_STRTEAM_GET, true)){
    }

    if (a2dp_cache_status == APP_AUDIO_CACHE_CACHEING){
#if FPGA==0
        LOG_PRINT_A2DP( "a2dp_audio_more_data cache not ready skip frame %d\n" ,a2dp_get_codec_type((enum BT_DEVICE_ID_T)0));
#endif
        memset(buf, 0, len);
    }else{
#if defined(__AUDIO_RESAMPLE__) && defined(SW_PLAYBACK_RESAMPLE)
        if (allow_resample)
        {
            app_playback_resample_run(a2dp_resample, buf, len);
        }
        else
#endif
        {
#if 0
#if defined(A2DP_AAC_ON)
            if(codec_type == AVDTP_CODEC_TYPE_MPEG2_4_AAC)
            {
                l = decode_aac_frame(buf, len);
            }
            else
#endif
#endif
            {
                l = decode_sbc_frame(buf, len);
                if (l!=len){
                    memset(buf+l, 0, len-l);
                    LOG_PRINT_A2DP( "a2dp_audio_more_data decode err %d/%d", l, len);
                    a2dp_cache_status = APP_AUDIO_CACHE_CACHEING;
                    LOG_PRINT_A2DP( "set to APP_AUDIO_CACHE_CACHEING");
                    a2dp_timestamp_parser_needsync();
                }
            }
        }
    }

    A2DP_SYNC_WITH_GET_MUTUX_SET();

    return len;
}
extern "C" uint8_t app_tws_get_codec_type(void);

#if defined(A2DP_LHDC_ON)
extern void initial_lhdc_assemble_packet(bool splitFlg);
int a2dp_audio_lhdc_init(void)
{
    uint8_t bits_depth = bt_sbc_player_get_bitsDepth();
    AUD_SAMPRATE_T sample_rate = bt_get_sbc_sample_rate();

    LOG_PRINT_A2DP("a2dp_audio_init sample Rate=%d, bits_depth = %d\n", sample_rate, bits_depth);
    lhdcInit(bits_depth, sample_rate, bits_depth >= 24 ? 1 : 0);
    initial_lhdc_assemble_packet(true);

    return 0;
}
#endif

int a2dp_audio_init(void)
{
    const float EQLevel[25] = {
        0.0630957,  0.0794328, 0.1,       0.1258925, 0.1584893,
        0.1995262,  0.2511886, 0.3162278, 0.398107 , 0.5011872,
        0.6309573,  0.794328 , 1,         1.258925 , 1.584893 ,
        1.995262 ,  2.5118864, 3.1622776, 3.9810717, 5.011872 ,
        6.309573 ,  7.943282 , 10       , 12.589254, 15.848932
    };//-12~12
    //uint8_t *buff = NULL;
    uint8_t i;

#if defined(__AUDIO_RESAMPLE__) && defined(SW_PLAYBACK_RESAMPLE)

    APP_RESAMPLE_ITER_CALLBACK iter_cb;
    uint32_t iter_len;

#if 0
#if defined(A2DP_AAC_ON)
    if(bt_sbc_player_get_codec_type() == AVDTP_CODEC_TYPE_MPEG2_4_AAC)
    {
        void * aac_info_prt = NULL;
        app_audio_mempool_get_buff((uint8_t **)&aac_info_prt, GetAACinfoSize());
        SetAACInfoBase(aac_info_prt);
        //LOG_PRINT_A2DP("a2dp_audio_init raw_aac_pcm_data = 0x%x f=%d\n", raw_aac_pcm_data, app_audio_mempool_free_buff_size());
        iter_cb = a2dp_resample_iter_aac;
        iter_len = AAC_RESAMPLE_ITER_LEN;
    }
    else
#endif
#endif
    {
        iter_cb = a2dp_resample_iter_sbc;
        iter_len = SBC_RESAMPLE_ITER_LEN;
    }

///todo ...for 2200
#ifdef CHIP_BEST1000
    if(hal_get_chip_metal_id() >= HAL_CHIP_METAL_ID_2 && hal_sysfreq_get() <= HAL_CMU_FREQ_52M)
    {
        allow_resample = true;
    }
#endif
    if (allow_resample)
    {
#ifdef RESAMPLE_ANY_SAMPLE_RATE
        a2dp_resample = app_playback_resample_any_open(AUD_CHANNEL_NUM_2,
            iter_cb, iter_len, (float)a2dp_sample_rate / AUD_SAMPRATE_50781);
#else
        a2dp_resample = app_playback_resample_open(a2dp_sample_rate, AUD_CHANNEL_NUM_2,
            iter_cb, iter_len);
#endif
    }
#else
#if 0
#if defined(A2DP_AAC_ON)
    if(bt_sbc_player_get_codec_type() == AVDTP_CODEC_TYPE_MPEG2_4_AAC){
        void * aac_info_prt = NULL;
        app_audio_mempool_get_buff((uint8_t **)&aac_info_prt, GetAACinfoSize());
        SetAACInfoBase(aac_info_prt);
        LOG_PRINT_A2DP("a2dp_audio_init f=%d %d\n", app_audio_mempool_free_buff_size(), GetAACinfoSize());
    }
#endif
#endif
#endif

    for (i=0; i<sizeof(sbc_eq_band_gain)/sizeof(float); i++){
        sbc_eq_band_gain[i] = EQLevel[cfg_aud_eq_sbc_band_settings[i]+12];
    }

    A2DP_SYNC_WITH_GET_MUTUX_FREE();
    A2DP_SYNC_WITH_PUT_MUTUX_FREE();

#if 0
#if defined(A2DP_AAC_ON)
    if (a2dp_get_codec_type((enum BT_DEVICE_ID_T)0) == AVDTP_CODEC_TYPE_SBC)
#endif
    {
        app_audio_mempool_get_buff((uint8_t **)&sbc_decoder, sizeof(SbcDecoder));
    }

#ifdef SBC_QUEUE_SIZE
    g_sbc_queue_size = SBC_QUEUE_SIZE;
#else
    g_sbc_queue_size = app_audio_mempool_free_buff_size();
#endif
#endif

#if 0
    app_audio_mempool_get_buff(&buff, g_sbc_queue_size);
    memset(buff, 0, g_sbc_queue_size);
#endif
    a2dp_audio_isrunning(A2DPPLAY_STRTEAM_PUT, false);
    a2dp_audio_isrunning(A2DPPLAY_STRTEAM_GET, false);
#if 0
    LOCK_APP_AUDIO_QUEUE();
    APP_AUDIO_InitCQueue(&sbc_queue, g_sbc_queue_size, buff);
    UNLOCK_APP_AUDIO_QUEUE();
#endif
#ifndef __TWS_USE_PLL_SYNC__
#ifdef A2DP_AUDIO_SYNC_WITH_LOCAL
    a2dp_audio_sync_proc(A2DPPLAY_SYNC_STATUS_SET|A2DPPLAY_SYNC_STATUS_RESET, A2DP_AUDIO_SYNC_WITH_LOCAL_SAMPLERATE_DEFAULT);
#endif
#endif

#if defined(A2DP_LHDC_ON)
    if ((a2dp_get_codec_type(BT_DEVICE_ID_1) == BTIF_AVDTP_CODEC_TYPE_LHDC) ||
        (app_tws_get_codec_type() == BTIF_AVDTP_CODEC_TYPE_LHDC)){
        a2dp_audio_lhdc_init();
    }
#endif

    a2dp_cache_status = APP_AUDIO_CACHE_CACHEING;
    need_init_decoder =  true;

    sbc_frame_rev_len = 0;
    sbc_frame_num = 0;
    sbc_frame_size = SBC_TEMP_BUFFER_SIZE;
#if 0
#if defined(A2DP_AAC_ON)
    hAACDecoder = NULL;
#endif
#endif
    return 0;
}

#ifdef _AMA_
void a2dp_clean_sbc_queue(void)
{
	if (APP_AUDIO_LengthOfCQueue(&sbc_queue) > 0)
	{
		LOCK_APP_AUDIO_QUEUE();
		APP_AUDIO_DeCQueue(&sbc_queue,0,APP_AUDIO_LengthOfCQueue(&sbc_queue));
		UNLOCK_APP_AUDIO_QUEUE();
	}
    a2dp_cache_status = APP_AUDIO_CACHE_CACHEING;
}
#endif

int a2dp_audio_deinit(void)
{
#if defined(__AUDIO_RESAMPLE__) && defined(SW_PLAYBACK_RESAMPLE)
    if (allow_resample) {
        app_playback_resample_close(a2dp_resample);
        a2dp_resample = NULL;
#ifdef CHIP_BEST1000
        allow_resample = false;
#endif
    }
#endif

    A2DP_SYNC_WITH_GET_MUTUX_SET();
    A2DP_SYNC_WITH_PUT_MUTUX_SET();

    A2DP_SYNC_WITH_GET_MUTUX_FREE();
    A2DP_SYNC_WITH_PUT_MUTUX_FREE();

    a2dp_audio_isrunning(A2DPPLAY_STRTEAM_PUT, false);
    a2dp_audio_isrunning(A2DPPLAY_STRTEAM_GET, false);

    a2dp_cache_status = APP_AUDIO_CACHE_QTY;
    need_init_decoder =  true;

    sbc_frame_rev_len = 0;
    sbc_frame_num = 0;
    sbc_frame_size = SBC_TEMP_BUFFER_SIZE;

    return 0;
}

