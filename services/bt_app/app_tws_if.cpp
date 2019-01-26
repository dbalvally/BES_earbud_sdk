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
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "cmsis_os.h"
#include "hal_uart.h"
#include "hal_timer.h"
#include "hwtimer_list.h"
#include "lockcqueue.h"
#include "hal_trace.h"
#include "hal_cmu.h"
#include "analog.h"
#include "bt_drv.h"
#include "bt_drv_interface.h"
#include "hal_codec.h"
#include "app_audio.h"
#include "audioflinger.h"
#include "nvrecord.h"
#include "app_key.h"
#include "app_utils.h"
#include "app_overlay.h"
#include "nvrecord.h"
#include "nvrecord_env.h"
#include "hal_location.h"

#include "bt_drv_reg_op.h"

#include "app_tws.h"


#ifdef __TWS__

#include "rtos.h"
#include "besbt.h"
#include "cqueue.h"
#include "btapp.h"
#include "apps.h"
#include "resources.h"
#include "app_bt_media_manager.h"
#include "tgt_hardware.h"
#include "app_tws.h"
#include "app_bt.h"
#include "app_bt_conn_mgr.h"
#include "hal_chipid.h"

extern  tws_dev_t  tws;
extern struct BT_DEVICE_T  app_bt_device;


#if defined(SLAVE_USE_ENC_SINGLE_SBC) && defined(SLAVE_USE_ENC_SINGLE_SBC_ENQUEUE_ENCODE)
static uint8_t tws_encode_queue_buffer[SBCTRANSMITSZ+MASTER_DECODE_PCM_FRAME_LENGTH];
#endif

#define __TWS_SHARE_BUFF__

#ifdef __TWS_SHARE_BUFF__


//static uint8_t tws_pcm_buffer[PCM_BUFFER_SZ_TWS];
//static uint8_t tws_sbc_buffer[TRANS_BUFFER_SZ_TWS];
//static uint8_t tws_a2dp_sbc_buffer[A2DP_BUFFER_SZ_TWS];

//uint8_t tws_big_buff[TWS_BUFFER_TOTAL_SIZE];

unsigned char *sbc_transmit_buffer=NULL;
unsigned char *sbc_packet_buffer[SBCBANK_PACK_NUM] = {NULL, NULL};
#if defined(SLAVE_USE_ENC_SINGLE_SBC) && !defined(SBC_TRANS_TO_AAC)
unsigned char *sbsamples_buf=NULL;

#endif

#else

static uint8_t tws_pcm_buffer[PCM_BUFFER_SZ_TWS];
static uint8_t tws_sbc_buffer[TRANS_BUFFER_SZ_TWS];
static uint8_t tws_a2dp_sbc_buffer[A2DP_BUFFER_SZ_TWS];
unsigned char sbc_transmit_buffer[SBCTRANSMITSZ];

char sbc_packet_buffer[SBCTRANSMITSZ];
#if defined(SLAVE_USE_ENC_SINGLE_SBC) && !defined(SBC_TRANS_TO_AAC)
unsigned char sbsamples_buf[SBCTRANSMITSZ];
#endif
#endif

#if defined(SLAVE_USE_ENC_SINGLE_SBC)
tws_slave_SbcStreamInfo_t g_app_tws_slave_streaminfo = {
    TRANS_BTA_AV_CO_SBC_MAX_BITPOOL,
    BTIF_SBC_ALLOC_METHOD_SNR
};
#endif

#if defined(SLAVE_USE_ENC_SINGLE_SBC)

extern btif_sbc_encoder_t sbc_encoder;

void app_tws_adjust_encode_bitpool(uint8_t updown)
{
#if defined(TWS_ADJUST_SBC_ENCODE_BITPOOL)
    uint32_t   lock = int_lock();

    if(updown == 1) ///up
    {
        sbc_encoder.streamInfo.bitPool = TRANS_BTA_AV_CO_SBC_MAX_BITPOOL;
        g_app_tws_slave_streaminfo.bitPool = TRANS_BTA_AV_CO_SBC_MAX_BITPOOL;
    }
    else  //down
    {
        sbc_encoder.streamInfo.bitPool = 10;
        g_app_tws_slave_streaminfo.bitPool = 10;
    }
    int_unlock(lock);
    
    LOG_PRINT_TWS("app_tws_adjust_encode_bitpool BITPOOL=%d,%d",sbc_encoder.streamInfo.bitPool,g_app_tws_slave_streaminfo.bitPool);
#endif    
}

uint32_t music_cadun_count=0;
uint32_t first_cadun_tick=0;
uint32_t ticksOfTheLastStuttering;

#if defined(TWS_ADJUST_SBC_ENCODE_BITPOOL)
static uint8_t stuttering_handling_in_progress = 0;
static uint8_t isLowBitPoolForStutteringExisting = false;
#endif

void app_tws_reset_cadun_count(void)
{
#if defined(TWS_ADJUST_SBC_ENCODE_BITPOOL)
    LOG_PRINT_TWS("%s bitpool reset", __func__);
    stuttering_handling_in_progress = 0;
    isLowBitPoolForStutteringExisting = 0;
    music_cadun_count = 0;
    sbc_encoder.streamInfo.bitPool = TRANS_BTA_AV_CO_SBC_MAX_BITPOOL;
    g_app_tws_slave_streaminfo.bitPool = TRANS_BTA_AV_CO_SBC_MAX_BITPOOL;
#endif
}   

void app_tws_check_to_restore_from_stuttering(void)
{
#if defined(TWS_ADJUST_SBC_ENCODE_BITPOOL)
    if (stuttering_handling_in_progress || isLowBitPoolForStutteringExisting)
    {
        uint32_t curr_tick = hal_sys_timer_get();
        uint32_t msSinceLastStuterring = 0;
        if (curr_tick > ticksOfTheLastStuttering)
        {
            msSinceLastStuterring = TICKS_TO_MS(curr_tick - ticksOfTheLastStuttering);
        }
        else
        {
            msSinceLastStuterring = 
                TICKS_TO_MS((hal_sys_timer_get_max() - ticksOfTheLastStuttering + 1) + curr_tick);
        }

        // if no stuttering since the latest stuttering for 10s, need to raise the bitpool
        if (msSinceLastStuterring > 10000)
        {
            stuttering_handling_in_progress = 0;

            if (g_app_tws_slave_streaminfo.bitPool == 2)
            {
                LOG_PRINT_TWS("%s bitpool adjust 2->10", __func__);
                app_tws_adjust_encode_bitpool(0);
            }
            else if (g_app_tws_slave_streaminfo.bitPool == 10)
            {
                LOG_PRINT_TWS("%s bitpool adjust 10->25", __func__);
                app_tws_reset_cadun_count();
            }
        }
    }
#endif
}

void app_tws_kadun_count(void)
{
#if defined(TWS_ADJUST_SBC_ENCODE_BITPOOL)
    uint32_t curr_tick = hal_sys_timer_get();   
    ticksOfTheLastStuttering = curr_tick;
    
    LOG_PRINT_TWS("app_tws_kadun_count");
    if (0 == stuttering_handling_in_progress)
    {
        first_cadun_tick = curr_tick;
        stuttering_handling_in_progress = 1;
    }

    uint32_t msSinceFirstStuterring = 0;
    if (curr_tick > first_cadun_tick)
    {
        msSinceFirstStuterring = TICKS_TO_MS(curr_tick-first_cadun_tick);
    }
    else
    {
        msSinceFirstStuterring = 
            TICKS_TO_MS((hal_sys_timer_get_max() - first_cadun_tick + 1) + curr_tick);
    }

    // within 10s, ten times of stuttering happens, need to lower the bitpool
    if(msSinceFirstStuterring < 20000)
    {
        music_cadun_count++;
        if(music_cadun_count >= 30)
        {
            music_cadun_count = 0;
            stuttering_handling_in_progress = 0;
            isLowBitPoolForStutteringExisting = 1;
            if (g_app_tws_slave_streaminfo.bitPool == TRANS_BTA_AV_CO_SBC_MAX_BITPOOL)
            {
                LOG_PRINT_TWS("%s bitpool adjust 25->10", __func__);
                app_tws_adjust_encode_bitpool(0);
            }
            else if (g_app_tws_slave_streaminfo.bitPool == 10)
            {
                LOG_PRINT_TWS("%s bitpool adjust 10->2", __func__);
                sbc_encoder.streamInfo.bitPool = 2;
                g_app_tws_slave_streaminfo.bitPool = 2;
            }
        }
    }
    else
    {
        if (music_cadun_count > 0)
        {
            music_cadun_count--;
        }
        stuttering_handling_in_progress = 0;
    }
#endif
}
#endif

static float sbc_eq_band_gain[CFG_HW_AUD_EQ_NUM_BANDS];

tws_dev_t* app_tws_get_env_pointer(void)
{
    return &tws;
}

void app_tws_set_conn_state(TWS_CONN_STATE_E state)
{
    tws.tws_conn_state = state;
}

uint8_t app_tws_get_mode(void)
{
    return tws_mode_ptr()->mode;
}

bool app_tws_is_master_mode(void)
{
    return (app_tws_get_mode() == TWSMASTER)?true:false;
}

bool app_tws_is_slave_mode(void)
{
    return (app_tws_get_mode() == TWSSLAVE)?true:false;
}

bool app_tws_is_freeman_mode(void)
{
    return (app_tws_get_mode() == TWSFREE)?true:false;
}

void app_tws_set_sink_stream(a2dp_stream_t *stream)
{
    tws.tws_sink.stream = stream;
}

a2dp_stream_t* app_tws_get_sink_stream(void)
{
    return tws.tws_sink.stream;
}

a2dp_stream_t* app_tws_get_mobile_sink_stream(void)
{
    return tws.mobile_sink.stream;
}

extern "C" char app_tws_mode_is_master(void)
{
    return (tws.tws_conn_state == TWS_MASTER_CONN_SLAVE);
}

extern "C" char app_tws_mode_is_slave(void)
{
    return (tws.tws_conn_state == TWS_SLAVE_CONN_MASTER);
}

char app_tws_mode_is_only_mobile(void)
{
    return (tws.tws_conn_state == TWS_MASTER_CONN_MOBILE_ONLY);
}

extern "C" uint16_t app_tws_get_tws_conhdl(void)
{
    return tws.tws_conhdl;
}

int8_t app_tws_get_tws_hfp_vol(void)
{
    return tws.hfp_volume;
}


void app_tws_set_tws_conhdl(uint16_t conhdl)
{
     tws.tws_conhdl = conhdl;
}

btif_avrcp_chnl_handle_t app_tws_get_avrcp_TGchannel(void)
{
    return tws.rcp;
}

a2dp_stream_t *app_tws_get_tws_source_stream(void)
{
    return tws.tws_source.stream;
}

a2dp_stream_t *app_tws_get_tws_sink_stream(void)
{
    return tws.tws_sink.stream;
}

bool app_tws_get_source_stream_connect_status(void)
{
    return tws.tws_source.connected;
}

bool app_tws_get_sink_stream_connect_status(void)
{
    return tws.tws_sink.connected;
}

uint8_t *app_tws_get_peer_bdaddr(void)
{
    return tws.peer_addr.address;
}

void app_tws_set_have_peer(bool is_have)
{
    tws.has_tws_peer = is_have;
}

void app_tws_set_media_suspend(bool enable)
{
    tws.media_suspend =enable;
}

int app_tws_get_a2dpbuff_available(void)
{
    return tws.a2dpbuff.available(&(tws.a2dpbuff));
}

void app_tws_set_eq_band_settings(int8_t *eq_band_settings)
{
    uint8_t i;
    const float EQLevel[25] = {
        0.0630957,  0.0794328, 0.1,       0.1258925, 0.1584893,
        0.1995262,  0.2511886, 0.3162278, 0.398107 , 0.5011872,
        0.6309573,  0.794328 , 1,         1.258925 , 1.584893 ,
        1.995262 ,  2.5118864, 3.1622776, 3.9810717, 5.011872 ,
        6.309573 ,  7.943282 , 10       , 12.589254, 15.848932
    };//-12~12
    for (i=0; i<sizeof(sbc_eq_band_gain)/sizeof(float); i++){
        sbc_eq_band_gain[i] = EQLevel[eq_band_settings[i]+12];
    }
}

void app_tws_get_eq_band_gain(float **eq_band_gain)
{
    *eq_band_gain = sbc_eq_band_gain;
}

void app_tws_get_eq_band_init(void)
{
    app_tws_set_eq_band_settings(cfg_hw_aud_eq_band_settings);
}


float FRAM_TEXT_LOC app_tws_sample_rate_hacker(uint32_t sample_rate)
{
    float tws_shift_level = 0;
    float sample =sample_rate;

#if defined(A2DP_LHDC_ON)
    if(tws.mobile_codectype == BTIF_AVDTP_CODEC_TYPE_LHDC)
    {
        //TRACE("tws sample rate level sbc");        
#ifdef __AUDIO_RESAMPLE__
        if(sample_rate == 44100)
        {
            tws_shift_level = A2DP_SMAPLE_RATE_44100_SHIFT_LEVEL;
        }
        else if(sample_rate == 48000)
        {
            tws_shift_level = A2DP_SMAPLE_RATE_48000_SHIFT_LEVEL;
        }
        else if(sample_rate == 96000)
        {
            tws_shift_level = A2DP_SMAPLE_RATE_48000_SHIFT_LEVEL;
        }
        else
        {
            ASSERT(0,"ERROR SAMPLE RATE! type:%d sample_rate:%d", tws.mobile_codectype, sample_rate);
        }
#else
        tws_shift_level = A2DP_SMAPLE_RATE_SHIFT_LEVEL;
#endif
    }else
#endif
#if defined(A2DP_AAC_ON)
    if(tws.mobile_codectype == BTIF_AVDTP_CODEC_TYPE_MPEG2_4_AAC)
    {
       // TRACE("tws sample rate level aac");            
#ifdef __AUDIO_RESAMPLE__
        if(sample_rate == 44100)
        {
            tws_shift_level = AAC_SMAPLE_RATE_44100_SHIFT_LEVEL;
        }
        else if(sample_rate == 48000)
        {
            tws_shift_level = AAC_SMAPLE_RATE_48000_SHIFT_LEVEL;
        }
        else
        {
            ASSERT(0,"ERROR SAMPLE RATE! type:%d sample_rate:%d", tws.mobile_codectype, sample_rate);
        }

#else
        tws_shift_level = AAC_SMAPLE_RATE_SHIFT_LEVEL;
#endif
    }else
#endif
    
    if(tws.mobile_codectype == BTIF_AVDTP_CODEC_TYPE_SBC)
    {
        //TRACE("tws sample rate level sbc");        
#ifdef __AUDIO_RESAMPLE__
        if(sample_rate == 44100)
        {

#if  !defined(SW_PLAYBACK_RESAMPLE) && defined(__TWS_RESAMPLE_ADJUST__)
            if(hal_get_chip_metal_id() >= HAL_CHIP_METAL_ID_2)
            {
                tws_shift_level = A2DP_SMAPLE_RATE_44100_SHIFT_LEVEL_C;
            }
            else
#endif     
            {
                tws_shift_level = A2DP_SMAPLE_RATE_44100_SHIFT_LEVEL;
            }
        }
        else if(sample_rate == 48000)
        {
#if  !defined(SW_PLAYBACK_RESAMPLE) && defined(__TWS_RESAMPLE_ADJUST__)
            if(hal_get_chip_metal_id() >= HAL_CHIP_METAL_ID_2)
            {
                tws_shift_level = A2DP_SMAPLE_RATE_48000_SHIFT_LEVEL_C;
            }
            else
#endif   
            {
                tws_shift_level = A2DP_SMAPLE_RATE_48000_SHIFT_LEVEL;
            }
        }
        else
        {
            ASSERT(0,"ERROR SAMPLE RATE! type:%d sample_rate:%d", tws.mobile_codectype, sample_rate);
        }
#else
        tws_shift_level = A2DP_SMAPLE_RATE_SHIFT_LEVEL;
#endif
    }

    sample -= (sample*tws_shift_level*5/1000);
    return sample;
}

uint8_t app_tws_get_codec_type(void)
{
    return (uint8_t)tws.mobile_codectype;
}


extern uint16_t media_seqnum;
extern uint32_t media_timestamp;
void app_tws_reset_slave_medianum(void)
{
    media_seqnum = 0;
    media_timestamp = 0;
}


uint16_t app_tws_get_slave_medianum(void)
{
    return media_seqnum;
}


a2dp_stream_t *app_tws_get_main_sink_stream(void)
{
    return app_bt_device.a2dp_stream[BT_DEVICE_ID_1]->a2dp_stream; 
}

trans_conf_t tws_trans_conf;

 btif_a2dp_sbc_packet_t tws_sbcPacket;





sbcbank_t  sbcbank;

sbcpack_t *get_sbcPacket(void)
{
   int index = sbcbank.free;
   sbcbank.free +=1;
   if(sbcbank.free == SBCBANK_PACK_NUM) {
      sbcbank.free = 0;
   }
   sbcbank.sbcpacks[index].buffer = (char *)sbc_packet_buffer[index];
   return &(sbcbank.sbcpacks[index]);
}



#if defined(SLAVE_USE_OPUS) || defined(MASTER_USE_OPUS) || defined(ALL_USE_OPUS)
char opus_temp_buffer[4096];
#endif
 float *sbc_eq_band_gain_p;


static void tws_sbc_init(void)
{
    app_tws_get_eq_band_init();
    app_tws_get_eq_band_gain(&sbc_eq_band_gain_p);
    //set config for sbc trans process
    tws_trans_conf.transesz = SBCTRANSMITSZ;
    tws_trans_conf.framesz = SBCTRANFRAMESZ;
    tws_trans_conf.discardframes = TRANSDISCARDSFRAMES;
#if defined(SLAVE_USE_OPUS) || defined(MASTER_USE_OPUS) || defined(ALL_USE_OPUS)
    tws_trans_conf.opus_temp_buff = opus_temp_buffer;
#endif
}


extern "C"  void app_tws_env_init(tws_env_cfg_t *cfg)
{    

    tws_sbc_init();

#if defined(SLAVE_USE_ENC_SINGLE_SBC) && defined(SLAVE_USE_ENC_SINGLE_SBC_ENQUEUE_ENCODE)
    cfg->env_encode_queue_buff.buff = tws_encode_queue_buffer;
    cfg->env_encode_queue_buff.size = SBCTRANSMITSZ+MASTER_DECODE_PCM_FRAME_LENGTH;
#else
    cfg->env_encode_queue_buff.buff = 0;
    cfg->env_encode_queue_buff.size = 0;
#endif
#ifdef __TWS_SHARE_BUFF__
    cfg->env_sbsample_buff.buff = 0;
    cfg->env_sbsample_buff.size = 0;

    cfg->env_pcm_buff.buff = 0;
    cfg->env_pcm_buff.size = 0;

    cfg->env_sbc_buff.buff = 0;
    cfg->env_sbc_buff.size = 0;

    cfg->env_a2dp_buff.buff = 0;
    cfg->env_a2dp_buff.size = 0;
#else

#if defined(SLAVE_USE_ENC_SINGLE_SBC)
    cfg->env_sbsample_buff.buff = sbsamples_buf;
    cfg->env_sbsample_buff.size = SBCTRANSMITSZ;
#else
    cfg->env_sbsample_buff.buff = 0;
    cfg->env_sbsample_buff.size = 0;

#endif
    cfg->env_pcm_buff.buff = tws_pcm_buffer;
    cfg->env_pcm_buff.size = PCM_BUFFER_SZ_TWS;

    cfg->env_sbc_buff.buff = tws_sbc_buffer;
    cfg->env_sbc_buff.size = TRANS_BUFFER_SZ_TWS;

    cfg->env_a2dp_buff.buff = tws_a2dp_sbc_buffer;
    cfg->env_a2dp_buff.size = A2DP_BUFFER_SZ_TWS;
#endif
    cfg->env_trigger_delay = TWS_PLAYER_TRIGGER_DELAY;
    cfg->env_trigger_delay_lhdc = TWS_PLAYER_TRIGGER_DELAY_LHDC;
    cfg->env_trigger_a2dp_buff_thr = A2DP_BUFFER_SZ_TWS/8;
    cfg->env_trigger_aac_buff_thr = 5;
    cfg->env_trigger_lhdc_buff_thr = 5;

    cfg->env_slave_trigger_thr = TWS_PLAYER__SLAVE_TRIGGER_THR;

#ifdef __LEFT_SIDE__
    cfg->earside = TWS_EARPHONE_LEFT_SIDE;
#else
    cfg->earside = TWS_EARPHONE_RIGHT_SIDE;
#endif

    cfg->tws_player_stopped_func = conn_tws_player_stopped_handler;
}

uint8_t *tws_pcm_buff=NULL;
uint8_t *tws_sbc_buff=NULL;
extern "C" void app_tws_buff_alloc(void)
{
    LOG_PRINT_TWS("%s entry",__FUNCTION__);

#ifdef __TWS_SHARE_BUFF__
	tws_env_cfg_t *cfg = app_tws_get_cfg();
	cfg->env_pcm_buff.size = PCM_BUFFER_SZ_TWS;
	cfg->env_a2dp_buff.size = A2DP_BUFFER_SZ_TWS;
	cfg->env_sbc_buff.size = TRANS_BUFFER_SZ_TWS;
	app_audio_mempool_get_buff(&(cfg->env_pcm_buff.buff), cfg->env_pcm_buff.size);
	app_audio_mempool_get_buff(&(cfg->env_a2dp_buff.buff), cfg->env_a2dp_buff.size);
	app_audio_mempool_get_buff(&(cfg->env_sbc_buff.buff), cfg->env_sbc_buff.size);
    for (uint8_t i=0; i<SBCBANK_PACK_NUM; i++){
        if (sbc_packet_buffer[i] != NULL)
        {
            LOG_PRINT_TWS("sbc_packet_buffer %d IS NOT FREE SOMTHING IS WRONG", i);
            break;
        }
    }

    if (sbc_transmit_buffer != NULL)
    {
        LOG_PRINT_TWS("BUFF IS NOT FREE SOMTHING IS WRONG");
    }
#if defined(SLAVE_USE_ENC_SINGLE_SBC) && !defined(SBC_TRANS_TO_AAC)
    
    if(sbsamples_buf !=NULL)
    {
        LOG_PRINT_TWS("BUFF2 IS NOT FREE SOMTHING IS WRONG");
    }
#endif    
    int lock = int_lock();
    if(sbc_transmit_buffer == NULL)
        app_audio_mempool_get_buff(&sbc_transmit_buffer, SBCTRANSMITSZ);
    for (uint8_t i=0; i<SBCBANK_PACK_NUM; i++){
        if(sbc_packet_buffer[i] == NULL)
            app_audio_mempool_get_buff(&sbc_packet_buffer[i], 1024);
    }
#if defined(SLAVE_USE_ENC_SINGLE_SBC) && !defined(SBC_TRANS_TO_AAC)
    if(sbsamples_buf == NULL)
        app_audio_mempool_get_buff(&sbsamples_buf, 512*12);
    cfg->env_sbsample_buff.buff = sbsamples_buf;
    cfg->env_sbsample_buff.size = SBCTRANSMITSZ;    
#endif


    int_unlock(lock);

#endif

//    cfg->env_sbsample_buff.buff = sbsamples_buf;
//    cfg->env_sbsample_buff.size = SBCTRANSMITSZ;

    

    LOG_PRINT_TWS("%s exit",__FUNCTION__);

}

extern "C" void app_tws_buff_free(void)
{
    LOG_PRINT_TWS("%s entry",__FUNCTION__);

#ifdef __TWS_SHARE_BUFF__
#if defined(SLAVE_USE_ENC_SINGLE_SBC) && !defined(SBC_TRANS_TO_AAC)
tws_env_cfg_t *cfg = app_tws_get_cfg();
#endif
    int lock = int_lock();
    sbc_transmit_buffer = NULL;
    for (uint8_t i=0; i<SBCBANK_PACK_NUM; i++){
        sbc_packet_buffer[i] = NULL;
    }
#if defined(SLAVE_USE_ENC_SINGLE_SBC) && !defined(SBC_TRANS_TO_AAC)
    sbsamples_buf = NULL;
    cfg->env_sbsample_buff.buff = 0;
    cfg->env_sbsample_buff.size = 0;
    
#endif
    int_unlock(lock);

#endif
    
    LOG_PRINT_TWS("%s exit",__FUNCTION__);
    
}


extern "C" uint8_t *app_tws_get_transmitter_buff(void)
{
    return sbc_transmit_buffer;
}

uint8_t pcm_wait_triggle=0;

extern "C" void app_tws_set_pcm_wait_triggle(uint8_t triggle_en)
{
    pcm_wait_triggle = triggle_en;
}

extern "C" uint8_t app_tws_get_pcm_wait_triggle(void)
{
    return pcm_wait_triggle;
}


#ifdef __TWS_CALL_DUAL_CHANNEL__


uint8_t btpcm_wait_triggle=0;

extern "C" void app_tws_set_btpcm_wait_triggle(uint8_t triggle_en)
{
    btpcm_wait_triggle = triggle_en;
}

extern "C" uint8_t app_tws_get_btpcm_wait_triggle(void)
{
    return btpcm_wait_triggle;
}

extern "C" void app_tws_set_pcm_triggle(void)
{
    uint32_t curr_ticks;
    uint32_t triggler_time;
    if(app_tws_mode_is_master() || app_tws_mode_is_slave())
    {
        curr_ticks = bt_syn_get_curr_ticks(app_tws_get_tws_conhdl());
    }
    else
    {
        curr_ticks = btdrv_syn_get_curr_ticks();
    }
    triggler_time = (curr_ticks+0x180)-((curr_ticks+0x180)%192);
    btdrv_syn_trigger_codec_en(0);
    btdrv_syn_clr_trigger();
    
    btdrv_enable_playback_triggler(SCO_TRIGGLE_MODE);
    if(app_tws_mode_is_master() || app_tws_mode_is_slave())
    {
        bt_syn_set_tg_ticks(triggler_time,app_tws_get_tws_conhdl());
    }
    else
    {
        bt_syn_set_tg_ticks(triggler_time,0);
    }
    
    btdrv_syn_trigger_codec_en(1);    
    LOG_PRINT_TWS("enable pcm triggler curr clk =%x, triggle clk=%x,bt clk=%x",btdrv_syn_get_curr_ticks(),triggler_time,bt_syn_get_curr_ticks(app_tws_get_tws_conhdl()));
    bt_drv_reg_op_trigger_time_checker();
}
#endif



uint32_t sco_active_bak;
extern "C" void FRAM_TEXT_LOC app_tws_set_trigger_time(uint32_t trigger_time, bool play_only)
{
    uint32_t curr_ticks;

    if(app_tws_mode_is_master() || app_tws_mode_is_slave())
    {
        curr_ticks = bt_syn_get_curr_ticks(app_tws_get_tws_conhdl());
    }
    else
    {
        curr_ticks = btdrv_syn_get_curr_ticks();
    }
    LOG_PRINT_TWS("[%s] trigger curr(%d)-->tg(%d)", __func__, curr_ticks, trigger_time);

    if (trigger_time && trigger_time != 1)
    {
        btdrv_syn_trigger_codec_en(0);
#ifndef FPGA
        af_codec_sync_config(AUD_STREAM_PLAYBACK, AF_CODEC_SYNC_TYPE_BT, false);
        if (!play_only) {
            af_codec_sync_config(AUD_STREAM_CAPTURE, AF_CODEC_SYNC_TYPE_BT, false);
        }
#endif

        btdrv_syn_clr_trigger();

        btdrv_enable_playback_triggler(ACL_TRIGGLE_MODE);

#ifdef FPGA
        trigger_time = curr_ticks + 0x200;
#endif

#ifdef CHIP_BEST2300
        if(app_tws_mode_is_slave())
        {
            if(em_bt_bitoff_getf(app_tws_get_tws_conhdl()-0x80) >=0 && 
                em_bt_bitoff_getf(app_tws_get_tws_conhdl()-0x80)<=14)
            {
                trigger_time -=2;
            }
        }
#endif

        if(app_tws_mode_is_master() || app_tws_mode_is_slave())
        {
            bt_syn_set_tg_ticks(trigger_time, app_tws_get_tws_conhdl());
        }
        else
        {
            bt_syn_set_tg_ticks(trigger_time, 0);
        }

        btdrv_syn_trigger_codec_en(1);
#ifndef FPGA
        af_codec_sync_config(AUD_STREAM_PLAYBACK, AF_CODEC_SYNC_TYPE_BT, true);
        if (!play_only) {
            af_codec_sync_config(AUD_STREAM_CAPTURE, AF_CODEC_SYNC_TYPE_BT, true);
        }
#endif
    }
    else if (trigger_time == 1)
    {

        if(app_tws_mode_is_master() || app_tws_mode_is_slave())
        {
            curr_ticks = bt_syn_get_curr_ticks(app_tws_get_tws_conhdl());
        }
        else
        {
            curr_ticks = btdrv_syn_get_curr_ticks();
        }

        trigger_time = (curr_ticks+0x180) - ((curr_ticks+0x180)%192);
#ifdef CHIP_BEST2300
        if(app_tws_mode_is_slave())
        {
            if(em_bt_bitoff_getf(app_tws_get_tws_conhdl()-0x80) >=0 && 
                em_bt_bitoff_getf(app_tws_get_tws_conhdl()-0x80)<=14)
            {
                trigger_time -=2;
            }
        }
#endif
        btdrv_syn_trigger_codec_en(0);
#ifndef FPGA
        af_codec_sync_config(AUD_STREAM_PLAYBACK, AF_CODEC_SYNC_TYPE_BT, false);
        if (!play_only) {
            af_codec_sync_config(AUD_STREAM_CAPTURE, AF_CODEC_SYNC_TYPE_BT, false);
        }
#endif
        btdrv_syn_clr_trigger();
        btdrv_enable_playback_triggler(ACL_TRIGGLE_MODE);
        if  (app_tws_mode_is_master() || app_tws_mode_is_slave())
        {
            bt_syn_set_tg_ticks(trigger_time,  app_tws_get_tws_conhdl());
        }
        else
        {
            bt_syn_set_tg_ticks(trigger_time,  0);
        }

        if(app_tws_mode_is_slave())
        {
            bt_drv_reg_op_sco_status_store();
        }
        btdrv_syn_trigger_codec_en(1);
        app_tws_set_pcm_wait_triggle(1);
        LOG_PRINT_TWS("enable sco playback curr clk=%x, triggle clk=%x,bt clk=%x",btdrv_syn_get_curr_ticks(), trigger_time,bt_syn_get_curr_ticks(app_tws_get_tws_conhdl()));

        bt_drv_reg_op_trigger_time_checker();

#ifndef FPGA
        af_codec_sync_config(AUD_STREAM_PLAYBACK, AF_CODEC_SYNC_TYPE_BT, true);
        if (!play_only) {
            af_codec_sync_config(AUD_STREAM_CAPTURE, AF_CODEC_SYNC_TYPE_BT, true);
        }
#endif
    }
    else
    {
        LOG_PRINT_TWS("dont use triggler");
        btdrv_syn_trigger_codec_en(0);
#ifndef FPGA
        af_codec_sync_config(AUD_STREAM_PLAYBACK, AF_CODEC_SYNC_TYPE_BT, false);
        if (!play_only) {
            af_codec_sync_config(AUD_STREAM_CAPTURE, AF_CODEC_SYNC_TYPE_BT, false);
        }
#endif
        btdrv_syn_clr_trigger();

    }
}

#endif
