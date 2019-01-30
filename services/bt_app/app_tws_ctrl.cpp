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

#include "me_api.h"
#include "a2dp_api.h"


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
#include "app_tws_if.h"
#include "hal_chipid.h"
#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)
#include "player_role.h"
#endif

#include "app_tws_cmd_handler.h"

#include "app_bt_func.h"
#include "app_tws_ui.h"
extern  tws_dev_t  tws;
extern int hfp_volume_set(int vol);


static void tws_ctrl_thread(const void *arg);
 osThreadDef(tws_ctrl_thread, osPriorityHigh, 1536);

#define TWS_CTRL_MAILBOX_MAX (10)
 osMailQDef (tws_ctl_mailbox, TWS_CTRL_MAILBOX_MAX, TWS_MSG_BLOCK);
int hfp_volume_get(void);


uint8_t tws_mail_count=0;

int tws_ctrl_mailbox_put(TWS_MSG_BLOCK* msg_src)
{
    osStatus status;

    TWS_MSG_BLOCK *msg_p = NULL;

    msg_p = (TWS_MSG_BLOCK*)osMailAlloc(tws.tws_mailbox, 0);
    if(!msg_p) {
        TRACE("%s fail, evt:%d,arg=%d \n",__func__,msg_src->evt,msg_src->arg);
        return -1;
    }

    ASSERT(msg_p, "osMailAlloc error");

    msg_p->evt = msg_src->evt;
    msg_p->arg = msg_src->arg;

    status = osMailPut(tws.tws_mailbox, msg_p);
    if(status == osOK)
        tws_mail_count ++;
    return (int)status;
}

int tws_ctrl_mailbox_free(TWS_MSG_BLOCK* msg_p)
{
    osStatus status;

    status = osMailFree(tws.tws_mailbox, msg_p);
    if (osOK == status)
    tws_mail_count--;
    return (int)status;
}

int tws_ctrl_mailbox_get(TWS_MSG_BLOCK** msg_p)
{
    osEvent evt;
    evt = osMailGet(tws.tws_mailbox, osWaitForever);
    if (evt.status == osEventMail) {
        *msg_p = (TWS_MSG_BLOCK *)evt.value.p;
        return 0;
    }
    return -1;
}

static int tws_ctrl_mailbox_init(void)
{
    tws.tws_mailbox = osMailCreate(osMailQ(tws_ctl_mailbox), NULL);
    if (tws.tws_mailbox == NULL)  {
        TRACE("Failed to Create tws_ctl_mailbox\n");
        return -1;
    }
    return 0;
}

void tws_ctrl_thread_init(void)
{
    tws_ctrl_mailbox_init();
    tws.tws_ctrl_tid = osThreadCreate(osThread(tws_ctrl_thread), NULL);
}

static bool app_tws_set_codec_type(uint8_t codec_type, uint8_t sample_bit, uint32_t sample_rate)
{
    APP_TWS_SET_TWS_CODEC_TYPE_T req;
    req.codec_type = codec_type;
    req.sample_bit = sample_bit;
    req.sample_rate = sample_rate;

    bool ret = app_tws_send_cmd_with_rsp(APP_TWS_CMD_SET_CODEC_TYPE, (uint8_t *)&req, sizeof(req), false);
    TRACE("Set codec type setting completed type:%d bit:%d samplerate:%d\n", app_tws_get_env_pointer()->mobile_codectype,
                                                                             app_tws_get_env_pointer()->mobile_samplebit,
                                                                             app_tws_get_env_pointer()->mobile_samplerate);
    tws.tws_set_codectype_cmp = 1;
    return ret;
}

static void app_tws_codec_configuraiton_handler(uint8_t* ptrParam, uint32_t paramLen)
{
    APP_TWS_SET_TWS_CODEC_TYPE_T* pReq = (APP_TWS_SET_TWS_CODEC_TYPE_T *)ptrParam;
#if defined(SBC_TRANS_TO_AAC)
    if (pReq->codec_type == BTIF_AVDTP_CODEC_TYPE_SBC)
        pReq->codec_type = BTIF_AVDTP_CODEC_TYPE_MPEG2_4_AAC;
#endif
    a2dp_set_codec_type(BT_DEVICE_ID_1, pReq->codec_type);
    a2dp_set_sample_bit(BT_DEVICE_ID_1, pReq->sample_bit);
    app_tws_get_env_pointer()->mobile_codectype = pReq->codec_type;
    app_tws_get_env_pointer()->mobile_samplebit = pReq->sample_bit;
    app_tws_get_env_pointer()->mobile_samplerate = pReq->sample_rate;
    TRACE("APP_TWS_CMD_SET_CODEC_TYPE type:%d bit:%d samplerate:%d\n", app_tws_get_env_pointer()->mobile_codectype,
                                                                       app_tws_get_env_pointer()->mobile_samplebit,
                                                                       app_tws_get_env_pointer()->mobile_samplerate);
    app_tws_send_cmd_rsp(NULL, 0);
}

TWS_CUSTOM_COMMAND_TO_ADD(APP_TWS_CMD_SET_CODEC_TYPE, app_tws_codec_configuraiton_handler);

static bool app_tws_set_hfp_volume(int8_t volume)
{
    APP_TWS_SET_TWS_HFP_VOLUME_T req;
    req.vol = volume+0x10;

    return app_tws_send_cmd_with_rsp(APP_TWS_CMD_SET_HFP_VOLUME, (uint8_t *)&req, sizeof(req), false);
}

static void app_tws_hfp_volume_configuraiton_handler(uint8_t* ptrParam, uint32_t paramLen)
{
    APP_TWS_SET_TWS_HFP_VOLUME_T* pReq = (APP_TWS_SET_TWS_HFP_VOLUME_T *)ptrParam;
    tws.hfp_volume = pReq->vol-0x10;
    TRACE("APP_TWS_CMD_SET_HFP_VOLUME %d \n", tws.hfp_volume);
    hfp_volume_set(tws.hfp_volume);

    app_tws_send_cmd_rsp(NULL, 0);
}

TWS_CUSTOM_COMMAND_TO_ADD(APP_TWS_CMD_SET_HFP_VOLUME, app_tws_hfp_volume_configuraiton_handler);

static bool app_tws_restart_player(int8_t restart)
{
    APP_TWS_SET_TWS_PLATER_RESTART_T set_tws_player_restart;
    TRACE("APP_SET_TWS_PLATER_RESTART restart=%d",restart);
    set_tws_player_restart.restart = 1;

    return app_tws_send_cmd_with_rsp(APP_SET_TWS_PLATER_RESTART, (uint8_t *)&set_tws_player_restart, sizeof(set_tws_player_restart), false);
}

static void app_tws_restart_player_handler(uint8_t* ptrParam, uint32_t paramLen)
{
    APP_TWS_SET_TWS_PLATER_RESTART_T* pReq = (APP_TWS_SET_TWS_PLATER_RESTART_T *)ptrParam;
    if(TWS_MASTER_CONN_SLAVE == app_tws_get_conn_state() &&
        (bt_syn_get_curr_ticks(app_tws_get_tws_conhdl()) > tws.player.last_trigger_time))
        tws.tws_player_restart = pReq->restart;
    TRACE("APP_SET_TWS_PLATER_RESTART %d \n", tws.tws_player_restart);

    app_tws_send_cmd_rsp(NULL, 0);
}

TWS_CUSTOM_COMMAND_TO_ADD(APP_SET_TWS_PLATER_RESTART, app_tws_restart_player_handler);

static bool app_tws_notify_key(uint32_t key)
{
    APP_TWS_SET_TWS_NOTIFY_KEY_T tws_player_notify_key;
    TRACE("tws_spp_notify_key restart=%d",key);
    tws_player_notify_key.key = key;
    return app_tws_send_cmd_with_rsp(APP_TWS_CMD_CTRL_SLAVE_KEY_NOTIFY, (uint8_t *)&tws_player_notify_key, sizeof(tws_player_notify_key), false);
}

static void app_tws_notify_key_handler(uint8_t* ptrParam, uint32_t paramLen)
{
    APP_TWS_SET_TWS_NOTIFY_KEY_T* key_notify = (APP_TWS_SET_TWS_NOTIFY_KEY_T *)ptrParam;

    if (TWS_MASTER_CONN_SLAVE == app_tws_get_conn_state())
    {
        bt_key_send((uint16_t)(key_notify->key&0xffff),(uint16_t)(key_notify->key>>16));
    }
    TRACE("TWS_SPP_CMD_NOTIFY_KEY %d \n", key_notify->key);
    app_tws_send_cmd_rsp(NULL, 0);
}

TWS_CUSTOM_COMMAND_TO_ADD(APP_TWS_CMD_CTRL_SLAVE_KEY_NOTIFY, app_tws_notify_key_handler);

static APP_TWS_SET_TWS_CODEC_TYPE_T old_mobilecodec = {
    BTIF_AVDTP_CODEC_TYPE_NON_A2DP,
    0,
    0
};

void tws_reset_saved_codectype(void)
{
    old_mobilecodec.codec_type = BTIF_AVDTP_CODEC_TYPE_NON_A2DP;
    old_mobilecodec.sample_bit = 0;
    old_mobilecodec.sample_rate = 0;
}

static APP_TWS_SET_TWS_CODEC_TYPE_T* tws_get_saved_codec(void)
{
    return &old_mobilecodec;
}

void tws_set_saved_codectype(uint8_t codec_type, uint8_t sample_bit, uint32_t sample_rate)
{
    old_mobilecodec.codec_type = codec_type;
    old_mobilecodec.sample_bit = sample_bit;
    old_mobilecodec.sample_rate = sample_rate;
}

#ifdef TWS_RING_SYNC

uint32_t slave_media_trigger_time=0;
uint32_t master_media_trigger_time=0;
#define MEDIA_TRIGGER_DELAY 0x600

uint32_t tws_get_media_trigger_time()
{
    uint32_t curr_ticks;
    if(app_tws_mode_is_master())
    {
        return master_media_trigger_time;
    }
    if(app_tws_mode_is_slave())
    {
        curr_ticks = bt_syn_get_curr_ticks(app_tws_get_tws_conhdl());
        if(slave_media_trigger_time<curr_ticks)
            TRACE("TRIGGER ERROR !!!");
        else
            TRACE("TRIGGER OK !!!");
       
        return slave_media_trigger_time;
    }
}

void tws_set_media_trigger_time(uint32_t triggerTime)
{

    if(app_tws_mode_is_master())
    {
         master_media_trigger_time=triggerTime;
         TRACE("master_media_trigger_time:%d",master_media_trigger_time);
    }
    if(app_tws_mode_is_slave())
    {
         slave_media_trigger_time=triggerTime;
         TRACE("slave_media_trigger_time:%d",slave_media_trigger_time);
    }
}

uint32_t app_media_caculate_trigger_time()
{
    uint32_t curr_ticks;
    uint32_t trigger_time=0;
    if(app_tws_mode_is_master())
    {
        curr_ticks = bt_syn_get_curr_ticks(app_tws_get_tws_conhdl());
        trigger_time=curr_ticks+MEDIA_TRIGGER_DELAY;
    }
    else
    {
        curr_ticks = btdrv_syn_get_curr_ticks();
    }

    return trigger_time;
    
}
static bool app_tws_send_ring_sync(U16 hf_event)
{
    TRACE("%s hf_event:%d", __func__,hf_event);

    APP_TWS_CMD_RING_SYNC_T req;
    req.hf_event = hf_event;
    req.trigger_time=app_media_caculate_trigger_time();
    tws_set_media_trigger_time(req.trigger_time);

    return app_tws_send_cmd_without_rsp(APP_TWS_CMD_RING_SYNC, (uint8_t *)&req, sizeof(req));
}

static void app_tws_cmd_ring_sync_handler(uint8_t* ptrParam, uint32_t paramLen)
{
    TRACE("%s", __func__);
    APP_TWS_CMD_RING_SYNC_T* pReq = (APP_TWS_CMD_RING_SYNC_T *)ptrParam;
    TRACE("pReq->hf_event,pReq->trigger_time:%x,%d\n", pReq->hf_event,pReq->trigger_time);
    tws_set_media_trigger_time(pReq->trigger_time);
    if(pReq->hf_event == BTIF_HF_EVENT_RING_IND){
        app_voice_report(APP_STATUS_INDICATION_INCOMINGCALL,0);
    }

}

TWS_CUSTOM_COMMAND_TO_ADD(APP_TWS_CMD_RING_SYNC, app_tws_cmd_ring_sync_handler);

int tws_ring_sync(U16 hf_event)
{
    TWS_MSG_BLOCK msg;
    msg.arg = hf_event;
    msg.evt = TWS_CTRL_RING_SYNC;
    //becase tws_audio_sendrequest will block the evmprocess
    //had better to create a new thread to avoid fall into a dead trap;
    tws_ctrl_mailbox_put(&msg);
    return 0;
}
#endif

#ifdef ANC_APP
bool app_tws_player_set_anc_status(uint32_t status)
{
    TRACE("app_tws_player_set_anc_status =%d",status);

    APP_TWS_SET_TWS_NOTIFY_KEY_T app_tws_player_notify_key;
    app_tws_player_notify_key.key = status;

    return app_tws_send_cmd_without_rsp(APP_TWS_CMD_CTRL_ANC_SET_STATUS, (uint8_t *)&app_tws_player_notify_key, sizeof(app_tws_player_notify_key));
}
extern "C" void app_anc_change_to_status(uint32_t status);
static void app_tws_player_set_anc_status_handler(uint8_t* ptrParam, uint32_t paramLen)
{
    TRACE("%s", __func__);
    APP_TWS_SET_TWS_NOTIFY_KEY_T* app_tws_player_notify_key = (APP_TWS_SET_TWS_NOTIFY_KEY_T *)ptrParam;
    app_anc_change_to_status(app_tws_player_notify_key->key);
}

TWS_CUSTOM_COMMAND_TO_ADD(APP_TWS_CMD_CTRL_ANC_SET_STATUS, app_tws_player_set_anc_status_handler);

int tws_player_set_anc_status(uint32_t status)
{
    TWS_MSG_BLOCK msg;
    msg.arg = (uint32_t)(status);
    msg.evt = TWS_CTRL_ANC_SET_STATUS;
    //becase tws_audio_sendrequest will block the evmprocess
    //had better to create a new thread to avoid fall into a dead trap;
    tws_ctrl_mailbox_put(&msg);

    return 0;
}
/*
bool app_tws_player_notify_anc_status(uint32_t status)
{
    TRACE("app_tws_player_notify_anc_status = %d",status);

    APP_TWS_SET_TWS_NOTIFY_KEY_T app_tws_player_notify_key;
    app_tws_player_notify_key.key = status;

    return app_tws_send_cmd_without_rsp(APP_TWS_CMD_CTRL_ANC_NOTIFY_STATUS, (uint8_t *)&app_tws_player_notify_key, sizeof(app_tws_player_notify_key));
}

static void app_tws_player_notify_anc_status_handler(uint8_t* ptrParam, uint32_t paramLen)
{
    TRACE("%s", __func__);
    APP_TWS_SET_TWS_NOTIFY_KEY_T* app_tws_player_notify_key = (APP_TWS_SET_TWS_NOTIFY_KEY_T *)ptrParam;
}

TWS_CUSTOM_COMMAND_TO_ADD(APP_TWS_CMD_CTRL_ANC_NOTIFY_STATUS, app_tws_player_notify_anc_status_handler);

int tws_player_notify_anc_status(void)
{
    TWS_MSG_BLOCK msg;
    msg.arg = 0;
    msg.evt = TWS_CTRL_ANC_NOTIFY_STATUS;
    //becase tws_audio_sendrequest will block the evmprocess
    //had better to create a new thread to avoid fall into a dead trap;
    tws_ctrl_mailbox_put(&msg);

    return 0;
}


bool app_tws_player_get_anc_status(uint32_t status)
{
    TRACE("app_tws_player_get_anc_status = %d",status);

    APP_TWS_SET_TWS_NOTIFY_KEY_T app_tws_player_notify_key;
    app_tws_player_notify_key.key = status;

    return app_tws_send_cmd_without_rsp(APP_TWS_CMD_CTRL_ANC_GET_STATUS, (uint8_t *)&app_tws_player_notify_key, sizeof(app_tws_player_notify_key));
}

static void app_tws_player_get_anc_status_handler(uint8_t* ptrParam, uint32_t paramLen)
{
    TRACE("%s", __func__);
    APP_TWS_SET_TWS_NOTIFY_KEY_T* app_tws_player_notify_key = (APP_TWS_SET_TWS_NOTIFY_KEY_T *)ptrParam;
}

TWS_CUSTOM_COMMAND_TO_ADD(APP_TWS_CMD_CTRL_ANC_GET_STATUS, app_tws_player_get_anc_status_handler);

int tws_player_get_anc_status(void)
{
    TWS_MSG_BLOCK msg;
    msg.arg = 0;
    msg.evt = TWS_CTRL_ANC_GET_STATUS;
    //becase tws_audio_sendrequest will block the evmprocess
    //had better to create a new thread to avoid fall into a dead trap;
    tws_ctrl_mailbox_put(&msg);

    return 0;
}
*/
#endif

void tws_ctrl_thread(const void *arg)
{
    TWS_MSG_BLOCK *msg_p = NULL;
    tws_dev_t *twsd = &tws;

    while(1){
        tws_ctrl_mailbox_get(&msg_p);
        TRACE("tws_ctrl_thread evt = %d\n", msg_p->evt);
        switch(msg_p->evt){

            case TWS_CTRL_PLAYER_SETCODEC:
                if (TWS_MASTER_CONN_SLAVE == app_tws_get_conn_state()){
                    APP_TWS_SET_TWS_CODEC_TYPE_T* codec = tws_get_saved_codec();
                    if(codec->codec_type != tws.mobile_codectype ||
                       codec->sample_bit != tws.mobile_samplebit ||
                       codec->sample_rate != tws.mobile_samplerate){
                       app_tws_set_codec_type(tws.mobile_codectype,tws.mobile_samplebit, tws.mobile_samplerate);
                    }else{
                        twsd->tws_set_codectype_cmp = 1;
                    }
                    tws_set_saved_codectype(tws.mobile_codectype, tws.mobile_samplebit, tws.mobile_samplerate);
                }

                break;

            case TWS_CTRL_PLAYER_SET_HFP_VOLUME:
                if (TWS_MASTER_CONN_SLAVE == app_tws_get_conn_state()) {
                        app_tws_set_hfp_volume(hfp_volume_get());
                }
                break;
            case TWS_CTRL_PLAYER_RESTART_PLAYER:
                if(TWS_SLAVE_CONN_MASTER == app_tws_get_conn_state()){
                        app_tws_restart_player(1);
                }
                break;
            case TWS_CTRL_SLAVE_KEY_NOTIFY:
                if(TWS_SLAVE_CONN_MASTER == app_tws_get_conn_state()){
                        app_tws_notify_key(msg_p->arg);
                }
                break;
            case TWS_CTRL_LBRT_PING_REQ:
                if(TWS_MASTER_CONN_SLAVE == app_tws_get_conn_state()){
#ifdef LBRT
                     app_tws_req_lbrt_ping(msg_p->arg);
#endif
                }
                break;
#ifdef ANC_APP
             case TWS_CTRL_ANC_SET_STATUS:
                app_tws_player_set_anc_status(msg_p->arg);
                break;
/*
             case TWS_CTRL_ANC_NOTIFY_STATUS:
                app_tws_player_notify_anc_status(msg_p->arg);
                break;

             case TWS_CTRL_ANC_GET_STATUS:
                app_tws_player_get_anc_status(msg_p->arg);
                break;
*/
#endif
            case TWS_CTRL_RING_SYNC:
#ifdef TWS_RING_SYNC
                if (TWS_MASTER_CONN_SLAVE == app_tws_get_conn_state()) {
                    app_tws_send_ring_sync(msg_p->arg);
                }
#endif
                break;
#ifdef __TWS_ROLE_SWITCH__
             case TWS_CTRL_SEND_PROFILE_DATA:
                tws_role_switch_exchange_profile_data(msg_p->arg);
                //app_bt_SPP_Write_Cmd(TWS_SPP_SEND_PROFILE_DATA, msg_p->arg, spp_data_test, 500);
                break;
#endif

            case TWS_CTRL_SHARE_TWS_ESCO_RETX_NB:
                 if (TWS_MASTER_CONN_SLAVE == app_tws_get_conn_state()) {
                     app_tws_share_esco_retx_nb(msg_p->arg);
                 }
               break;

            default:
                break;
        }
        tws_ctrl_mailbox_free(msg_p);
    }
}

int tws_player_set_codec( btif_avdtp_codec_type_t codec_type)
{
    TRACE("Set codec as %d", codec_type);
    TWS_MSG_BLOCK msg;
    msg.arg = 0;
    msg.evt = TWS_CTRL_PLAYER_SETCODEC;
    //becase tws_audio_sendrequest will block the evmprocess
    //had better to create a new thread to avoid fall into a dead trap;
    tws_ctrl_mailbox_put(&msg);
    return 0;
}

int tws_player_set_hfp_vol( int8_t vol)
{
    TWS_MSG_BLOCK msg;
    msg.arg = vol;
    msg.evt = TWS_CTRL_PLAYER_SET_HFP_VOLUME;
    //becase tws_audio_sendrequest will block the evmprocess
    //had better to create a new thread to avoid fall into a dead trap;
    tws_ctrl_mailbox_put(&msg);
    return 0;
}

int tws_player_pause_player_req( int8_t req)
{
    TWS_MSG_BLOCK msg;
    msg.arg = req;
    msg.evt = TWS_CTRL_PLAYER_RESTART_PLAYER;
    //becase tws_audio_sendrequest will block the evmprocess
    //had better to create a new thread to avoid fall into a dead trap;
    tws_ctrl_mailbox_put(&msg);
    return 0;
}

int tws_player_spp_connected( unsigned char para)
{
#if defined(A2DP_AAC_ON)
    TWS_MSG_BLOCK msg;
    msg.arg = (uint32_t)para;
    msg.evt = TWS_CTRL_PLAYER_SPPCONNECT;
    //becase tws_audio_sendrequest will block the evmprocess
    //had better to create a new thread to avoid fall into a dead trap;
    tws_ctrl_mailbox_put(&msg);
#endif
    return 0;
}

int tws_player_notify_key( unsigned short key,unsigned short event)
{
    TWS_MSG_BLOCK msg;
    msg.arg = (uint32_t)(key | (event<<16));
    msg.evt = TWS_CTRL_SLAVE_KEY_NOTIFY;
    //becase tws_audio_sendrequest will block the evmprocess
    //had better to create a new thread to avoid fall into a dead trap;
    tws_ctrl_mailbox_put(&msg);
    return 0;
}

#ifdef LBRT
int tws_lbrt_ping_req(uint32_t current_ticks)
{
    TWS_MSG_BLOCK msg;
    msg.arg = current_ticks;
    msg.evt = TWS_CTRL_LBRT_PING_REQ;
    //becase tws_audio_sendrequest will block the evmprocess
    //had better to create a new thread to avoid fall into a dead trap;
    tws_ctrl_mailbox_put(&msg);
    return 0;
}
#endif// LBRT
#ifdef __TWS_ROLE_SWITCH__
int tws_send_profile_data( uint32_t data_structure)
{
    TWS_MSG_BLOCK msg;
    msg.arg = data_structure;
    msg.evt = TWS_CTRL_SEND_PROFILE_DATA;
    //becase tws_audio_sendrequest will block the evmprocess
    //had better to create a new thread to avoid fall into a dead trap;
    tws_ctrl_mailbox_put(&msg);
    return 0;
}
#endif
int tws_send_esco_retx_nb( uint32_t retx_nb)
{
    TWS_MSG_BLOCK msg;
    msg.arg = retx_nb;
    msg.evt = TWS_CTRL_SHARE_TWS_ESCO_RETX_NB;
    tws_ctrl_mailbox_put(&msg);
    return 0;
}

#endif// TWS
