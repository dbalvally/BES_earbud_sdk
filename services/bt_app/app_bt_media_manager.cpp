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
//#include "mbed.h"
#include <stdio.h>
#include <assert.h>

#include "cmsis_os.h"
#include "cmsis.h"
#include "hal_uart.h"
#include "hal_timer.h"
#include "hal_chipid.h"
#include "audioflinger.h"
#include "lockcqueue.h"
#include "hal_trace.h"
#include "hal_cmu.h"
#include "analog.h"
#include "app_bt_stream.h"
#include "app_overlay.h"
#include "app_audio.h"
#include "app_utils.h"

#include "resources.h"
#ifdef MEDIA_PLAYER_SUPPORT
#include "app_media_player.h"
#endif


#include "me_api.h"
#include "a2dp_api.h"
#include "conmgr_api.h"
#include "mei_api.h"

#include "rtos.h"
#include "besbt.h"

#include "cqueue.h"
#include "btapp.h"

#ifdef EQ_PROCESS
#include "eq_export.h"
#endif

#include "app_bt_media_manager.h"
#include "app_thread.h"

#include "app_ring_merge.h"
#include "app_hfp.h"
#ifdef __TWS__ 
#include "app_tws.h"
#include "app_tws_if.h"
#endif

#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)
#include "app_key.h"
#include "player_role.h"
#endif
#include "bt_drv.h"

#define MEDIA_MANAGE_DEBUG


#ifdef MEDIA_MANAGE_DEBUG
#define MEDIA_MANAGE_TRACE   LOG_PRINT_MEDIA_MGR
#else
#define MEDIA_MANAGE_TRACE(...)   
#endif

uint8_t btapp_hfp_get_call_state(void);

extern struct BT_DEVICE_T  app_bt_device;

typedef struct
{
    uint16_t media_active[BT_DEVICE_NUM];
    uint8_t media_current_call_state[BT_DEVICE_NUM];
    uint8_t media_curr_sbc;
    uint16_t curr_active_media; // low 8 bits are out direciton, while high 8 bits are in direction
}BT_MEDIA_MANAGER_STRUCT;


BT_MEDIA_MANAGER_STRUCT  bt_meida;

extern void notify_tws_player_status(enum APP_BT_SETTING_T status);
#ifdef TWS_RBCODEC_PLAYER
extern void notify_local_player_status(enum APP_BT_SETTING_T status);
#endif

//extern uint32_t tws_get_player_callback(void);

//void notify_tws_player_start(void)
//{
//    uint32_t opt;
//    uint32_t callback;

//    opt = APP_BT_SBC_PLAYER_SET_VAL(APP_BT_SBC_PLAYER_EXTERN_START, 0);
//    callback = tws_get_player_callback();
//    app_audio_sendrequest(APP_BT_STREAM_A2DP_SBC, (uint8_t)APP_BT_SETTING_OPEN, opt, callback);
//}

//void notify_tws_player_stop(void)
//{
//    uint32_t opt;
//    uint32_t callback;

//    opt = APP_BT_SBC_PLAYER_SET_VAL(APP_BT_SBC_PLAYER_EXTERN_STOP, 0);
//    callback = tws_get_player_callback();
//    app_audio_sendrequest(APP_BT_STREAM_A2DP_SBC, (uint8_t)APP_BT_SETTING_CLOSE, opt, callback);
//}



uint8_t bt_media_is_media_active_by_type(uint16_t media_type)
{
    uint8_t i;
    for(i=0;i<BT_DEVICE_NUM;i++)
    {
        if(bt_meida.media_active[i] & media_type)
            return 1;

    }
    return 0;
}

static enum BT_DEVICE_ID_T bt_media_get_active_device_by_type(uint16_t media_type)
{
    uint8_t i;
    for(i=0;i<BT_DEVICE_NUM;i++)
    {
        if(bt_meida.media_active[i] & media_type)
            return (enum BT_DEVICE_ID_T)i;

    }
    return BT_DEVICE_NUM;
}

static uint8_t bt_media_is_media_active_by_device(uint16_t media_type,enum BT_DEVICE_ID_T device_id)
{
    if(bt_meida.media_active[device_id] & media_type)
        return 1;
    return 0;
}

bool bt_media_is_media_active(void)
{
    return bt_meida.media_active[BT_DEVICE_ID_1];
}

uint16_t bt_media_get_current_media(void)
{
    return bt_meida.curr_active_media;
}


static void bt_media_set_current_media(uint16_t media_type)
{
    if (media_type < 0x100)
    {
        bt_meida.curr_active_media &= (~0xFF);
        bt_meida.curr_active_media |= media_type;
    }
    else
    {
        bt_meida.curr_active_media &= (~0xFF00);
        bt_meida.curr_active_media |= media_type;        
    }

    LOG_PRINT_MEDIA_MGR("curr_active_media is set to 0x%x", bt_meida.curr_active_media);
}

static void bt_media_clear_current_media(uint16_t media_type)
{
    if (media_type < 0x100)
    {
        bt_meida.curr_active_media &= (~0xFF);
    }
    else
    {
        bt_meida.curr_active_media &= (~0xFF00);
    }     
}

static void bt_media_clear_all_media_type(void)
{
    uint8_t i;
    for(i=0;i<BT_DEVICE_NUM;i++)
    {
        bt_meida.media_active[i] &= (~BT_STREAM_MEDIA);
    }
}

static void bt_media_clear_media_type(uint16_t media_type,enum BT_DEVICE_ID_T device_id)
{
    if (media_type==BT_STREAM_MEDIA){
        for(uint8_t i=0;i<BT_DEVICE_NUM;i++){
            bt_meida.media_active[i] &= (~media_type);
        }
    }else{
        bt_meida.media_active[device_id] &= (~media_type);
    }
}

static enum BT_DEVICE_ID_T bt_media_get_active_sbc_device(void)
{
    enum BT_DEVICE_ID_T  device = BT_DEVICE_NUM;
    uint8_t i;
    for(i=0;i<BT_DEVICE_NUM;i++)
    {
        if((bt_meida.media_active[i] & BT_STREAM_SBC)  && (i==bt_meida.media_curr_sbc))
            device = (enum BT_DEVICE_ID_T)i;
    }
    return device;
}

#ifdef TWS_RBCODEC_PLAYER
int app_rbplay_audio_onoff(bool onoff, uint16_t aud_id);
bool  bt_media_rbcodec_start_process(uint16_t stream_type,enum BT_DEVICE_ID_T device_id,AUD_ID_ENUM media_id, uint32_t param, uint32_t ptr)
{
        int ret_SendReq2AudioThread = -1;
        bt_meida.media_active[device_id] |= stream_type;
//priority
//vioce
//media
//rbcodec
//sbc

    if(bt_media_is_media_active_by_type(BT_STREAM_VOICE | BT_STREAM_MEDIA))
    {
		LOG_PRINT_MEDIA_MGR("voice is on");
    }

    if(bt_media_get_current_media() & BT_STREAM_SBC){
        goto exit;
    }else{
        app_rbplay_audio_onoff(true, 0);
        //ret_SendReq2AudioThread = app_audio_sendrequest(APP_BT_STREAM_RBCODEC, (uint8_t)APP_BT_SETTING_OPEN, 200, ptr);
        bt_media_set_current_media(BT_STREAM_RBCODEC);  

		notify_tws_player_status(APP_BT_SETTING_OPEN);
		notify_local_player_status(APP_BT_SETTING_OPEN);

    }

    return true;
exit:
    return false;
}
#endif

#if VOICE_DATAPATH
static bool  bt_media_voicepath_start_process(uint16_t stream_type,enum BT_DEVICE_ID_T device_id,AUD_ID_ENUM media_id, uint32_t param, uint32_t ptr)
{
    bt_meida.media_active[device_id] |= stream_type;
	app_audio_sendrequest(APP_BT_STREAM_VOICEPATH, (uint8_t)APP_BT_SETTING_OPEN, media_id, ptr);
	bt_media_set_current_media(BT_STREAM_CAPTURE);
    return true;
}
#endif

#ifdef TWS_LINEIN_PLAYER
bool  bt_media_linein_start_process(uint16_t stream_type,enum BT_DEVICE_ID_T device_id,AUD_ID_ENUM media_id, uint32_t param, uint32_t ptr)
{
        int ret_SendReq2AudioThread = -1;
        bt_meida.media_active[device_id] |= stream_type;
//priority
//vioce
//media
//rbcodec
//sbc
    if(bt_media_is_media_active_by_type(BT_STREAM_VOICE | BT_STREAM_MEDIA))
    {
        //voice on then return it
        goto exit;
    }
    //bt is playing exit
    if(bt_media_get_current_media() & BT_STREAM_SBC){
        goto exit;
    }
    else{
        ret_SendReq2AudioThread = app_audio_sendrequest(APP_BT_STREAM_LINEIN, (uint8_t)APP_BT_SETTING_OPEN, 100, ptr);
        bt_media_set_current_media(BT_STREAM_LINEIN);  
    }
    return true;
exit:
    return false;
}
#endif


#if defined(__EARPHONE_AUDIO_MONITOR__)  && defined(__TWS__)
void bt_media_loop_close_callback(void)
{
    LOG_PRINT_MEDIA_MGR("%s ",__FUNCTION__);
    app_tws_reset_slave_medianum();
}
#endif

//only used in iamain thread ,can't used in other thread or interrupt
void  bt_media_start(uint16_t stream_type,enum BT_DEVICE_ID_T device_id,AUD_ID_ENUM media_id, uint32_t param, uint32_t ptr)
{
	int ret_SendReq2AudioThread = -1;
#ifdef __BT_ONE_BRING_TWO__
    enum BT_DEVICE_ID_T other_device_id = (device_id == BT_DEVICE_ID_1) ? BT_DEVICE_ID_2 : BT_DEVICE_ID_1;
#endif

    bt_meida.media_active[device_id] |= stream_type;



    MEDIA_MANAGE_TRACE("STREAM MANAGE bt_media_start type= %x,device id = %x,media_id = %x,param=%d",stream_type,device_id,media_id,param);
#ifdef __BT_ONE_BRING_TWO__
    MEDIA_MANAGE_TRACE("bt_media_start media_active = %x,%x,curr_active_media = %x",
        bt_meida.media_active[0],bt_meida.media_active[1], bt_meida.curr_active_media);
#else
    MEDIA_MANAGE_TRACE("bt_media_start media_active = %x,curr_active_media = %x",
        bt_meida.media_active[0], bt_meida.curr_active_media);

#endif
    switch(stream_type)
    {
#ifdef TWS_RBCODEC_PLAYER
        case BT_STREAM_RBCODEC:
            if(!bt_media_rbcodec_start_process(stream_type,device_id,media_id,param,ptr))
                goto exit;
            break;
#endif
#if VOICE_DATAPATH
        case BT_STREAM_CAPTURE:
#ifdef MEDIA_PLAYER_SUPPORT
#if !ISOLATED_AUDIO_STREAM_ENABLED
            if(bt_media_is_media_active_by_type(BT_STREAM_MEDIA))
            {
                return;
            }
#endif
#endif
            if(!bt_media_voicepath_start_process(stream_type,device_id,media_id, (uint32_t)NULL, (uint32_t)NULL))
                goto exit;
            break;
#endif	
#ifdef TWS_LINEIN_PLAYER
            case BT_STREAM_LINEIN:
            if(!bt_media_linein_start_process(stream_type,device_id,media_id,param,ptr))
                goto exit;
            break;
#endif    
        case BT_STREAM_SBC:
             ////because voice is the highest priority and media report will stop soon
            //// so just store the sbc type
            if(bt_meida.media_curr_sbc == BT_DEVICE_NUM)
                bt_meida.media_curr_sbc = device_id;
#ifdef MEDIA_PLAYER_SUPPORT
            if(bt_media_is_media_active_by_type(BT_STREAM_MEDIA))
            {
                goto exit;
            }
#endif

#if VOICE_DATAPATH
#if !ISOLATED_AUDIO_STREAM_ENABLED
            if(bt_media_is_media_active_by_type(BT_STREAM_CAPTURE))
            {
                goto exit;
            }
#endif
#endif

            if(bt_media_is_media_active_by_type(BT_STREAM_VOICE))
            {
                ////sbc and voice is all on so set sys freq to 104m                
#if defined(SLAVE_USE_OPUS) || defined(MASTER_USE_OPUS) || defined(ALL_USE_OPUS)
               app_sysfreq_req(APP_SYSFREQ_USER_APP_0, APP_SYSFREQ_208M);
#else
               app_sysfreq_req(APP_SYSFREQ_USER_APP_0, APP_SYSFREQ_104M);
#endif
               goto exit;
            }
#ifdef TWS_RBCODEC_PLAYER
            if(bt_media_get_current_media() & BT_STREAM_RBCODEC)
            {
                if(app_tws_get_master_stream_state() == TWS_STREAM_START){
                    app_audio_sendrequest(APP_BT_STREAM_RBCODEC, (uint8_t)APP_BT_SETTING_CLOSE, param, NULL);
                    bt_media_clear_current_media(BT_STREAM_OUT_MASK); 
                    ret_SendReq2AudioThread = app_audio_manager_sendrequest( APP_BT_STREAM_MANAGER_START, BT_STREAM_SBC, device_id, 0, param, ptr);
                    goto exit;
                }else{
                    goto exit;
                }
            }
#endif
#ifdef TWS_LINEIN_PLAYER
           else if(bt_media_get_current_media() & (BT_STREAM_LINEIN) )
            {
                if(app_tws_get_master_stream_state() == TWS_STREAM_START){
                   app_audio_sendrequest(APP_BT_STREAM_LINEIN, (uint8_t)APP_BT_SETTING_CLOSE, param, NULL);
                   bt_media_clear_current_media(BT_STREAM_OUT_MASK); 
                   ret_SendReq2AudioThread = app_audio_manager_sendrequest( APP_BT_STREAM_MANAGER_START, BT_STREAM_SBC, device_id, 0, param, ptr);
                    goto exit;
                }else{
                    goto exit;
                }
            }
#endif           
#ifdef __BT_ONE_BRING_TWO__
             //if  another device audio is playing,check the active audio device
             //
            else if(bt_media_is_media_active_by_device(BT_STREAM_SBC,other_device_id))
            {
                //if another device is the active stream do nothing
                if(bt_meida.media_curr_sbc == other_device_id)
                {

                    ///2 device is play sbc,so set sys freq to 104m
#if defined(SLAVE_USE_OPUS) || defined(MASTER_USE_OPUS) || defined(ALL_USE_OPUS)
                    app_sysfreq_req(APP_SYSFREQ_USER_APP_0, APP_SYSFREQ_208M);
#else
                    app_sysfreq_req(APP_SYSFREQ_USER_APP_0, APP_SYSFREQ_104M);
#endif
                    goto exit;
                }
                ////if curr active media is not sbc,wrong~~
                if(bt_meida.curr_active_media != BT_STREAM_SBC)
                {
                    ASSERT(0,"curr_active_media is wrong!");
                }
                ///stop the old audio sbc and start the new audio sbc
                ret_SendReq2AudioThread = app_audio_sendrequest(APP_BT_STREAM_A2DP_SBC, (uint8_t)APP_BT_SETTING_CLOSE, 0, ptr);
            }
#endif
#ifdef __EARPHONE_AUDIO_MONITOR__
            else if(bt_media_is_media_active_by_type(BT_STREAM_MONITOR))
            {
                MEDIA_MANAGE_TRACE("monitor is active ,so close monitor and start sbc");
                if(bt_media_get_current_media() & BT_STREAM_MONITOR)
                {
                    MEDIA_MANAGE_TRACE("close monitor!");
#ifdef __TWS__ 
                    if(app_tws_is_slave_mode())
                    {
                        app_audio_sendrequest(APP_FACTORYMODE_AUDIO_LOOP, (uint8_t)APP_BT_SETTING_CLOSE, 0, (uint32_t)&bt_media_loop_close_callback);                
                    }
                    else
                    {
                        app_audio_sendrequest(APP_FACTORYMODE_AUDIO_LOOP, (uint8_t)APP_BT_SETTING_CLOSE, 0, 0);                
                    }
#else                    
                    app_audio_sendrequest(APP_FACTORYMODE_AUDIO_LOOP, (uint8_t)APP_BT_SETTING_CLOSE, 0, 0);                
#endif
#ifdef __TWS__ 
                    bt_media_clear_media_type(BT_STREAM_SBC,device_id);
                    bt_media_clear_current_media(BT_STREAM_SBC);
                    notify_tws_player_status(APP_BT_SETTING_OPEN);
#else                
                    bt_parse_store_sbc_sample_rate(app_bt_device.sample_rate[device_id]);
                    ret_SendReq2AudioThread = app_audio_sendrequest(APP_BT_STREAM_A2DP_SBC, (uint8_t)APP_BT_SETTING_OPEN, param, ptr);
                    bt_media_set_current_media(BT_STREAM_SBC);                
#endif   
                }
                else
                {
                    //start audio sbc stream
                    bt_parse_store_sbc_sample_rate(app_bt_device.sample_rate[device_id]);
                    ret_SendReq2AudioThread = app_audio_sendrequest(APP_BT_STREAM_A2DP_SBC, (uint8_t)APP_BT_SETTING_OPEN, param, ptr);
                    bt_media_set_current_media(BT_STREAM_SBC);                
                }

            }

#endif
            else{
                //start audio sbc stream
                bt_parse_store_sbc_sample_rate(app_bt_device.sample_rate[device_id]);
                ret_SendReq2AudioThread = app_audio_sendrequest(APP_BT_STREAM_A2DP_SBC, (uint8_t)APP_BT_SETTING_OPEN, param, ptr);
                bt_media_set_current_media(BT_STREAM_SBC);
            }
            break;
#ifdef MEDIA_PLAYER_SUPPORT
        case BT_STREAM_MEDIA:
            //first,if the voice is active so  mix "dudu" to the stream
            if(bt_media_is_media_active_by_type(BT_STREAM_VOICE))
            {
#ifdef __TWS_CALL_DUAL_CHANNEL__
                if(1)
#else
                if(bt_media_get_current_media() & BT_STREAM_VOICE)
#endif                    
                {
#ifdef __TWS_CALL_DUAL_CHANNEL__
                if(1)
#else
                    //if call is not active so do media report
                    if(btapp_hfp_get_call_state())
#endif                        
                    {
                        //todo ..mix the "dudu"
                        MEDIA_MANAGE_TRACE("BT_STREAM_VOICE-->app_ring_merge_start\n");
                        app_ring_merge_start(AUD_ID_BT_WARNING);
                        //meida is done here
                        bt_media_clear_all_media_type();
                    }
                    else
                    {
                        MEDIA_MANAGE_TRACE("stop sco and do media report\n");

                        ret_SendReq2AudioThread = app_audio_sendrequest(APP_BT_STREAM_HFP_PCM, (uint8_t)APP_BT_SETTING_CLOSE, 0, ptr);
                        ret_SendReq2AudioThread = app_audio_sendrequest(APP_PLAY_BACK_AUDIO, (uint8_t)APP_BT_SETTING_OPEN, media_id, ptr);
                        bt_media_set_current_media(BT_STREAM_MEDIA);

                    }
                }
                else if(bt_media_get_current_media() & BT_STREAM_MEDIA)
                {
                    ret_SendReq2AudioThread = app_audio_sendrequest(APP_PLAY_BACK_AUDIO, (uint8_t)APP_BT_SETTING_OPEN, media_id, ptr);
                    bt_media_set_current_media(BT_STREAM_MEDIA);
                }
                else
                {
                    ///if voice is active but current is not voice something is unkown
#ifdef __BT_ONE_BRING_TWO__
                    MEDIA_MANAGE_TRACE("STREAM MANAGE voice  active media_active = %x,%x,curr_active_media = %x",
                        bt_meida.media_active[0],bt_meida.media_active[1], bt_meida.curr_active_media);
#else
                    MEDIA_MANAGE_TRACE("STREAM MANAGE voice  active media_active = %x,curr_active_media = %x",
                        bt_meida.media_active[0], bt_meida.curr_active_media);

#endif
                }
            }
#ifdef TWS_RBCODEC_PLAYER
            else if(bt_media_get_current_media() & BT_STREAM_RBCODEC)
            {
                if(app_tws_get_master_stream_state() == TWS_STREAM_START){
                    app_audio_sendrequest(APP_BT_STREAM_RBCODEC, (uint8_t)APP_BT_SETTING_CLOSE, param, NULL);
                    ret_SendReq2AudioThread = app_audio_sendrequest(APP_PLAY_BACK_AUDIO, (uint8_t)APP_BT_SETTING_OPEN, media_id, ptr);
                    bt_media_set_current_media(BT_STREAM_MEDIA);
                }else{
                    ret_SendReq2AudioThread = app_audio_sendrequest(APP_PLAY_BACK_AUDIO, (uint8_t)APP_BT_SETTING_OPEN, media_id, ptr);
                    bt_media_set_current_media(BT_STREAM_MEDIA);
                }
            }
#endif
#ifdef TWS_LINEIN_PLAYER
           else if(bt_media_get_current_media() & (BT_STREAM_LINEIN) )
            {
                if(app_tws_get_master_stream_state() == TWS_STREAM_START){
                    app_audio_sendrequest(APP_BT_STREAM_LINEIN, (uint8_t)APP_BT_SETTING_CLOSE, param, NULL);
                    ret_SendReq2AudioThread = app_audio_sendrequest(APP_PLAY_BACK_AUDIO, (uint8_t)APP_BT_SETTING_OPEN, media_id, ptr);
                    bt_media_set_current_media(BT_STREAM_MEDIA);
                }else{
                    ret_SendReq2AudioThread = app_audio_sendrequest(APP_PLAY_BACK_AUDIO, (uint8_t)APP_BT_SETTING_OPEN, media_id, ptr);
                    bt_media_set_current_media(BT_STREAM_MEDIA);
                }
            }
#endif
#if VOICE_DATAPATH		
#if !ISOLATED_AUDIO_STREAM_ENABLED 
            else if( bt_media_is_media_active_by_type(BT_STREAM_CAPTURE) ) {
                if(bt_media_get_current_media() & BT_STREAM_MEDIA)
                {
                    app_audio_sendrequest(APP_PLAY_BACK_AUDIO, (uint8_t)APP_BT_SETTING_OPEN, media_id, ptr);
                }
                else {
                    goto exit;
                }
            }
#endif
#endif
            ////if sbc active so
            else if(bt_media_is_media_active_by_type(BT_STREAM_SBC))
            {
                if(bt_media_get_current_media() & BT_STREAM_SBC)
                {
#ifdef __BT_WARNING_TONE_MERGE_INTO_STREAM_SBC__
                    if (1)
                    {
                        MEDIA_MANAGE_TRACE("BT_STREAM_SBC-->app_ring_merge_start\n");
                        app_ring_merge_start(media_id);
                        //meida is done here
                        bt_media_clear_all_media_type();						
                    }
                    else
#endif
                    {
                        ret_SendReq2AudioThread = app_audio_sendrequest(APP_BT_STREAM_A2DP_SBC, (uint8_t)APP_BT_SETTING_CLOSE, 0, 0);
			            // callback is real close, now is not real close			
#ifdef __TWS__
                        notify_tws_player_status(APP_BT_SETTING_CLOSE);
#endif              
                        ret_SendReq2AudioThread = app_audio_sendrequest(APP_PLAY_BACK_AUDIO, (uint8_t)APP_BT_SETTING_OPEN, media_id, ptr);
                        bt_media_set_current_media(BT_STREAM_MEDIA);
                    }
                }
                else if(bt_media_get_current_media() & BT_STREAM_MEDIA)
                {
                    ret_SendReq2AudioThread = app_audio_sendrequest(APP_PLAY_BACK_AUDIO, (uint8_t)APP_BT_SETTING_OPEN, media_id, ptr);
                }
                else if ((bt_media_get_current_media()&0xFF) == 0)
                {
                    ASSERT(0,"media in sbc  current wrong");
                }
            }
#ifdef __EARPHONE_AUDIO_MONITOR__
            else if(bt_media_is_media_active_by_type(BT_STREAM_MONITOR))
            {
                MEDIA_MANAGE_TRACE("monitor is active ,so close monitor and start meida");
            
                if(bt_media_get_current_media() & BT_STREAM_MONITOR)
                {
                    MEDIA_MANAGE_TRACE("stop moniter!!");
                    app_audio_sendrequest(APP_FACTORYMODE_AUDIO_LOOP, (uint8_t)APP_BT_SETTING_CLOSE, 0, 0);                
                }
                ret_SendReq2AudioThread = app_audio_sendrequest(APP_PLAY_BACK_AUDIO, (uint8_t)APP_BT_SETTING_OPEN, media_id, ptr);
                bt_media_set_current_media(BT_STREAM_MEDIA);          
            }

#endif            
            /// just play the media
            else
            {
                ret_SendReq2AudioThread = app_audio_sendrequest(APP_PLAY_BACK_AUDIO, (uint8_t)APP_BT_SETTING_OPEN, media_id, ptr);
                bt_media_set_current_media(BT_STREAM_MEDIA);
            }
            break;
#endif
        case BT_STREAM_VOICE:
#ifdef TWS_RBCODEC_PLAYER
            if(bt_media_is_media_active_by_type(BT_STREAM_RBCODEC))
            {
                ///if rbcodec is open  stop rbcodec
                if(bt_media_get_current_media() & BT_STREAM_RBCODEC)
                {
                    ret_SendReq2AudioThread = app_audio_sendrequest(APP_BT_STREAM_RBCODEC, (uint8_t)APP_BT_SETTING_CLOSE, 0, 0);
                    bt_media_clear_current_media(BT_STREAM_RBCODEC);
                }
            }
#endif
#if VOICE_DATAPATH
            if(bt_media_is_media_active_by_type(BT_STREAM_CAPTURE)) {
                if(bt_media_get_current_media() & BT_STREAM_CAPTURE) {
                    app_audio_sendrequest(APP_BT_STREAM_VOICEPATH, (uint8_t)APP_BT_SETTING_CLOSE, 0, ptr);
                    bt_media_clear_current_media(BT_STREAM_CAPTURE);
                }
            }
#endif

#ifdef MEDIA_PLAYER_SUPPORT
            if(bt_media_is_media_active_by_type(BT_STREAM_MEDIA))
            {
#ifdef __TWS_CALL_DUAL_CHANNEL__
                if(1)
#else            
                //if call is active ,so disable media report
                if(btapp_hfp_get_call_state())
#endif                    
                {
                    //if meida is open ,close media clear all media type
                    if(bt_media_get_current_media() & BT_STREAM_MEDIA)
                    {
                        MEDIA_MANAGE_TRACE("call active so start sco and stop media report\n");
#ifdef __AUDIO_QUEUE_SUPPORT__
                        app_audio_list_clear();
#endif
                        ret_SendReq2AudioThread = app_audio_sendrequest(APP_PLAY_BACK_AUDIO, (uint8_t)APP_BT_SETTING_CLOSE, 0, ptr);

                    }
                    bt_media_clear_all_media_type();
#ifdef __TWS_CALL_DUAL_CHANNEL__
                    ret_SendReq2AudioThread = app_audio_sendrequest(APP_BT_STREAM_HFP_PCM, (uint8_t)APP_BT_SETTING_OPEN, 1, ptr);

#else                    
                    ret_SendReq2AudioThread = app_audio_sendrequest(APP_BT_STREAM_HFP_PCM, (uint8_t)APP_BT_SETTING_OPEN, 0, ptr);
#endif
                    bt_media_set_current_media(BT_STREAM_VOICE);

                }
                else
                {
                    ////call is not active so media report continue
                }
            }
            else
#endif
            if(bt_media_is_media_active_by_type(BT_STREAM_SBC))
            {
                ///if sbc is open  stop sbc
                if(bt_media_get_current_media() & BT_STREAM_SBC)
                {
                    ret_SendReq2AudioThread = app_audio_sendrequest(APP_BT_STREAM_A2DP_SBC, (uint8_t)APP_BT_SETTING_CLOSE, 0, 0);
#ifdef __TWS__
                    notify_tws_player_status(APP_BT_SETTING_CLOSE);
#endif
                }
                 ////start voice stream
                ret_SendReq2AudioThread = app_audio_sendrequest(APP_BT_STREAM_HFP_PCM, (uint8_t)APP_BT_SETTING_OPEN, param, ptr);
                bt_media_set_current_media(BT_STREAM_VOICE);       
            }
#ifdef __EARPHONE_AUDIO_MONITOR__
            else if(bt_media_is_media_active_by_type(BT_STREAM_MONITOR))
            {
                MEDIA_MANAGE_TRACE("monitor is active ,so close monitor and start meida");
                if(bt_media_get_current_media() & BT_STREAM_MONITOR)
                {
                    MEDIA_MANAGE_TRACE("stop moniter!!!");
                    app_audio_sendrequest(APP_FACTORYMODE_AUDIO_LOOP, (uint8_t)APP_BT_SETTING_CLOSE, 0, 0);                
                }
                ret_SendReq2AudioThread = app_audio_sendrequest(APP_BT_STREAM_HFP_PCM, (uint8_t)APP_BT_SETTING_OPEN, 0, ptr);
                bt_media_set_current_media(BT_STREAM_VOICE);                
            }

#endif            
            
            else
            {
                //voice is open already so do nothing
                if(bt_media_get_current_media() & BT_STREAM_VOICE)
                {                
                    //do nohting
                }
                else
                {
                    ret_SendReq2AudioThread = app_audio_sendrequest(APP_BT_STREAM_HFP_PCM, (uint8_t)APP_BT_SETTING_OPEN, param, ptr);
                    bt_media_set_current_media(BT_STREAM_VOICE);
                }
            }

            break;
#ifdef __EARPHONE_AUDIO_MONITOR__
        case BT_STREAM_MONITOR:
            MEDIA_MANAGE_TRACE("start moniter!!");
            if(bt_media_is_media_active_by_type(BT_STREAM_VOICE))
            {
                MEDIA_MANAGE_TRACE("there is a voice exist ,do nothing");    
            }
            else if(bt_media_is_media_active_by_type(BT_STREAM_SBC))
            {
                MEDIA_MANAGE_TRACE("there is a music exist ,do nothing");    
            }
            else if(bt_media_is_media_active_by_type(BT_STREAM_MEDIA))
            {
                MEDIA_MANAGE_TRACE("there is a media prompt exist ,do nothing");                
            }
            else
            {
                ret_SendReq2AudioThread = app_audio_sendrequest(APP_FACTORYMODE_AUDIO_LOOP, (uint8_t)APP_BT_SETTING_OPEN, 0, 0);
                bt_media_set_current_media(BT_STREAM_MONITOR);
            }
            break;
#endif            
        default:
            ASSERT(0,"bt_media_open ERROR TYPE");
            break;
    }

exit:
	if (ret_SendReq2AudioThread<0){
		APP_AUDIO_CALLBACK_T callback = (APP_AUDIO_CALLBACK_T)ptr;
		if (callback)
			callback(APP_BT_STREAM_MANAGER_START,0);
	}
}

//only used in iamain thread ,can't used in other thread or interrupt
void  bt_media_setup(uint8_t stream_type,enum BT_DEVICE_ID_T device_id,AUD_ID_ENUM media_id, uint32_t param, uint32_t ptr)
{
	int ret_SendReq2AudioThread = -1;

    MEDIA_MANAGE_TRACE("bt_media_setup");

    switch(stream_type)
    {
        case BT_STREAM_SBC:
            ret_SendReq2AudioThread = app_audio_sendrequest(APP_BT_STREAM_A2DP_SBC, (uint8_t)APP_BT_SETTING_SETUP, param, ptr);
            break;
//        case BT_STREAM_MEDIA:
//            break;
//        case BT_STREAM_VOICE:
//            break;
#ifdef TWS_RBCODEC_PLAYER
          case BT_STREAM_RBCODEC:
            ret_SendReq2AudioThread = app_audio_sendrequest(APP_BT_STREAM_RBCODEC, (uint8_t)APP_BT_SETTING_SETUP, param, ptr);
            break;
#endif
#ifdef TWS_LINEIN_PLAYER
            case BT_STREAM_LINEIN:
            ret_SendReq2AudioThread = app_audio_sendrequest(APP_BT_STREAM_LINEIN, (uint8_t)APP_BT_SETTING_SETUP, param, ptr);
            break;
#endif        
        default:
            ASSERT(0,"bt_media_open ERROR TYPE");
            break;
    }

	if (ret_SendReq2AudioThread<0){
		APP_AUDIO_CALLBACK_T callback = (APP_AUDIO_CALLBACK_T)ptr;
		if (callback)
			callback(APP_BT_STREAM_MANAGER_START,0);
	}
	
}

#ifdef TWS_RBCODEC_PLAYER
bool bt_media_rbcodec_stop_process(uint8_t stream_type,enum BT_DEVICE_ID_T device_id, uint32_t ptr)
{
    int ret_SendReq2AudioThread = -1;
    bt_media_clear_media_type(stream_type,device_id);
    //if current stream is the stop one ,so stop it
    if(bt_media_get_current_media() & BT_STREAM_RBCODEC )
    {
        app_rbplay_audio_onoff(false, 0);
        app_audio_sendrequest(APP_BT_STREAM_RBCODEC, (uint8_t)APP_BT_SETTING_CLOSE, 0, ptr);
        bt_media_clear_current_media(APP_BT_STREAM_RBCODEC);
        MEDIA_MANAGE_TRACE("RBCODEC STOPED!");
    }

    if(bt_media_is_media_active_by_type(BT_STREAM_SBC))
    {
        enum BT_DEVICE_ID_T sbc_id  = bt_media_get_active_device_by_type(BT_STREAM_SBC);
        MEDIA_MANAGE_TRACE("sbc_id %d",sbc_id);
        if(sbc_id < BT_DEVICE_NUM)
        {
            bt_meida.media_curr_sbc = sbc_id;
        }
    }
    else
    {
        bt_meida.media_curr_sbc = BT_DEVICE_NUM;
    }
    
    MEDIA_MANAGE_TRACE("bt_meida.media_curr_sbc %d",bt_meida.media_curr_sbc);
    
    if(bt_media_is_media_active_by_type(BT_STREAM_VOICE))
    {
        //ASSERT(bt_media_get_current_media() & BT_STREAM_VOICE);
    }
    else if(bt_media_is_media_active_by_type(BT_STREAM_SBC))
    {
        // __BT_ONE_BRING_TWO__
        enum BT_DEVICE_ID_T sbc_id  = bt_media_get_active_device_by_type(BT_STREAM_SBC);
        if(sbc_id < BT_DEVICE_NUM)
        {
#ifdef __TWS__ 
            bt_media_clear_media_type(BT_STREAM_SBC,sbc_id);
            bt_media_clear_current_media(BT_STREAM_SBC);
            notify_tws_player_status(APP_BT_SETTING_OPEN);
#else 
            bt_parse_store_sbc_sample_rate(app_bt_device.sample_rate[sbc_id]);
            app_audio_sendrequest(APP_BT_STREAM_A2DP_SBC, (uint8_t)APP_BT_SETTING_OPEN, 0, 0);
            bt_media_set_current_media(BT_STREAM_SBC);
#endif
        }
    }
    else if(bt_media_is_media_active_by_type(BT_STREAM_MEDIA))
    {
        //do nothing
    }
    return ret_SendReq2AudioThread;
}
#endif
#if VOICE_DATAPATH
bool bt_media_voicepath_stop_process(uint16_t stream_type,enum BT_DEVICE_ID_T device_id, uint32_t ptr)
{
    int ret_SendReq2AudioThread __attribute__((unused));
	ret_SendReq2AudioThread= -1;
    bt_media_clear_media_type(stream_type,device_id);
    //if current stream is the stop one ,so stop it
    if(bt_media_get_current_media() & BT_STREAM_CAPTURE) {
        ret_SendReq2AudioThread = app_audio_sendrequest(APP_BT_STREAM_VOICEPATH, (uint8_t)APP_BT_SETTING_CLOSE, 0, ptr);
        bt_media_clear_current_media(BT_STREAM_CAPTURE);
        LOG_PRINT_MEDIA_MGR(" Voice Path STOPED! ");
    }

    if(bt_media_is_media_active_by_type(BT_STREAM_SBC)) {
        enum BT_DEVICE_ID_T sbc_id  = bt_media_get_active_device_by_type(BT_STREAM_SBC);
        LOG_PRINT_MEDIA_MGR("sbc_id %d",sbc_id);
        if(sbc_id < BT_DEVICE_NUM) {
            bt_meida.media_curr_sbc = sbc_id;
        }
    } else {
        bt_meida.media_curr_sbc = BT_DEVICE_NUM;
    }

    LOG_PRINT_MEDIA_MGR("bt_meida.media_curr_sbc %d",bt_meida.media_curr_sbc);

    if(bt_media_is_media_active_by_type(BT_STREAM_VOICE)) {
    } 
#if !ISOLATED_AUDIO_STREAM_ENABLED    
	else if(bt_media_is_media_active_by_type(BT_STREAM_MEDIA)) {
        app_audio_sendrequest(APP_PLAY_BACK_AUDIO, (uint8_t)APP_BT_SETTING_OPEN, 0, ptr);
        bt_media_set_current_media(BT_STREAM_MEDIA);
	}  
    else if(bt_media_is_media_active_by_type(BT_STREAM_SBC)) {
        enum BT_DEVICE_ID_T sbc_id  = bt_media_get_active_device_by_type(BT_STREAM_SBC);
        if(sbc_id < BT_DEVICE_NUM) {
#ifdef __TWS__
            bt_media_clear_media_type(BT_STREAM_SBC,sbc_id);
            bt_media_clear_current_media(BT_STREAM_SBC);
            notify_tws_player_status(APP_BT_SETTING_OPEN);
#else
            //bt_parse_store_sbc_sample_rate(app_bt_device.sample_rate[sbc_id]);
            ret_SendReq2AudioThread = app_audio_sendrequest(APP_BT_STREAM_A2DP_SBC, (uint8_t)APP_BT_SETTING_OPEN, 0, ptr);
            bt_media_set_current_media(BT_STREAM_SBC);
#endif
        }
    } 
#endif
		return true;
}
#endif
#ifdef TWS_LINEIN_PLAYER
bool bt_media_linein_stop_process(uint8_t stream_type,enum BT_DEVICE_ID_T device_id, uint32_t ptr)
{
    int ret_SendReq2AudioThread = -1;
    bt_media_clear_media_type(stream_type,device_id);
    //if current stream is the stop one ,so stop it
    if(bt_media_get_current_media() & BT_STREAM_LINEIN)
    {
        app_audio_sendrequest(APP_BT_STREAM_LINEIN, (uint8_t)APP_BT_SETTING_CLOSE, 0, ptr);
        bt_media_clear_current_media(APP_BT_STREAM_LINEIN);
        MEDIA_MANAGE_TRACE("LINEIN STOPED!");
    }

    if(bt_media_is_media_active_by_type(BT_STREAM_SBC))
    {
        enum BT_DEVICE_ID_T sbc_id  = bt_media_get_active_device_by_type(BT_STREAM_SBC);
        MEDIA_MANAGE_TRACE("sbc_id %d",sbc_id);
        if(sbc_id < BT_DEVICE_NUM)
        {
            bt_meida.media_curr_sbc = sbc_id;
        }
    }
    else
    {
        bt_meida.media_curr_sbc = BT_DEVICE_NUM;
    }
    
    MEDIA_MANAGE_TRACE("bt_meida.media_curr_sbc %d",bt_meida.media_curr_sbc);
    
    if(bt_media_is_media_active_by_type(BT_STREAM_VOICE))
    {
        //ASSERT(bt_media_get_current_media() & BT_STREAM_VOICE);
    }
    else if(bt_media_is_media_active_by_type(BT_STREAM_SBC))
    {
        // __BT_ONE_BRING_TWO__
        enum BT_DEVICE_ID_T sbc_id  = bt_media_get_active_device_by_type(BT_STREAM_SBC);
        if(sbc_id < BT_DEVICE_NUM)
        {
#ifdef __TWS__ 
            bt_media_clear_media_type(BT_STREAM_SBC,sbc_id);
            bt_media_clear_current_media(BT_STREAM_SBC);
            notify_tws_player_status(APP_BT_SETTING_OPEN);
#else 
            bt_parse_store_sbc_sample_rate(app_bt_device.sample_rate[sbc_id]);
            ret_SendReq2AudioThread = app_audio_sendrequest(APP_BT_STREAM_A2DP_SBC, (uint8_t)APP_BT_SETTING_OPEN, 0, 0);
            bt_media_set_current_media(BT_STREAM_SBC);
#endif
        }
    }
    else if(bt_media_is_media_active_by_type(BT_STREAM_MEDIA))
    {
        //do nothing
    }
    return ret_SendReq2AudioThread;
}
#endif

/*
    bt_media_stop function is called to stop media by app or media play callback
    sbc is just stop by a2dp stream suspend or close
    voice is just stop by hfp audio disconnect
    media is stop by media player finished call back

*/
void bt_media_stop(uint16_t stream_type,enum BT_DEVICE_ID_T device_id, uint32_t ptr)
{
	int ret_SendReq2AudioThread = -1;

    MEDIA_MANAGE_TRACE("STREAM MANAGE bt_media_stop type= %x,device id = %x,ptr=%x",stream_type,device_id,ptr);
#ifdef __BT_ONE_BRING_TWO__
    MEDIA_MANAGE_TRACE("bt_media_stop media_active = %x,%x,curr_active_media = %x",
        bt_meida.media_active[0],bt_meida.media_active[1], bt_meida.curr_active_media);
#else
    MEDIA_MANAGE_TRACE("bt_media_stop media_active = %x,curr_active_media = %x",
        bt_meida.media_active[0], bt_meida.curr_active_media);

#endif

    switch(stream_type)
    {
#if VOICE_DATAPATH
        case BT_STREAM_CAPTURE:
            bt_media_voicepath_stop_process(stream_type, device_id, ptr);
            break;
#endif	
#ifdef TWS_RBCODEC_PLAYER
        case BT_STREAM_RBCODEC:
            ret_SendReq2AudioThread = bt_media_rbcodec_stop_process(stream_type,device_id,ptr);
        break;
#endif
#ifdef TWS_LINEIN_PLAYER
        case BT_STREAM_LINEIN:
            ret_SendReq2AudioThread = bt_media_linein_stop_process(stream_type,device_id,ptr);
        break;
#endif    
        case BT_STREAM_SBC:
            MEDIA_MANAGE_TRACE("SBC STOPPING");
            ////if current media is sbc ,stop the sbc streaming
            bt_media_clear_media_type(stream_type,device_id);
            //if current stream is the stop one ,so stop it
            if(bt_media_get_current_media() & BT_STREAM_SBC && bt_meida.media_curr_sbc  == device_id)
            {
                ret_SendReq2AudioThread = app_audio_sendrequest(APP_BT_STREAM_A2DP_SBC, (uint8_t)APP_BT_SETTING_CLOSE, 0, ptr);
#ifdef __TWS__
                notify_tws_player_status(APP_BT_SETTING_CLOSE);
#endif              
                    bt_media_clear_current_media(BT_STREAM_SBC);
                MEDIA_MANAGE_TRACE("SBC STOPED!");

            }
            if(bt_media_is_media_active_by_type(BT_STREAM_SBC))
            {
                enum BT_DEVICE_ID_T sbc_id  = bt_media_get_active_device_by_type(BT_STREAM_SBC);
                if(sbc_id < BT_DEVICE_NUM)
                {
                    bt_meida.media_curr_sbc =sbc_id;
                }
            }
            else
            {
                bt_meida.media_curr_sbc = BT_DEVICE_NUM;
            }
            
            if(bt_media_is_media_active_by_type(BT_STREAM_VOICE))
            {
                //ASSERT(bt_media_get_current_media() & BT_STREAM_VOICE);
            }
            else if(bt_media_is_media_active_by_type(BT_STREAM_MEDIA))
            {
                //do nothing
            }
            #if VOICE_DATAPATH
			#if !ISOLATED_AUDIO_STREAM_ENABLED
    			else if(bt_media_is_media_active_by_type(BT_STREAM_CAPTURE)) {
                    //do nothing
                }
			#endif
            #endif
            else if(bt_media_is_media_active_by_type(BT_STREAM_SBC))
            {
                // __BT_ONE_BRING_TWO__
                enum BT_DEVICE_ID_T sbc_id  = bt_media_get_active_device_by_type(BT_STREAM_SBC);
                if(sbc_id < BT_DEVICE_NUM)
                {
#ifdef __TWS__ 
                    bt_media_clear_media_type(BT_STREAM_SBC,sbc_id);
                    bt_media_clear_current_media(BT_STREAM_SBC);
					notify_tws_player_status(APP_BT_SETTING_OPEN);
#else 
                    bt_parse_store_sbc_sample_rate(app_bt_device.sample_rate[sbc_id]);
                    ret_SendReq2AudioThread = app_audio_sendrequest(APP_BT_STREAM_A2DP_SBC, (uint8_t)APP_BT_SETTING_OPEN, 0, 0);
                    bt_media_set_current_media(BT_STREAM_SBC);
#endif
                }
            }
#ifdef __EARPHONE_AUDIO_MONITOR__
            else if(bt_media_is_media_active_by_type(BT_STREAM_MONITOR))
            {
                ret_SendReq2AudioThread = app_audio_sendrequest(APP_FACTORYMODE_AUDIO_LOOP, (uint8_t)APP_BT_SETTING_OPEN, 0, 0);
                bt_media_set_current_media(BT_STREAM_MONITOR);
            }
#endif
#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)
            else if(app_get_play_role() < PLAYER_ROLE_TWS){
                if(app_get_play_role() == PLAYER_ROLE_SD){
                    //app_audio_sendrequest(APP_BT_STREAM_RBCODEC, (uint8_t)APP_BT_SETTING_OPEN, 0, NULL);
                    //bt_media_set_current_media(BT_STREAM_RBCODEC);
                    app_audio_manager_sendrequest( APP_BT_STREAM_MANAGER_START, BT_STREAM_RBCODEC, 0, 0, 100, 0);
                }else if(app_get_play_role() == PLAYER_ROLE_LINEIN){
                    //app_audio_sendrequest(APP_BT_STREAM_LINEIN, (uint8_t)APP_BT_SETTING_OPEN, 0, NULL);
                   // bt_media_set_current_media(BT_STREAM_LINEIN); 
                    app_audio_manager_sendrequest( APP_BT_STREAM_MANAGER_START, BT_STREAM_LINEIN, 0, 0, 100, 0);
                }
            }
#endif            
            
            break;
#ifdef MEDIA_PLAYER_SUPPORT
        case BT_STREAM_MEDIA:

            bt_media_clear_all_media_type();
            // bt_media_set_current_media(0);

            if(bt_media_is_media_active_by_type(BT_STREAM_MEDIA))
            {
                //also have media report so do nothing
            }
            else if(bt_media_is_media_active_by_type(BT_STREAM_VOICE))
            {
                if(bt_media_get_current_media() & BT_STREAM_VOICE)
                {
                    //do nothing
                }
                else if(bt_media_get_current_media() & BT_STREAM_MEDIA)
                {
                    ///media report is end ,so goto voice
                  //  app_audio_sendrequest(APP_PLAY_BACK_AUDIO, (uint8_t)APP_BT_SETTING_CLOSE, 0, ptr);
#ifdef __TWS_CALL_DUAL_CHANNEL__
                    ret_SendReq2AudioThread = app_audio_sendrequest(APP_BT_STREAM_HFP_PCM, (uint8_t)APP_BT_SETTING_OPEN, 1, ptr);
#else
                    ret_SendReq2AudioThread = app_audio_sendrequest(APP_BT_STREAM_HFP_PCM, (uint8_t)APP_BT_SETTING_OPEN, 0, ptr);
#endif
                    bt_media_set_current_media(BT_STREAM_VOICE);
                }
            }
#if VOICE_DATAPATH
		#if !ISOLATED_AUDIO_STREAM_ENABLED
			else if(bt_media_is_media_active_by_type(BT_STREAM_CAPTURE)) {
                app_audio_sendrequest(APP_BT_STREAM_VOICEPATH, (uint8_t)APP_BT_SETTING_OPEN, 0, 0);
                bt_media_set_current_media(BT_STREAM_CAPTURE);
			} 
		#endif
#endif	   
            else if(bt_media_is_media_active_by_type(BT_STREAM_SBC))
            {
                ///if another device is also in sbc mode
                enum BT_DEVICE_ID_T sbc_id  = bt_media_get_active_sbc_device();
#ifdef __TWS__ 
                bt_media_clear_media_type(BT_STREAM_SBC,sbc_id);
                bt_media_clear_current_media(BT_STREAM_SBC);
				notify_tws_player_status(APP_BT_SETTING_OPEN);
#else                
                bt_parse_store_sbc_sample_rate(app_bt_device.sample_rate[sbc_id]);
                ret_SendReq2AudioThread = app_audio_sendrequest(APP_BT_STREAM_A2DP_SBC, (uint8_t)APP_BT_SETTING_OPEN, 0, 0);
                bt_media_set_current_media(BT_STREAM_SBC);
#endif
            }
#ifdef __EARPHONE_AUDIO_MONITOR__
            else if(bt_media_is_media_active_by_type(BT_STREAM_MONITOR))
            {
                ret_SendReq2AudioThread = app_audio_sendrequest(APP_FACTORYMODE_AUDIO_LOOP, (uint8_t)APP_BT_SETTING_OPEN, 0, 0);
                bt_media_set_current_media(BT_STREAM_MONITOR);
            }
#endif
#ifdef TWS_RBCODEC_PLAYER
            else if(bt_media_is_media_active_by_type(BT_STREAM_RBCODEC))
            {
                enum BT_DEVICE_ID_T sbc_id  = bt_media_get_active_sbc_device();
                bt_media_clear_current_media(BT_STREAM_RBCODEC);
                notify_tws_player_status(APP_BT_SETTING_OPEN);
                notify_local_player_status(APP_BT_SETTING_OPEN);
                bt_media_set_current_media(BT_STREAM_RBCODEC);
            }
#endif
#ifdef TWS_LINEIN_PLAYER
            else if(bt_media_is_media_active_by_type(BT_STREAM_LINEIN))
            {
                bt_media_clear_current_media(BT_STREAM_LINEIN);
                LOG_PRINT_MEDIA_MGR("send %x \n", APP_BT_STREAM_LINEIN);
                app_audio_sendrequest(APP_BT_STREAM_LINEIN, (uint8_t)APP_BT_SETTING_OPEN, 0, NULL);
                bt_media_set_current_media(BT_STREAM_LINEIN);
            }
#endif
            else
            {
                //have no meida task,so goto idle
                //app_audio_sendrequest(APP_PLAY_BACK_AUDIO, (uint8_t)APP_BT_SETTING_CLOSE, 0, ptr);
                bt_media_clear_current_media(BT_STREAM_OUT_MASK);
            }
            break;
#endif
        case BT_STREAM_VOICE:
            if(bt_media_is_media_active_by_device(BT_STREAM_VOICE,device_id)==0)
            {
                goto stop_exit;
            }
            bt_media_clear_media_type(stream_type,device_id);
			#if ISOLATED_AUDIO_STREAM_ENABLED
#if VOICE_DATAPATH
            if(bt_media_is_media_active_by_type(BT_STREAM_CAPTURE)) {
                app_audio_sendrequest(APP_BT_STREAM_VOICEPATH, (uint8_t)APP_BT_SETTING_OPEN, 0, 0);
                bt_media_set_current_media(BT_STREAM_CAPTURE);
            }
#endif  
#endif

#ifdef MEDIA_PLAYER_SUPPORT
            if(bt_media_is_media_active_by_type(BT_STREAM_MEDIA))
            {
                if(bt_media_get_current_media() & BT_STREAM_MEDIA)
                {
                    //do nothing
                }
            #if VOICE_DATAPATH
			#if !ISOLATED_AUDIO_STREAM_ENABLED	
				else if(bt_media_is_media_active_by_type(BT_STREAM_CAPTURE)) {
					bt_media_clear_all_media_type();
                    app_audio_sendrequest(APP_BT_STREAM_VOICEPATH, (uint8_t)APP_BT_SETTING_OPEN, 0, 0);
                    bt_media_set_current_media(BT_STREAM_CAPTURE);
                }
			#endif
            #endif	
    
                else
                {
                    ASSERT(0,"if voice and media is all on,media should be the current media");
                }

            }
            else
#endif
        if(bt_media_is_media_active_by_type(BT_STREAM_VOICE))
            {


                //another device is in sco mode,so change to the device
//#ifdef __BT_REAL_ONE_BRING_TWO__
///to do by mql for 2300
#ifdef CHIP_BEST2000
                if(hal_get_chip_metal_id() >= HAL_CHIP_METAL_ID_2)
                {
                    enum BT_DEVICE_ID_T sbc_id  = bt_media_get_active_device_by_type(BT_STREAM_VOICE);
                    btif_hfp_switch_sco(app_bt_device.hf_channel[sbc_id]);
                }
#endif				
//#endif
                bt_media_set_current_media(BT_STREAM_VOICE);
            }
#ifdef TWS_RBCODEC_PLAYER
            else if(bt_media_is_media_active_by_type(BT_STREAM_RBCODEC)) {
                //resume rbcodec
                ret_SendReq2AudioThread = app_audio_sendrequest(APP_BT_STREAM_RBCODEC, (uint8_t)APP_BT_SETTING_OPEN, 0, 0);
                bt_media_set_current_media(BT_STREAM_RBCODEC);
            }
#endif
#ifdef TWS_LINEIN_PLAYER
            else if(bt_media_is_media_active_by_type(BT_STREAM_LINEIN)) {
                //resume linein
                ret_SendReq2AudioThread = app_audio_sendrequest(APP_BT_STREAM_LINEIN, (uint8_t)APP_BT_SETTING_OPEN, 100, 0);
                bt_media_set_current_media(BT_STREAM_LINEIN);
            }
#endif
#if VOICE_DATAPATH		
#if !ISOLATED_AUDIO_STREAM_ENABLED			
            else if(bt_media_is_media_active_by_type(BT_STREAM_CAPTURE)) {
                app_audio_sendrequest(APP_BT_STREAM_VOICEPATH, (uint8_t)APP_BT_SETTING_OPEN, 0, 0);
                bt_media_set_current_media(BT_STREAM_CAPTURE);
            } 
#endif				
#endif
            else if(bt_media_is_media_active_by_type(BT_STREAM_SBC))
            {
                // if another device is also in sbc mode
                enum BT_DEVICE_ID_T sbc_id  = bt_media_get_active_sbc_device();
                ret_SendReq2AudioThread = app_audio_sendrequest(APP_BT_STREAM_HFP_PCM, (uint8_t)APP_BT_SETTING_CLOSE, 0, ptr);
#ifdef __TWS__ 
                bt_media_clear_media_type(BT_STREAM_SBC,sbc_id);
                bt_media_clear_current_media(BT_STREAM_SBC);
				notify_tws_player_status(APP_BT_SETTING_OPEN);
#else
                bt_parse_store_sbc_sample_rate(app_bt_device.sample_rate[sbc_id]);
                ret_SendReq2AudioThread = app_audio_sendrequest(APP_BT_STREAM_A2DP_SBC, (uint8_t)APP_BT_SETTING_OPEN, 0, 0);
                bt_media_set_current_media(BT_STREAM_SBC);
#endif
            }
#ifdef __EARPHONE_AUDIO_MONITOR__
            else if(bt_media_is_media_active_by_type(BT_STREAM_MONITOR))
            {
                ret_SendReq2AudioThread = app_audio_sendrequest(APP_BT_STREAM_HFP_PCM, (uint8_t)APP_BT_SETTING_CLOSE, 0, ptr);
                ret_SendReq2AudioThread = app_audio_sendrequest(APP_FACTORYMODE_AUDIO_LOOP, (uint8_t)APP_BT_SETTING_OPEN, 0, 0);
                bt_media_set_current_media(BT_STREAM_MONITOR);
            }
#endif            
            else
            {
                ret_SendReq2AudioThread = app_audio_sendrequest(APP_BT_STREAM_HFP_PCM, (uint8_t)APP_BT_SETTING_CLOSE, 0, ptr);
                bt_media_set_current_media(0);
            }
            break;
#ifdef __EARPHONE_AUDIO_MONITOR__
        case BT_STREAM_MONITOR:
            bt_media_clear_media_type(stream_type,device_id);
            ///if monitor is working,so stop the monitor
            if(bt_media_get_current_media() & BT_STREAM_MONITOR)
            {
                MEDIA_MANAGE_TRACE("stop the monitor");
                ret_SendReq2AudioThread = app_audio_sendrequest(APP_FACTORYMODE_AUDIO_LOOP, (uint8_t)APP_BT_SETTING_CLOSE, 0, 0);  
                bt_media_set_current_media(0);
            }
#ifdef __TWS__ 
            if(bt_media_is_media_active_by_type(BT_STREAM_SBC))
            {
                enum BT_DEVICE_ID_T sbc_id  = bt_media_get_active_sbc_device();
                bt_media_clear_media_type(BT_STREAM_SBC,sbc_id);
                bt_media_clear_current_media(BT_STREAM_SBC);
                notify_tws_player_status(APP_BT_SETTING_OPEN);
            }
#endif 
            
            break;

#endif         
        default:
            ASSERT(0,"bt_media_close ERROR TYPE");
            break;
    }
#ifdef __BT_ONE_BRING_TWO__
    MEDIA_MANAGE_TRACE("bt_media_stop end media_active = %x,%x,curr_active_media = %x",
        bt_meida.media_active[0],bt_meida.media_active[1], bt_meida.curr_active_media);
#else
    MEDIA_MANAGE_TRACE("bt_media_stop end media_active = %x,curr_active_media = %x",
        bt_meida.media_active[0], bt_meida.curr_active_media);

#endif
stop_exit:
	if (ret_SendReq2AudioThread<0){
		APP_AUDIO_CALLBACK_T callback = (APP_AUDIO_CALLBACK_T)ptr;
		if (callback)
			callback(APP_BT_STREAM_MANAGER_STOP,0);
	}
}


void app_media_stop_media(uint8_t stream_type,enum BT_DEVICE_ID_T device_id, uint32_t ptr)
{
    MEDIA_MANAGE_TRACE("app_media_stop_media");

#ifdef MEDIA_PLAYER_SUPPORT
    if(bt_media_is_media_active_by_type(BT_STREAM_MEDIA))
    {
#ifdef __AUDIO_QUEUE_SUPPORT__
            ////should have no sbc
            app_audio_list_clear();
#endif
        if(bt_media_get_current_media() & BT_STREAM_MEDIA)
        {

            MEDIA_MANAGE_TRACE("bt_media_switch_to_voice stop the media");
            app_audio_sendrequest(APP_PLAY_BACK_AUDIO, (uint8_t)APP_BT_SETTING_CLOSE, 0, ptr);
            bt_media_clear_current_media(BT_STREAM_OUT_MASK);

        }
        bt_media_clear_all_media_type();
        if(bt_media_is_media_active_by_type(BT_STREAM_VOICE))
        {

#ifdef __TWS_CALL_DUAL_CHANNEL__
            app_audio_sendrequest(APP_BT_STREAM_HFP_PCM, (uint8_t)APP_BT_SETTING_OPEN, 1, ptr);
#else        
            app_audio_sendrequest(APP_BT_STREAM_HFP_PCM, (uint8_t)APP_BT_SETTING_OPEN, 0, ptr);
#endif
            bt_media_set_current_media(BT_STREAM_VOICE);
        }
	#if !ISOLATED_AUDIO_STREAM_ENABLED
    #if VOICE_DATAPATH
		else if(bt_media_is_media_active_by_type(BT_STREAM_CAPTURE))
		{
			app_audio_sendrequest(APP_BT_STREAM_VOICEPATH, (uint8_t)APP_BT_SETTING_OPEN, 0, 0);
			bt_media_set_current_media(BT_STREAM_CAPTURE);
		}
    #endif   

	#endif 
        else if(bt_media_is_media_active_by_type(BT_STREAM_SBC))
        {
            enum BT_DEVICE_ID_T sbc_id  = bt_media_get_active_sbc_device();
#ifdef __TWS__ 
            bt_media_clear_media_type(BT_STREAM_SBC,sbc_id);
            bt_media_clear_current_media(BT_STREAM_SBC);
            notify_tws_player_status(APP_BT_SETTING_OPEN);
#else
            bt_parse_store_sbc_sample_rate(app_bt_device.sample_rate[sbc_id]);
            app_audio_sendrequest(APP_BT_STREAM_A2DP_SBC, (uint8_t)APP_BT_SETTING_OPEN, 0, 0);
            bt_media_set_current_media(BT_STREAM_SBC);
#endif
        }
    }
#endif
}

void bt_media_switch_to_voice(uint16_t stream_type,enum BT_DEVICE_ID_T device_id, uint32_t ptr)
{

    MEDIA_MANAGE_TRACE("bt_media_switch_to_voice stream_type= %x,device_id=%x ",stream_type,device_id);
#ifdef __BT_ONE_BRING_TWO__
    MEDIA_MANAGE_TRACE("bt_media_switch_to_voice media_active = %x,%x,curr_active_media = %x",
        bt_meida.media_active[0],bt_meida.media_active[1], bt_meida.curr_active_media);
#else
    MEDIA_MANAGE_TRACE("bt_media_switch_to_voice media_active = %x,curr_active_media = %x",
        bt_meida.media_active[0], bt_meida.curr_active_media);

#endif


    ///already in voice ,so return
    if(bt_media_get_current_media() & BT_STREAM_VOICE)
        return;
    app_media_stop_media(stream_type,device_id, ptr);


}


static bool app_audio_manager_init = false;
static int16_t app_bt_stream_voice_request_cnt = 0;

int app_audio_manager_sendrequest(  uint8_t massage_id,
                                    uint16_t stream_type, 
                                    uint8_t device_id, 
                                    uint8_t aud_id,
                                    uint32_t param,
                                    uint32_t ptr)
{
    uint32_t audevt;
    uint32_t msg0;
    APP_MESSAGE_BLOCK msg;

    if(app_audio_manager_init == false)
        return -1;
    
    if (aud_id == BT_STREAM_VOICE)
        app_bt_stream_voice_request_cnt++;
 
    msg.mod_id = APP_MODUAL_AUDIO_MANAGE;
    APP_AUDIO_MANAGER_SET_MESSAGE(audevt, massage_id, stream_type);
    APP_AUDIO_MANAGER_SET_MESSAGE0(msg0,device_id,aud_id);
    msg.msg_body.message_id = audevt;
    msg.msg_body.message_ptr = ptr;
    msg.msg_body.message_Param0 = msg0;
    msg.msg_body.message_Param1 = param;
    app_mailbox_put(&msg);

    return 0;
}

#if VOICE_DATAPATH&&MIX_MIC_DURING_MUSIC
static bool app_audio_handle_pre_processing(APP_MESSAGE_BODY *msg_body)
{
    uint16_t stream_type;
    APP_AUDIO_MANAGER_GET_STREAM_TYPE(msg_body->message_id, stream_type);
    
    bool isToResetCaptureStream = false;
    if ((BT_STREAM_SBC == stream_type) || (BT_STREAM_MEDIA == stream_type))
    {
        if (app_audio_manager_capture_is_active())
        {
            isToResetCaptureStream = true;
        }
    }

    if (isToResetCaptureStream)
    {
        app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_STOP, 
                BT_STREAM_CAPTURE, 0, 0);

        APP_MESSAGE_BLOCK msg;
        msg.msg_body = *msg_body; 
        msg.mod_id = APP_MODUAL_AUDIO_MANAGE;
        
        app_mailbox_put(&msg);
        app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_START, 
                BT_STREAM_CAPTURE, 0, 0);

        return false;
    }
    else
    {
        return true;
    }
}
#endif
static int app_audio_manager_handle_process(APP_MESSAGE_BODY *msg_body)
{
    int nRet = -1;

    uint8_t id;
    uint16_t stream_type;
    uint8_t device_id;
    uint8_t aud_id;

    uint32_t param;
    uint32_t ptr;

    if(app_audio_manager_init == false)
        return -1;
#if VOICE_DATAPATH&&MIX_MIC_DURING_MUSIC
    bool isContinue = app_audio_handle_pre_processing(msg_body);
    if (!isContinue)
    {
        return -1;
    }
#endif
    LOG_PRINT_MEDIA_MGR("%s enter",__func__);
    
    APP_AUDIO_MANAGER_GET_ID(msg_body->message_id, id);
    APP_AUDIO_MANAGER_GET_STREAM_TYPE(msg_body->message_id, stream_type);
    APP_AUDIO_MANAGER_GET_DEVICE_ID(msg_body->message_Param0, device_id);
    APP_AUDIO_MANAGER_GET_AUD_ID(msg_body->message_Param0, aud_id);
    APP_AUDIO_MANAGER_GET_PARAM(msg_body->message_Param1, param);
    APP_AUDIO_MANAGER_GET_PTR(msg_body->message_ptr, ptr);

    if (aud_id == BT_STREAM_VOICE)
        app_bt_stream_voice_request_cnt--;

    switch (id ) {
        case APP_BT_STREAM_MANAGER_START:
            bt_media_start(stream_type,(enum BT_DEVICE_ID_T) device_id,(AUD_ID_ENUM)aud_id, param, ptr);
            break;
        case APP_BT_STREAM_MANAGER_SETUP:
            bt_media_setup(stream_type,(enum BT_DEVICE_ID_T) device_id,(AUD_ID_ENUM)aud_id, param, ptr);
            break;
        case APP_BT_STREAM_MANAGER_STOP:
            bt_media_stop(stream_type, (enum BT_DEVICE_ID_T)device_id, ptr);
            break;
        case APP_BT_STREAM_MANAGER_SWITCHTO_SCO:
            bt_media_switch_to_voice(stream_type, (enum BT_DEVICE_ID_T)device_id, ptr);
            break;
        case APP_BT_STREAM_MANAGER_STOP_MEDIA:
            app_media_stop_media(stream_type, (enum BT_DEVICE_ID_T)device_id, ptr);
            break;

        default:
            break;
    }

    LOG_PRINT_MEDIA_MGR("%s exit %d",__func__, nRet);

    return nRet;
}

#if VOICE_DATAPATH
bool app_audio_manager_capture_is_active(void)
{
    uint16_t media_type;
    bool nRet = false;

    media_type = bt_media_get_current_media();
    if (media_type & BT_STREAM_CAPTURE){
            nRet = true;
    }

    return nRet;
}
#endif
bool app_audio_manager_bt_stream_voice_request_exist(void)
{
    ASSERT(app_bt_stream_voice_request_cnt>=0,"app_bt_stream_voice_request_cnt is wrong!");

    if (app_bt_stream_voice_request_cnt)
        return true;
    else
        return false;
}

uint16_t app_audio_manager_get_current_media_type(void)
{
    return bt_meida.curr_active_media;
}


void app_audio_manager_open(void)
{
    app_bt_stream_voice_request_cnt = 0;
    bt_meida.media_curr_sbc = BT_DEVICE_NUM;
    bt_meida.curr_active_media = 0;
    app_set_threadhandle(APP_MODUAL_AUDIO_MANAGE, app_audio_manager_handle_process);
//    app_bt_stream_init();
    app_audio_manager_init = true;
}

void app_audio_manager_close(void)
{
    app_set_threadhandle(APP_MODUAL_AUDIO_MANAGE, NULL);
    app_audio_manager_init = false;
}

// FIXME: Manage priority
void app_audio_set_key_priority(void)
{
    LOG_PRINT_MEDIA_MGR("[%s] app: osPriorityHigh, af: osPriorityNormal", __func__);
    app_set_high_priority();
    af_set_priority(osPriorityNormal);
}

void app_audio_reset_priority(void)
{
    LOG_PRINT_MEDIA_MGR("[%s] app: default", __func__);
    app_set_default_priority();
    af_reset_priority();
}

#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)
static bool app_rbcodec_play_status = false;
static bool app_rbplay_player_mode = false;

void app_rbcodec_ctr_play_onoff(bool on ) {
	LOG_PRINT_MEDIA_MGR("local player: %d -> %d", app_rbcodec_play_status, on);
    if(app_rbcodec_play_status == on)
        return;
    app_rbcodec_play_status = on;
    if(on)
        app_audio_manager_sendrequest( APP_BT_STREAM_MANAGER_START, BT_STREAM_RBCODEC, 0, 0, 0, 0);         
    else
        app_audio_manager_sendrequest( APP_BT_STREAM_MANAGER_STOP, BT_STREAM_RBCODEC, 0, 0, 0, 0);      
}

static bool app_linein_play_status = false;
void app_linein_ctr_play_onoff(bool on ) {
    if(app_linein_play_status == on)
        return;
    app_linein_play_status = on;
    if(on)
        app_audio_manager_sendrequest( APP_BT_STREAM_MANAGER_START, BT_STREAM_LINEIN, 0, 0, 0, 0);      
    else
        app_audio_manager_sendrequest( APP_BT_STREAM_MANAGER_STOP, BT_STREAM_LINEIN, 0, 0, 0, 0);       
}


bool app_rbplay_is_localplayer_mode(void)
{
    return app_rbplay_player_mode;
}

bool app_rbplay_mode_switch(void)
{
    return (app_rbplay_player_mode = !app_rbplay_player_mode);
}

void app_rbplay_set_player_mode(bool isInPlayerMode)
{
    app_rbplay_player_mode = isInPlayerMode;
}

void app_rbcodec_ctl_set_play_status(bool st)
{
    app_rbcodec_play_status = st;
    LOG_PRINT_MEDIA_MGR("%s %d",__func__,app_rbcodec_play_status);
}

bool app_rbcodec_get_play_status(void)
{
    LOG_PRINT_MEDIA_MGR("%s %d",__func__,app_rbcodec_play_status);
    return app_rbcodec_play_status;
}

void app_rbcodec_toggle_play_stop(void)
{
    if(app_rbcodec_get_play_status()) {
        app_rbcodec_ctr_play_onoff(false);
    } else {
        app_rbcodec_ctr_play_onoff(true);
    }
}

bool app_rbcodec_check_hfp_active(void )
{
    return (bool)bt_media_is_media_active_by_type(BT_STREAM_VOICE);
}
#endif

//extern uint16_t bt_sco_get_current_codecid(void);
extern "C" hfp_sco_codec_t bt_sco_get_current_codecid(void);
int app_audio_manager_sco_status_checker(void)
{
    btif_cmgr_handler_t    *cmgrHandler;
    uint32_t scoTransmissionInterval_reg;
    
    TRACE("%s enter", __func__);
#ifdef CHIP_BEST2300
    //cmgrHandler = &app_bt_device.hf_channel[BT_DEVICE_ID_1].cmgrHandler;
    cmgrHandler = btif_hf_get_chan_manager_handler(app_bt_device.hf_channel[BT_DEVICE_ID_1]);
    LOG_PRINT_MEDIA_MGR("type:%d Interval:%d Retrans:%d Interval_reg:%d", 
                                                     btif_cmgr_get_sco_connect_sco_link_type(cmgrHandler),
                                                     btif_cmgr_get_sco_connect_sco_rx_parms_sco_transmission_interval(cmgrHandler),
                                                     btif_cmgr_get_sco_connect_sco_rx_parms_sco_retransmission_window(cmgrHandler),
                                                     scoTransmissionInterval_reg);
#else
    BTDIGITAL_REG_GET_FIELD(0xD0220120, 0xff, 24, scoTransmissionInterval_reg);
    //cmgrHandler = &app_bt_device.hf_channel[BT_DEVICE_ID_1].cmgrHandler;
    cmgrHandler = btif_hf_get_chan_manager_handler(app_bt_device.hf_channel[BT_DEVICE_ID_1]);
    //TRACE("type:%d Interval:%d Retrans:%d Interval_reg:%d", 
    if ( btif_cmgr_is_audio_up(cmgrHandler) ){
        if (btif_cmgr_get_sco_connect_sco_link_type(cmgrHandler) == BTIF_BLT_ESCO){
            if (btif_cmgr_get_sco_connect_sco_rx_parms_sco_transmission_interval(cmgrHandler) != scoTransmissionInterval_reg){
                BTDIGITAL_REG_SET_FIELD(0xD0220120, 0xff, 24, btif_cmgr_get_sco_connect_sco_rx_parms_sco_transmission_interval(cmgrHandler));
            }
        }
    }        
    LOG_PRINT_MEDIA_MGR("type:%d Interval:%d Retrans:%d Interval_reg:%d", 
                                                     btif_cmgr_get_sco_connect_sco_link_type(cmgrHandler),
                                                     btif_cmgr_get_sco_connect_sco_rx_parms_sco_transmission_interval(cmgrHandler),
                                                     btif_cmgr_get_sco_connect_sco_rx_parms_sco_retransmission_window(cmgrHandler),
                                                     scoTransmissionInterval_reg);
    
    //TODO:
    //DUMP8("%02x ", app_bt_device.hf_channel[BT_DEVICE_ID_1].cmgrHandler.remDev->bdAddr.addr, 6);

#if defined(HFP_1_6_ENABLE)
    hfp_sco_codec_t  code_type;
    uint32_t code_type_reg;		
    code_type = bt_sco_get_current_codecid();
    code_type_reg = BTDIGITAL_REG(0xD0222000);
    if (code_type == BTIF_HF_SCO_CODEC_MSBC) {
        BTDIGITAL_REG(0xD0222000) = (code_type_reg & (~(7<<1))) | (3<<1);
        LOG_PRINT_MEDIA_MGR("MSBC!!!!!!!!!!!!!!!!!!! REG:0xD0222000=0x%08x B:%d", BTDIGITAL_REG(0xD0222000), (BTDIGITAL_REG(0xD0222000)>>15)&1);
        
    }else{    
        BTDIGITAL_REG(0xD0222000) = (code_type_reg & (~(7<<1))) | (2<<1);
        LOG_PRINT_MEDIA_MGR("CVSD!!!!!!!!!!!!!!!!!!! REG:0xD0222000=0x%08x B:%d", BTDIGITAL_REG(0xD0222000), (BTDIGITAL_REG(0xD0222000)>>15)&1);
    }
#else
    uint32_t code_type_reg;		
    code_type_reg = BTDIGITAL_REG(0xD0222000);
    BTDIGITAL_REG(0xD0222000) = (code_type_reg & (~(7<<1))) | (2<<1);
    LOG_PRINT_MEDIA_MGR("CVSD!!!!!!!!!!!!!!!!!!! REG:0xD0222000=0x%08x B:%d", BTDIGITAL_REG(0xD0222000), (BTDIGITAL_REG(0xD0222000)>>15)&1);
#endif
#endif
    LOG_PRINT_MEDIA_MGR("%s exit", __func__);
    return 0;
}

