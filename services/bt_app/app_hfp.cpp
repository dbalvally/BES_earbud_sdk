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
#include <stdio.h>
#include "cmsis_os.h"
#include "hal_uart.h"
#include "hal_timer.h"
#include "audioflinger.h"
#include "lockcqueue.h"
#include "hal_trace.h"
#include "hal_cmu.h"
#include "hal_chipid.h"
#include "analog.h"
#include "app_audio.h"
#include "app_status_ind.h"
#include "nvrecord.h"
#include "nvrecord_env.h"
#include "bt_drv_interface.h"

#include "hfp_api.h"
#include "bluetooth.h"
#include "me_api.h"
#include "a2dp_api.h"

#include "bt_drv_reg_op.h"
#include "app_bt_func.h"

#include "rtos.h"
#include "besbt.h"

#include "cqueue.h"
#include "btapp.h"
#include "app_bt.h"
#include "app_hfp.h"

#include "nvrecord.h"

#include "apps.h"
#include "resources.h"

#include "app_bt_media_manager.h"
#include "app_battery.h"

#include "app_tws.h"
#include "app_tws_if.h"
#include "app_bt_conn_mgr.h"
#include "app_tws_ui.h"
#include "app_tws_role_switch.h"
#include "log_section.h"
#if VOICE_DATAPATH
#include "app_voicepath.h"
#endif

#include "ble_tws.h"

#ifdef __AMA_VOICE__
#include "app_ama_handle.h"
#endif

#define HF_VOICE_DISABLE  0

#define HF_VOICE_ENABLE   1

#define HF_SENDBUFF_SIZE (320)
#define HF_SENDBUFF_MEMPOOL_NUM (2)

/*
struct HF_SENDBUFF_CONTROL_T {
	struct HF_SENDBUFF_MEMPOOL_T {
		BtPacket packet;
		uint8_t buffer[HF_SENDBUFF_SIZE];
	} mempool[HF_SENDBUFF_MEMPOOL_NUM];
	uint8_t index;
};
*/


/* hfp */
int store_voicebtpcm_m2p_buffer(unsigned char *buf, unsigned int len);
int get_voicebtpcm_p2m_frame(unsigned char *buf, unsigned int len);

int store_voicepcm_buffer(unsigned char *buf, unsigned int len);
int store_voicecvsd_buffer(unsigned char *buf, unsigned int len);
int store_voicemsbc_buffer(unsigned char *buf, unsigned int len);
int8_t app_battery_current_level(void);

extern "C" void switch_to_high_speed_conn_interval(void);
extern "C" void switch_to_low_speed_conn_interval(void);

#ifndef _SCO_BTPCM_CHANNEL_
static struct HF_SENDBUFF_CONTROL_T  hf_sendbuff_ctrl;
#endif

#if defined(SCO_LOOP)
#define HF_LOOP_CNT (20)
#define HF_LOOP_SIZE (360)

static uint8_t hf_loop_buffer[HF_LOOP_CNT*HF_LOOP_SIZE];
static uint32_t hf_loop_buffer_len[HF_LOOP_CNT];
static uint32_t hf_loop_buffer_valid = 1;
static uint32_t hf_loop_buffer_size = 0;
static char hf_loop_buffer_w_idx = 0;
#endif

static void app_hfp_event_callback(hf_chan_handle_t chan, struct hfp_context *ctx);

typedef void (*btapp_sniffer_sco_status_callback)(uint16_t conhdl,uint8_t sco_status,uint8_t airmode,uint32_t bandwidth);

btapp_sniffer_sco_status_callback sniffer_sco_status_callback;

extern struct BT_DEVICE_T  app_bt_device;

extern void app_bt_fill_mobile_hfp_channel(uint32_t deviceId, hf_chan_handle_t chan);

#define HFP_AUDIO_CLOSED_DELAY_RESUME_ADV_IN_MS		1500
static void app_hfp_audio_closed_delay_resume_ble_adv_timer_cb(void const *n);
osTimerDef (APP_HFP_AUDIO_CLOSED_DELAY_RESUME_BLE_ADV_TIMER, 
    app_hfp_audio_closed_delay_resume_ble_adv_timer_cb); 
osTimerId	app_hfp_audio_closed_delay_resume_ble_adv_timer_id = NULL;

#ifdef __TWS_CALL_DUAL_CHANNEL__
#define SNIFFER_SCO_CHECK_TIMEOUT (2000)

void btapp_sniffer_sco_check_timeout_handler(void const *param);
osTimerDef (SNIFFER_SCO_CHECK_TIMER, btapp_sniffer_sco_check_timeout_handler);
osTimerId POSSIBLY_UNUSED btapp_sniffer_sco_check_timer = NULL;
uint16_t btapp_sniffer_sco_check_connHandle;

void btapp_sniffer_sco_check_timeout_handler(void const *param)
{
    uint16_t connHandle = *(uint16_t *)param;

    LOG_PRINT_HFP("btapp_sniffer_sco_check_timeout_handler:%x",connHandle);
    app_bt_ME_BLOCK_RF(connHandle,0);
}

void btapp_sniffer_sco_check_start(uint16_t conhandle)
{
    btapp_sniffer_sco_check_connHandle = conhandle;
    osTimerStop(btapp_sniffer_sco_check_timer);
    osTimerStart(btapp_sniffer_sco_check_timer, SNIFFER_SCO_CHECK_TIMEOUT);
}

void btapp_sniffer_sco_check_init(void)
{
    if (btapp_sniffer_sco_check_timer == NULL){
        btapp_sniffer_sco_check_timer = osTimerCreate(osTimer(SNIFFER_SCO_CHECK_TIMER), osTimerOnce, &btapp_sniffer_sco_check_connHandle);
    }
}

#ifndef FPGA
extern uint8_t app_poweroff_flag;
#endif
uint8_t slave_sco_active = 0;
uint16_t sniff_sco_hcihandle = 0;
void btapp_sniffer_sco_start(uint16_t conhdl,uint8_t sco_status,uint8_t airmode,uint32_t bandwidth)
{
    LOG_PRINT_HFP("conhdl=%x,scostatus=%x,airmode=%x,samplerate=%x ct=%d",conhdl,sco_status,airmode,bandwidth,hal_sys_timer_get());

#if defined(HFP_1_6_ENABLE)
    if(airmode == 3) {
        bt_sco_set_current_codecid(BTIF_HF_SCO_CODEC_MSBC);
    }
    else if(airmode == 2) {
        bt_sco_set_current_codecid(BTIF_HF_SCO_CODEC_CVSD);
    }
#endif

#ifndef FPGA
    if(app_poweroff_flag ==1)
        return;
#endif
    if(sco_status == 1 || sco_status == 3) {
        sniff_sco_hcihandle = conhdl;
        bt_drv_reg_op_sco_sniffer_checker();
#ifdef __ENABLE_WEAR_STATUS_DETECT__
        app_tws_syn_wearing_status();
#endif
        slave_sco_active = 1;
        app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_START,BT_STREAM_VOICE,BT_DEVICE_ID_1,MAX_RECORD_NUM,1,0);
    }
    else if (sco_status == 0 || sco_status == 2) {
        if ( masterBtRemDev)
        {
                btdrv_dynamic_patch_sco_status_clear();
#ifdef __LARGE_SYNC_WIN__
                if(sco_status == 0)
                {
                    //dynamic close the enlarge winsize patch
                    bt_drv_reg_op_enlarge_sync_winsize(sniff_sco_hcihandle, false);
                    app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_STOP,BT_STREAM_VOICE,BT_DEVICE_ID_1,MAX_RECORD_NUM,0,0);
                }
                else if(sco_status == 2)
                {
                    bt_drv_reg_op_enlarge_sync_winsize(sniff_sco_hcihandle, true);
                }
#endif //__LARGE_SYNC_WIN__
                if (!app_bt_device.a2dp_streamming[BT_DEVICE_ID_1]) {
                    app_bt_set_linkpolicy(masterBtRemDev, BTIF_BLP_MASTER_SLAVE_SWITCH|BTIF_BLP_SNIFF_MODE);
                }
        }

        app_tws_close_emsack_mode();
        slave_sco_active = 0;     
    }
}

#ifdef CHIP_BEST2300
static uint32_t sniffer_sco_block_flag=0;
#endif
extern uint8_t esco_retx_nb;
void app_bt_block_sniffer_sco_for_trigger(void)
{
    //2300 trigger problem please contact luofei
#ifdef CHIP_BEST2300
    if(sniffer_sco_block_flag==0) {
        if (app_tws_mode_is_slave()) {
            LOG_PRINT_HFP("%s,sniffer hci=%x",__func__,sniff_sco_hcihandle);
            bt_drv_reg_op_retx_att_nb_manager(RETX_NB_0);
            app_bt_ME_BLOCK_RF(sniff_sco_hcihandle,1);
            bt_drv_reg_op_lock_sniffer_sco_resync();
#ifdef __LARGE_SYNC_WIN__
            //dynamic close the enlarge winsize patch
            bt_drv_reg_op_enlarge_sync_winsize(sniff_sco_hcihandle, false);
#endif //__LARGE_SYNC_WIN__
        } else if (app_tws_mode_is_master()) {
            esco_retx_nb = bt_drv_reg_op_retx_att_nb_manager(RETX_NB_1);
            tws_send_esco_retx_nb(esco_retx_nb);
            bt_drv_reg_op_modify_bitoff_timer(0x5);
        }
        sniffer_sco_block_flag=1;
    }
#endif //CHIP_BEST2300
}

void app_bt_resume_sniffer_sco_for_trigger(uint8_t reason)
{
    //2300 trigger problem please contact luofei
#ifdef CHIP_BEST2300
    if(hal_get_chip_metal_id() > HAL_CHIP_METAL_ID_1) {
         sniffer_sco_block_flag=0;
        if (app_tws_mode_is_slave()) {
            LOG_PRINT_HFP("%s,sniffer hci=%x",__func__,sniff_sco_hcihandle);
            //update new bt clock
            bt_drv_reg_op_update_sniffer_bitoffset(sniff_sco_hcihandle, app_tws_get_tws_conhdl());

            app_bt_ME_BLOCK_RF(sniff_sco_hcihandle,0);
#ifdef __LARGE_SYNC_WIN__
            //dynamic enable the enlarge winsize patch
            bt_drv_reg_op_enlarge_sync_winsize(sniff_sco_hcihandle, true);
#endif //__LARGE_SYNC_WIN__

            if(reason == TRIGGER_SUCCESS) {
                bt_drv_reg_op_unlock_sniffer_sco_resync();
            }
#ifdef __EMSACK__
            if ((bt_sco_get_current_codecid() == BTIF_HF_SCO_CODEC_MSBC) &&
                                            (reason == TRIGGER_SUCCESS)) {
                //enable emsack mode
                bt_drv_reg_op_enable_emsack_mode(sniff_sco_hcihandle,0, true);
                //notify master
                app_tws_set_emsack_mode(true);
            }
#endif//__EMSACK__
            //if current mode is in sniff mode, resume retx derectly
#ifdef __3RETX_SNIFF__
            if (btif_me_get_current_mode(masterBtRemDev) == BTIF_BLM_SNIFF_MODE)
#endif
            {
                 //Both master and slave can come here to resume retx
                bt_drv_reg_op_retx_att_nb_manager(RETX_NEGO);
            }
        } else if (app_tws_mode_is_master()) {
            bt_drv_reg_op_modify_bitoff_timer(0xc8);
            //if current mode is in sniff mode, resume retx derectly
#ifdef __3RETX_SNIFF__
            if (   btif_me_get_current_mode(slaveBtRemDev) == BTIF_BLM_SNIFF_MODE)
#endif
            {
                 //Both master and slave can come here to resume retx
                bt_drv_reg_op_retx_att_nb_manager(RETX_NEGO);
            }
        }
    }
#endif //CHIP_BEST2300
}


extern btif_remote_device_t* mobileBtRemDev;

void app_tws_clean_up_emsack(void)
{
#ifdef CHIP_BEST2300
#ifdef __EMSACK__
    if (app_tws_is_master_mode() && btapp_hfp_call_is_present()) {
        bt_drv_reg_op_enable_emsack_mode(
                btif_me_get_remote_device_hci_handle(mobileBtRemDev),1, 0);
    } else if(app_tws_is_slave_mode() && slave_sco_active) {
        bt_drv_reg_op_enable_emsack_mode(sniff_sco_hcihandle,1, 0);
    }
#endif//__EMSACK__
#endif//CHIP_BEST2300
}

void app_tws_close_emsack_mode(void)
{
#ifdef CHIP_BEST2300
#ifdef __EMSACK__
    if(hal_get_chip_metal_id()>HAL_CHIP_METAL_ID_1) {
        if( app_tws_is_slave_mode() && slave_sco_active) {
            if (bt_sco_get_current_codecid()  == BTIF_HF_SCO_CODEC_MSBC) {
                //disable emsack mode
                bt_drv_reg_op_enable_emsack_mode(sniff_sco_hcihandle,0, false);
                //notify master
                app_tws_set_emsack_mode(false);
            }
        }
    }
#endif//__EMSACK__
#endif//CHIP_BEST2300
}

#endif //__TWS_CALL_DUAL_CHANNEL__


extern struct BT_DEVICE_T  app_bt_device;

static bool hf_conn_flag[BT_DEVICE_NUM];
static uint8_t hf_voice_en[BT_DEVICE_NUM];

void app_hfp_init(void)
{
    hfp_hfcommand_mempool_init();
    app_bt_device.curr_hf_channel_id = BT_DEVICE_ID_1;
    app_bt_device.hf_mute_flag = 0;

    for (uint8_t i = 0; i < BT_DEVICE_NUM; i++) {
        app_bt_device.hf_channel[i] = btif_hf_create_channel();
        if (!app_bt_device.hf_channel[i]) {
            ASSERT(0, "Serious error: cannot create hf channel\n");
        }
        btif_hf_init_channel(app_bt_device.hf_channel[i]);
        app_bt_device.hfchan_call[i] = 0;
        app_bt_device.hfchan_callSetup[i] = 0;
        app_bt_device.hf_audio_state[i] = BTIF_HF_AUDIO_DISCON,
        hf_conn_flag[i] = false;
        hf_voice_en[i] = 0;
    }

    btif_hf_register_callback(app_hfp_event_callback);
#ifdef __TWS_CALL_DUAL_CHANNEL__
    sniffer_sco_status_callback = btapp_sniffer_sco_start;
#else
    sniffer_sco_status_callback = NULL;
#endif
    btapp_sniffer_sco_check_init();
}

#if defined(SUPPORT_BATTERY_REPORT) || defined(SUPPORT_HF_INDICATORS)
#ifdef __BT_ONE_BRING_TWO__
static uint8_t battery_level[BT_DEVICE_NUM] = {0xff, 0xff};
#else
static uint8_t battery_level[BT_DEVICE_NUM] = {0xff};
#endif

static int app_hfp_battery_report_reset(uint8_t bt_device_id)
{
    ASSERT(bt_device_id < BT_DEVICE_NUM, "bt_device_id error");
    battery_level[bt_device_id] = 0xff;
    return 0;
}

int app_hfp_battery_report(uint8_t level)
{
    // Care: BT_DEVICE_NUM<-->{0xff, 0xff, ...}
    bt_status_t status = BT_STS_LAST_CODE;

    uint8_t i;
    int nRet = 0;
    hf_chan_handle_t chan;

    if (level>9)
        return -1;

    for (i = 0; i < BT_DEVICE_NUM; i++) {
        chan = app_bt_device.hf_channel[i];
        if (btif_get_hf_chan_state(chan) == BTIF_HF_STATE_OPEN){
            if (btif_hf_is_hf_indicators_support(chan)) {
                if (battery_level[i] != level){
                    status = btif_hf_update_indicators_batt_level(chan, level);
                }
            } else if (btif_hf_is_batt_report_support(chan)) {
                if (battery_level[i] != level){
                    status = btif_hf_batt_report(chan, level);
                }
            }
            if (BT_STS_PENDING == status){
                battery_level[i] = level;
            } else {
                nRet = -1;
            }
        }else{
             battery_level[i] = 0xff;
             nRet = -1;
        }
    }
    return nRet;
}
#endif


#ifdef SUPPORT_SIRI
int app_hfp_siri_report(void)
{
    uint8_t i;
    bt_status_t status = BT_STS_LAST_CODE;
    hf_chan_handle_t chan;

    for (i = 0; i < BT_DEVICE_NUM; i++) {
        chan = app_bt_device.hf_channel[i];
        if (btif_get_hf_chan_state(chan) == BTIF_HF_STATE_OPEN) {
            status = btif_hf_siri_report(chan);
            if (status == BT_STS_PENDING){
                return 0;
            } else {
                return -1;
            }
        }
    }

    return 0;
}

int app_hfp_siri_voice(bool en)
{
    bt_status_t res = BT_STS_LAST_CODE;
    hf_chan_handle_t chan_dev_1 = app_bt_device.hf_channel[BT_DEVICE_ID_1];
#ifdef __BT_ONE_BRING_TWO__
    hf_chan_handle_t chan_dev_2 = app_bt_device.hf_channel[BT_DEVICE_ID_2];
#endif

    /*
    log_fill_HFP_WAKEUP_SIRI_event(
        app_bt_device.hf_channel[BT_DEVICE_ID_1].cmgrHandler.remDev->hciHandle & 3, en);
    */

    if(btif_get_hf_chan_state(chan_dev_1) == BTIF_HF_STATE_OPEN) {
        res = btif_hf_enable_voice_recognition(chan_dev_1, en);
    }
#ifdef __BT_ONE_BRING_TWO__
    else if((btif_get_hf_chan_state(chan_dev_1) == BTIF_HF_STATE_CLOSED) &&
                ((btif_get_hf_chan_state(dev_chan_2) == BTIF_HF_STATE_OPEN))) {
        res = btif_hf_enable_voice_recognition(chan_dev_2, en);
    }
#endif

    TRACE("[%s] Line = %d, res = %d", __func__, __LINE__, res);

    return 0;
}

bool app_hfp_siri_is_active(void)
{
    return btif_hf_is_voice_rec_active(app_bt_device.hf_channel[BT_DEVICE_ID_1]);
}
#endif

int hfp_handle_add_to_earphone_1_6(hf_chan_handle_t chan)
{
    bt_status_t status = BT_STS_LAST_CODE;

    status = btif_hf_send_at_cmd(chan, "AT+BCC");
    if (status != BT_STS_PENDING)
        return -1;
    else
        return 0;
}

static struct BT_DEVICE_ID_DIFF chan_id_flag;
#ifdef __BT_ONE_BRING_TWO__
void hfp_chan_id_distinguish(hf_chan_handle_t chan)
{
    if(chan == app_bt_device.hf_channel[BT_DEVICE_ID_1]){
        chan_id_flag.id = BT_DEVICE_ID_1;
        chan_id_flag.id_other = BT_DEVICE_ID_2;
    }else if(chan == app_bt_device.hf_channel[BT_DEVICE_ID_2]){
        chan_id_flag.id = BT_DEVICE_ID_2;
        chan_id_flag.id_other = BT_DEVICE_ID_1;
    }
}
#endif

int hfp_volume_get(void)
{
    int vol = app_bt_stream_volume_get_ptr()->hfp_vol - 2;

    if (vol > 15)
        vol = 15;
    if (vol < 0)
        vol = 0;
    LOG_PRINT_HFP("Get hfp volume %d", vol);
    return (vol);
}

void hfp_volume_local_set(int8_t vol)
{
    nvrec_btdevicerecord *record = NULL;

    if (app_tws_is_mobile_dev_in_nv(mobileDevBdAddr.address, (int*)&record)){
        LOG_PRINT_HFP("save hfp vol to mobileDevBdAddr");
        record->device_vol.hfp_vol = vol;
    } else {
        LOG_PRINT_HFP("save hfp vol to currDevBdAddr");
        app_bt_stream_volume_get_ptr()->hfp_vol = vol;
    }

#ifndef FPGA
    nv_record_touch_cause_flush();
    LOG_PRINT_HFP("Set local hfp volume %d", vol);
#endif
}


int hfp_volume_set(int vol)
{
    if (vol > 15)
        vol = 15;
    if (vol < 0)
        vol = 0;

    hfp_volume_local_set(vol+2);
    LOG_PRINT_HFP("Set hfp volume %d", vol);
    if (app_bt_stream_isrun(APP_BT_STREAM_HFP_PCM)){
        app_bt_stream_volumeset(vol+2);
    }
    return 0;
}


void app_tws_stop_sniff(void);
static void hfp_connected_ind_handler(hf_chan_handle_t chan, struct hfp_context *ctx)
{
    TRACE("HFP with mobile is opened, BD address is:");
    DUMP8("0x%02x ", ctx->remote_dev_bdaddr.address, BTIF_BD_ADDR_SIZE);


    app_bt_device.ptrHfChannel[chan_id_flag.id] = chan;

#if defined(HFP_1_6_ENABLE)
    btif_hf_set_negotiated_codec(chan, BTIF_HF_SCO_CODEC_CVSD);
#endif

    log_event_1(EVENT_HFP_CONNECTED, ctx->remote_dev_hcihandle & 3);

    app_bt_device.phone_earphone_mark = 1;

    TRACE("::HF_EVENT_SERVICE_CONNECTED  Chan_id:%d\n", chan_id_flag.id);

#ifdef __TWS_PAIR_DIRECTLY__
    //TRACE("#########tws_mode_ptr()->mode:%d,open ble scan",tws_mode_ptr()->mode);
    if(app_tws_is_slave_mode())
    {
        TRACE("hfp:tws_mode_ptr()->mode:%d,open ble scan",tws_mode_ptr()->mode);
        //if slave connect phone ,restart scan for tws, slave to master
        conn_ble_reconnection_machanism_handler(false);
    }
#endif

#if !defined(FPGA) && defined(__BTIF_EARPHONE__)
    if (ctx->state == BTIF_HF_STATE_OPEN) {
        ////report connected voice
        hf_conn_flag[chan_id_flag.id] = true;
    }
#endif
#if defined(SUPPORT_BATTERY_REPORT) || defined(SUPPORT_HF_INDICATORS)
#if defined(__TWS_ROLE_SWITCH__) && defined(__ENABLE_ROLE_SWITCH_FOR_BATTERY_BALANCE__)
    app_hfp_battery_report_reset(chan_id_flag.id);
    {
        uint8_t report_level = app_tws_get_report_level();
        uint8_t level = app_battery_current_level();
        if(0xff != report_level){
            level = report_level;
        }
        app_hfp_battery_report(level);
    }
#else
    app_hfp_battery_report_reset(chan_id_flag.id);
    {
        uint8_t level = app_battery_current_level();
        app_hfp_battery_report(level);
    }
#endif
#endif
#if defined(HFP_DISABLE_NREC)
    btif_hf_disable_nrec(chan);
#endif

    app_bt_stream_volume_ptr_update((uint8_t *)ctx->remote_dev_bdaddr.address);
    if(app_tws_mode_is_master())
    {   
        TRACE("sync tws volume !");
        app_tws_set_slave_volume(a2dp_volume_get_tws());
    }    

    btif_hf_report_speaker_volume(chan, hfp_volume_get());

#ifdef __BT_ONE_BRING_TWO__
    if (app_bt_device.hf_audio_state[chan_id_flag.id_other] == BTIF_HF_AUDIO_CON) {
        app_bt_device.curr_hf_channel_id = chan_id_flag.id_other;
    } else {
        app_bt_device.curr_hf_channel_id = chan_id_flag.id;
    }
#endif

    app_bt_fill_mobile_hfp_channel(BT_DEVICE_ID_1, chan);
    if (!app_tws_is_role_switching_on()) {
        TRACE("%s mobile profile %d", __func__, mobileDevConnectionState);
        if(IS_JUST_LINK_ON(mobileDevConnectionState)) {
            if ((app_tws_is_master_mode()) && IS_CONNECTED_WITH_TWS()) {
                app_tws_sniffer_magic_config_channel_map();
                btif_me_set_sniffer_env(1, FORWARD_ROLE, mobileDevBdAddr.address, tws_mode_ptr()->slaveAddr.address);
            }
#ifdef MEDIA_PLAYER_SUPPORT
            app_voice_report(APP_STATUS_INDICATION_CONNECTED, 0);
            app_tws_ask_slave_to_play_voice_prompt(APP_STATUS_INDICATION_CONNECTED);
#endif            
        }
    }

    // update the run-time profile active state
    SET_MOBILE_DEV_ACTIVE_PROFILE(BT_PROFILE_HFP);

    // update the sate of hfp profile
    uint32_t formerMobileConnState = mobileDevConnectionState;
    SET_CONN_STATE_HFP(mobileDevConnectionState);

    if (formerMobileConnState != mobileDevConnectionState) {
        conn_stop_connecting_mobile_supervising();
        // update profile active state in NV
        btdevice_profile *btdevice_plf_p =
            (btdevice_profile *)app_bt_profile_active_store_ptr_get((uint8_t *)ctx->remote_dev_bdaddr.address);
        if (true != btdevice_plf_p->hfp_act) {
            TRACE("set hfp act as true");
            btdevice_plf_p->hfp_act = true;
            nv_record_touch_cause_flush();
        }

        // continue to connect a2dp profile
        if (IS_MOBILE_DEV_PROFILE_ACTIVE(BT_PROFILE_A2DP) && !IS_CONN_STATE_A2DP_ON(mobileDevConnectionState)) {
            if (!IS_WAITING_FOR_MOBILE_CONNECTION()) {
                TRACE("HFP has been connected, now start connecting A2DP.");
                app_start_custom_function_in_bt_thread(0, 0,(uint32_t)conn_connecting_mobile_handler);
            }
        }

        app_tws_check_max_slot_setting();
        mobile_device_connection_state_updated_handler(BTIF_BEC_NO_ERROR);
    }

    if (TO_DISCONNECT_BEFORE_ROLE_SWITCH == currentSimulateReconnectionState) {
        bt_status_t ret = app_bt_disconnect_hfp();
        TRACE("HF_DisconnectServiceLink returns 0x%x", ret);
    }
    
#ifdef __AMA_VOICE__
    post_hfp_connected();
#endif
}

static void hfp_disconnected_ind_common_handler(hf_chan_handle_t chan, struct hfp_context *ctx)
{
    TRACE("::HF_EVENT_SERVICE_DISCONNECTED Chan_id:%d, reason=%x\n", chan_id_flag.id, ctx->disconnect_reason);
    app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_STOP,BT_STREAM_VOICE,chan_id_flag.id,MAX_RECORD_NUM,0,0);
#if defined(HFP_1_6_ENABLE)
    btif_hf_set_negotiated_codec(chan, BTIF_HF_SCO_CODEC_CVSD);
#endif
#if !defined(FPGA) && defined(__BTIF_EARPHONE__)
    if (hf_conn_flag[chan_id_flag.id] ) {
        ////report device disconnected voice
        hf_conn_flag[chan_id_flag.id] = false;
    }
#endif
    app_bt_device.ptrHfChannel[chan_id_flag.id] = NULL;

#ifdef __BT_ONE_BRING_TWO__
    if (hf_conn_flag[chan_id_flag.id]) {
        app_bt_stream_volume_ptr_update(app_bt_device.hf_channel[chan_id_flag.id].cmgrHandler.remDev->bdAddr.addr);
    } else if (hf_conn_flag[chan_id_flag.id_other]) {
        app_bt_stream_volume_ptr_update(app_bt_device.hf_channel[chan_id_flag.id_other].cmgrHandler.remDev->bdAddr.addr);
    } else {
        app_bt_stream_volume_ptr_update(NULL);
    }
#else
    app_bt_stream_volume_ptr_update(NULL);
#endif
    for (uint8_t i=0; i<BT_DEVICE_NUM; i++) {
        if (chan == app_bt_device.hf_channel[i]) {
            app_bt_device.hfchan_call[i] = 0;
            app_bt_device.hfchan_callSetup[i] = 0;
            app_bt_device.hf_audio_state[i] = BTIF_HF_AUDIO_DISCON,
            hf_conn_flag[i] = false;
            hf_voice_en[i] = 0;
        }
    }

    // update the sate of hfp profile connection
    CLEAR_CONN_STATE_HFP(mobileDevConnectionState);

    app_disconnection_handler_before_role_switch();
}

static void hfp_disconnected_ind_handler(hf_chan_handle_t chan, struct hfp_context *ctx)
{
    log_event_2(EVENT_HFP_DISCONNECTED, ctx->remote_dev_hcihandle & 3, ctx->error_code);
    TRACE("HFP with mobile is closed, reason is 0x%x. BD address is:", ctx->error_code);
    DUMP8("0x%02x ", ctx->remote_dev_bdaddr.address, BTIF_BD_ADDR_SIZE);

    hfp_disconnected_ind_common_handler(chan, ctx);

    mobile_device_connection_state_updated_handler(ctx->error_code);

    app_tws_preparation_for_roleswich_after_hf_disconnection();

    if (app_tws_is_reconnecting_hfp_in_progress()) {
        TRACE("HFP disconnection happens during the reconnecting hfp of the role switch!");
        app_tws_process_hfp_disconnection_event_during_role_switch((bt_bdaddr_t*)&(ctx->remote_dev_bdaddr));
    }

    if (is_simulating_reconnecting_in_progress())
        return;

    if (IS_CONNECTED_WITH_MOBILE())
        return;

#ifdef __AMA_VOICE__
    post_hfp_disconnected();
#endif

    if (IS_CONNECTING_MOBILE()) {
        set_conn_op_state(CONN_OP_STATE_IDLE);
    }

    if (is_mobile_connection_creation_supervisor_timer_on_going()) {
        // put the mobile in the idle state, to assure that if the mobile is connecting
        // in the meantime, the device can be connected
        conn_start_mobile_connection_creation_idle_timer();
    } else {
        if (!((BTIF_BEC_USER_TERMINATED == ctx->error_code) ||
            (BTIF_BEC_LOCAL_TERMINATED == ctx->error_code))) {
            app_start_custom_function_in_bt_thread(0, 0, \
                (uint32_t)connecting_mobile_timeout_handler);
        }
    }
}

static void hfp_audio_data_sent_handler(hf_chan_handle_t chan, struct hfp_context *ctx)
{
#if defined(SCO_LOOP)
    hf_loop_buffer_valid = 1;
#endif
}

static void hfp_audio_data_handler(hf_chan_handle_t chan, struct hfp_context *ctx)
{
#ifdef __BT_ONE_BRING_TWO__
    if (hf_voice_en[chan_id_flag.id]) {
#endif

#ifndef _SCO_BTPCM_CHANNEL_
        uint32_t idx = 0;
        if (app_bt_stream_isrun(APP_BT_STREAM_HFP_PCM)) {
            store_voicebtpcm_m2p_buffer(ctx->audio_data, ctx->audio_data_len);

            idx = hf_sendbuff_ctrl.index % HF_SENDBUFF_MEMPOOL_NUM;
            get_voicebtpcm_p2m_frame(&(hf_sendbuff_ctrl.mempool[idx].buffer[0]), ctx->audio_data_len);
            hf_sendbuff_ctrl.mempool[idx].packet.data = &(hf_sendbuff_ctrl.mempool[idx].buffer[0]);
            hf_sendbuff_ctrl.mempool[idx].packet.dataLen = ctx->audio_data_len;
            hf_sendbuff_ctrl.mempool[idx].packet.flags = BTP_FLAG_NONE;
            if (!app_bt_device.hf_mute_flag) {
                HF_SendAudioData(chan, &hf_sendbuff_ctrl.mempool[idx].packet);
            }
            hf_sendbuff_ctrl.index++;
        }
#endif

#ifdef __BT_ONE_BRING_TWO__
    }
#endif

#if defined(SCO_LOOP)
    uint8_t *sco_loop_buf = hf_loop_buffer + hf_loop_buffer_w_idx*HF_LOOP_SIZE;
    memcpy(sco_loop_buf, ctx->audio_data, ctx->audio_data_len);
    hf_loop_buffer_len[hf_loop_buffer_w_idx] = ctx->audio_data_len;
    hf_loop_buffer_w_idx = (hf_loop_buffer_w_idx+1) % HF_LOOP_CNT;
    ++hf_loop_buffer_size;

    if (hf_loop_buffer_size >= 18 && hf_loop_buffer_valid == 1) {
        hf_loop_buffer_valid = 0;
        idx = (hf_loop_buffer_w_idx-17 < 0) ?
            (HF_LOOP_CNT - (17 - hf_loop_buffer_w_idx)) : (hf_loop_buffer_w_idx - 17);
        pkt.flags = BTP_FLAG_NONE;
        pkt.dataLen = hf_loop_buffer_len[idx];
        pkt.data = hf_loop_buffer + idx*HF_LOOP_SIZE;
        HF_SendAudioData(chan, &pkt);
    }
#endif
}

static void hfp_call_ind_handler(hf_chan_handle_t chan, struct hfp_context *ctx)
{
    enum BT_DEVICE_ID_T chan_id;

    chan_id = chan_id_flag.id;
    TRACE("::HF_EVENT_CALL_IND  chan_id:%d, call:%d\n",chan_id, ctx->call);
    if (ctx->call == BTIF_HF_CALL_ACTIVE) {
#if defined(_AUTO_TEST_)
        AUTO_TEST_SEND("Call setup ok.");
#endif
        ///call is active so check if it's a outgoing call
        if (app_bt_device.hfchan_callSetup[chan_id_flag.id] == BTIF_HF_CALL_SETUP_ALERT) {
            TRACE("HF CALLACTIVE TIME=%d",hal_sys_timer_get());
            if (TICKS_TO_MS(hal_sys_timer_get()-app_bt_device.hf_callsetup_time[chan_id_flag.id])<1000) {
                TRACE("DISABLE HANGUPCALL PROMPT");
                app_bt_device.hf_endcall_dis[chan_id_flag.id] = true;
            }
        }
        /////stop media of (ring and call number) and switch to sco
        app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_SWITCHTO_SCO,BT_STREAM_VOICE,chan_id_flag.id,0,0,0);

    } else {
#if defined(_AUTO_TEST_)
        AUTO_TEST_SEND("Call hangup ok.");
#endif
    }
#if !defined(FPGA) && defined(__BTIF_EARPHONE__)
    if (ctx->call == BTIF_HF_CALL_NONE &&
            app_bt_device.hfchan_call[chan_id] == BTIF_HF_CALL_ACTIVE) {
        //////report call hangup voice
        TRACE("!!!HF_EVENT_CALL_IND  APP_STATUS_INDICATION_HANGUPCALL  chan_id:%d\n",chan_id);
        app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_STOP_MEDIA,BT_STREAM_VOICE,chan_id,0,0,0);
        ///disable media prompt
        if (app_bt_device.hf_endcall_dis[chan_id] == false) {
            TRACE("HANGUPCALL PROMPT");
            app_voice_report(APP_STATUS_INDICATION_HANGUPCALL,chan_id);
        }
    }
#endif


    if (ctx->call == BTIF_HF_CALL_ACTIVE) {
        app_bt_device.curr_hf_channel_id = chan_id_flag.id;
    }
#ifdef __BT_ONE_BRING_TWO__
    else if ((ctx->call == BTIF_HF_CALL_NONE) &&
            (app_bt_device.hfchan_call[chan_id_flag.id_other] == BTIF_HF_CALL_ACTIVE)) {
        app_bt_device.curr_hf_channel_id = chan_id_flag.id_other;
    }
#endif
    app_bt_device.hfchan_call[chan_id_flag.id] = ctx->call;
    if (ctx->call == BTIF_HF_CALL_NONE) {
        app_bt_device.hf_endcall_dis[chan_id_flag.id] = false;
    }
#if defined( __BT_ONE_BRING_TWO__)
#if !defined(HFP_1_6_ENABLE)
#ifdef CHIP_BEST1000
    if (hal_get_chip_metal_id() >= HAL_CHIP_METAL_ID_2)
#endif
#ifdef CHIP_BEST2000
    if(1)
#endif
    {
        ////a call is active:
        if (app_bt_device.hfchan_call[chan_id_flag.id] == BTIF_HF_CALL_ACTIVE) {
            if (app_bt_device.hf_audio_state[chan_id_flag.id_other] == BTIF_HF_AUDIO_CON) {
                hf_chan_handle_t chan_tmp = app_bt_device.hf_channel[chan_id_flag.id];
                app_bt_device.curr_hf_channel_id = chan_id_flag.id;
                btif_hfp_switch_sco(chan_tmp);
            }
            hf_voice_en[chan_id_flag.id] = HF_VOICE_ENABLE;
            hf_voice_en[chan_id_flag.id_other] = HF_VOICE_DISABLE;
        } else {
            ////a call is hung up:
            ///if one device  setup a sco connect so get the other device's sco state, if both connect mute the earlier one
            if(app_bt_device.hf_audio_state[chan_id_flag.id_other] == BTIF_HF_AUDIO_CON){
                hf_voice_en[chan_id_flag.id_other] = HF_VOICE_ENABLE;
                hf_voice_en[chan_id_flag.id] = HF_VOICE_DISABLE;
            }
        }
    }
#endif
#endif
}

static void hfp_callheld_ind_handler(hf_chan_handle_t chan, struct hfp_context *ctx)
{
    TRACE("###callheld:%d",ctx->callHeld);
}

static void hfp_callsetup_ind_handler(hf_chan_handle_t chan, struct hfp_context *ctx)
{
    enum BT_DEVICE_ID_T chan_id;

    chan_id = chan_id_flag.id;

    TRACE("::HF_EVENT_CALLSETUP_IND chan_id:%d, callSetup = %d\n", chan_id, ctx->call_setup);
#if !defined(FPGA) && defined(__BTIF_EARPHONE__)
    if(ctx->call_setup == BTIF_HF_CALL_SETUP_NONE &&
            (app_bt_device.hfchan_call[chan_id] != BTIF_HF_CALL_ACTIVE) &&
            (app_bt_device.hfchan_callSetup[chan_id] != BTIF_HF_CALL_SETUP_NONE)) {
        ////check the call refuse and stop media of (ring and call number)
        TRACE("!!!HF_EVENT_CALLSETUP_IND  APP_STATUS_INDICATION_REFUSECALL  chan_id:%d\n",chan_id);
        app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_STOP_MEDIA,BT_STREAM_VOICE,chan_id,0,0,0);
        app_voice_report(APP_STATUS_INDICATION_REFUSECALL,chan_id);
    }
#endif

#ifdef __BT_ONE_BRING_TWO__
    if(ctx->call_setup == 0){
        //do nothing
    } else {
        if ((app_bt_device.hfchan_call[chan_id_flag.id_other] == BTIF_HF_CALL_ACTIVE) ||
                ((app_bt_device.hfchan_callSetup[chan_id_flag.id_other] == BTIF_HF_CALL_SETUP_IN) &&
                 (app_bt_device.hfchan_call[chan_id_flag.id] != BTIF_HF_CALL_ACTIVE))) {
            app_bt_device.curr_hf_channel_id = chan_id_flag.id_other;
        } else {
            app_bt_device.curr_hf_channel_id = chan_id_flag.id;
        }
    }
    TRACE("!!!HF_EVENT_CALLSETUP_IND curr_hf_channel_id:%d\n",app_bt_device.curr_hf_channel_id);
#endif
    app_bt_device.hfchan_callSetup[chan_id_flag.id] = ctx->call_setup;
    /////call is alert so remember this time
    if (app_bt_device.hfchan_callSetup[chan_id_flag.id] == BTIF_HF_CALL_SETUP_ALERT) {
         TRACE("HF CALLSETUP TIME=%d",hal_sys_timer_get());
        app_bt_device.hf_callsetup_time[chan_id_flag.id] = hal_sys_timer_get();
    }
    if (app_bt_device.hfchan_callSetup[chan_id_flag.id] == BTIF_HF_CALL_SETUP_IN) {
        btif_hf_list_current_calls(chan);
    }
    #ifndef __3RETX_SNIFF__
    app_tws_stop_sniff();
    #endif
}

static void hfp_current_call_state_handler(hf_chan_handle_t chan, struct hfp_context *ctx)
{
    enum BT_DEVICE_ID_T chan_id;

    chan_id = chan_id_flag.id;

    TRACE("::HF_EVENT_CURRENT_CALL_STATE  chan_id:%d\n", chan_id);
#if !defined(FPGA) && defined(__BTIF_EARPHONE__)
    TRACE("!!!HF_EVENT_CURRENT_CALL_STATE  chan_id:%d, call_number:%s\n", chan_id, ctx->call_number);
    if (app_bt_device.hfchan_callSetup[chan_id] == BTIF_HF_CALL_SETUP_IN) {
        //////report incoming call number
#ifndef __TWS_CALL_DUAL_CHANNEL__
        app_status_set_num(ctx->call_number);
        app_voice_report(APP_STATUS_INDICATION_CALLNUMBER,chan_id);
#endif
    }
#endif
}

bool btapp_hfp_is_sco_active(void)
{
    uint8_t i;
    for (i = 0;i < BT_DEVICE_NUM;i++)
    {
        if ((BTIF_HF_AUDIO_CON == app_bt_device.hf_audio_state[i]))
        {
            return true;
        }
    }
    return false;    
}

bool btapp_is_bt_active(void)
{
    return btapp_hfp_is_sco_active()||a2dp_is_music_ongoing();
}

static void hfp_audio_connected_handler(hf_chan_handle_t chan, struct hfp_context *ctx)
{
    hf_chan_handle_t chan_tmp;

    if (ctx->status != BT_STS_SUCCESS)
        return;

#ifdef ADAPTIVE_BLE_CONN_PARAM_ENABLED
    switch_to_low_speed_conn_interval();
#endif

    log_event_1(EVENT_HFP_SCO_LINK_CREATED, ctx->remote_dev_hcihandle & 3);
#ifdef __ENABLE_WEAR_STATUS_DETECT__
    app_tws_check_if_need_to_switch_mic();
#endif
    bt_drv_reg_op_sco_sniffer_checker();
    bt_drv_reg_op_rssi_set(0xffff);
#if defined(HFP_1_6_ENABLE)
    chan_tmp = app_bt_device.hf_channel[chan_id_flag.id];
    TRACE("::HF_EVENT_AUDIO_CONNECTED  chan_id:%d, codec_id %d\n", chan_id_flag.id,
            btif_hf_get_negotiated_codec(chan_tmp));

    bt_sco_set_current_codecid(btif_hf_get_negotiated_codec(chan_tmp));
#else
    TRACE("::HF_EVENT_AUDIO_CONNECTED  chan_id:%d\n", chan_id_flag.id);
#endif

    app_bt_device.phone_earphone_mark = 0;
    app_bt_device.hf_mute_flag = 0;

    app_bt_device.hf_audio_state[chan_id_flag.id] = BTIF_HF_AUDIO_CON;

#if defined(__FORCE_REPORTVOLUME_SOCON__)
    hf_report_speaker_volume(chan, hfp_volume_get());
#endif

#ifdef __BT_ONE_BRING_TWO__

#if !defined(HFP_1_6_ENABLE)
#ifdef CHIP_BEST1000
    if(hal_get_chip_metal_id() >= HAL_CHIP_METAL_ID_2)
#endif
#ifdef CHIP_BEST2000
        if(1)
#endif
        {
            if (app_bt_device.hfchan_call[chan_id_flag.id] == BTIF_HF_CALL_ACTIVE) {
                hf_chan_handle_t chan_tmp = app_bt_device.hf_channel[chan_id_flag.id];
                btif_hfp_switch_sco(chan_tmp);
                hf_voice_en[chan_id_flag.id] = HF_VOICE_ENABLE;
                hf_voice_en[chan_id_flag.id_other] = HF_VOICE_DISABLE;
            } else if (app_bt_device.hf_audio_state[chan_id_flag.id_other] == BTIF_HF_AUDIO_CON) {
                hf_voice_en[chan_id_flag.id] = HF_VOICE_DISABLE;
                hf_voice_en[chan_id_flag.id_other] = HF_VOICE_ENABLE;
            }
        }
        else
#endif
        {
            ///if one device  setup a sco connect so get the other device's sco state, if both connect mute the earlier one
            if (app_bt_device.hf_audio_state[chan_id_flag.id_other] == BTIF_HF_AUDIO_CON) {
                hf_voice_en[chan_id_flag.id_other] = HF_VOICE_DISABLE;
            }
            hf_voice_en[chan_id_flag.id] = HF_VOICE_ENABLE;
        }

            app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_START,BT_STREAM_VOICE,chan_id_flag.id,MAX_RECORD_NUM, 0, 0);


#else
#ifdef __TWS_CALL_DUAL_CHANNEL__
    app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_START,BT_STREAM_VOICE,BT_DEVICE_ID_1,MAX_RECORD_NUM,1,0);

    if (TWS_MASTER_CONN_SLAVE == app_tws_get_conn_state()) {
        tws_player_set_hfp_vol(hfp_volume_get());

    }

#else
    app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_START,BT_STREAM_VOICE,BT_DEVICE_ID_1,MAX_RECORD_NUM,0,0);
#endif
#endif
}

static void app_hfp_audio_closed_delay_resume_ble_adv_timer_cb(void const *n)
{
#ifdef IAG_BLE_INCLUDE 
    app_start_connectable_ble_adv(BLE_ADVERTISING_INTERVAL);
#endif
}
static void app_hfp_resume_ble_adv(void)
{
    if (NULL == app_hfp_audio_closed_delay_resume_ble_adv_timer_id)
    {
        app_hfp_audio_closed_delay_resume_ble_adv_timer_id = 
            osTimerCreate(osTimer(APP_HFP_AUDIO_CLOSED_DELAY_RESUME_BLE_ADV_TIMER),
            osTimerOnce, NULL);
    }

    osTimerStart(app_hfp_audio_closed_delay_resume_ble_adv_timer_id, 
        HFP_AUDIO_CLOSED_DELAY_RESUME_ADV_IN_MS);
}

static void hfp_audio_disconnected_handler(hf_chan_handle_t chan, struct hfp_context *ctx)
{
    log_event_2(EVENT_HFP_SCO_LINK_DISCONNECTED, ctx->remote_dev_hcihandle & 3,
            ctx->error_code);

    if (!app_bt_device.a2dp_streamming[BT_DEVICE_ID_1]) {
        app_bt_set_linkpolicy(mobileBtRemDev, BTIF_BLP_MASTER_SLAVE_SWITCH|BTIF_BLP_SNIFF_MODE);
    }
    TRACE("::HF_EVENT_AUDIO_DISCONNECTED  chan_id:%d\n", chan_id_flag.id);
    if (app_bt_device.hfchan_call[chan_id_flag.id] == BTIF_HF_CALL_ACTIVE) {
        app_bt_device.phone_earphone_mark = 1;
    }

    app_bt_device.hf_audio_state[chan_id_flag.id] = BTIF_HF_AUDIO_DISCON;

#ifdef IAG_BLE_INCLUDE 

#ifdef ADAPTIVE_BLE_CONN_PARAM_ENABLED
    switch_to_high_speed_conn_interval();
#endif

    if (!IS_CONNECTING_MOBILE())
    {
        app_hfp_resume_ble_adv();
    }
#endif
    
    app_tws_preparation_for_roleswich_after_hf_audio_disconnection();
    btdrv_dynamic_patch_sco_status_clear();
#ifdef __BT_ONE_BRING_TWO__
    if (app_bt_device.hf_channel[chan_id_flag.id_other].state != BTIF_HF_STATE_OPEN)

        TRACE("!!!HF_EVENT_AUDIO_DISCONNECTED  hfchan_call[chan_id_flag.id_other]:%d\n",app_bt_device.hfchan_call[chan_id_flag.id_other]);
    if ((app_bt_device.hfchan_call[chan_id_flag.id_other] == BTIF_HF_CALL_ACTIVE) ||
            (app_bt_device.hfchan_callSetup[chan_id_flag.id_other] == BTIF_HF_CALL_SETUP_IN)) {
        app_bt_device.curr_hf_channel_id = chan_id_flag.id_other;
        TRACE("!!!HF_EVENT_AUDIO_DISCONNECTED  app_bt_device.curr_hf_channel_id:%d\n",app_bt_device.curr_hf_channel_id);
    } else {
        app_bt_device.curr_hf_channel_id = chan_id_flag.id;
    }

    hf_voice_en[chan_id_flag.id] = HF_VOICE_DISABLE;
    if (app_bt_device.hf_audio_state[chan_id_flag.id_other] == BTIF_HF_AUDIO_CON) {
        hf_voice_en[chan_id_flag.id_other] = HF_VOICE_ENABLE;
        TRACE("chan_id:%d AUDIO_DISCONNECTED, then enable id_other voice",chan_id_flag.id);
    }
    app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_STOP,BT_STREAM_VOICE,chan_id_flag.id,MAX_RECORD_NUM,0,0);
#else

    app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_STOP,BT_STREAM_VOICE,BT_DEVICE_ID_1,MAX_RECORD_NUM,0,0);
#endif
}

static void hfp_ring_ind_handler(hf_chan_handle_t chan, struct hfp_context *ctx)
{
    enum BT_DEVICE_ID_T chan_id;

    chan_id = chan_id_flag.id;

    TRACE("::HF_EVENT_RING_IND  chan_id:%d\n", chan_id);
//#if !defined(FPGA) && defined(__BTIF_EARPHONE__)
#ifndef FPGA
    if (app_bt_device.hf_audio_state[chan_id_flag.id] != BTIF_HF_AUDIO_CON) {
#if defined(TWS_RING_SYNC) && defined(__TWS__)
        TRACE("tws_ring_sync\n");
        tws_ring_sync(ctx->event);
        log_event_1(EVENT_HFP_INCOMING_CALL, ctx->remote_dev_hcihandle & 3);
        app_voice_report(APP_STATUS_INDICATION_INCOMINGCALL,chan_id);
#else
        log_event_1(EVENT_HFP_INCOMING_CALL, ctx->remote_dev_hcihandle & 3);
        app_voice_report(APP_STATUS_INDICATION_INCOMINGCALL,chan_id);
#endif
    }
#endif
}

static void hfp_speak_volume_handler(hf_chan_handle_t chan, struct hfp_context *ctx)
{
    TRACE("::HF_EVENT_SPEAKER_VOLUME  chan_id:%d,speaker gain = %x\n",
                                    chan_id_flag.id, ctx->speaker_volume);
    hfp_volume_set((int)(uint32_t)ctx->speaker_volume);

#ifdef __TWS_CALL_DUAL_CHANNEL__
#ifdef __TWS__
    tws_player_set_hfp_vol(hfp_volume_get());
#endif
#endif
}

static void hfp_battery_report(hf_chan_handle_t chan, struct hfp_context *ctx)
{
    TRACE("report battry level");
    app_hfp_battery_report_reset(chan_id_flag.id);
    app_hfp_battery_report(app_battery_current_level());
}

static void app_hfp_event_callback(hf_chan_handle_t chan, struct hfp_context *ctx)
{
#ifdef __BT_ONE_BRING_TWO__
    hfp_chan_id_distinguish(chan);
#else
    chan_id_flag.id = BT_DEVICE_ID_1;
#endif

    switch(ctx->event) {
    case BTIF_HF_EVENT_SERVICE_CONNECTED:
        hfp_connected_ind_handler(chan, ctx);
        hfp_service_connected_set(true);
        break;
    case BTIF_HF_EVENT_AUDIO_DATA_SENT:
        hfp_audio_data_sent_handler(chan, ctx);
        break;
    case BTIF_HF_EVENT_AUDIO_DATA:
        hfp_audio_data_handler(chan, ctx);
        break;
    case BTIF_HF_EVENT_SERVICE_DISCONNECTED:
        hfp_disconnected_ind_handler(chan, ctx);
        hfp_service_connected_set(false);
        break;
    case BTIF_HF_EVENT_CALL_IND:
        hfp_call_ind_handler(chan, ctx);
        break;
    case BTIF_HF_EVENT_CALLSETUP_IND:
        hfp_callsetup_ind_handler(chan, ctx);
        break;
    case BTIF_HF_EVENT_CALLHELD_IND:
        hfp_callheld_ind_handler(chan, ctx);
        break;
    case BTIF_HF_EVENT_CURRENT_CALL_STATE:
        hfp_current_call_state_handler(chan, ctx);
        break;
    case BTIF_HF_EVENT_AUDIO_CONNECTED:
        hfp_audio_connected_handler(chan, ctx);
        break;
    case BTIF_HF_EVENT_AUDIO_DISCONNECTED:
        hfp_audio_disconnected_handler(chan, ctx);
        break;
    case BTIF_HF_EVENT_RING_IND:
        hfp_ring_ind_handler(chan, ctx);
        break;
    case BTIF_HF_EVENT_SPEAKER_VOLUME:
        hfp_speak_volume_handler(chan, ctx);
        break;
    case BTIF_HF_EVENT_AT_ACCESSORY_COMPLETE:
        hfp_battery_report(chan,ctx);
        break;       
#ifdef SUPPORT_SIRI
    case BTIF_HF_EVENT_SIRI_STATUS:
        break;
    case BTIF_HF_EVENT_VOICE_REC_STATE:
        break;
#endif
    case BTIF_HF_EVENT_READ_AG_INDICATORS_STATUS:
        break;
    case BTIF_HF_EVENT_COMMAND_COMPLETE:
        break;
    default:
        break;
    }
}

#ifdef __BTIF_EARPHONE__
/////////profile safely exit
bt_status_t btapp_link_disconnect_directly(void)
{
    hf_chan_handle_t chan;
	a2dp_stream_t *a2dp_stream1;	//stack A2dpStream  object

    chan = app_bt_device.hf_channel[BT_DEVICE_ID_1];
    if (btif_get_hf_chan_state(chan) == BTIF_HF_STATE_OPEN)
        btif_hf_disconnect_service_link(chan);

    a2dp_stream1 = app_bt_device.a2dp_stream[BT_DEVICE_ID_1]->a2dp_stream;
    if(btif_a2dp_get_stream_state(a2dp_stream1) == BTIF_AVDTP_STRM_STATE_STREAMING ||
                btif_a2dp_get_stream_state(a2dp_stream1) == BTIF_AVDTP_STRM_STATE_OPEN)
        btif_a2dp_close_stream(app_bt_device.a2dp_stream[BT_DEVICE_ID_1]->a2dp_stream);

#ifdef __BT_ONE_BRING_TWO__
	a2dp_stream_t *a2dp_stream2;	//stack A2dpStream  object

    chan = app_bt_device.hf_channel[BT_DEVICE_ID_2];
    if (btif_get_hf_chan_state(chan) == BTIF_HF_STATE_OPEN) {
        btif_hf_disconnect_service_link(chan);
    }

    a2dp_stream2 = app_bt_device.a2dp_stream[BT_DEVICE_ID_2]->a2dp_stream;
    if(btif_a2dp_get_stream_state(a2dp_stream2) == BTIF_AVDTP_STRM_STATE_STREAMING ||
            btif_a2dp_get_stream_state(a2dp_stream2) == BTIF_AVDTP_STRM_STATE_OPEN)
        btif_a2dp_close_stream(a2dp_stream2);

#endif

    return BT_STS_SUCCESS;
}
#endif

bool btapp_hfp_call_is_active(void)
{
    uint8_t i;

    for (i = 0; i < BT_DEVICE_NUM; i++) {
        if(app_bt_device.hfchan_call[i] == BTIF_HF_CALL_ACTIVE)
            return true;
    }
    return false;
}

bool btapp_hfp_call_is_present(void)
{
    uint8_t i;

    for (i = 0; i < BT_DEVICE_NUM; i++) {
        if((app_bt_device.hfchan_call[i] == BTIF_HF_CALL_ACTIVE) ||
            (app_bt_device.hfchan_callSetup[i] != BTIF_HF_CALL_SETUP_NONE)){
            return true;
        }
    }
    return false;
}

void btapp_hfp_report_speak_gain(void)
{
    uint8_t i;
    hf_chan_handle_t chan;

    for(i = 0; i < BT_DEVICE_NUM; i++) {
        chan = app_bt_device.hf_channel[i];
        if ((btif_get_hf_chan_state(chan) == BTIF_HF_STATE_OPEN))
            btif_hf_report_speaker_volume(chan, hfp_volume_get());
    }
}

static hfp_sco_codec_t bt_sco_current_codecid = BTIF_HF_SCO_CODEC_CVSD;

void bt_sco_set_current_codecid(hfp_sco_codec_t id)
{
    bt_sco_current_codecid = id;
}

hfp_sco_codec_t bt_sco_get_current_codecid(void)
{
    return bt_sco_current_codecid;
}

uint8_t btapp_hfp_need_mute(void)
{
    return app_bt_device.hf_mute_flag;
}

bool btapp_hfp_is_voice_ongoing(void)
{
    return BTIF_HF_AUDIO_CON == app_bt_device.hf_audio_state[chan_id_flag.id];
}

#ifndef _SCO_BTPCM_CHANNEL_
void app_hfp_clear_ctrl_buffer(void)
{
    memset(&hf_sendbuff_ctrl, 0, sizeof(hf_sendbuff_ctrl));
}
#endif

/*TODO: remove to another files*/
bt_status_t app_bt_disconnect_hfp(void)
{
    hf_chan_handle_t chan = app_bt_device.ptrHfChannel[BT_DEVICE_ID_1];

    return btif_hf_disconnect_service_link(chan);
}
