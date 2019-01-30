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
#ifdef MBED
#include "SDFileSystem.h"
#endif
#include "plat_types.h"
#include "hal_uart.h"
#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_location.h"
#include "cqueue.h"
#include "app_audio.h"
#include "app_ring_merge.h"
#include "tgt_hardware.h"
#include "analog.h"
#include "bt_sco_chain.h"
#include "iir_resample.h"

#define MSBC_PCM_PLC_ENABLE

// BT
extern "C" {
#include "hfp_api.h"
#include "plc_8000.h"
#include "speech_utils.h"

#if defined(MSBC_PLC_ENABLE)
#include "sbcplc.h"
#if defined(MSBC_PCM_PLC_ENABLE)
#include "plc_16000.h"
#endif
#endif


#include "app_tws.h"
#if defined(MSBC_PLC_ENCODER) && !defined(MSBC_PCM_PLC_ENABLE)
#error "When enable MSBC_PLC_ENCODER, must enable MSBC_PCM_PLC_ENABLE!"
#endif

#if defined(_CVSD_BYPASS_)
#include "Pcm8k_Cvsd.h"
#endif

extern short *aec_echo_buf;

#if defined( SPEECH_TX_AEC ) || defined( SPEECH_TX_AEC2 ) || defined(SPEECH_TX_AEC2FLOAT)
#if defined(__AUDIO_RESAMPLE__) && defined(SW_SCO_RESAMPLE)
static uint8_t *echo_buf_queue;
static uint16_t echo_buf_q_size;
static uint16_t echo_buf_q_wpos;
static uint16_t echo_buf_q_rpos;
static bool echo_buf_q_full;
#endif
#endif

#if defined (SPEECH_RX_PLC)
static void *speech_plc;
#endif
}

// app_bt_stream.cpp::bt_sco_player(), used buffer size
#define APP_BT_STREAM_USE_BUF_SIZE      (1024*2)

#if defined(MSBC_8K_SAMPLE_RATE)
static int16_t *upsample_buf_for_msbc = NULL;
static int16_t *downsample_buf_for_msbc = NULL;
static IirResampleState *upsample_st = NULL;
static IirResampleState *downsample_st = NULL;
#endif

#define VOICEBTPCM_TRACE(s,...)
//LOG_PRINT_VOICE_CALL_AUD(s, ##__VA_ARGS__)

/* voicebtpcm_pcm queue */
// fs: 16000(msbc: 1 frame; cvsd: 2 frames)
#define VOICEBTPCM_PCM_FRAME_LENGTH (SPEECH_FRAME_MS_TO_LEN(16000, SPEECH_SCO_FRAME_MS))
#define VOICEBTPCM_PCM_TEMP_BUFFER_SIZE (VOICEBTPCM_PCM_FRAME_LENGTH*2)
#define VOICEBTPCM_PCM_QUEUE_SIZE (VOICEBTPCM_PCM_TEMP_BUFFER_SIZE*3)

CQueue voicebtpcm_p2m_queue;
CQueue voicebtpcm_m2p_queue;

static enum APP_AUDIO_CACHE_T voicebtpcm_cache_m2p_status = APP_AUDIO_CACHE_QTY;
static enum APP_AUDIO_CACHE_T voicebtpcm_cache_p2m_status = APP_AUDIO_CACHE_QTY;

extern bool bt_sco_codec_is_msbc(void);
#define ENCODE_TEMP_PCM_LEN (120)

#if defined(HFP_1_6_ENABLE)
//extern uint16_t bt_sco_get_current_codecid(void);
extern "C" hfp_sco_codec_t bt_sco_get_current_codecid(void);
#define MSBC_FRAME_SIZE (60)
static uint8_t *msbc_buf_before_decode;
static uint8_t *msbc_buf_before_plc;
static char need_init_decoder = true;
static btif_sbc_decoder_t msbc_decoder;
static float msbc_eq_band_gain[CFG_HW_AUD_EQ_NUM_BANDS]= {0,0,0,0,0,0,0,0};

#define MSBC_ENCODE_PCM_LEN (240)

unsigned char *temp_msbc_buf;
unsigned char *temp_msbc_buf1;
static char need_init_encoder = true;
#if defined(MSBC_PLC_ENABLE)
PLC_State msbc_plc_state;
#endif

#if defined(MSBC_PLC_ENCODER)
#define MSBC_PLC_ENCODER_SIZE 57
#define MSBC_PLC_ENCODER_BUFFER_SIZE 193
#define MSBC_PLC_ENCODER_BUFFER_OFFSET 73

static btif_sbc_encoder_t *msbc_plc_encoder;
static btif_sbc_pcm_data_t *msbc_plc_encoder_data;
unsigned char *msbc_plc_encoder_buffer;
short *msbc_plc_encoder_inbuffer;
short *msbc_plc_output_pcmbuffer;
#endif

static bool msbc_input_valid = false;
static uint32_t msbc_input_offset = 0;
static btif_sbc_encoder_t *msbc_encoder;
static btif_sbc_pcm_data_t msbc_encoder_pcmdata;
static unsigned char msbc_counter = 0x08;
#endif

extern "C" int tws_player_set_hfp_info(uint16_t msbc_sync_offset);

static uint32_t master_msbc_sync_offset = 0;

int FLASH_TEXT_LOC voicebtpcm_pcm_msbc_sync_offset_get_local(uint16_t *msbc_sync_offset)
{
    if (msbc_input_valid){
        *msbc_sync_offset = msbc_input_offset;
        return 0;
    }
    return -1;
}

int FLASH_TEXT_LOC voicebtpcm_pcm_set_master_msbc_sync_offset(uint16_t msbc_sync_offset)
{
    master_msbc_sync_offset = msbc_sync_offset;
    if (msbc_input_valid){
        if (master_msbc_sync_offset != msbc_input_offset){
            bt_sco_player_restart_requeset(true);
        }
    }
    return 0;
}

bool FLASH_TEXT_LOC voicebtpcm_pcm_msbc_sync_offset_check_with_master(uint16_t msbc_sync_offset)
{
    if (msbc_input_valid && app_tws_is_slave_mode()){
        if (msbc_sync_offset != master_msbc_sync_offset){
            return false;
        }
    }
    return true;
}

inline int sco_parse_synchronization_header(uint8_t *buf, uint8_t *sn);

extern "C" uint32_t bt_sco_btpcm_btpcm_type(void);
extern "C" uint32_t bt_sco_btpcm_get_playback_count(void);

//playback flow
//bt-->store_voicebtpcm_m2p_buffer-->decode_voicebtpcm_m2p_frame-->audioflinger playback-->speaker
//used by playback, store data from bt to memory
int store_voicebtpcm_m2p_buffer(unsigned char *buf, unsigned int len)
{
    int size;
    unsigned int avail_size;

#if defined(HFP_1_6_ENABLE)
    static uint32_t error_cnt = 0;
    if (bt_sco_get_current_codecid() == BTIF_HF_SCO_CODEC_MSBC){
        uint8_t msbc_raw_sn = 0xff;
        uint8_t frame_num = 0;
        if (!msbc_input_valid){
            unsigned int i;
            for(i=0; i<len; i++){
                if(!sco_parse_synchronization_header(&buf[i], &msbc_raw_sn) &&
                    0 == msbc_raw_sn){
                    LOG_PRINT_VOICE_CALL_AUD("!!!!!!!!!!!input msbc find sync sn:%d offset:%d", msbc_raw_sn, i); 
                    msbc_input_valid = true;
                    msbc_input_offset = i;
                    error_cnt = 0;
                    break;
                }
            }

            if (msbc_input_valid){
                LOG_PRINT_VOICE_CALL_AUD("!!!!!!!!!!!!!!!!!!!!!! input:%08x len:%d", buf+i, len-i);        
                LOCK_APP_AUDIO_QUEUE();
                APP_AUDIO_EnCQueue(&voicebtpcm_m2p_queue, buf+i, len-i);
                UNLOCK_APP_AUDIO_QUEUE();
                if(bt_sco_btpcm_btpcm_type() == 4)
                {
                    if(msbc_input_offset >=0 && msbc_input_offset<60)
                    {
                        voicebtpcm_cache_m2p_status = APP_AUDIO_CACHE_OK;
                    }
                }
            }
            return len;
        }else{
            int nRet = sco_parse_synchronization_header(&buf[msbc_input_offset], &msbc_raw_sn);
            if (nRet){
                frame_num = len/60;
                if (frame_num%4 == 0){
                    if (msbc_raw_sn == 0){
                        error_cnt = 0;
                    }else{
                        LOG_PRINT_VOICE_CALL_AUD("!!!!!!!!!!!!!!!!!!!!!! inputerror sn:%d cnt:%d", msbc_raw_sn, error_cnt);
                        LOG_PRINT_VOICE_CALL_AUD_DUMP8("%02x ", &buf[msbc_input_offset], 6);
                        error_cnt++;
                    }
                }else if (frame_num == 2){
                    if ((msbc_raw_sn == 0) ||
                        (msbc_raw_sn == 2)){
                        error_cnt = 0;
                    }else{
                        LOG_PRINT_VOICE_CALL_AUD("!!!!!!!!!!!!!!!!!!!!!! inputerror sn:%d cnt:%d", msbc_raw_sn, error_cnt);
                        LOG_PRINT_VOICE_CALL_AUD_DUMP8("%02x ", &buf[msbc_input_offset], 6);
                        error_cnt++;
                    }
                }else{
                    ASSERT(0, "%s err len", __func__, len);
                }
            }else{
                error_cnt = 0;
            }
            if (error_cnt>10){
                bt_sco_player_restart_requeset(true);
            }
        }
    }
#endif
    LOCK_APP_AUDIO_QUEUE();
    avail_size = APP_AUDIO_AvailableOfCQueue(&voicebtpcm_m2p_queue);
    if (len <= avail_size)
    {
        APP_AUDIO_EnCQueue(&voicebtpcm_m2p_queue, buf, len);
    }
    else
    {
        VOICEBTPCM_TRACE( "spk buff overflow %d/%d", len, avail_size);
        if (bt_sco_codec_is_msbc())
        {
            LOG_PRINT_VOICE_CALL_AUD( "drop frame so maybe playback irq is not run");
            bt_sco_player_restart_requeset(true);   
        }
        else
        {
            APP_AUDIO_DeCQueue(&voicebtpcm_m2p_queue, 0, len - avail_size);
            APP_AUDIO_EnCQueue(&voicebtpcm_m2p_queue, buf, len);
        }
    }
    size = APP_AUDIO_LengthOfCQueue(&voicebtpcm_m2p_queue);
    UNLOCK_APP_AUDIO_QUEUE();
    if(voicebtpcm_cache_m2p_status !=APP_AUDIO_CACHE_OK)
    {
        LOG_PRINT_VOICE_CALL_AUD("type=%d,size=%d,msbc_input_offset=%d",bt_sco_btpcm_btpcm_type(),size,msbc_input_offset);
    }
    if (bt_sco_get_current_codecid() == BTIF_HF_SCO_CODEC_MSBC)
    {
        if(bt_sco_btpcm_btpcm_type() == 0)
        {
            LOG_PRINT_VOICE_CALL_AUD("ERROR TYPE OF BTPCM IRQ!!");
        }
        else if(bt_sco_btpcm_btpcm_type() == 1)
        {
                if(size > (VOICEBTPCM_PCM_TEMP_BUFFER_SIZE-240))
                {
                    voicebtpcm_cache_m2p_status = APP_AUDIO_CACHE_OK;
                }
        }
        else if(bt_sco_btpcm_btpcm_type() == 2)
        {
            if(msbc_input_offset >=180)
           {
                if(size > (VOICEBTPCM_PCM_TEMP_BUFFER_SIZE))
                {
                    voicebtpcm_cache_m2p_status = APP_AUDIO_CACHE_OK;
                }
            }
            else
            {
                if(size > (VOICEBTPCM_PCM_TEMP_BUFFER_SIZE-240))
                {
                    voicebtpcm_cache_m2p_status = APP_AUDIO_CACHE_OK;
                }
            }
        }
        else if(bt_sco_btpcm_btpcm_type() == 3)
        {
#if 1
            if(msbc_input_offset >=0 && msbc_input_offset<60)
            {
                if(size > (VOICEBTPCM_PCM_TEMP_BUFFER_SIZE-60))
                {
                    voicebtpcm_cache_m2p_status = APP_AUDIO_CACHE_OK;
                }
            }
           else if(msbc_input_offset >=180)
           {
                if(size > (VOICEBTPCM_PCM_TEMP_BUFFER_SIZE))
                {
                    voicebtpcm_cache_m2p_status = APP_AUDIO_CACHE_OK;
                }
            }
        else if(msbc_input_offset >=120 && msbc_input_offset<180)
           {
                if(size > (VOICEBTPCM_PCM_TEMP_BUFFER_SIZE))
                {
                    voicebtpcm_cache_m2p_status = APP_AUDIO_CACHE_OK;
                }
            }
        else if(msbc_input_offset >=60 && msbc_input_offset<120)
           {
                if(size > (VOICEBTPCM_PCM_TEMP_BUFFER_SIZE-120))
                {
                    voicebtpcm_cache_m2p_status = APP_AUDIO_CACHE_OK;
                }
            }
#endif

        }
        else if(bt_sco_btpcm_btpcm_type() == 4)
        {
#if 1
            if(msbc_input_offset >=180)
           {
                if(size > (VOICEBTPCM_PCM_TEMP_BUFFER_SIZE-240))
                {
                    voicebtpcm_cache_m2p_status = APP_AUDIO_CACHE_OK;
                }
            }
        else if(msbc_input_offset >=120 && msbc_input_offset<180)
           {
                if(size > (VOICEBTPCM_PCM_TEMP_BUFFER_SIZE-180))
                {
                    voicebtpcm_cache_m2p_status = APP_AUDIO_CACHE_OK;
                }
            }
        else if(msbc_input_offset >=60 && msbc_input_offset<120)
           {
                if(size > (VOICEBTPCM_PCM_TEMP_BUFFER_SIZE-120))
                {
                    voicebtpcm_cache_m2p_status = APP_AUDIO_CACHE_OK;
                }
            }
            else if(msbc_input_offset >=0 && msbc_input_offset<60)
            {
                if(size > (VOICEBTPCM_PCM_TEMP_BUFFER_SIZE-300))
                {
                    voicebtpcm_cache_m2p_status = APP_AUDIO_CACHE_OK;
                }
            }
#endif
        }
    }
    else
        {

        if(size >VOICEBTPCM_PCM_TEMP_BUFFER_SIZE)
        {
            if((bt_sco_btpcm_btpcm_type() == 2 || bt_sco_btpcm_btpcm_type() == 4) && voicebtpcm_cache_m2p_status !=APP_AUDIO_CACHE_OK)
            {
                LOCK_APP_AUDIO_QUEUE();
                APP_AUDIO_DeCQueue(&voicebtpcm_m2p_queue, buf, 60);
              UNLOCK_APP_AUDIO_QUEUE();
            }
            voicebtpcm_cache_m2p_status = APP_AUDIO_CACHE_OK;
        }
    }

    VOICEBTPCM_TRACE("m2p :%d/%d", len, size);

    return 0;
}

//used by playback, decode data from memory to speaker
int decode_voicebtpcm_m2p_frame(unsigned char *pcm_buffer, unsigned int pcm_len)
{
    int r = 0, got_len = 0;
    unsigned char *e1 = NULL, *e2 = NULL;
    unsigned int len1 = 0, len2 = 0;

    LOCK_APP_AUDIO_QUEUE();
    r = APP_AUDIO_PeekCQueue(&voicebtpcm_m2p_queue, pcm_len - got_len, &e1, &len1, &e2, &len2);
    UNLOCK_APP_AUDIO_QUEUE();

    if (r)
    {
        VOICEBTPCM_TRACE( "spk buff underflow");
    }

    if(r == CQ_OK)
    {
        if (len1)
        {
            app_audio_memcpy_16bit((short *)(pcm_buffer), (short *)e1, len1/2);
            LOCK_APP_AUDIO_QUEUE();
            APP_AUDIO_DeCQueue(&voicebtpcm_m2p_queue, 0, len1);
            UNLOCK_APP_AUDIO_QUEUE();
            got_len += len1;
        }
        if (len2)
        {
            app_audio_memcpy_16bit((short *)(pcm_buffer + len1), (short *)e2, len2/2);
            LOCK_APP_AUDIO_QUEUE();
            APP_AUDIO_DeCQueue(&voicebtpcm_m2p_queue, 0, len2);
            UNLOCK_APP_AUDIO_QUEUE();
            got_len += len2;
        }
    }

    return got_len;
}

#if defined(_CVSD_BYPASS_)
#define VOICECVSD_TEMP_BUFFER_SIZE 120
#define VOICECVSD_ENC_SIZE 60
static short cvsd_decode_buff[VOICECVSD_TEMP_BUFFER_SIZE*2];

int decode_cvsd_frame(unsigned char *pcm_buffer, unsigned int pcm_len)
{
    uint32_t r = 0, decode_len = 0;
    unsigned char *e1 = NULL, *e2 = NULL;
    unsigned int len1 = 0, len2 = 0;

    while (decode_len < pcm_len)
    {
        LOCK_APP_AUDIO_QUEUE();
        len1 = len2 = 0;
        e1 = e2 = 0;
        r = APP_AUDIO_PeekCQueue(&voicebtpcm_m2p_queue, VOICECVSD_TEMP_BUFFER_SIZE, &e1, &len1, &e2, &len2);
        UNLOCK_APP_AUDIO_QUEUE();

        if (r == CQ_ERR)
        {
            memset(pcm_buffer, 0, pcm_len);
            LOG_PRINT_VOICE_CALL_AUD( "cvsd decoder spk buff underflow");
            return 0;
        }

        if (len1 != 0)
        {
            CvsdToPcm8k(e1, (short *)(cvsd_decode_buff), len1, 0);

            LOCK_APP_AUDIO_QUEUE();
            DeCQueue(&voicebtpcm_m2p_queue, 0, len1);
            UNLOCK_APP_AUDIO_QUEUE();

            decode_len += len1*2;
        }

        if (len2 != 0)
        {
            CvsdToPcm8k(e2, (short *)(cvsd_decode_buff), len2, 0);

            LOCK_APP_AUDIO_QUEUE();
            DeCQueue(&voicebtpcm_m2p_queue, 0, len2);
            UNLOCK_APP_AUDIO_QUEUE();

            decode_len += len2*2;
        }
    }

    memcpy(pcm_buffer, cvsd_decode_buff, decode_len);

    return decode_len;
}

int encode_cvsd_frame(unsigned char *pcm_buffer, unsigned int pcm_len)
{
    uint32_t r = 0;
    unsigned char *e1 = NULL, *e2 = NULL;
    unsigned int len1 = 0, len2 = 0;
    uint32_t processed_len = 0;
    uint32_t remain_len = 0, enc_len = 0;

    while (processed_len < pcm_len)
    {
        remain_len = pcm_len-processed_len;

        if (remain_len>=(VOICECVSD_ENC_SIZE*2))
        {
            enc_len = VOICECVSD_ENC_SIZE*2;
        }
        else
        {
            enc_len = remain_len;
        }

        LOCK_APP_AUDIO_QUEUE();
        len1 = len2 = 0;
        e1 = e2 = 0;
        r = APP_AUDIO_PeekCQueue(&voicebtpcm_p2m_queue, enc_len, &e1, &len1, &e2, &len2);
        UNLOCK_APP_AUDIO_QUEUE();

        if (r == CQ_ERR)
        {
            memset(pcm_buffer, 0x55, pcm_len);
            LOG_PRINT_VOICE_CALL_AUD( "cvsd encoder spk buff underflow");
            return 0;
        }

        if (e1)
        {
            Pcm8kToCvsd((short *)e1, (unsigned char *)(pcm_buffer + processed_len), len1/2);
            LOCK_APP_AUDIO_QUEUE();
            DeCQueue(&voicebtpcm_p2m_queue, NULL, len1);
            UNLOCK_APP_AUDIO_QUEUE();
            processed_len += len1;
        }

        if (e2)
        {
            Pcm8kToCvsd((short *)e2, (unsigned char *)(pcm_buffer + processed_len), len2/2);
            LOCK_APP_AUDIO_QUEUE();
            DeCQueue(&voicebtpcm_p2m_queue, NULL, len2);
            UNLOCK_APP_AUDIO_QUEUE();
            processed_len += len2;
        }
    }

// #if 0
//     for (int cc = 0; cc < 32; ++cc)
//     {
//         LOG_PRINT_VOICE_CALL_AUD("%x-", e1[cc]);
//     }
// #endif

//     LOG_PRINT_VOICE_CALL_AUD("%s: processed_len %d, pcm_len %d", __func__, processed_len, pcm_len);

    return processed_len;
}
#endif

#if defined(HFP_1_6_ENABLE)
#define MSBC_SYNC_HACKER 1
inline int sco_parse_synchronization_header(uint8_t *buf, uint8_t *sn)
{
    uint8_t sn1, sn2;
#if defined(MSBC_SYNC_HACKER)
    if (((buf[0] != 0x01) && (buf[0] != 0x00))||
        ((buf[1]&0x0f) != 0x08) ||
        (buf[2] != 0xad)){
        return -1;
    }
#else
    if ((buf[0] != 0x01) ||
        ((buf[1]&0x0f) != 0x08) ||
        (buf[2] != 0xad)){
        return -1;
    }
#endif

    sn1 = (buf[1]&0x30)>>4;
    sn2 = (buf[1]&0xc0)>>6;
    if ((sn1 != 0) && (sn1 != 0x3)){
        return -2;
    }
    if ((sn2 != 0) && (sn2 != 0x3)){
        return -3;
    }

    *sn = (sn1&0x01)|(sn2&0x02);

    return 0;
}

int decode_msbc_frame(unsigned char *pcm_buffer, unsigned int pcm_len)
{
    int ttt = 0;
    //int t = 0;
#if defined(MSBC_PLC_ENABLE)
    uint8_t plc_type = 0;
    uint8_t msbc_raw_sn = 0xff;
    static uint8_t msbc_raw_sn_pre;
    static bool msbc_find_first_sync = 0;
#endif
    int r = 0;
    unsigned char *e1 = NULL, *e2 = NULL;
    unsigned int len1 = 0, len2 = 0;
    static btif_sbc_pcm_data_t pcm_data;
    bt_status_t ret = BT_STS_SUCCESS;
    unsigned short byte_decode = 0;

    unsigned int pcm_offset = 0;
    unsigned int pcm_processed = 0;

#if defined(MSBC_PLC_ENABLE)
    pcm_data.data = (unsigned char*)msbc_buf_before_plc;
#else
    pcm_data.data = (unsigned char*)pcm_buffer;
#endif

    if(need_init_decoder)
    {
        LOG_PRINT_VOICE_CALL_AUD("init msbc decoder\n");
        pcm_data.data = (unsigned char*)(pcm_buffer + pcm_offset);
        pcm_data.dataLen = 0;
        btif_sbc_init_decoder(&msbc_decoder);

        msbc_decoder.streamInfo.mSbcFlag = 1;
        msbc_decoder.streamInfo.bitPool = 26;
        msbc_decoder.streamInfo.sampleFreq = BTIF_SBC_CHNL_SAMPLE_FREQ_16;
        msbc_decoder.streamInfo.channelMode = BTIF_SBC_CHNL_MODE_MONO;
        msbc_decoder.streamInfo.allocMethod = BTIF_SBC_ALLOC_METHOD_LOUDNESS;
        /* Number of blocks used to encode the stream (4, 8, 12, or 16) */
        msbc_decoder.streamInfo.numBlocks = BTIF_MSBC_BLOCKS;
        /* The number of subbands in the stream (4 or 8) */
        msbc_decoder.streamInfo.numSubBands = 8;
        msbc_decoder.streamInfo.numChannels = 1;
#if defined(MSBC_PLC_ENABLE)
        InitPLC(&msbc_plc_state);
        msbc_raw_sn_pre = 0xff;
        msbc_find_first_sync = true;
#endif
#if defined(MSBC_PLC_ENCODER)
       btif_sbc_init_encoder(msbc_plc_encoder);
       msbc_plc_encoder->streamInfo.mSbcFlag = 1;
       msbc_plc_encoder->streamInfo.numChannels = 1;
       msbc_plc_encoder->streamInfo.channelMode = BTIF_SBC_CHNL_MODE_MONO;

       msbc_plc_encoder->streamInfo.bitPool   = 26;
       msbc_plc_encoder->streamInfo.sampleFreq  = BTIF_SBC_CHNL_SAMPLE_FREQ_16;
       msbc_plc_encoder->streamInfo.allocMethod = BTIF_SBC_ALLOC_METHOD_LOUDNESS;
       msbc_plc_encoder->streamInfo.numBlocks     = BTIF_MSBC_BLOCKS;
       msbc_plc_encoder->streamInfo.numSubBands = 8;
#endif
        need_init_decoder = false;
    }

get_again:
    LOCK_APP_AUDIO_QUEUE();
    len1 = len2 = 0;
    e1 = e2 = 0;
    r = APP_AUDIO_PeekCQueue(&voicebtpcm_m2p_queue, MSBC_FRAME_SIZE, &e1, &len1, &e2, &len2);
    UNLOCK_APP_AUDIO_QUEUE();

    if (r == CQ_ERR)
    {
        pcm_processed = pcm_len;
        memset(pcm_buffer, 0, pcm_len);
        LOG_PRINT_VOICE_CALL_AUD( "msbc spk buff underflow");
        goto exit;
    }

    if (!len1)
    {
        LOG_PRINT_VOICE_CALL_AUD("len1 underflow %d/%d\n", len1, len2);
        goto get_again;
    }

    if (len1 > 0 && e1)
    {
        memcpy(msbc_buf_before_decode, e1, len1);
    }
    if (len2 > 0 && e2)
    {
        memcpy(msbc_buf_before_decode + len1, e2, len2);
    }


    if (msbc_find_first_sync){
#ifdef CHIP_BEST2300
    if((bt_sco_btpcm_get_playback_count()%2) !=0)
    {
        memset(pcm_buffer,0,pcm_len);
        pcm_processed = pcm_len;
        goto exit1;
    }
#endif
        for(uint8_t i=0;i<MSBC_FRAME_SIZE-3;i++){
            if(!sco_parse_synchronization_header(&msbc_buf_before_decode[i], &msbc_raw_sn)){
                LOG_PRINT_VOICE_CALL_AUD("[PLC]  msbc find sync sn:%d offset:%d", msbc_raw_sn, i); 
                msbc_find_first_sync = false;
                goto start_decoder;
            }
        }
        memset(pcm_buffer,0,pcm_len);
        pcm_processed = pcm_len;
        goto exit;
    }
start_decoder:

    if (sco_parse_synchronization_header(msbc_buf_before_decode, &msbc_raw_sn)){
        plc_type = 2;
    }else{
        plc_type = 1;
    }

    //LOG_PRINT_VOICE_CALL_AUD("sn %d %d", msbc_raw_sn_pre, msbc_raw_sn);

    if (msbc_raw_sn_pre == 0xff &&
        msbc_raw_sn != 0xff){
        // do nothing
        msbc_raw_sn_pre = msbc_raw_sn;
    }else{
        if (msbc_raw_sn == 0xff){
#if defined(MSBC_SYNC_HACKER)
            LOG_PRINT_VOICE_CALL_AUD("[PLC] sbchd err.");   
#else
            uint8_t zero_cnt = 0;
            for (zero_cnt=0; zero_cnt<MSBC_FRAME_SIZE; zero_cnt++){
                if (msbc_buf_before_decode[zero_cnt] != 0)
                    break;
            } 
            LOG_PRINT_VOICE_CALL_AUD("[PLC] sbchd zero:%d", zero_cnt);   
            LOG_PRINT_VOICE_CALL_AUD_DUMP8("%02x ", msbc_buf_before_decode, 6);
#endif
            msbc_raw_sn_pre = 0xff; //(msbc_raw_sn_pre+1)%4;
            plc_type = 2;
        }else if (((msbc_raw_sn_pre+1)%4) == msbc_raw_sn){
            // do nothing
            msbc_raw_sn_pre = msbc_raw_sn;
        }else{
            LOG_PRINT_VOICE_CALL_AUD("[PLC] seq err:%d/%d", msbc_raw_sn, msbc_raw_sn_pre);

            //skip seq err
            msbc_raw_sn_pre = 0xff;
            plc_type = 0;
        }
    }


#if defined(MSBC_PLC_ENABLE)
    if (plc_type == 1)
    {
        ret = btif_sbc_decode_frames(&msbc_decoder, (unsigned char *)msbc_buf_before_decode,
                                    MSBC_FRAME_SIZE, &byte_decode,
                                    &pcm_data, pcm_len - pcm_offset,
                                    msbc_eq_band_gain);
        ttt = hal_sys_timer_get();
#if defined(MSBC_PCM_PLC_ENABLE)
        speech_plc_16000_AddToHistory((PlcSt_16000 *)speech_plc, (short *)pcm_data.data, pcm_len/2);
        memcpy(pcm_buffer, pcm_data.data, pcm_len);
#else
        PLC_good_frame(&msbc_plc_state, (short *)pcm_data.data, (short *)pcm_buffer);
#endif
    }
    else if (plc_type == 2)
    {
#if defined(MSBC_PCM_PLC_ENABLE)
        ttt = hal_sys_timer_get();
#if defined(MSBC_PLC_ENCODER)
        speech_plc_16000_Dofe((PlcSt_16000 *)speech_plc, (short *)pcm_buffer, msbc_plc_encoder_inbuffer, pcm_len/2);
#else
        speech_plc_16000_Dofe((PlcSt_16000 *)speech_plc, (short *)pcm_buffer, NULL, pcm_len/2);
#endif
#else
        PLC_bad_frame(&msbc_plc_state, (short *)pcm_data.data, (short *)pcm_buffer);
#endif
#if defined(MSBC_PLC_ENCODER)
        {
            uint16_t bytes_encoded = 0, buf_len = MSBC_ENCODE_PCM_LEN * sizeof(short);
            msbc_plc_encoder_data->data = (uint8_t *)(msbc_plc_encoder_inbuffer+MSBC_PLC_ENCODER_BUFFER_OFFSET);
            msbc_plc_encoder_data->dataLen = pcm_len;
            msbc_plc_encoder_data->numChannels = 1;
            msbc_plc_encoder_data->sampleFreq = BTIF_SBC_CHNL_SAMPLE_FREQ_16;
            btif_sbc_encode_frames(msbc_plc_encoder, msbc_plc_encoder_data, &bytes_encoded, msbc_plc_encoder_buffer, &buf_len, 0xFFFF);

            btif_sbc_decode_frames(&msbc_decoder, msbc_plc_encoder_buffer,
                                MSBC_FRAME_SIZE - 3, &byte_decode,
                                &pcm_data, pcm_len - pcm_offset,
                                msbc_eq_band_gain);
        }
#endif
        LOG_PRINT_VOICE_CALL_AUD("[PLC] b t:%d ret:%d\n", (hal_sys_timer_get()-ttt), ret);
    }
    else
#endif
    {
        ret = btif_sbc_decode_frames(&msbc_decoder, (unsigned char *)msbc_buf_before_decode,
                                    MSBC_FRAME_SIZE, &byte_decode,
                                    &pcm_data, pcm_len - pcm_offset,
                                    msbc_eq_band_gain);

#if defined(MSBC_PCM_PLC_ENABLE)
        speech_plc_16000_AddToHistory((PlcSt_16000 *)speech_plc, (short *)pcm_data.data, pcm_len/2);
        memcpy(pcm_buffer, pcm_data.data, pcm_len);
#endif
    }

    if(ret == BT_STS_FAILED)
    {
#if defined(MSBC_PCM_PLC_ENABLE)
        ttt = hal_sys_timer_get();
#if defined(MSBC_PLC_ENCODER)
        speech_plc_16000_Dofe((PlcSt_16000 *)speech_plc, (short *)pcm_buffer, msbc_plc_encoder_inbuffer, pcm_len/2);
#else
        speech_plc_16000_Dofe((PlcSt_16000 *)speech_plc, (short *)pcm_buffer, NULL, pcm_len/2);
#endif
#else
        PLC_bad_frame(&msbc_plc_state, (short *)pcm_data.data, (short *)pcm_buffer);
#endif
#if defined(MSBC_PLC_ENCODER)
        {
            uint16_t bytes_encoded = 0, buf_len = MSBC_ENCODE_PCM_LEN * sizeof(short);
            pcm_data.dataLen = 0;
            msbc_plc_encoder_data->data = (uint8_t *)(msbc_plc_encoder_inbuffer+MSBC_PLC_ENCODER_BUFFER_OFFSET);
            msbc_plc_encoder_data->dataLen = pcm_len;
            msbc_plc_encoder_data->numChannels = 1;
            msbc_plc_encoder_data->sampleFreq = BTIF_SBC_CHNL_SAMPLE_FREQ_16;
            btif_sbc_encode_frames(msbc_plc_encoder, msbc_plc_encoder_data, &bytes_encoded, msbc_plc_encoder_buffer, &buf_len, 0xFFFF);

            btif_sbc_decode_frames(&msbc_decoder, msbc_plc_encoder_buffer,
                                MSBC_FRAME_SIZE - 3,
                                &byte_decode,
                                &pcm_data, pcm_len - pcm_offset,
                                msbc_eq_band_gain);
        }
#endif
        plc_type = 2;
        LOG_PRINT_VOICE_CALL_AUD("[PLC]sbcerr b t:%d ret:%d\n", (hal_sys_timer_get()-ttt), ret);
    }


    if (plc_type == 2)
    {
#if defined(MSBC_PLC_ENCODER)
        need_init_decoder = false;
        pcm_processed = pcm_data.dataLen;
        pcm_data.dataLen = 0;
#else
        pcm_processed = pcm_len;
#endif
    }
    else if(ret == BT_STS_SUCCESS)
    {
        need_init_decoder = false;
        pcm_processed = pcm_data.dataLen;
        pcm_data.dataLen = 0;
    }
    else
    {
        LOG_PRINT_VOICE_CALL_AUD( "msbc decode error");
    }

exit:
    if (pcm_processed != pcm_len)
    {
        LOG_PRINT_VOICE_CALL_AUD( "media_msbc_decoder error~~~ %d/%d/%d %d/%d\n ", pcm_processed, pcm_len, byte_decode, ret, plc_type);
    }

    LOCK_APP_AUDIO_QUEUE();
    APP_AUDIO_DeCQueue(&voicebtpcm_m2p_queue, 0, MSBC_FRAME_SIZE);
    UNLOCK_APP_AUDIO_QUEUE();
exit1:
    return pcm_processed;
}
#endif

//capture flow
//mic-->audioflinger capture-->store_voicebtpcm_p2m_buffer-->get_voicebtpcm_p2m_frame-->bt
//used by capture, store data from mic to memory
int store_voicebtpcm_p2m_buffer(unsigned char *buf, unsigned int len)
{
    int POSSIBLY_UNUSED size;
    unsigned int avail_size = 0;
    LOCK_APP_AUDIO_QUEUE();
//    merge_two_trace_to_one_track_16bits(0, (uint16_t *)buf, (uint16_t *)buf, len>>1);
//    r = APP_AUDIO_EnCQueue(&voicebtpcm_p2m_queue, buf, len>>1);
    avail_size = APP_AUDIO_AvailableOfCQueue(&voicebtpcm_p2m_queue);
    if (len <= avail_size)
    {
        APP_AUDIO_EnCQueue(&voicebtpcm_p2m_queue, buf, len);
    }
    else
    {
        VOICEBTPCM_TRACE( "mic buff overflow %d/%d", len, avail_size);
        APP_AUDIO_DeCQueue(&voicebtpcm_p2m_queue, 0, len - avail_size);
        APP_AUDIO_EnCQueue(&voicebtpcm_p2m_queue, buf, len);
    }
    size = APP_AUDIO_LengthOfCQueue(&voicebtpcm_p2m_queue);
    UNLOCK_APP_AUDIO_QUEUE();

    VOICEBTPCM_TRACE("p2m :%d/%d", len, size);

    return 0;
}

#if defined(HFP_1_6_ENABLE)
unsigned char get_msbc_counter(void)
{
    if (msbc_counter == 0x08)
    {
        msbc_counter = 0x38;
    }
    else if (msbc_counter == 0x38)
    {
        msbc_counter = 0xC8;
    }
    else if (msbc_counter == 0xC8)
    {
        msbc_counter = 0xF8;
    }
    else if (msbc_counter == 0xF8)
    {
        msbc_counter = 0x08;
    }

    return msbc_counter;
}
#endif


#ifdef SW_INSERT_MSBC_TX_OFFSET
#define  msbc_tx_offset (58)
static uint8_t bu=0;
static uint16_t bu_buff[msbc_tx_offset];
#endif

//used by capture, get the memory data which has be stored by store_voicebtpcm_p2m_buffer()
int get_voicebtpcm_p2m_frame(unsigned char *buf, unsigned int len)
{
    int r = 0, got_len = 0;
    unsigned char *e1 = NULL, *e2 = NULL;
    unsigned int len1 = 0, len2 = 0;

    // LOG_PRINT_VOICE_CALL_AUD("[%s] pcm_len = %d", __func__, len / 2);
    // Perhaps need to move this into for (int cnt=0; cnt<loop_cnt; cnt++)
    if (voicebtpcm_cache_p2m_status == APP_AUDIO_CACHE_CACHEING)
    {
        app_audio_memset_16bit((short *)buf, 0, len/2);
        LOG_PRINT_VOICE_CALL_AUD("[%s] APP_AUDIO_CACHE_CACHEING", __func__);
        return len;
    }

    int msbc_encode_temp_len = ENCODE_TEMP_PCM_LEN * sizeof(short);

    ASSERT(len % msbc_encode_temp_len == 0 , "[%s] len(%d) is invalid", __func__, len);

    int loop_cnt = len / msbc_encode_temp_len;
    len = msbc_encode_temp_len;

    for (int cnt=0; cnt<loop_cnt; cnt++)
    {
        got_len = 0;
        buf += msbc_encode_temp_len * cnt;
        if (bt_sco_codec_is_msbc())
        {
#ifdef HFP_1_6_ENABLE
            uint16_t bytes_encoded = 0, buf_len = len;
            uint16_t *dest_buf = 0, offset = 0;

            dest_buf = (uint16_t *)buf;

            if(need_init_encoder)
            {
                btif_sbc_init_encoder(msbc_encoder);
                msbc_encoder->streamInfo.mSbcFlag = 1;
                msbc_encoder->streamInfo.numChannels = 1;
                msbc_encoder->streamInfo.channelMode = BTIF_SBC_CHNL_MODE_MONO;

                msbc_encoder->streamInfo.bitPool     = 26;
                msbc_encoder->streamInfo.sampleFreq  = BTIF_SBC_CHNL_SAMPLE_FREQ_16;
                msbc_encoder->streamInfo.allocMethod = BTIF_SBC_ALLOC_METHOD_LOUDNESS;
                msbc_encoder->streamInfo.numBlocks   = BTIF_MSBC_BLOCKS;
                msbc_encoder->streamInfo.numSubBands = 8;
                need_init_encoder = 0;
            }
            LOCK_APP_AUDIO_QUEUE();
            r = APP_AUDIO_PeekCQueue(&voicebtpcm_p2m_queue, MSBC_ENCODE_PCM_LEN * sizeof(short), &e1, &len1, &e2, &len2);
            UNLOCK_APP_AUDIO_QUEUE();

            if(r == CQ_OK)
            {
                if (len1)
                {
                    memcpy(temp_msbc_buf, e1, len1);
                    LOCK_APP_AUDIO_QUEUE();
                    APP_AUDIO_DeCQueue(&voicebtpcm_p2m_queue, 0, len1);
                    UNLOCK_APP_AUDIO_QUEUE();
                    got_len += len1;
                }
                if (len2 != 0)
                {
                    memcpy(temp_msbc_buf+got_len, e2, len2);
                    got_len += len2;
                    LOCK_APP_AUDIO_QUEUE();
                    APP_AUDIO_DeCQueue(&voicebtpcm_p2m_queue, 0, len2);
                    UNLOCK_APP_AUDIO_QUEUE();
                }

                //int t = 0;
                //t = hal_sys_timer_get();
                msbc_encoder_pcmdata.data = temp_msbc_buf;
                msbc_encoder_pcmdata.dataLen = MSBC_ENCODE_PCM_LEN;
                memset(temp_msbc_buf1, 0, MSBC_ENCODE_PCM_LEN * sizeof(short));
                btif_sbc_encode_frames(msbc_encoder, &msbc_encoder_pcmdata, &bytes_encoded, temp_msbc_buf1, (uint16_t *)&buf_len, 0xFFFF);
                //LOG_PRINT_VOICE_CALL_AUD("enc msbc %d t\n", hal_sys_timer_get()-t);
                //LOG_PRINT_VOICE_CALL_AUD("encode len %d, out len %d\n", bytes_encoded, buf_len);

#if SW_INSERT_MSBC_TX_OFFSET //sw insert 58 samples
                if(bu ==0)
                {
                    memset((uint8_t *)dest_buf,0,msbc_tx_offset*2);
                    bu =1;
                }
                else
                {
                    memcpy((uint8_t *)dest_buf,(uint8_t *)bu_buff,msbc_tx_offset*2);
                }
                offset = msbc_tx_offset;
#endif

                dest_buf[offset++] = 1<<8;
                dest_buf[offset++] = get_msbc_counter()<<8;

                for (int i = 0; i < buf_len; ++i)
                {
                    dest_buf[offset++] = temp_msbc_buf1[i]<<8;
                }

                dest_buf[offset++] = 0; //padding

                msbc_encoder_pcmdata.data = temp_msbc_buf + MSBC_ENCODE_PCM_LEN;
                msbc_encoder_pcmdata.dataLen = MSBC_ENCODE_PCM_LEN;
                memset(temp_msbc_buf1, 0, MSBC_ENCODE_PCM_LEN * sizeof(short));
                btif_sbc_encode_frames(msbc_encoder, &msbc_encoder_pcmdata, &bytes_encoded, temp_msbc_buf1, (uint16_t *)&buf_len, 0xFFFF);
                //LOG_PRINT_VOICE_CALL_AUD("encode len %d, out len %d\n", bytes_encoded, buf_len);


                dest_buf[offset++] = 1<<8;
                dest_buf[offset++] = get_msbc_counter()<<8;

#if SW_INSERT_MSBC_TX_OFFSET //sw insert 58 samples
                for(int i = 0;i <(120-(msbc_tx_offset+2+1+2+buf_len));i++)
                {
                    dest_buf[offset++] = temp_msbc_buf1[i]<<8;
                }
#if 0
                for (int i = 0; i < buf_len-msbc_tx_offset; ++i) {
                    dest_buf[offset++] = temp_msbc_buf1[i]<<8;
                }
#endif
                uint8_t count = buf_len-i;
                for (int j =0; j < count; j++) {
                    bu_buff[j] = temp_msbc_buf1[i++]<<8;
                }
#else

                for (int i = 0; i < buf_len; ++i)
                {
                    dest_buf[offset++] = temp_msbc_buf1[i]<<8;
                }
                dest_buf[offset++] = 0; //padding
#endif
                got_len = len;
            }
        }
        else
#endif
        {
#if defined(_CVSD_BYPASS_)
            got_len = encode_cvsd_frame(buf, len);
#else
            LOCK_APP_AUDIO_QUEUE();
            //        size = APP_AUDIO_LengthOfCQueue(&voicebtpcm_p2m_queue);
            r = APP_AUDIO_PeekCQueue(&voicebtpcm_p2m_queue, len - got_len, &e1, &len1, &e2, &len2);
            UNLOCK_APP_AUDIO_QUEUE();

            //        VOICEBTPCM_TRACE("p2m :%d/%d", len, APP_AUDIO_LengthOfCQueue(&voicebtpcm_p2m_queue));

            if(r == CQ_OK)
            {
                if (len1)
                {
                    app_audio_memcpy_16bit((short *)buf, (short *)e1, len1/2);
                    LOCK_APP_AUDIO_QUEUE();
                    APP_AUDIO_DeCQueue(&voicebtpcm_p2m_queue, 0, len1);
                    UNLOCK_APP_AUDIO_QUEUE();
                    got_len += len1;
                }
                if (len2 != 0)
                {
                    app_audio_memcpy_16bit((short *)(buf+got_len), (short *)e2, len2/2);
                    got_len += len2;
                    LOCK_APP_AUDIO_QUEUE();
                    APP_AUDIO_DeCQueue(&voicebtpcm_p2m_queue, 0, len2);
                    UNLOCK_APP_AUDIO_QUEUE();
                }
            }
            else
            {
                VOICEBTPCM_TRACE( "mic buff underflow");
                app_audio_memset_16bit((short *)buf, 0, len/2);
                got_len = len;
                voicebtpcm_cache_p2m_status = APP_AUDIO_CACHE_CACHEING;
            }
#endif
        }
    }

    return got_len;
}

#if 0
void get_mic_data_max(short *buf, uint32_t len)
{
    int max0 = -32768, min0 = 32767, diff0 = 0;
    int max1 = -32768, min1 = 32767, diff1 = 0;

    for(uint32_t i=0; i<len/2; i+=2)
    {
        if(buf[i+0]>max0)
        {
            max0 = buf[i+0];
        }

        if(buf[i+0]<min0)
        {
            min0 = buf[i+0];
        }

        if(buf[i+1]>max1)
        {
            max1 = buf[i+1];
        }

        if(buf[i+1]<min1)
        {
            min1 = buf[i+1];
        }
    }
    LOG_PRINT_VOICE_CALL_AUD("min0 = %d, max0 = %d, diff0 = %d, min1 = %d, max1 = %d, diff1 = %d", min0, max0, max0 - min0, min1, max1, max1 - min1);
}
#endif

#ifdef __BT_ANC__
extern uint8_t bt_sco_samplerate_ratio;
#define US_COEF_NUM (24)

/*
8K -->48K
single phase coef Number:=24,    upsample factor:=6,
      64, -311,  210, -320,  363, -407,  410, -356,  196,  191,-1369,30721, 3861,-2265, 1680,-1321, 1049, -823,  625, -471,  297, -327,   34,   19,
   72, -254,   75, -117,   46,   58, -256,  583,-1141, 2197,-4866,28131,10285,-4611, 2935,-2061, 1489,-1075,  755, -523,  310, -291,  -19,   31,
   65, -175,  -73,   84, -245,  457, -786, 1276,-2040, 3377,-6428,23348,17094,-6200, 3592,-2347, 1588,-1073,  703, -447,  236, -203,  -92,   49,
   49,  -92, -203,  236, -447,  703,-1073, 1588,-2347, 3592,-6200,17094,23348,-6428, 3377,-2040, 1276, -786,  457, -245,   84,  -73, -175,   65,
   31,  -19, -291,  310, -523,  755,-1075, 1489,-2061, 2935,-4611,10285,28131,-4866, 2197,-1141,  583, -256,   58,   46, -117,   75, -254,   72,
   19,   34, -327,  297, -471,  625, -823, 1049,-1321, 1680,-2265, 3861,30721,-1369,  191,  196, -356,  410, -407,  363, -320,  210, -311,   64,
*/

const static short coef_8k_upto_48k[6][US_COEF_NUM]__attribute__((section(".sram_data")))  =
{
    {64, -311,  210, -320,  363, -407,  410, -356,  196,    191,-1369,30721, 3861,-2265, 1680,-1321, 1049, -823,  625, -471,  297, -327,   34,   19},
    {72, -254,  75, -117,   46,   58, -256,  583,-1141, 2197,-4866,28131,10285,-4611, 2935,-2061, 1489,-1075,  755, -523,  310, -291,  -19,   31   },
    {65, -175,  -73,   84, -245,  457, -786, 1276,-2040, 3377,-6428,23348,17094,-6200, 3592,-2347, 1588,-1073,  703, -447,  236, -203,  -92,   49  },
    {49,  -92, -203,  236, -447,  703,-1073, 1588,-2347, 3592,-6200,17094,23348,-6428, 3377,-2040, 1276, -786,  457, -245,  84,  -73, -175,   65  },
    {31,  -19, -291,  310, -523,  755,-1075, 1489,-2061, 2935,-4611,10285,28131,-4866, 2197,-1141,  583, -256,  58,   46, -117,   75, -254,   72  },
    {19,   34, -327,  297, -471,  625, -823, 1049,-1321, 1680,-2265, 3861,30721,-1369,  191,  196, -356,  410, -407,  363, -320,  210, -311,   64 }
};

/*
16K -->48K

single phase coef Number:=24,    upsample factor:=3,
       1, -291,  248, -327,  383, -405,  362, -212, -129,  875,-2948,29344, 7324,-3795, 2603,-1913, 1418,-1031,  722, -478,  292, -220,  -86,   16,
   26, -212,    6,   45, -185,  414, -764, 1290,-2099, 3470,-6431,20320,20320,-6431, 3470,-2099, 1290, -764,  414, -185,   45,    6, -212,   26,
   16,  -86, -220,  292, -478,  722,-1031, 1418,-1913, 2603,-3795, 7324,29344,-2948,  875, -129, -212,  362, -405,  383, -327,  248, -291,    1,
*/

const static short coef_16k_upto_48k[3][US_COEF_NUM]  __attribute__((section(".sram_data"))) =
{
    {1, -291,  248, -327,  383, -405,  362, -212, -129, 875,-2948,29344, 7324,-3795, 2603,-1913, 1418,-1031,  722, -478,  292, -220,  -86,   16},
    {26, -212,   6,   45, -185,  414, -764, 1290,-2099, 3470,-6431,20320,20320,-6431, 3470,-2099, 1290, -764,  414, -185,   45,    6, -212,   26},
    {16,  -86, -220,  292, -478,  722,-1031, 1418,-1913, 2603,-3795, 7324,29344,-2948,  875, -129, -212,  362, -405,  383, -327,  248, -291,    1}
};

static short us_para_lst[US_COEF_NUM-1];

static inline short us_get_coef_para(U32 samp_idx,U32 coef_idx)
{
    if(bt_sco_samplerate_ratio == 6)
        return coef_8k_upto_48k[samp_idx][coef_idx];
    else
        return coef_16k_upto_48k[samp_idx][coef_idx];
}

void us_fir_init (void)
{
    app_audio_memset_16bit(us_para_lst, 0, sizeof(us_para_lst)/sizeof(short));
}


__attribute__((section(".fast_text_sram"))) U32 us_fir_run (short* src_buf, short* dst_buf, U32 in_samp_num)
{
    U32 in_idx, samp_idx, coef_idx, real_idx, out_idx;
    int para, out;

    for (in_idx = 0, out_idx = 0; in_idx < in_samp_num; in_idx++)
    {
        for (samp_idx = 0; samp_idx < bt_sco_samplerate_ratio; samp_idx++)
        {
            out = 0;
            for (coef_idx = 0; coef_idx < US_COEF_NUM; coef_idx++)
            {
                real_idx = coef_idx + in_idx;
                para = (real_idx < (US_COEF_NUM-1))?us_para_lst[real_idx]:src_buf[real_idx - (US_COEF_NUM-1)];
                out += para * us_get_coef_para(samp_idx,coef_idx);
            }

            dst_buf[out_idx] = (short)(out>>16);
            out_idx++;
        }
    }

    if (in_samp_num >= (US_COEF_NUM-1))
    {
        app_audio_memcpy_16bit(us_para_lst,
                               (src_buf+in_samp_num-US_COEF_NUM+1),
                               (US_COEF_NUM-1));
    }
    else
    {
        U32 start_idx = (US_COEF_NUM-1-in_samp_num);

        app_audio_memcpy_16bit(us_para_lst,
                               (us_para_lst+in_samp_num),
                               start_idx);

        app_audio_memcpy_16bit((us_para_lst + start_idx),
                               src_buf,
                               in_samp_num);
    }
    return out_idx;
}

uint32_t voicebtpcm_pcm_resample (short* src_samp_buf, uint32_t src_smpl_cnt, short* dst_samp_buf)
{
    return us_fir_run (src_samp_buf, dst_samp_buf, src_smpl_cnt);
}
#endif

static int speech_tx_aec_frame_len = 0;

int speech_tx_aec_get_frame_len(void)
{
    return speech_tx_aec_frame_len;
}

void speech_tx_aec_set_frame_len(int len)
{
    LOG_PRINT_VOICE_CALL_AUD("[%s] len = %d", __func__, len);
    speech_tx_aec_frame_len = len;
}

#if 1
//used by capture, store data from mic to memory
uint32_t voicebtpcm_pcm_audio_data_come(uint8_t *buf, uint32_t len)
{
    int16_t POSSIBLY_UNUSED ret = 0;
    bool POSSIBLY_UNUSED vdt = false;
    int size = 0;

#if defined(SPEECH_TX_AEC) || defined(SPEECH_TX_AEC2) || defined(SPEECH_TX_AEC2FLOAT)
#if defined(__AUDIO_RESAMPLE__) && defined(SW_SCO_RESAMPLE)
    uint32_t queue_len;

    uint32_t len_per_channel = len / SPEECH_CODEC_CAPTURE_CHANNEL_NUM;

    ASSERT(len_per_channel == speech_tx_aec_get_frame_len() * sizeof(short), "%s: Unmatched len: %u != %u", __func__, len_per_channel, speech_tx_aec_get_frame_len() * sizeof(short));
    ASSERT(echo_buf_q_rpos + len_per_channel <= echo_buf_q_size, "%s: rpos (%u) overflow: len=%u size=%u", __func__, echo_buf_q_rpos, len_per_channel, echo_buf_q_size);

    if (echo_buf_q_rpos == echo_buf_q_wpos) {
        queue_len = echo_buf_q_full ? echo_buf_q_size : 0;
        echo_buf_q_full = false;
    } else if (echo_buf_q_rpos < echo_buf_q_wpos) {
        queue_len = echo_buf_q_wpos - echo_buf_q_rpos;
    } else {
        queue_len = echo_buf_q_size + echo_buf_q_wpos - echo_buf_q_rpos;
    }
    ASSERT(queue_len >= len_per_channel, "%s: queue underflow: q_len=%u len=%u rpos=%u wpos=%u size=%u",
        __func__, queue_len, len_per_channel, echo_buf_q_rpos, echo_buf_q_wpos, echo_buf_q_size);

    aec_echo_buf = (int16_t *)(echo_buf_queue + echo_buf_q_rpos);
    echo_buf_q_rpos += len_per_channel;
    if (echo_buf_q_rpos >= echo_buf_q_size) {
        echo_buf_q_rpos = 0;
    }
#endif
#endif

    short   *pcm_buf = (short*)buf;
    int     pcm_len = len / 2;

    speech_tx_process(pcm_buf, aec_echo_buf, &pcm_len);

#if defined(MSBC_8K_SAMPLE_RATE)
    if (bt_sco_codec_is_msbc() == true) {
        iir_resample_process(upsample_st, pcm_buf, upsample_buf_for_msbc, pcm_len);
        pcm_buf = upsample_buf_for_msbc;
        pcm_len <<= 1;
    }
#endif

    LOCK_APP_AUDIO_QUEUE();
    store_voicebtpcm_p2m_buffer((uint8_t *)pcm_buf, pcm_len * sizeof(short));
    size = APP_AUDIO_LengthOfCQueue(&voicebtpcm_p2m_queue);
    UNLOCK_APP_AUDIO_QUEUE();

    if (size > VOICEBTPCM_PCM_TEMP_BUFFER_SIZE)
    {
        voicebtpcm_cache_p2m_status = APP_AUDIO_CACHE_OK;
    }

    return pcm_len*2;
}
#else
//used by capture, store data from mic to memory
uint32_t voicebtpcm_pcm_audio_data_come(uint8_t *buf, uint32_t len)
{
    int16_t POSSIBLY_UNUSED ret = 0;
    bool POSSIBLY_UNUSED vdt = false;
    int size = 0;

    short *pcm_buf = (short*)buf;
    uint32_t pcm_len = len / 2;

    // LOG_PRINT_VOICE_CALL_AUD("[%s] pcm_len = %d", __func__, pcm_len);

    LOCK_APP_AUDIO_QUEUE();
    store_voicebtpcm_p2m_buffer((uint8_t *)pcm_buf, pcm_len*2);
    size = APP_AUDIO_LengthOfCQueue(&voicebtpcm_p2m_queue);
    UNLOCK_APP_AUDIO_QUEUE();

    if (size > VOICEBTPCM_PCM_TEMP_BUFFER_SIZE)
    {
        voicebtpcm_cache_p2m_status = APP_AUDIO_CACHE_OK;
    }

    return pcm_len*2;
}
#endif

//used by playback, play data from memory to speaker
uint32_t voicebtpcm_pcm_audio_more_data(uint8_t *buf, uint32_t len)
{
    uint32_t l = 0;

    //LOG_PRINT_VOICE_CALL_AUD("[%s] pcm_len = %d", __func__, len/2);

    if (voicebtpcm_cache_m2p_status == APP_AUDIO_CACHE_CACHEING)
    {
        app_audio_memset_16bit((short *)buf, 0, len/2);
        l = len;
    }
    else
    {
#if defined(SPEECH_TX_AEC) || defined(SPEECH_TX_AEC2) || defined(SPEECH_TX_AEC2FLOAT)
#if !(defined(__AUDIO_RESAMPLE__) && defined(SW_SCO_RESAMPLE))
        short *buf_p=(short *)buf;
        app_audio_memcpy_16bit(aec_echo_buf, buf_p, len/2);
#endif
#endif

        int decode_len = len;
        uint8_t *decode_buf = buf;
#if defined(MSBC_8K_SAMPLE_RATE)
        if (bt_sco_codec_is_msbc() == true)
        {
            decode_len = len * 2;
            decode_buf = (uint8_t *)downsample_buf_for_msbc;
        }
#endif

        // Just support Frame size = 240 bytes
        uint32_t decode_len_unit = 240;
        ASSERT(decode_len % decode_len_unit == 0, "[%s] ERROR: decode_len = %d", __func__, decode_len);

        for(uint32_t i = 0; i < decode_len; i += decode_len_unit)
        {
            decode_buf += i;

            if (bt_sco_codec_is_msbc() == true)
            {
#if defined(HFP_1_6_ENABLE)
                l = decode_msbc_frame(decode_buf, decode_len_unit);
#endif
            }
            else
            {
#if defined(_CVSD_BYPASS_)
                l = decode_cvsd_frame(decode_buf, decode_len_unit);
#else
                l = decode_voicebtpcm_m2p_frame(decode_buf, decode_len_unit);
#endif
            }

            if (l != decode_len_unit)
            {
                app_audio_memset_16bit((short *)decode_buf, 0, decode_len_unit/2);
                voicebtpcm_cache_m2p_status = APP_AUDIO_CACHE_CACHEING;
            }

            if (bt_sco_codec_is_msbc() == false)
            {
#if defined (SPEECH_RX_PLC)
                speech_plc_8000((PlcSt_8000 *)speech_plc,(short *)decode_buf,decode_len_unit);
#endif
            }
        } // for(int i = 0; i < decode_len; i+=decode_len_unit)
    }

#if defined(MSBC_8K_SAMPLE_RATE)
    // downsample_buf_for_msbc size is len * 2
    if (bt_sco_codec_is_msbc() == true)
    {
        iir_resample_process(downsample_st, downsample_buf_for_msbc, (int16_t *)buf, len);
    }
#endif

    short   * POSSIBLY_UNUSED pcm_buf = (short*)buf;
    int     POSSIBLY_UNUSED pcm_len = len / 2;

    speech_rx_process(pcm_buf, &pcm_len);

    buf = (uint8_t *)pcm_buf;
    len = pcm_len * sizeof(short);

    app_ring_merge_more_data(buf, len);

#if defined(SPEECH_TX_AEC) || defined(SPEECH_TX_AEC2) || defined(SPEECH_TX_AEC2FLOAT)
#if defined(__AUDIO_RESAMPLE__) && defined(SW_SCO_RESAMPLE)
    uint32_t queue_len;

    ASSERT(len == speech_tx_aec_get_frame_len() * sizeof(short), "%s: Unmatched len: %u != %u", __func__, len, speech_tx_aec_get_frame_len() * sizeof(short));
    ASSERT(echo_buf_q_wpos + len <= echo_buf_q_size, "%s: wpos (%u) overflow: len=%u size=%u", __func__, echo_buf_q_wpos, len, echo_buf_q_size);

    if (echo_buf_q_rpos == echo_buf_q_wpos) {
        queue_len = echo_buf_q_full ? echo_buf_q_size : 0;
    } else if (echo_buf_q_rpos < echo_buf_q_wpos) {
        queue_len = echo_buf_q_wpos - echo_buf_q_rpos;
    } else {
        queue_len = echo_buf_q_size + echo_buf_q_wpos - echo_buf_q_rpos;
    }
    ASSERT(queue_len + len <= echo_buf_q_size, "%s: queue overflow: q_len=%u len=%u rpos=%u wpos=%u size=%u",
        __func__, queue_len, len, echo_buf_q_rpos, echo_buf_q_wpos, echo_buf_q_size);

    app_audio_memcpy_16bit((int16_t *)(echo_buf_queue + echo_buf_q_wpos), (int16_t *)buf, len / 2);
    echo_buf_q_wpos += len;
    if (echo_buf_q_wpos >= echo_buf_q_size) {
        echo_buf_q_wpos = 0;
    }
    if (echo_buf_q_rpos == echo_buf_q_wpos) {
        echo_buf_q_full = true;
    }
#endif
#endif

    return l;
}

void *voicebtpcm_get_ext_buff(int size)
{
    uint8_t *pBuff = NULL;
    if (size % 4)
    {
        size = size + (4 - size % 4);
    }
    app_audio_mempool_get_buff(&pBuff, size);
    VOICEBTPCM_TRACE( "[%s] len:%d", __func__, size);
    return (void*)pBuff;
}

int voicebtpcm_pcm_echo_buf_queue_init(uint32_t size)
{
#if defined(SPEECH_TX_AEC) || defined(SPEECH_TX_AEC2) || defined(SPEECH_TX_AEC2FLOAT)
#if defined(__AUDIO_RESAMPLE__) && defined(SW_SCO_RESAMPLE)
    echo_buf_queue = (uint8_t *)voicebtpcm_get_ext_buff(size);
    echo_buf_q_size = size;
    echo_buf_q_wpos = 0;
    echo_buf_q_rpos = 0;
    echo_buf_q_full = false;
#endif
#endif
    return 0;
}

void voicebtpcm_pcm_echo_buf_queue_reset(void)
{
#if defined(SPEECH_TX_AEC) || defined(SPEECH_TX_AEC2) || defined(SPEECH_TX_AEC2FLOAT)
#if defined(__AUDIO_RESAMPLE__) && defined(SW_SCO_RESAMPLE)
    echo_buf_q_wpos = 0;
    echo_buf_q_rpos = 0;
    echo_buf_q_full = false;
#endif
#endif
}

void voicebtpcm_pcm_echo_buf_queue_deinit(void)
{
#if defined(SPEECH_TX_AEC) || defined(SPEECH_TX_AEC2) || defined(SPEECH_TX_AEC2FLOAT)
#if defined(__AUDIO_RESAMPLE__) && defined(SW_SCO_RESAMPLE)
    echo_buf_queue = NULL;
    echo_buf_q_size = 0;
    echo_buf_q_wpos = 0;
    echo_buf_q_rpos = 0;
    echo_buf_q_full = false;
#endif
#endif
}

extern enum AUD_SAMPRATE_T speech_codec_get_sample_rate(void);

int FLASH_TEXT_LOC voicebtpcm_pcm_audio_init(void)
{
    // Get system parameter
    int POSSIBLY_UNUSED tx_sample_rate;
    int POSSIBLY_UNUSED tx_frame_length;

    int POSSIBLY_UNUSED rx_sample_rate;
    int POSSIBLY_UNUSED rx_frame_length;

    uint8_t POSSIBLY_UNUSED *speech_buf = NULL;
    int POSSIBLY_UNUSED speech_len = 0;

    rx_sample_rate = tx_sample_rate = speech_codec_get_sample_rate();
    rx_frame_length = tx_frame_length = SPEECH_FRAME_MS_TO_LEN(tx_sample_rate, SPEECH_SCO_FRAME_MS);

    LOG_PRINT_VOICE_CALL_AUD("[%s] TX: sample rate = %d, frame len = %d", __func__, tx_sample_rate, tx_frame_length);
    LOG_PRINT_VOICE_CALL_AUD("[%s] RX: sample rate = %d, frame len = %d", __func__, rx_sample_rate, rx_frame_length);

    // init cqueue
    uint8_t *p2m_buff = NULL;
    uint8_t *m2p_buff = NULL;

    app_audio_mempool_get_buff(&p2m_buff, VOICEBTPCM_PCM_QUEUE_SIZE);
    app_audio_mempool_get_buff(&m2p_buff, VOICEBTPCM_PCM_QUEUE_SIZE);

    LOCK_APP_AUDIO_QUEUE();
    /* voicebtpcm_pcm queue*/
    APP_AUDIO_InitCQueue(&voicebtpcm_p2m_queue, VOICEBTPCM_PCM_QUEUE_SIZE, p2m_buff);
    APP_AUDIO_InitCQueue(&voicebtpcm_m2p_queue, VOICEBTPCM_PCM_QUEUE_SIZE, m2p_buff);
    memset(p2m_buff, 0, VOICEBTPCM_PCM_QUEUE_SIZE);
    memset(m2p_buff, 0, VOICEBTPCM_PCM_QUEUE_SIZE);
    UNLOCK_APP_AUDIO_QUEUE();

    voicebtpcm_cache_m2p_status = APP_AUDIO_CACHE_CACHEING;
    voicebtpcm_cache_p2m_status = APP_AUDIO_CACHE_CACHEING;

#if defined(HFP_1_6_ENABLE)
    const float EQLevel[25] =
    {
        0.0630957,  0.0794328, 0.1,       0.1258925, 0.1584893,
        0.1995262,  0.2511886, 0.3162278, 0.398107, 0.5011872,
        0.6309573,  0.794328, 1,         1.258925, 1.584893,
        1.995262,  2.5118864, 3.1622776, 3.9810717, 5.011872,
        6.309573,  7.943282, 10, 12.589254, 15.848932
    };//-12~12
    uint8_t i;

    for (i=0; i<sizeof(msbc_eq_band_gain)/sizeof(float); i++)
    {
        msbc_eq_band_gain[i] = EQLevel[cfg_aud_eq_sbc_band_settings[i]+12];
    }

    bt_sco_player_restart_requeset(false);

    msbc_input_valid = false;
    msbc_input_offset = 0;
    need_init_encoder = 1;
    msbc_counter = 0x08;

    if (bt_sco_codec_is_msbc())
    {
        need_init_decoder = true;
        app_audio_mempool_get_buff((uint8_t **)&msbc_encoder, sizeof(btif_sbc_encoder_t));
#if defined(MSBC_PLC_ENABLE)
        app_audio_mempool_get_buff(&msbc_buf_before_plc, MSBC_ENCODE_PCM_LEN);
#endif
        app_audio_mempool_get_buff(&msbc_buf_before_decode, MSBC_FRAME_SIZE);
        app_audio_mempool_get_buff(&temp_msbc_buf, MSBC_ENCODE_PCM_LEN * sizeof(short));
        app_audio_mempool_get_buff(&temp_msbc_buf1, MSBC_ENCODE_PCM_LEN * sizeof(short));
#if defined(MSBC_PLC_ENCODER)
        app_audio_mempool_get_buff((uint8_t **)&msbc_plc_encoder_buffer, MSBC_PLC_ENCODER_SIZE);
        app_audio_mempool_get_buff((uint8_t **)&msbc_plc_encoder_data, sizeof(btif_sbc_pcm_data_t));
        app_audio_mempool_get_buff((uint8_t **)&msbc_plc_output_pcmbuffer, (MSBC_PLC_ENCODER_BUFFER_SIZE-MSBC_PLC_ENCODER_BUFFER_OFFSET)*sizeof(short));
        app_audio_mempool_get_buff((uint8_t **)&msbc_plc_encoder_inbuffer, MSBC_PLC_ENCODER_BUFFER_SIZE*sizeof(short));
        app_audio_mempool_get_buff((uint8_t **)&msbc_plc_encoder, sizeof(btif_sbc_encoder_t));
        btif_sbc_init_encoder(msbc_plc_encoder);
        msbc_plc_encoder->streamInfo.mSbcFlag = 1;
        msbc_plc_encoder->streamInfo.numChannels = 1;
        msbc_plc_encoder->streamInfo.channelMode = BTIF_SBC_CHNL_MODE_MONO;
        msbc_plc_encoder->streamInfo.bitPool     = 26;
        msbc_plc_encoder->streamInfo.sampleFreq  = BTIF_SBC_CHNL_SAMPLE_FREQ_16;
        msbc_plc_encoder->streamInfo.allocMethod = BTIF_SBC_ALLOC_METHOD_LOUDNESS;
        msbc_plc_encoder->streamInfo.numBlocks   = BTIF_MSBC_BLOCKS;
        msbc_plc_encoder->streamInfo.numSubBands = 8;
#endif
    }
#endif

#if defined(_CVSD_BYPASS_)
    Pcm8k_CvsdInit();
#endif

#if defined(SPEECH_RX_PLC)
#if defined(HFP_1_6_ENABLE)
    if (bt_sco_codec_is_msbc())
    {
#ifdef MSBC_PCM_PLC_ENABLE
        speech_plc = (PlcSt_16000 *)speech_plc_16000_init(voicebtpcm_get_ext_buff);
#else
        // Add
#endif
    }
    else
#endif
    {
        speech_plc = (PlcSt_8000 *)speech_plc_8000_init(voicebtpcm_get_ext_buff);
    }
#endif

#if defined(MSBC_8K_SAMPLE_RATE)
    upsample_buf_for_msbc = (int16_t *)voicebtpcm_get_ext_buff(sizeof(int16_t) * tx_frame_length * 2);
    downsample_buf_for_msbc = (int16_t *)voicebtpcm_get_ext_buff(sizeof(int16_t) * tx_frame_length * 2);
#endif

    speech_len = app_audio_mempool_free_buff_size() - APP_BT_STREAM_USE_BUF_SIZE;
    speech_buf = (uint8_t *)voicebtpcm_get_ext_buff(speech_len);
    speech_init(tx_sample_rate, rx_sample_rate, SPEECH_PROCESS_FRAME_MS, speech_buf, speech_len);

#if defined(MSBC_8K_SAMPLE_RATE)
    // Resample state must be created after speech init, as it uses speech heap
    upsample_st = iir_resample_init(tx_frame_length, IIR_RESAMPLE_MODE_1TO2);
    downsample_st = iir_resample_init(rx_frame_length * 2, IIR_RESAMPLE_MODE_2TO1);
#endif

    return 0;
}

int FLASH_TEXT_LOC voicebtpcm_pcm_audio_deinit(void)
{
    LOG_PRINT_VOICE_CALL_AUD("[%s] Close...", __func__);
    // LOG_PRINT_VOICE_CALL_AUD("[%s] app audio buffer free = %d", __func__, app_audio_mempool_free_buff_size());

#if defined(MSBC_8K_SAMPLE_RATE)
    iir_resample_destroy(upsample_st);
    iir_resample_destroy(downsample_st);
#endif

    speech_deinit();

    voicebtpcm_pcm_echo_buf_queue_deinit();

    // LOG_PRINT_VOICE_CALL_AUD("Free buf = %d", app_audio_mempool_free_buff_size());

    return 0;
}

