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

#include "a2dp_api.h"
#include "me_api.h"
#include "hci_api.h"
#include "bt_drv_reg_op.h"
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
#include "app_tws_if.h"
#include "app_bt_conn_mgr.h"
#include "app_tws_role_switch.h"

#include "log_section.h"

#ifdef __TWS_PAIR_DIRECTLY__
#include "ble_tws.h"
#endif

#define TWS_CONNECT_DEBUG
#ifdef TWS_CONNECT_DEBUG
#define TWSCON_DBLOG LOG_PRINT_TWS        
#else
#define TWSCON_DBLOG(...)
#endif

#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)



#define TWS_SET_STREAM_STATE(x) \
    {\
    tws.tws_source.stream_state  = x; \
    }

#define TWS_SET_M_STREAM_STATE(x) \
    {\
    tws.mobile_sink.stream_state  = x; \
    }
#endif
//#define __TWS_MASTER_STAY_MASTER__

static btif_avdtp_codec_t setconfig_codec;
extern btif_avdtp_media_header_t media_header;
extern U8 get_valid_bit(U8 elements, U8 mask);

//extern  tws_dev_t  tws;
extern struct BT_DEVICE_T  app_bt_device;

//MeCommandToken me_cmd_tkn;

uint8_t tws_inq_addr_used;

typedef struct {
    uint8_t used;
    bt_bdaddr_t bdaddr;
}TWS_INQ_ADDR_STRUCT;

TWS_INQ_ADDR_STRUCT tws_inq_addr[5];


static void app_tws_inquiry_timeout_handler(void const *param);

void btdrv_rf_set_conidx(uint32_t conidx);

osTimerDef (APP_TWS_INQ, app_tws_inquiry_timeout_handler);

static osTimerId app_tws_timer = NULL;

static uint8_t tws_find_process=0;
static uint8_t tws_inquiry_count=0;
#define MAX_TWS_INQUIRY_TIMES   10

#if FPGA==0
extern uint8_t app_poweroff_flag;
#endif
int hfp_volume_get(void);
void a2dp_restore_sample_rate_after_role_switch(a2dp_stream_t *Stream, const a2dp_callback_parms_t *Info);

#ifdef __TWS_RECONNECT_USE_BLE__

app_tws_ble_reconnect_t app_tws_ble_reconnect;

void app_tws_set_slave_adv_data(void)
{
#if !defined(IAG_BLE_INCLUDE)
    uint8_t adv_data[] = {
        0x04, 0x09, 'T', 'W', 'S',  // Ble Name
        0x02, 0xff, 0x00  // manufacturer data
    };

    if (app_tws_ble_reconnect.isConnectedPhone)
    {
        adv_data[7] = 1; 
    }

    btif_me_ble_set_adv_data(sizeof(adv_data), adv_data);
#else
    tws_reconnect_ble_set_adv_data(app_tws_ble_reconnect.isConnectedPhone);
#endif
}

void app_tws_set_slave_adv_para(void)
{
#if !defined(IAG_BLE_INCLUDE)
    uint8_t  peer_addr[BTIF_BD_ADDR_SIZE] = {0};
    adv_para_struct para; 

    if (app_tws_ble_reconnect.isConnectedPhone)
    {
        para.interval_min =             0x0320; // 1s
        para.interval_max =             0x0320; // 1s
    
    } else {
        para.interval_min =             0x0020; // 20ms
        para.interval_max =             0x0020; // 20ms
    }
    para.adv_type =                 0x03;
    para.own_addr_type =            0x01;
    para.peer_addr_type =           0x01;
    para.adv_chanmap =              0x07;
    para.adv_filter_policy =        0x00;
    memcpy(para.bd_addr.addr, peer_addr, BTIF_BD_ADDR_SIZE);

    btif_me_ble_set_adv_parameters(&para);
#else
    tws_reconnect_ble_set_adv_para(app_tws_ble_reconnect.isConnectedPhone);
#endif
}

void app_tws_set_slave_adv_en(BOOL listen)
{
#if !defined(IAG_BLE_INCLUDE)
    TRACE("%s,adv en=%x",__func__,listen);

    btif_me_ble_set_adv_en(listen);
#else
    tws_ble_listen(listen);
#endif
}


bt_status_t app_tws_set_slave_adv_disen(void)
{
    bt_status_t  status;
#if !defined(IAG_BLE_INCLUDE)
    TRACE("%s",__func__);

    status = btif_me_ble_set_adv_disen();
    return status;
#else
    tws_ble_listen(0);
#endif
}



void app_tws_set_master_scan_para(void)
{
    btif_scan_para_struct_t  para; 

    para.scan_type =            0x00;
    if (app_tws_ble_reconnect.isConnectedPhone)
    {
        TRACE("%s ConnectedPhone", __func__);
        para.scan_interval =        0x0640; // 1 s
        para.scan_window =          0x0020; // 10 ms
    } else {
        TRACE("%s Not ConnectedPhone", __func__);
        para.scan_interval =        0xa0; // 100 ms
        para.scan_window =          0x50; //50 ms
    }
    para.own_addr_type =        0x00;
    para.scan_filter_policy =   0x01;

    TRACE("%s", __func__);

    btif_me_set_scan_parameter(&para);
}


void app_tws_set_master_scan_en(BOOL enable)
{
    uint8_t le_scan_enable = enable ? 0x01: 0x00;
    uint8_t filter_duplicates = 0x01;
    TRACE("%s,scan en=%x",__func__,le_scan_enable);
    btif_me_ble_set_scan_en(le_scan_enable, filter_duplicates);
}


void app_tws_master_add_whitelist(bt_bdaddr_t *addr)
{
#if BLE_IS_USE_RPA
    btif_me_ble_add_dev_to_whitelist(1,addr);
#else
    btif_me_ble_add_dev_to_whitelist(0,addr);
#endif
}

static void POSSIBLY_UNUSED app_tws_clear_whitelist(void)
{
    btif_me_ble_clear_whitelist();
}

void app_tws_set_private_address(void)
{
#if !defined(IAG_BLE_INCLUDE)
    btif_me_ble_set_private_address((bt_bdaddr_t *)&bt_addr[0]);
#endif
}

bt_status_t app_tws_master_ble_scan_process(uint8_t enable)
{
    bt_status_t status; 
    btif_device_record_t  record;
    struct nvrecord_env_t *nvrecord_env;
    nv_record_env_get(&nvrecord_env);

   // status = SEC_FindDeviceRecord((bt_bdaddr_t *)&nvrecord_env->tws_mode.record.bdAddr, &record);
    status = btif_sec_find_device_record((bt_bdaddr_t *)&nvrecord_env->tws_mode.peerBleAddr, &record);
    if (enable == 1 && app_bt_device.hf_audio_state[0] == BTIF_HF_AUDIO_DISCON) {
        if (status == BT_STS_SUCCESS){
            app_tws_master_add_whitelist(&record.bdAddr);
            app_tws_set_master_scan_para();
            TRACE("%s open_ble_scan", __func__);
            app_tws_set_master_scan_en(true);
        }
    } else if (enable == 0) {
        TRACE("%s close_ble_scan", __func__);
        app_tws_set_master_scan_en(false);
    } else {
        return  XA_STATUS_FAILED;
    }

    return status;
}

bt_status_t app_tws_slave_ble_adv_process(uint8_t enable)
{
    bt_status_t status = BT_STS_SUCCESS; 

    if (enable == 1 && app_bt_device.hf_audio_state[0] == BTIF_HF_AUDIO_DISCON) {
        app_tws_set_private_address();
        app_tws_set_slave_adv_para();
        app_tws_set_slave_adv_data();

        TRACE("%s open_ble_adv",__func__);
        app_tws_set_slave_adv_en(true);
    } else if (enable == 0) {
        TRACE("%s close_ble_adv",__func__);
        app_tws_set_slave_adv_en(false);
    } else {
        return  XA_STATUS_FAILED;
    }

    return status;
}


bt_status_t app_tws_slave_ble_close_adv_process(uint8_t enable)
{
    bt_status_t status = BT_STS_SUCCESS; 


    status = app_tws_set_slave_adv_disen();

    return status;
}



void app_tws_reconnect_phone_start(void* arg);
void master_ble_scan_timer_callback(void const *n) {
    TRACE("%s master_ble_scan_timer fired", __func__);
    if (app_tws_ble_reconnect.scan_func)
        app_tws_ble_reconnect.scan_func(APP_TWS_CLOSE_BLE_SCAN);
    
    app_start_custom_function_in_app_thread(0 ,0, (uint32_t)app_tws_reconnect_phone_start);
}
osTimerDef(master_ble_scan_timer, master_ble_scan_timer_callback);

void app_tws_slave_delay_to_roleSwitch(void);
void slave_delay_role_switch_timer_callback(void const *n) {
    TRACE("%s slave_role_switch_timer_callback", __func__);
    app_tws_slave_delay_to_roleSwitch();
}

osTimerDef(slave_delay_role_switch_timer, slave_delay_role_switch_timer_callback);

void tws_ble_init(void);
void app_tws_ble_reconnect_init(void)
{
    app_tws_ble_reconnect.adv_func = app_tws_slave_ble_adv_process;
    app_tws_ble_reconnect.scan_func = app_tws_master_ble_scan_process;
    app_tws_ble_reconnect.adv_close_func = app_tws_slave_ble_close_adv_process;
    app_tws_ble_reconnect.bleReconnectPending = 0;
    app_tws_ble_reconnect.tws_start_stream_pending = 0;

    app_tws_ble_reconnect.master_ble_scan_timerId = osTimerCreate(osTimer(master_ble_scan_timer), osTimerOnce, (void *)0); 
    app_tws_ble_reconnect.slave_delay_role_switch_timerId = osTimerCreate(osTimer(slave_delay_role_switch_timer), osTimerOnce, (void *)0); 
    app_tws_ble_reconnect.isConnectedPhone = 0;

    tws_ble_init();
}
#endif



btif_avdtp_codec_t* app_tws_get_set_config_codec_info(void)
{
    return &setconfig_codec;
}

void set_a2dp_reconfig_codec(a2dp_stream_t *Stream)
{
    tws_dev_t *twsd = &tws;
    enum AUD_SAMPRATE_T sample_rate;

    sample_rate = bt_parse_store_sbc_sample_rate(btif_a2dp_get_stream_codecCfg(Stream)->elements[0]);

    twsd->audout.sample_rate = (uint32_t)sample_rate;
    twsd->pcmbuff.sample_rate = (uint32_t)sample_rate;
    
    if(btif_a2dp_get_stream_codecCfg(Stream)->elements[1]==A2D_SBC_IE_BIT_NUM_24){
        twsd->mobile_samplebit = 24;
    }else if(btif_a2dp_get_stream_codecCfg(Stream)->elements[1]==A2D_SBC_IE_BIT_NUM_16){
        twsd->mobile_samplebit = 16;
    }else{
        twsd->mobile_samplebit = 16;
    }
    app_bt_device.sample_bit[BT_DEVICE_ID_1] = twsd->mobile_samplebit;

    TRACE("[%s] sample_rate:%d bit:%d", __func__, sample_rate, twsd->mobile_samplebit);
}

#if (defined(SLAVE_USE_OPUS) || defined(MASTER_USE_OPUS) || defined(ALL_USE_OPUS) || defined( SLAVE_USE_ENC_SINGLE_SBC) ||defined(A2DP_AAC_ON) ) && defined(__TWS_OUTPUT_CHANNEL_SELECT__)
void app_tws_split_channel_select(uint8_t *locaddr,uint8_t *peeraddr)
{
        uint32_t local_uap = (locaddr[2]<<16) | (locaddr[1]<<8) | locaddr[0];
        uint32_t peer_uap = (peeraddr[2]<<16) | (peeraddr[1]<<8) | peeraddr[0];
#ifdef __TWS_CHANNEL_LEFT__
        tws.decode_channel= SBC_DEC_LEFT;
        tws.tran_channel = SBC_SPLIT_RIGHT;

#elif defined(__TWS_CHANNEL_RIGHT__)
        tws.decode_channel = SBC_DEC_RIGHT;
        tws.tran_channel = SBC_SPLIT_LEFT;        

#else
        TWSCON_DBLOG("LUAP=%x,PUAP=%x",local_uap,peer_uap);
        if(local_uap >= peer_uap)
        {
            if(local_uap == peer_uap)
            {
                if(TWS_MASTER_CONN_SLAVE == app_tws_get_conn_state())
                {
                    tws.decode_channel= BTIF_SBC_DEC_LEFT;
                    tws.tran_channel = BTIF_SBC_SPLIT_RIGHT;                    
                }
                else
                {
                    tws.decode_channel= BTIF_SBC_DEC_RIGHT;
                    tws.tran_channel = BTIF_SBC_SPLIT_LEFT;                 
                }
            }
            else
            {
                tws.decode_channel= BTIF_SBC_DEC_LEFT;
                tws.tran_channel = BTIF_SBC_SPLIT_RIGHT;
            }
        }
        else
        {
            tws.decode_channel = BTIF_SBC_DEC_RIGHT;
            tws.tran_channel = BTIF_SBC_SPLIT_LEFT;        
        }
#endif  
}

#endif


void app_tws_channel_report_timeout_handler(void const *param)
{
    app_voice_report(APP_STATUS_INDICATION_TWS_RIGHTCHNL,0);    
}

osTimerDef (APP_TWS_CHANNEL_REPORT, app_tws_channel_report_timeout_handler);
static osTimerId POSSIBLY_UNUSED app_tws_channel_report_timer = NULL;

void app_tws_Indicate_connection(void)
{
    TRACE("TWSUI:%s",__func__ );
#ifndef __TWS_PAIR_DIRECTLY__
    return;
#endif

#ifdef MEDIA_PLAYER_SUPPORT
    //app_voice_report(APP_STATUS_INDICATION_CONNECTED,0);
#endif
    //      app_voice_report(APP_STATUS_INDICATION_TWS_ISSLAVE,0);
    app_status_indication_set(APP_STATUS_INDICATION_CONNECTED);
#if (defined(SLAVE_USE_OPUS) || defined(MASTER_USE_OPUS) || defined( SLAVE_USE_ENC_SINGLE_SBC) || defined(A2DP_AAC_ON)) && defined(__TWS_OUTPUT_CHANNEL_SELECT__)
#ifdef __TWS_CHANNEL_LEFT__
    //app_voice_report(APP_STATUS_INDICATION_TWS_LEFTCHNL,0);
#elif defined(__TWS_CHANNEL_RIGHT__)
    if (app_tws_channel_report_timer == NULL)
        app_tws_channel_report_timer = osTimerCreate(osTimer(APP_TWS_CHANNEL_REPORT), osTimerOnce, NULL);  
    osTimerStart(app_tws_channel_report_timer, 2000);
    //app_voice_report(APP_STATUS_INDICATION_TWS_RIGHTCHNL,0);
#else
    tws_dev_t *twsd = &tws;
    if(twsd->decode_channel == BTIF_SBC_DEC_RIGHT)
    {
        //app_voice_report(APP_STATUS_INDICATION_TWS_RIGHTCHNL,0);
        if (app_tws_channel_report_timer == NULL)
            app_tws_channel_report_timer = osTimerCreate(osTimer(APP_TWS_CHANNEL_REPORT), osTimerOnce, NULL);  
        osTimerStart(app_tws_channel_report_timer, 2000);     
    }
    else
    {
        app_voice_report(APP_STATUS_INDICATION_TWS_LEFTCHNL,0);
    }
#endif
#endif
}

bool app_tws_store_info_to_nv(DeviceMode mode, uint8_t* peer_addr)
{
    bool isNeedToUpdateNv = true;
    struct nvrecord_env_t *nvrecord_env;
    nv_record_env_get(&nvrecord_env);
    
    // check whether the nv needs to be updated
    if (nvrecord_env->tws_mode.mode == mode)
    {
        if (TWSMASTER == mode)
        {
            if (!memcmp(nvrecord_env->tws_mode.slaveAddr.address, peer_addr, BTIF_BD_ADDR_SIZE))
            {
                isNeedToUpdateNv = false;
            }
        }
        else if (TWSSLAVE == mode)
        {
            if (!memcmp(nvrecord_env->tws_mode.masterAddr.address, peer_addr, BTIF_BD_ADDR_SIZE))
            {
                isNeedToUpdateNv = false;
            }
        }  
    }
    
    if (isNeedToUpdateNv)
    {
        nvrecord_env->tws_mode.mode = mode;
    
        if (TWSMASTER == mode)
        {
            TRACE("Update the TWS mode as Master");
            memcpy(nvrecord_env->tws_mode.masterAddr.address, bt_addr, BTIF_BD_ADDR_SIZE);
            memcpy(nvrecord_env->tws_mode.slaveAddr.address, peer_addr, BTIF_BD_ADDR_SIZE);
        }
        else if (TWSSLAVE == mode)
        {
            TRACE("Update the TWS mode as Slave");
            memcpy(nvrecord_env->tws_mode.masterAddr.address, peer_addr, BTIF_BD_ADDR_SIZE);
            memcpy(nvrecord_env->tws_mode.slaveAddr.address, bt_addr, BTIF_BD_ADDR_SIZE);
        }
    
        if (TWSFREE != mode)
        {
            TRACE("slave addr is:");
            DUMP8("0x%02x ", nvrecord_env->tws_mode.slaveAddr.address, sizeof(bt_bdaddr_t));
            TRACE("\nmaster addr is:");
            DUMP8("0x%02x ", nvrecord_env->tws_mode.masterAddr.address, sizeof(bt_bdaddr_t));
        }

        nv_record_env_set(nvrecord_env); 
    #if FPGA==0
        app_audio_switch_flash_flush_req();
        // nv_record_flash_flush();
    #endif    
    }

    return isNeedToUpdateNv;
}

#ifdef __TWS_USE_RANDOM_ADDRESS__
extern "C" void gapm_set_address_type(uint8_t value);
#include "gapm_task.h"
#endif
void app_tws_store_peer_ble_addr_to_nv(uint8_t* peer_ble_addr)
{
    struct nvrecord_env_t *nvrecord_env;
    nv_record_env_get(&nvrecord_env);
    memcpy(nvrecord_env->tws_mode.peerBleAddr.address, peer_ble_addr, BTIF_BD_ADDR_SIZE);
#if BLE_IS_USE_RPA 
    btif_me_ble_add_dev_to_whitelist(1, &(tws_mode_ptr()->peerBleAddr));
#else
    btif_me_ble_add_dev_to_whitelist(0, &(tws_mode_ptr()->peerBleAddr));
#endif
}

void app_tws_switch_role_in_nv(void)
{
    struct nvrecord_env_t *nvrecord_env;
    nv_record_env_get(&nvrecord_env);

    TRACE("Change run-time tws mode from %d", nvrecord_env->tws_mode.mode);
        
    if ((TWSMASTER == nvrecord_env->tws_mode.mode) || (TWSSLAVE == nvrecord_env->tws_mode.mode))
    {        
        if (TWSMASTER == nvrecord_env->tws_mode.mode)
        {
            nvrecord_env->tws_mode.mode = TWSSLAVE;
        }
        else
        {
            nvrecord_env->tws_mode.mode = TWSMASTER;
        }    

        nv_record_env_set(nvrecord_env); 
        nv_record_flash_flush();
    }

    TRACE("To %d", nvrecord_env->tws_mode.mode);
}

#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)            
extern void rb_play_reconfig_stream(uint32_t arg );
#endif

static void tws_retrigger_hfp_voice_callback(uint32_t status, uint32_t param)
{
  TRACE("tws_retrigger_hfp_voice_callback audio state=%d",app_bt_device.hf_audio_state[0]);
  if(app_bt_device.hf_audio_state[0] != BTIF_HF_AUDIO_CON)
  {
        app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_STOP,BT_STREAM_VOICE,BT_DEVICE_ID_1,MAX_RECORD_NUM,0,0);  
  }
}


static void tws_stop_hfp_voice_callback(uint32_t status, uint32_t param)
{
  TRACE("tws_retrigger_hfp_voice_callback audio state=%d",app_bt_device.hf_audio_state[0]);
  if(app_bt_device.hf_audio_state[0] == BTIF_HF_AUDIO_CON)
  {
//      osDelay(30);
      app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_START,BT_STREAM_VOICE,BT_DEVICE_ID_1,MAX_RECORD_NUM,1,(uint32_t)tws_retrigger_hfp_voice_callback);
  }
}

void a2dp_source_of_tws_opened_ind_handler(a2dp_stream_t *Stream, const a2dp_callback_parms_t *info)
{            
    btif_a2dp_callback_parms_t *Info =(btif_a2dp_callback_parms_t *)info;
    TRACE("A2DP with TWS slave is opened, BD address is:");
    DUMP8("0x%02x ", btif_a2dp_get_stream_devic_cmgrHandler_remdev_bdAddr(Stream), BTIF_BD_ADDR_SIZE);

#ifdef __TWS_PAIR_DIRECTLY__
    //update as tws master
    tws_cfg.mode = TWSMASTER;
#if 0
    if(!nv_record_is_find_tws_addr(btif_a2dp_get_stream_devic_cmgrHandler_remdev_bdAddr(Stream)))
    {   
        TRACE("tws opened firstly !");
        twsBoxEnv.twsModeToStart = TWSMASTER;
        memcpy(twsBoxEnv.twsBtBdAddr[0], bt_addr, BD_ADDR_LEN);
        memcpy(twsBoxEnv.twsBtBdAddr[1], btif_a2dp_get_stream_devic_cmgrHandler_remdev_bdAddr(Stream), BTIF_BD_ADDR_SIZE);
        nv_record_update_tws_mode(TWSMASTER);
        memcpy(tws_mode_ptr()->masterAddr.address, twsBoxEnv.twsBtBdAddr[0], BTIF_BD_ADDR_SIZE);
        memcpy(tws_mode_ptr()->slaveAddr.address, twsBoxEnv.twsBtBdAddr[1], BTIF_BD_ADDR_SIZE);
    }
#endif
    bt_drv_reg_op_set_swagc_mode(2);
        
#endif


    if (app_tws_is_to_update_sample_rate_after_role_switch())
    {
        a2dp_restore_sample_rate_after_role_switch(Stream, Info);
    }

    tws_dev_t *twsd = &tws;
    twsd->tws_source.connected = true;
#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)
    TWS_SET_STREAM_STATE(TWS_STREAM_OPEN)
#endif
    app_tws_set_conn_state(TWS_MASTER_CONN_SLAVE);
    TRACE("Master conhdl=%d\n",  btif_a2dp_get_stream_device_cmgrhandler_remDev_hciHandle(Stream));
    app_tws_set_tws_conhdl(    btif_a2dp_get_stream_device_cmgrhandler_remDev_hciHandle(Stream));
#ifdef __TWS_OUTPUT_POWER_FIX_SEPARATE__            
    bt_drv_reg_op_tws_output_power_fix_separate(btif_a2dp_get_stream_device_cmgrhandler_remDev_hciHandle(Stream), 6);
#endif
    btdrv_rf_bit_offset_track_enable(true);

    if(twsd->mobile_sink.connected == false) {
        btdrv_rf_trig_patch_enable(0);
    }
    
#ifdef __TWS_FOLLOW_MOBILE__            
    bt_drv_reg_op_afh_follow_mobile_twsidx_set(btif_a2dp_get_stream_device_cmgrhandler_remDev_hciHandle(Stream));
#endif

    memcpy(tws.peer_addr.address, btif_a2dp_get_stream_devic_cmgrHandler_remdev_bdAddr(Stream),6);       

    btdrv_set_tws_role_triggler(TWS_MASTER_CONN_SLAVE);
    APP_TWS_SET_CURRENT_OP(APP_TWS_IDLE_OP);
    APP_TWS_SET_NEXT_OP(APP_TWS_IDLE_OP);
#ifdef __TWS_CALL_DUAL_CHANNEL__
    if (!is_simulating_reconnecting_in_progress())
    {
        if(twsd->mobile_sink.connected == true)
        {
#ifdef CHIP_BEST2000
            if(hal_get_chip_metal_id() >=2)
            {
                a2dp_stream_t *stream =  NULL;
                stream = twsd->mobile_sink.stream;                
                if(btif_a2dp_get_stream_device(stream) && btif_a2dp_get_stream_devic_cmgrHandler_remdev(stream))
                {
                    bt_drv_reg_op_afh_bak_save(btif_a2dp_get_stream_devic_cmgrHandler_remdev_role(stream), tws.mobile_conhdl);
                }
                else
                {
                    TRACE("ROLE ERROR ,PLEASE CHECK THE MOBILE LINK STATUS!");
                }
            } 
#endif
            btif_me_set_sniffer_env(1, FORWARD_ROLE, mobileDevBdAddr.address, tws_mode_ptr()->slaveAddr.address);
            tws_player_set_hfp_vol(hfp_volume_get());
        }
    }
#endif

#if (defined(SLAVE_USE_OPUS) || defined(MASTER_USE_OPUS) || defined(ALL_USE_OPUS) || defined( SLAVE_USE_ENC_SINGLE_SBC)) && defined(__TWS_OUTPUT_CHANNEL_SELECT__)
    app_tws_split_channel_select(bt_addr,tws.peer_addr.address);
#endif            
    twsd->notify(twsd); //notify link task        
    twsd->unlock_mode(&tws);
    if(twsd->stream_idle == 1)
    {
        twsd->stream_idle = 0;
    }
    else
    {
        app_tws_Indicate_connection();
    }

    //if (!is_simulating_reconnecting_in_progress())
    //{

        // connect AVRCP with TWS slave
        //AVRCP_Connect(twsd->rcp, &Stream->device->cmgrHandler.remDev->bdAddr);
    //}

    // update connection state
    uint32_t formerTwsConnState = twsConnectionState;
    SET_CONN_STATE_A2DP(twsConnectionState);
    log_event_2(EVENT_A2DP_CONNECTED, BT_FUNCTION_ROLE_SLAVE, 3&    btif_me_get_remote_device_hci_handle( slaveBtRemDev));
    if (formerTwsConnState != twsConnectionState)
    {
        tws_connection_state_updated_handler(BTIF_BEC_NO_ERROR);
    }

    ///if phone has been connected
    if(app_bt_device.a2dp_state[0]){
        TWSCON_DBLOG("a2dp_callback_tws_source  tws samplerate:%x,mobile samplerate:%x\n",Info->p.codec->elements[0],app_bt_device.a2dp_codec_elements[0][0]);
        if((app_bt_device.a2dp_codec_elements[0][0] & 0xf0) != (Info->p.codec->elements[0]&0xf0)){
#ifdef A2DP_AAC_ON
            TWSCON_DBLOG("%s A2DP_EVENT_STREAM_OPEN  A2DP_ReconfigStream code = %d\n",__FUNCTION__, twsd->mobile_codectype);
#else
            TWSCON_DBLOG("%s A2DP_EVENT_STREAM_OPEN  A2DP_ReconfigStream\n",__FUNCTION__);
#endif
#if defined(A2DP_LHDC_ON)
            if(twsd->mobile_codectype == BTIF_AVDTP_CODEC_TYPE_LHDC){
                a2dp_stream_t *stream =  NULL;
                stream = twsd->tws_source.stream;
                if(twsd->mobile_samplerate == AUD_SAMPRATE_44100){
                    btif_a2dp_get_stream_codecCfg(stream)->elements[0] &= A2D_SBC_IE_SAMP_FREQ_MSK;
                     btif_a2dp_get_stream_codecCfg(stream)->elements[0] |= A2D_SBC_IE_SAMP_FREQ_44;
                }else if(twsd->audout.sample_rate == AUD_SAMPRATE_48000){
                     btif_a2dp_get_stream_codecCfg(stream)->elements[0] &= A2D_SBC_IE_SAMP_FREQ_MSK;
                     btif_a2dp_get_stream_codecCfg(stream)->elements[0] |= A2D_SBC_IE_SAMP_FREQ_48;
                }else if(twsd->audout.sample_rate == AUD_SAMPRATE_96000){
                     btif_a2dp_get_stream_codecCfg(stream)->elements[0] &= A2D_SBC_IE_SAMP_FREQ_MSK;
                     btif_a2dp_get_stream_codecCfg(stream)->elements[0] |= A2D_SBC_IE_SAMP_FREQ_96;
                }
                if(twsd->mobile_samplebit == 24){
                     btif_a2dp_get_stream_codecCfg(stream)->elements[1] = A2D_SBC_IE_BIT_NUM_24;
                }else if(twsd->mobile_samplebit == 16){
                     btif_a2dp_get_stream_codecCfg(stream)->elements[1] = A2D_SBC_IE_BIT_NUM_16;
                }
                btif_a2dp_reconfig_stream(twsd->tws_source.stream,  btif_a2dp_get_stream_codecCfg(stream), NULL);
            }                
#endif
#if defined(A2DP_AAC_ON)
            if(twsd->mobile_codectype == BTIF_AVDTP_CODEC_TYPE_MPEG2_4_AAC){
                a2dp_stream_t *stream =  NULL;
                stream = twsd->tws_source.stream;
                if(twsd->audout.sample_rate == AUD_SAMPRATE_44100){
                     btif_a2dp_get_stream_codecCfg(stream)->elements[0] &= A2D_SBC_IE_SAMP_FREQ_MSK;
                    btif_a2dp_get_stream_codecCfg(stream)->elements[0] |= A2D_SBC_IE_SAMP_FREQ_44;
                }else if(twsd->audout.sample_rate == AUD_SAMPRATE_48000){
                     btif_a2dp_get_stream_codecCfg(stream)->elements[0] &= A2D_SBC_IE_SAMP_FREQ_MSK;
                     btif_a2dp_get_stream_codecCfg(stream)->elements[0] |= A2D_SBC_IE_SAMP_FREQ_48;
                }
                btif_a2dp_reconfig_stream(twsd->tws_source.stream,  btif_a2dp_get_stream_codecCfg(stream), NULL);
            }else
#endif
            {
                btif_a2dp_reconfig_stream(twsd->tws_source.stream, btif_a2dp_get_stream_codecCfg(Stream),NULL);
            }
        }
        else {
            if (app_bt_device.a2dp_streamming[0] == 1) {
                uint8_t status;
                tws.tws_start_stream_pending = 1;

                log_event_2(EVENT_START_A2DP_STREAMING, btif_a2dp_get_stream_type(tws.tws_source.stream), 3& btif_a2dp_get_stream_device_cmgrhandler_remDev_hciHandle(tws.tws_source.stream));
                status = btif_a2dp_start_stream(tws.tws_source.stream);
                if(status == BT_STS_PENDING)
                {
                    APP_TWS_SET_CURRENT_OP(APP_TWS_START_STREAM);
                }
                else
                {
                    TRACE("%s,start stream error state = %d",__FUNCTION__,status);
                }                
            }
        }
        tws_reset_saved_codectype();
        tws_player_set_codec(tws.mobile_codectype);
    }

    if(twsd->mobile_sink.connected == true && app_bt_device.hf_audio_state[0] == BTIF_HF_AUDIO_CON && app_audio_manager_get_current_media_type() ==BT_STREAM_VOICE )
    {
        app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_STOP,BT_STREAM_VOICE,BT_DEVICE_ID_1,MAX_RECORD_NUM,0,(uint32_t)tws_stop_hfp_voice_callback);

    }
    
#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)            
    tws_local_player_set_tran_2_slave_flag(1);
#endif

#if defined (LBRT) && defined(__LBRT_PAIR__)
    //notify slave to open LBRT mode
    app_tws_toggle_lbrt_mode(LBRT_ENABLE);
#endif
    if (!is_simulating_reconnecting_in_progress())
    {
        TRACE("sync tws volume2");
        //tws_player_sync_vol();
        app_tws_set_slave_volume(a2dp_volume_get_tws());
    }

}

void a2dp_source_of_tws_closed_ind_common_handler(a2dp_stream_t *stream, const a2dp_callback_parms_t *info)
{    
    btif_a2dp_callback_parms_t *Info = ( btif_a2dp_callback_parms_t *)info;
    tws_dev_t *twsd = &tws;

    twsd->stream_idle = 0;
    //twsd->media_suspend = true;
    APP_TWS_SET_CURRENT_OP(APP_TWS_IDLE_OP);
    APP_TWS_SET_NEXT_OP(APP_TWS_IDLE_OP);  

#ifdef __TWS_FOLLOW_MOBILE__
    bt_drv_reg_op_afh_follow_mobile_twsidx_set(0xff);
#endif

#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)   
    TWS_SET_STREAM_STATE(TWS_STREAM_CLOSE)
#endif    
    if (tws.bleReconnectPending) {
        app_tws_reset_tws_data();                
        tws.bleReconnectPending = 0;
    } 
    
    twsd->tws_source.connected = false;
    twsd->tran_blocked =false;
    if(twsd->mobile_sink.connected == true)
    {
        app_tws_set_conn_state(TWS_MASTER_CONN_MOBILE_ONLY);
        restore_tws_channels();
    }else{
        app_tws_set_conn_state(TWS_NON_CONNECTED);
        restore_tws_channels();   
        restore_mobile_channels();
    }

#if defined(A2DP_AAC_ON)
    twsd->tws_set_codectype_cmp = 0;
#endif

#ifdef __TWS_PAIR_DIRECTLY__
    tws_cfg.mode = TWSFREE;
#endif
    // update connection state
    CLEAR_CONN_STATE_A2DP(twsConnectionState);

    log_event_3(EVENT_A2DP_DISCONNECTED, BT_FUNCTION_ROLE_SLAVE, 3&btif_me_get_remote_device_hci_handle( slaveBtRemDev),
        Info->error);

}

void a2dp_source_of_tws_closed_ind_handler(a2dp_stream_t *Stream, const a2dp_callback_parms_t *info)
{
    btif_a2dp_callback_parms_t *Info = ( btif_a2dp_callback_parms_t *)info;
    TRACE("A2DP with TWS slave is closed. Reason is 0x%x", Info->discReason);
    TRACE("Current op state %d", conn_op_state);
    
    a2dp_source_of_tws_closed_ind_common_handler(Stream, Info);
    if (!is_simulating_reconnecting_in_progress())
    {
        app_tws_role_switch_cleanup_a2dp( btif_me_get_remote_device_hci_handle(slaveBtRemDev));
    }

    tws_connection_state_updated_handler(Info->discReason);

    if (!is_simulating_reconnecting_in_progress())
    {
        if (!IS_CONNECTED_WITH_TWS())
        {
            if (IS_CONNECTING_SLAVE())
            {
                set_conn_op_state(CONN_OP_STATE_IDLE);
            }
        }
        
        if (is_tws_connection_creation_supervisor_need_retry() &&
            ((CONN_OP_STATE_MACHINE_DISCONNECT_ALL != CURRENT_CONN_OP_STATE_MACHINE()) ||
             (CONN_OP_STATE_MACHINE_DISCONNECT_ALL_BEFORE_RE_PAIRING != CURRENT_CONN_OP_STATE_MACHINE()) ||
             (CONN_OP_STATE_MACHINE_DISCONNECT_ALL_BEFORE_CHANGE_ROLE != CURRENT_CONN_OP_STATE_MACHINE()) ||
             (CONN_OP_STATE_MACHINE_DISCONNECT_SLAVE != CURRENT_CONN_OP_STATE_MACHINE())
            )
        )
        {
            app_start_custom_function_in_bt_thread(CONNECTING_SLAVE_TIMEOUT_RECONNECT_IN_MS, false, (uint32_t)conn_start_connecting_slave);
        }
        else
        {
            if (is_tws_connection_creation_supervisor_timer_on_going()){
                conn_stop_tws_connection_creation_supvervisor_timer();
            }        
            if (!IS_CONNECTED_WITH_TWS())
            {        
                if (!((BTIF_BEC_USER_TERMINATED == Info->discReason) ||
                    (BTIF_BEC_LOCAL_TERMINATED == Info->discReason)))
                {
                    app_start_custom_function_in_bt_thread(0, 0, 
                        (uint32_t)connecting_tws_slave_timeout_handler);
                }                  
            }
        }
    }  
}

static bool reconfig_sink_codec_request = false;
a2dp_stream_t *need_reconfig_rsp_stream = NULL;

void a2dp_source_reconfig_sink_codec_reset(void)
{
    reconfig_sink_codec_request = false;
}

void a2dp_source_reconfig_sink_codec_request(a2dp_stream_t *Stream)
{
    reconfig_sink_codec_request = true;
    need_reconfig_rsp_stream = Stream;
}

void a2dp_source_reconfig_sink_codec_proc(a2dp_stream_t *Stream)
{
    bt_status_t status;
    if (reconfig_sink_codec_request){
        status = btif_a2dp_reconfig_stream(Stream, btif_a2dp_get_stream_codecCfg(Stream), NULL);
        DUMP8("%02x ",&btif_a2dp_get_stream_codecCfg(Stream)->elements[0],4);
        TRACE("%s status:%d stream_state:%d", __func__, status, btif_a2dp_get_stream_state(Stream));
    }
}

void app_tws_clean_up_connections(void);

extern void a2dp_set_config_codec(btif_avdtp_codec_t *config_codec,const btif_a2dp_callback_parms_t *Info);
#ifdef ANC_APP
extern "C" void tws_anc_sync_status(void);
#endif
void a2dp_source_of_tws_callback(a2dp_stream_t *Stream, const a2dp_callback_parms_t *info)
{
    btif_a2dp_callback_parms_t *Info = ( btif_a2dp_callback_parms_t *)info;
    tws_dev_t *twsd = &tws;
    bool tran_blocked;

    //TRACE("[%s] event=%d ", __func__,Info->event);
    
    switch(Info->event)
    {
        case BTIF_A2DP_EVENT_STREAM_OPEN:
            a2dp_source_of_tws_opened_ind_handler(Stream, Info);
#ifdef ANC_APP
            tws_anc_sync_status();
#endif
            break;
        case BTIF_A2DP_EVENT_STREAM_SBC_PACKET_SENT:
            //TWS_DBLOG("ssbc sent curr t =%d",bt_syn_get_curr_ticks(app_tws_get_tws_conhdl()));
            twsd->tws_source.outpackets += 1;
            tws.lock_prop(&tws);
            tran_blocked = tws.tran_blocked ;
            if(tws.tran_blocked == true)
            {
                tws.tran_blocked = false;
            }
            tws.unlock_prop(&tws);
            if (!tran_blocked){
                // master has send first packet to salve
                if(twsd->tws_source.outpackets==0)
                {
                    TWSCON_DBLOG("Master send first packet...OK!!!");
                    BTCLK_STATUS_PRINT();
                }       
                
            }
            twsd->tws_source.notify_send(&(twsd->tws_source));
#ifdef __TWS_SYNC_WITH_DMA_COUNTER__
            if(a2dp_audio_drop_check_timeout() == 1){
                btif_avdtp_media_header_t* stream_media_header = btif_a2dp_get_stream_media_header(  tws.tws_source.stream);
                //jump seq-number  slave will resync again
                stream_media_header->sequenceNumber += 2;
                a2dp_audio_drop_set_timeout(2);
                LOG_PRINT_DMA_RESYNC("send timeout restore");
            }
#endif
            
#ifdef __TWS_USE_PLL_SYNC__                             
            if(tws.audio_pll_sync_trigger_success == 0xff){
                //reset to start a new adjust process
                tws.audio_pll_cfg = 0;
                tws.audio_dma_trigger_start_counter = 0;
                tws.audio_pll_sync_trigger_success = 0;
            }
#endif                            
            
            break;
        case BTIF_A2DP_EVENT_STREAM_CLOSED:
            if( (Info->p.capability) && Info->len ){
                TWSCON_DBLOG("%s  e:%d t:%d A2DP_EVENT_STREAM_CLOSED\n",__FUNCTION__, Info->event, Info->p.capability->type);
            }
            
            TWSCON_DBLOG("%s %d A2DP_EVENT_STREAM_CLOSED %d,discReason:0x%02x,stream=%x\n",__FUNCTION__,__LINE__, Info->event,Info->discReason,Stream);
            a2dp_source_of_tws_closed_ind_handler(Stream, Info);            
            break;
        case BTIF_A2DP_EVENT_AVDTP_CONNECT:
            break;
        case BTIF_A2DP_EVENT_AVDTP_DISCONNECT: 
        #if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)
            rb_sink_stream_stared();
            if(tws.local_player_wait_reconfig){
                osSemaphoreRelease(tws.wait_stream_reconfig._osSemaphoreId);
                tws.local_player_wait_reconfig = 0;
            }
        #endif        
            break;
        case BTIF_A2DP_EVENT_CODEC_INFO:
            TWSCON_DBLOG("%s %d A2DP_EVENT_CODEC_INFO,error=%d,status=%d,discReason:0x%02x,stream=%x\n",
                                               __FUNCTION__,__LINE__,Info->error,Info->status,Info->discReason,Stream);            
            a2dp_set_config_codec(&setconfig_codec,Info);
            break;
        case BTIF_A2DP_EVENT_GET_CONFIG_IND:
            TWSCON_DBLOG("%s %d A2DP_EVENT_GET_CONFIG_IND,error=%d,status=%d,discReason:0x%02x,stream=%x\n",
                                               __FUNCTION__,__LINE__,Info->error,Info->status,Info->discReason,Stream);            
            setconfig_codec.elements[0] &=0xcf;
            if (twsd->audout.sample_rate == 48000){
                TWSCON_DBLOG("%s %d A2DP_EVENT_GET_CONFIG_IND AUD_SAMPRATE_48000\n",__FUNCTION__,__LINE__);
                setconfig_codec.elements[0] |=A2D_SBC_IE_SAMP_FREQ_48;  
            }else if (twsd->audout.sample_rate == 44100){
                TWSCON_DBLOG("%s %d A2DP_EVENT_GET_CONFIG_IND AUD_SAMPRATE_44100\n",__FUNCTION__,__LINE__);
                setconfig_codec.elements[0] |=A2D_SBC_IE_SAMP_FREQ_44;  
            }else if (twsd->audout.sample_rate == 32000){
                setconfig_codec.elements[0] |=A2D_SBC_IE_SAMP_FREQ_32;
                TWSCON_DBLOG("%s %d A2DP_EVENT_GET_CONFIG_IND AUD_SAMPRATE_32000\n",__FUNCTION__,__LINE__);
            }else if (twsd->audout.sample_rate == 16000){
                setconfig_codec.elements[0] |=A2D_SBC_IE_SAMP_FREQ_16;
                TWSCON_DBLOG("%s %d A2DP_EVENT_GET_CONFIG_IND AUD_SAMPRATE_16000\n",__FUNCTION__,__LINE__);
            }
            btif_a2dp_set_stream_config(Stream, &setconfig_codec, NULL);            
            break;
        case BTIF_A2DP_EVENT_STREAM_START_IND:
            TWSCON_DBLOG("%s %d A2DP_EVENT_STREAM_START_IND,error=%d,status=%d,discReason:0x%02x,stream=%x\n",
                                               __FUNCTION__,__LINE__,Info->error,Info->status,Info->discReason,Stream);
            tws.media_suspend = false;
#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)  
            TWS_SET_STREAM_STATE(TWS_STREAM_START)
#endif
            break;
        case BTIF_A2DP_EVENT_STREAM_STARTED:
            TWSCON_DBLOG("%s  e:%d  eror=%d,status=%d \n",__FUNCTION__, Info->event,Info->error,Info->status);
#if defined(SLAVE_USE_ENC_SINGLE_SBC)
            app_tws_reset_cadun_count();
#endif
                    
            if(Info->error == 0)
            {
                 if(tws.tws_start_stream_pending == 1)
                 {
                     tws.tws_start_stream_pending = 0;
                     app_tws_reset_tws_data();                
                 }
                 tws.bleReconnectPending = 0;

                 if (app_tws_roleswitch_is_pending_for_post_handling_after_role_switch())
                 {
#if 1//def _DISABLR_HFP_RECONNECT_WHEN_ROLE_SWITCH_
                    //do not care hfp
                   //app_resume_mobile_air_path_and_connect_hfp();
                       // resume the air path with the mobile
                     app_resume_mobile_air_path_and_notify_switch_success();
#else
                    app_resume_mobile_air_path_and_connect_hfp();
#endif
                 }

                 tws_player_set_codec(tws.mobile_codectype);    

                 tws.media_suspend = false;
#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER) 
                 TWS_SET_STREAM_STATE(TWS_STREAM_START)
                 rb_sink_stream_stared();
#endif              

                 app_tws_stop_sniff();
            }
            else if(Info->status == BT_STS_TIMEOUT && Info->error == BTIF_AVDTP_ERR_UNKNOWN_ERROR)
            {
               // tws.tws_source.stream->stream.state = BTIF_AVDTP_STRM_STATE_STREAMING;
                btif_a2dp_set_stream_state(tws.tws_source.stream,BTIF_AVDTP_STRM_STATE_STREAMING);      
                tws.media_suspend = false;              
                TRACE("A2DP IDLE STREAM stream=%x",tws.tws_source.stream);
               
            }
            else if(Info->error == BTIF_AVDTP_ERR_BAD_STATE)
            {
                btif_a2dp_set_stream_state(tws.tws_source.stream,BTIF_AVDTP_STRM_STATE_STREAMING);
                tws.media_suspend = false;
            }
            else
            {
                TWSCON_DBLOG("%s,expect error occured here~",__FUNCTION__);
                
            }

            TRACE("A2DP_EVENT_STREAM_STARTED curr op=%d,next op=%d",tws.current_operation,tws.next_operation);
            if(tws.current_operation == APP_TWS_START_STREAM)
            {
                if(tws.next_operation == APP_TWS_START_STREAM)
                {
                    TRACE("next operation is start stream");
                    APP_TWS_SET_CURRENT_OP(APP_TWS_IDLE_OP);
                    APP_TWS_SET_NEXT_OP(APP_TWS_IDLE_OP);
                }
                else if(tws.next_operation == APP_TWS_SUSPEND_STREAM)
                {
                    uint8_t status;
                    TRACE("next is suspend stream");
                    log_event_2(EVENT_SUSPEND_A2DP_STREAMING,   btif_a2dp_get_stream_type(tws.tws_source.stream), 
                        3&  btif_a2dp_get_stream_device_cmgrhandler_remDev_hciHandle(tws.tws_source.stream));
                    status = btif_a2dp_suspend_stream(tws.tws_source.stream);
                    APP_TWS_SET_NEXT_OP(APP_TWS_IDLE_OP);
                    if(status == BT_STS_PENDING)
                    {
                        APP_TWS_SET_CURRENT_OP(APP_TWS_SUSPEND_STREAM);
                    }
                    else
                    {
                        TRACE("%s, suspend  stream error state = %d",__FUNCTION__,status);
                    }
                }
                else
                {
                    APP_TWS_SET_CURRENT_OP(APP_TWS_IDLE_OP);
                    TRACE("ELSE curr op=%d,next op=%d",tws.current_operation,tws.next_operation);
                }
            }
            else
            {
                TRACE("^^^^^ERROR!!!START OP MISMATCH!!!");
            }
            
            break;
        case BTIF_A2DP_EVENT_STREAM_SUSPENDED:
            TRACE("%s,A2DP_EVENT_STREAM_SUSPENDED,state=%d,error=%d",__FUNCTION__,Info->status,Info->error);
#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)  
            TWS_SET_STREAM_STATE(TWS_STREAM_SUSPEND) 
#endif
            TRACE("A2DP_EVENT_STREAM_SUSPENDED curr op=%d,next op=%d",tws.current_operation,tws.next_operation);
            if(Info->status == BT_STS_TIMEOUT){
                btif_a2dp_set_stream_state(tws.tws_source.stream,BTIF_AVDTP_STRM_STATE_OPEN);
            }
            a2dp_source_reconfig_sink_codec_proc(Stream);

 //           tws.suspend_media_pending = false;
            if(tws.current_operation == APP_TWS_SUSPEND_STREAM)
            {
                if(tws.next_operation == APP_TWS_START_STREAM)
                {
                    uint8_t status;
                    TRACE("current operation is start stream");                    
                    log_event_2(EVENT_START_A2DP_STREAMING,   btif_a2dp_get_stream_type(tws.tws_source.stream), 3&  btif_a2dp_get_stream_device_cmgrhandler_remDev_hciHandle(tws.tws_source.stream));
                    status = btif_a2dp_start_stream(tws.tws_source.stream);
                    if(status == BT_STS_PENDING)
                    {
                        APP_TWS_SET_CURRENT_OP(APP_TWS_START_STREAM);
                    }
                    else
                    {
                        //if start stream error, set current op to idle
                        APP_TWS_SET_CURRENT_OP(APP_TWS_IDLE_OP);
                        TRACE("%s, start stream error state = %d",__FUNCTION__,status);
                    }
                    APP_TWS_SET_NEXT_OP(APP_TWS_IDLE_OP);
                }
                else if(tws.next_operation == APP_TWS_SUSPEND_STREAM)
                {
                    APP_TWS_SET_NEXT_OP(APP_TWS_IDLE_OP);
                    APP_TWS_SET_CURRENT_OP(APP_TWS_IDLE_OP);
                }
                else
                {
                    APP_TWS_SET_CURRENT_OP(APP_TWS_IDLE_OP);
                    TRACE("ELSE curr op=%d,next op=%d",tws.current_operation,tws.next_operation);
                }
            }
            else
            {
                TRACE("^^^^^ERROR!!!SUSUPEND OP MISMATCH!!!");
            }
//do not disconnect avrcp any more
#if 0
            if ((CONN_OP_STATE_MACHINE_ROLE_SWITCH == CURRENT_CONN_OP_STATE_MACHINE()) &&
                (TO_DISCONNECT_BEFORE_ROLE_SWITCH == currentSimulateReconnectionState))
            {
                app_tws_further_handling_for_role_switch();
            }
#endif
 #if 0
            if(tws.start_media_pending == true)
            {
                tws.start_media_pending = false;
                if(tws.tws_source.stream->stream.state == AVDTP_STRM_STATE_OPEN)
                {
                    bt_status_t state;
                    state = A2DP_StartStream(tws.tws_source.stream);
                    TWSCON_DBLOG("send start to slave state = 0x%x",state);
                }            
                else
                {
                    TRACE("START SLAVE STREAM ERROR!!! CHECK THE STATE MECHINE");
                }
            }
#endif            
            
#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)  
            tws.media_suspend = true;
            if(tws.tws_source.stream_need_reconfig){
                rb_play_reconfig_stream(tws.tws_source.stream_need_reconfig);
                tws.tws_source.stream_need_reconfig = 0;
            }
#endif

            break;
        case BTIF_A2DP_EVENT_STREAM_RECONFIG_IND:
            btif_a2dp_reconfig_stream_rsp(Stream,BTIF_A2DP_ERR_NO_ERROR,0);
            break;
        case BTIF_A2DP_EVENT_STREAM_RECONFIG_CNF:
            TWSCON_DBLOG("%s %d A2DP_EVENT_STREAM_RECONFIG_CNF,error=%d,status=%d,discReason:0x%02x,stream=%x\n",
                                               __FUNCTION__,__LINE__,Info->error,Info->status,Info->discReason,Stream);
            if (app_bt_device.a2dp_streamming[0] == 1) {
                uint8_t status;
                tws.tws_start_stream_pending = 1;                 
                log_event_2(EVENT_START_A2DP_STREAMING,  btif_a2dp_get_stream_type(tws.tws_source.stream), 3&btif_a2dp_get_stream_device_cmgrhandler_remDev_hciHandle(tws.tws_source.stream));
                status = btif_a2dp_start_stream(tws.tws_source.stream);
                if(status == BT_STS_PENDING)
                {
                    APP_TWS_SET_CURRENT_OP(APP_TWS_START_STREAM);
                }
                else
                {
                    TRACE("%s, start stream error state = %d",__FUNCTION__,status);
                }
            }
#if defined(A2DP_LHDC_ON)
            if(Info->p.configReq->codec.codecType == BTIF_AVDTP_CODEC_TYPE_LHDC){

            }else
#endif            
#if defined(A2DP_AAC_ON)
            if(Info->p.configReq->codec.codecType == BTIF_AVDTP_CODEC_TYPE_MPEG2_4_AAC){
                
            }else
#endif
            {
#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)            
                if(tws.local_player_wait_reconfig){
                    osSemaphoreRelease(tws.wait_stream_reconfig._osSemaphoreId);
                    tws.local_player_wait_reconfig = 0;
                }
#endif                
                set_a2dp_reconfig_codec(Stream);
            }
            break;
        case BTIF_A2DP_EVENT_STREAM_IDLE:
            TWSCON_DBLOG("%s %d A2DP_EVENT_STREAM_IDLE,error=%d,status=%d,discReason:0x%02x,stream=%x\n",
                                               __FUNCTION__,__LINE__,Info->error,Info->status,Info->discReason,Stream);
            twsd->stream_idle = 1;
            if(twsd->stream_reset == 1 && btif_a2dp_get_stream_device(Stream) && btif_a2dp_get_stream_devic_cmgrHandler_remdev(Stream))
            {
                uint32_t status;
                setconfig_codec.elements[0] &=0xcf;
                if (twsd->audout.sample_rate == 48000){
                    TWSCON_DBLOG("%s %d A2DP_EVENT_GET_CONFIG_IND AUD_SAMPRATE_48000\n",__FUNCTION__,__LINE__);
                    setconfig_codec.elements[0] |=0x10;  
                }else{
                    TWSCON_DBLOG("%s %d A2DP_EVENT_GET_CONFIG_IND AUD_SAMPRATE_44100\n",__FUNCTION__,__LINE__);
                    setconfig_codec.elements[0] |=0x20;  
                }
                status = btif_a2dp_set_stream_config(Stream, &setconfig_codec, NULL);
                TRACE("A2DP_EVENT_STREAM_IDLE REOPEN STREAM STATUS=%x",status);
                twsd->stream_reset = 0;
            }            
            break;
    }
}

void a2dp_sink_of_tws_opened_ind_handler(a2dp_stream_t *Stream, const a2dp_callback_parms_t *info)
{
    btif_a2dp_callback_parms_t *Info = ( btif_a2dp_callback_parms_t *)info;
    TRACE("A2dp with TWS master is opened");        
    tws_dev_t *twsd = &tws;
    twsd->tws_sink.connected = true;
#ifdef __TWS_PAIR_DIRECTLY__
    //update as tws slave
    tws_cfg.mode=TWSSLAVE;
    twsBoxEnv.twsModeToStart = TWSSLAVE;
    memcpy(twsBoxEnv.twsBtBdAddr[0], btif_a2dp_get_stream_devic_cmgrHandler_remdev_bdAddr(Stream), BTIF_BD_ADDR_SIZE);
    memcpy(twsBoxEnv.twsBtBdAddr[1], bt_addr, BD_ADDR_LEN);
    
   // bt_drv_reg_op_reset_sniffer_env();
    slaveDisconMobileFlag=false;

    //nv_record_update_tws_mode(TWSSLAVE);
    //memcpy(tws_mode_ptr()->masterAddr.address, twsBoxEnv.twsBtBdAddr[0], BTIF_BD_ADDR_SIZE);
    //memcpy(tws_mode_ptr()->slaveAddr.address, twsBoxEnv.twsBtBdAddr[1], BTIF_BD_ADDR_SIZE);
#ifdef CHIP_BEST2000
    if (app_tws_is_slave_mode()) 
    {
        bt_drv_reg_op_afh_set_default();
    }
#endif
    if (!isMobileDevPaired)
    {
        btif_me_set_sniffer_env(1, SNIFFER_ROLE, tws_mode_ptr()->slaveAddr.address, tws_mode_ptr()->slaveAddr.address);
    }
    else
    {
       btif_me_set_sniffer_env(1, SNIFFER_ROLE,  mobileDevBdAddr.address, tws_mode_ptr()->slaveAddr.address);
    }
    bt_drv_reg_op_set_swagc_mode(2);
#endif

    app_tws_set_conn_state(TWS_SLAVE_CONN_MASTER);
    TRACE("Slave conhdl=%d\n",   btif_a2dp_get_stream_device_cmgrhandler_remDev_hciHandle(Stream));
    app_tws_set_tws_conhdl(btif_a2dp_get_stream_device_cmgrhandler_remDev_hciHandle(Stream));
    btdrv_rf_bit_offset_track_enable(true);
    btdrv_rf_set_conidx(btdrv_conhdl_to_linkid(btif_a2dp_get_stream_device_cmgrhandler_remDev_hciHandle(Stream)));
#ifdef __TWS_OUTPUT_POWER_FIX_SEPARATE__            
    bt_drv_reg_op_tws_output_power_fix_separate(Stream->device->cmgrHandler.remDev->hciHandle, 7);
#endif            
    btdrv_rf_trig_patch_enable(0);
    btdrv_set_tws_role_triggler(TWS_SLAVE_CONN_MASTER);
    
    memcpy(tws.peer_addr.address,   btif_a2dp_get_stream_devic_cmgrHandler_remdev_bdAddr(Stream),6);  

#if (defined(SLAVE_USE_OPUS) || defined(MASTER_USE_OPUS) || defined( SLAVE_USE_ENC_SINGLE_SBC)) && defined(__TWS_OUTPUT_CHANNEL_SELECT__)
    app_tws_split_channel_select(bt_addr,tws.peer_addr.address);
#endif                      
    twsd->unlock_mode(twsd);
    if(twsd->stream_idle ==1)
    {
        twsd->stream_idle = 0;
    }
    else
    {
        app_tws_Indicate_connection();
    }


    if (app_tws_is_to_update_sample_rate_after_role_switch())
    {
        a2dp_restore_sample_rate_after_role_switch(Stream, Info); 
    
        app_tws_clear_flag_of_update_sample_rate_after_role_switch();
    }

    set_a2dp_reconfig_codec(Stream);   

    // update state
    uint32_t formerTwsConnState = twsConnectionState;
    SET_CONN_STATE_A2DP(twsConnectionState);
    log_event_2(EVENT_A2DP_CONNECTED, BT_FUNCTION_ROLE_MASTER, 3&  btif_me_get_remote_device_hci_handle(masterBtRemDev));
    if (formerTwsConnState != twsConnectionState)
    {
        tws_connection_state_updated_handler(BTIF_BEC_NO_ERROR);
    }
    TRACE("A2DP with TWS master is opened, BD address is:");
    DUMP8("0x%02x ",   btif_a2dp_get_stream_devic_cmgrHandler_remdev_bdAddr(Stream), BTIF_BD_ADDR_SIZE);
}

void a2dp_sink_of_tws_closed_ind_common_handler(a2dp_stream_t *Stream, const a2dp_callback_parms_t *info)
{
    btif_a2dp_callback_parms_t *Info = ( btif_a2dp_callback_parms_t *)info;
    app_bt_hcireceived_data_clear(tws.tws_conhdl);
    btif_HciBypassProcessReceivedDataExt(NULL);
    app_tws_set_conn_state(TWS_NON_CONNECTED);
    
    tws_dev_t *twsd = &tws;
    twsd->stream_idle = 0;    
    twsd->tws_sink.connected = false;

    APP_TWS_SET_CURRENT_OP(APP_TWS_IDLE_OP);
    APP_TWS_SET_NEXT_OP(APP_TWS_IDLE_OP); 
    
    tws_player_stop(BT_STREAM_SBC);
    restore_tws_channels(); 

    // update state
    CLEAR_CONN_STATE_A2DP(twsConnectionState);

#ifdef __TWS_PAIR_DIRECTLY__
     tws_cfg.mode = TWSFREE;
#endif

    log_event_3(EVENT_A2DP_DISCONNECTED, BT_FUNCTION_ROLE_MASTER, 3& btif_me_get_remote_device_hci_handle(masterBtRemDev),
        Info->error);
}

void a2dp_sink_of_tws_closed_ind_handler(a2dp_stream_t *Stream, const a2dp_callback_parms_t *info)
{
    btif_a2dp_callback_parms_t *Info = ( btif_a2dp_callback_parms_t *)info;
    TRACE("A2DP with TWS master is closed. Reason is 0x%x", Info->discReason);

    a2dp_sink_of_tws_closed_ind_common_handler(Stream, Info);
    if (!is_simulating_reconnecting_in_progress())
    {
        app_bt_set_linkpolicy(masterBtRemDev, BTIF_BLP_MASTER_SLAVE_SWITCH|BTIF_BLP_SNIFF_MODE);
        app_tws_role_switch_cleanup_a2dp(btif_me_get_remote_device_hci_handle(masterBtRemDev));
    }
    tws_connection_state_updated_handler(Info->discReason);
}


#ifdef __TWS_PAIR_DIRECTLY__
bool is_tws_peer_device(void *tws, a2dp_stream_t *Stream, const a2dp_callback_parms_t *info)
{
    bool is_tws_peer = FALSE;
    tws_dev_t *twsd = (tws_dev_t *)tws;
    btif_a2dp_callback_parms_t *Info = ( btif_a2dp_callback_parms_t *)info;

    if(tws_cfg.mode == TWSSLAVE){
        return true;
    }

    if(Info->event == BTIF_A2DP_EVENT_AVDTP_CONNECT)
    {
        //DUMP8("%02x ", twsd->peer_addr.addr,6);
        //DUMP8("%02x ", Info->p.device->cmgrHandler.remDev->bdAddr.addr,6);    
        if (!memcmp((const char *)&(twsd->peer_addr), (const char *)(btif_a2dp_av_device_get_remote_bd_addr(Info->p.device)),6)){
            is_tws_peer = true;
        }        
    }
    else
    {
        //get remote device addr 
        btif_remote_device_t * addr = btif_a2dp_get_remote_device(Stream);
        //DUMP8("%02x ", twsd->peer_addr.addr,6);
        //DUMP8("%02x ", addr->bdAddr.addr,6);
        if (!memcmp((const char *)&(twsd->peer_addr), (const char *)(btif_me_get_remote_device_bdaddr(addr)),6)){
            is_tws_peer = true;
        }
    }
    return is_tws_peer;
}
#endif
extern bool a2dp_sink_of_mobile_dev_callback(a2dp_stream_t *Stream, const a2dp_callback_parms_t *Info);
extern void a2dp_sink_stream_data_checkhandler(a2dp_stream_t *Stream, const a2dp_callback_parms_t *Info);
void a2dp_sink_callback(a2dp_stream_t *Stream, const a2dp_callback_parms_t *Info)
{
    //TRACE("Enter %s event is %d", __FUNCTION__, Info->event);
    bool isAllHandlingDone = false;
    a2dp_sink_stream_data_checkhandler(Stream, Info);
#ifdef __TWS_PAIR_DIRECTLY__
    if (((uint32_t)Stream == (uint32_t)(app_tws_get_env_pointer()->tws_sink.stream))
        &&(is_tws_peer_device(app_tws_get_env_pointer(),Stream,Info)))
#else
    if ((app_tws_is_slave_mode()) && \
        ((uint32_t)Stream == (uint32_t)(app_tws_get_env_pointer()->tws_sink.stream)))

#endif
    {
        isAllHandlingDone = a2dp_sink_of_tws_callback(Stream, Info);
    }
    else
    {
        isAllHandlingDone = a2dp_sink_of_mobile_dev_callback(Stream, Info);
    }

    if (!isAllHandlingDone)
    {
        a2dp_sink_callback_common_handler(Stream, Info);
    }
}

#if defined(__NOT_STOP_AUDIO_IN_UNDERFLOW__) || defined(__TWS_SYNC_WITH_DMA_COUNTER__)
extern uint8_t g_trigger_flag;
#endif
bool a2dp_sink_of_tws_callback(a2dp_stream_t *Stream, const a2dp_callback_parms_t *info)
{
    btif_a2dp_callback_parms_t *Info = ( btif_a2dp_callback_parms_t *)info;
    //TRACE("Enter %s event is %d", __FUNCTION__, Info->event);
    
    tws_dev_t *twsd = &tws;

    switch(Info->event)
    {
        case BTIF_A2DP_EVENT_STREAM_OPEN:
            a2dp_sink_of_tws_opened_ind_handler(Stream, Info);           
            break;
        case BTIF_A2DP_EVENT_STREAM_CLOSED:
            a2dp_sink_of_tws_closed_ind_handler(Stream, Info);                              
            break;
        case BTIF_A2DP_EVENT_AVDTP_CONNECT:
            btdrv_rf_bit_offset_track_force_disable();
            break;
        case BTIF_A2DP_EVENT_STREAM_START_IND:
            app_bt_set_linkpolicy(masterBtRemDev, BTIF_BLP_DISABLE_ALL);
            TRACE("%s,A2DP_EVENT_STREAM_START_IND,state=%d,error=%d",__FUNCTION__,Info->status,Info->error);
#if defined(__NOT_STOP_AUDIO_IN_UNDERFLOW__) || defined(__TWS_SYNC_WITH_DMA_COUNTER__)
            g_trigger_flag = 1;
#endif
            app_tws_reset_tws_data();
            break;
        case BTIF_A2DP_EVENT_STREAM_SUSPENDED:
            tws.lock_prop(&tws);
            tws.media_suspend = true; 
            tws.unlock_prop(&tws);
            log_event_2(EVENT_A2DP_STREAMING_SUSPENDED,  btif_a2dp_get_stream_type(Stream),
                3&   btif_a2dp_get_stream_device_cmgrhandler_remDev_hciHandle(Stream));
            if  (app_audio_manager_get_current_media_type() != BT_STREAM_VOICE ){
                app_bt_set_linkpolicy(masterBtRemDev, BTIF_BLP_MASTER_SLAVE_SWITCH|BTIF_BLP_SNIFF_MODE);
            }
            TRACE("%s,A2DP_EVENT_STREAM_SUSPENDED,state=%d,error=%d",__FUNCTION__,Info->status,Info->error);
            tws_player_stop(BT_STREAM_SBC);
            break;
         case BTIF_A2DP_EVENT_STREAM_DATA_IND:
            if (!app_bt_stream_isrun(APP_BT_STREAM_HFP_PCM)&&
#ifdef MEDIA_PLAYER_SUPPORT
                !app_bt_stream_isrun(APP_PLAY_BACK_AUDIO)&&
#endif
                !app_audio_manager_bt_stream_voice_request_exist() &&
                (app_audio_manager_get_current_media_type() !=BT_STREAM_VOICE ) &&
                (app_audio_manager_get_current_media_type() !=BT_STREAM_MEDIA ) &&
            #if FPGA==0
                 app_poweroff_flag == 0 && 
            #endif
                 (tws.bleReconnectPending == 0)
                 )
            {            
                if(app_tws_media_frame_analysis(twsd,Stream,Info))
                {
                    return true;
                }
            }
            else
            {
                return true;
            }
            break;
        case BTIF_A2DP_EVENT_STREAM_IDLE:
            TRACE("%s,A2DP_EVENT_STREAM_IDLE,state=%d,error=%d",__FUNCTION__,Info->status,Info->error);
            twsd->stream_idle = 1;
            break;
         default:
            break;
    }

    return false;
}

void tws_set_mobile_sink_stream(a2dp_stream_t *stream)
{
    tws.mobile_sink.stream = stream;
}

int app_tws_fill_addr_to_array(const bt_bdaddr_t*addr)
{
    uint8_t i;
    for(i=0;i<sizeof(tws_inq_addr)/sizeof(tws_inq_addr[0]);i++)
    {
        if(tws_inq_addr[i].used == 0)
        {
            tws_inq_addr[i].used =1;
            memcpy(tws_inq_addr[i].bdaddr.address,addr->address,BTIF_BD_ADDR_SIZE);
            return 0;
        }
    }

    return -1;
}


uint8_t app_tws_get_tws_addr_inq_num(void)
{
    uint8_t i,count=0;
    for(i=0;i<sizeof(tws_inq_addr)/sizeof(tws_inq_addr[0]);i++)
    {
        if(tws_inq_addr[i].used == 1)
        {
            count++;
        }
    }    
    return count;
}



bool app_tws_is_addr_in_tws_inq_array(const bt_bdaddr_t* addr)
{
    uint8_t i;
    for(i=0;i<sizeof(tws_inq_addr)/sizeof(tws_inq_addr[0]);i++)
    {
        if(tws_inq_addr[i].used == 1)
        {
            if(!memcmp(tws_inq_addr[i].bdaddr.address,addr->address,BTIF_BD_ADDR_SIZE))
                return true;
        }
    }    
    return false;
}

void app_tws_clear_tws_inq_array(void)
{
    memset(&tws_inq_addr,0,sizeof(tws_inq_addr));
}


static void app_tws_inquiry_timeout_handler(void const *param)
{

    TWSCON_DBLOG("app_tws_inquiry_timeout_handler\n");
    btif_me_inquiry(0x9e8b33, 2, 0);

}




void tws_app_stop_find(void)
{
    tws_find_process=0;
    btif_me_unregister_globa_handler(btif_me_get_me_handler() );

}

void app_bt_call_back(const btif_event_t* event)
{
    //   TWS_DBLOG("\nenter: %s %d\n",__FUNCTION__,__LINE__);
    uint8_t device_name[64];
    uint8_t device_name_len;
    switch(btif_me_get_callback_event_type(event)){
        case BTIF_BTEVENT_NAME_RESULT:
            TWSCON_DBLOG("\n%s %d BTEVENT_NAME_RESULT\n",__FUNCTION__,__LINE__);
            break;
        case BTIF_BTEVENT_INQUIRY_RESULT: 
            TWSCON_DBLOG("\n%s %d BTEVENT_INQUIRY_RESULT\n",__FUNCTION__,__LINE__);
            DUMP8("%02x ", btif_me_get_callback_event_inq_result_bd_addr_addr(event), 6);
            TWSCON_DBLOG("inqmode = %x",btif_me_get_callback_event_inq_result_inq_mode(event));
            DUMP8("%02x ", btif_me_get_callback_event_inq_result_ext_inq_resp(event), 20);
            ///check the uap and nap if equal ,get the name for tws slave
            //TRACE("##RSSI:%d",(int8_t)btif_me_get_callback_event_rssi(event));

            if((btif_me_get_callback_event_inq_result_bd_addr(event)->address[5]== bt_addr[5]) && (btif_me_get_callback_event_inq_result_bd_addr(event)->address[4]== bt_addr[4]) &&
                    (btif_me_get_callback_event_inq_result_bd_addr(event)->address[3]== bt_addr[3]))
            {
                ///check the device is already checked
                if(app_tws_is_addr_in_tws_inq_array(btif_me_get_callback_event_inq_result_bd_addr(event)))
                {
                    break;
                }            
                ////if rssi event is eir,so find name derictly
                if(btif_me_get_callback_event_inq_result_inq_mode(event) == BTIF_INQ_MODE_EXTENDED)
                {
                    uint8_t *eir = (uint8_t *)btif_me_get_callback_event_inq_result_ext_inq_resp(event);
                    //device_name_len = ME_GetExtInqData(eir,0x09,device_name,sizeof(device_name));
                    device_name_len = btif_me_get_ext_inq_data(eir,0x09,device_name,sizeof(device_name));
                    if(device_name_len>0)
                    {
                        ////if name is the same as the local name so we think the device is the tws slave
                        if(!memcmp(device_name,BT_LOCAL_NAME,device_name_len))
                        {
                            bt_status_t status;
                            btif_device_record_t  record;
                            tws_dev_t *twsd = &tws;
                            a2dp_stream_t *src = twsd->tws_source.stream;
                            btif_me_cancel_inquiry();// ME_CancelInquiry(); 
                            osTimerStop(app_tws_timer);
                            tws.create_conn_pending = true;
                            memcpy(tws.connecting_addr.address,btif_me_get_callback_event_inq_result_bd_addr_addr(event),6);
                            status =  btif_sec_find_device_record((bt_bdaddr_t*)&(tws.connecting_addr), &record);
                            
                            if (status == BT_STS_SUCCESS){
                                //SEC_DeleteDeviceRecord(&(tws.connecting_addr));
                                btif_sec_delete_device_record((bt_bdaddr_t*)&(tws.connecting_addr));                
                            }
                            log_event_1(EVENT_START_CONNECTING_A2DP, BT_FUNCTION_ROLE_SLAVE);
#ifdef __TWS_PAIR_DIRECTLY__
                            app_tws_master_enter_pairing((uint8_t *)btif_me_get_callback_event_inq_result_bd_addr(event));
#else
                            btif_a2dp_open_stream(src,btif_me_get_callback_event_inq_result_bd_addr(event));
#endif

//                            tws.notify(&tws);
                        }

                        else
                        {
                            if(app_tws_get_tws_addr_inq_num()<sizeof(tws_inq_addr)/sizeof(tws_inq_addr[0]))
                            {
                                app_tws_fill_addr_to_array(btif_me_get_callback_event_inq_result_bd_addr(event));
                                if(app_tws_get_tws_addr_inq_num()==sizeof(tws_inq_addr)/sizeof(tws_inq_addr[0]))
                                {
                                    ///fail to find a tws slave
                                    btif_me_cancel_inquiry(); 
                                    tws_app_stop_find();
                                }
                            }
                            else
                            {
                                ///fail to find a tws slave
                                btif_me_cancel_inquiry();     
                                tws_app_stop_find();
                            }
                        }
                        break;
                    }
                    /////have no name so just wait for next device
                    //////we can do remote name req for tws slave if eir can't received correctly

                }
            }
            break;
        case BTIF_BTEVENT_INQUIRY_COMPLETE: 
            TWSCON_DBLOG("\n%s %d BTEVENT_INQUIRY_COMPLETE\n",__FUNCTION__,__LINE__);
           if(tws_inquiry_count>=MAX_TWS_INQUIRY_TIMES)
            {
                tws_app_stop_find();
                return;
            }                

            if(tws.create_conn_pending ==false)
            {
                ////inquiry complete if bt don't find any slave ,so do inquiry again

                uint8_t rand_delay = rand() % 5;
                 tws_inquiry_count++;
                
                if(rand_delay == 0)
                {
                    btif_me_inquiry(0x9e8b33, 2, 0);
                }
                else
                {
                    osTimerStart(app_tws_timer, rand_delay*1000);
                }
            }

            break;
            /** The Inquiry process is canceled. */
        case BTIF_BTEVENT_INQUIRY_CANCELED:
            TWSCON_DBLOG("\n%s %d BTEVENT_INQUIRY_CANCELED\n",__FUNCTION__,__LINE__);
            // tws.notify(&tws);
            break;
        case BTIF_BTEVENT_LINK_CONNECT_CNF:
            TWSCON_DBLOG("\n%s %d BTEVENT_LINK_CONNECT_CNF stats=%x\n",__FUNCTION__,__LINE__,btif_me_get_callback_event_err_code(event));
            tws.create_conn_pending = false;
            //connect fail start inquiry again
            if(btif_me_get_callback_event_err_code(event) ==4 && tws_find_process == 1)
            {
               if(tws_inquiry_count>=MAX_TWS_INQUIRY_TIMES)
                {
                    tws_app_stop_find();
                    return;
                }             
                uint8_t rand_delay = rand() % 5;
                 tws_inquiry_count++;
                if(rand_delay == 0)
                {

                   
                    btif_me_inquiry(0x9e8b33, 2, 0);
                    
                }
                else
                {
                    osTimerStart(app_tws_timer, rand_delay*1000);
                }            
            }
            ///connect succ,so stop the finding tws procedure
            else if(btif_me_get_callback_event_err_code(event) ==0)
            {
                tws_app_stop_find();
            }
            break;
        case BTIF_BTEVENT_LINK_CONNECT_IND:
            TWSCON_DBLOG("\n%s %d BTEVENT_LINK_CONNECT_IND stats=%x\n",__FUNCTION__,__LINE__,btif_me_get_callback_event_err_code(event));
            ////there is a incoming connect so cancel the inquiry and the timer and the  connect creating
            btif_me_cancel_inquiry();
            osTimerStop(app_tws_timer);
            if(tws.create_conn_pending == true)
            {
                tws_dev_t *twsd = &tws;
                a2dp_stream_t *src = twsd->tws_source.stream;
                tws.create_conn_pending = false;
                if( btif_a2dp_get_stream_device(src) && btif_a2dp_get_stream_devic_cmgrHandler_remdev(src))
                {
                    btif_me_cancel_create_link( (btif_handler*) btif_a2dp_get_stream_devic_cmgrHandler_bt_handler(src), btif_a2dp_get_stream_devic_cmgrHandler_remdev(src));
                }
            }
            break;
        default:
            //TWS_DBLOG("\n%s %d etype:%d\n",__FUNCTION__,__LINE__,event->eType);
            break;


    }

    //TWS_DBLOG("\nexit: %s %d\n",__FUNCTION__,__LINE__);

}



uint8_t is_find_tws_peer_device_onprocess(void)
{
    return tws_find_process;
}




void find_tws_peer_device_start(void)
{
    TWSCON_DBLOG("\nenter: %s %d\n",__FUNCTION__,__LINE__);
    bt_status_t  stat;
    if(tws_find_process ==0)
    {
        tws_find_process = 1;
        tws_inquiry_count = 0;
        if (app_tws_timer == NULL)
            app_tws_timer = osTimerCreate(osTimer(APP_TWS_INQ), osTimerOnce, NULL);
        app_tws_clear_tws_inq_array();


         btif_me_set_handler(btif_me_get_bt_handler(),app_bt_call_back);
        
       // me_handler.callback = btif_bt_call_back;
        
       // ME_RegisterGlobalHandler(btif_me_get_bt_handler());
    btif_me_register_global_handler(btif_me_get_bt_handler());
     
       // ME_SetEventMask(&me_handler, BEM_LINK_DISCONNECT|BEM_ROLE_CHANGE|BEM_INQUIRY_RESULT|
        //        BEM_INQUIRY_COMPLETE|BEM_INQUIRY_CANCELED|BEM_LINK_CONNECT_CNF|BEM_LINK_CONNECT_IND);

        btif_me_set_event_mask(btif_me_get_bt_handler(), BTIF_BEM_LINK_DISCONNECT|BTIF_BEM_ROLE_CHANGE|BTIF_BEM_INQUIRY_RESULT|
               BTIF_BEM_INQUIRY_COMPLETE|BTIF_BEM_INQUIRY_CANCELED|BTIF_BEM_LINK_CONNECT_CNF|BTIF_BEM_LINK_CONNECT_IND);
        
        
#ifdef __TWS_MASTER_STAY_MASTER__        
        HF_SetMasterRole(&app_bt_device.hf_channel[BT_DEVICE_ID_1], true);
#endif
    again:  
        TWSCON_DBLOG("\n%s %d\n",__FUNCTION__,__LINE__);
        stat = btif_me_inquiry(BTIF_BT_IAC_GIAC, 2, 0);
        //stat = btif_me_inquiry(BTIF_BT_IAC_LIAC, 2, 0);
        TWSCON_DBLOG("\n%s %d\n",__FUNCTION__,__LINE__);
        if (stat != BT_STS_PENDING){
            osDelay(500);
            goto again;
        }
        TWSCON_DBLOG("\n%s %d\n",__FUNCTION__,__LINE__);
    }
}

void find_tws_peer_device_stop(void)
{
    btif_me_cancel_inquiry();     
    tws_app_stop_find();
}

void app_tws_disconnect_slave(void)
{
    TRACE("Disconnect slave rem dev 0x%x", slaveBtRemDev);
    if (NULL != slaveBtRemDev)
    {
        app_bt_disconnect_link(slaveBtRemDev);
    }
}

void app_tws_disconnect_master(void)
{
    TRACE("Disconnect master rem dev 0x%x", masterBtRemDev);
    if (NULL != masterBtRemDev)
    {
        app_bt_disconnect_link(masterBtRemDev);    
    }
}

void a2dp_handleKey(uint8_t a2dp_key);

void app_tws_suspend_tws_stream(void)
{
    a2dp_stream_t *src;
    a2dp_stream_t* streamToSuspend = app_bt_get_mobile_stream(BT_DEVICE_ID_1);

    if (app_tws_is_master_mode())
    {
        if (app_tws_get_source_stream_connect_status()){
            src = app_tws_get_tws_source_stream();
            
            if( btif_a2dp_get_stream_devic_cmgrHandler_remdev(src))
            {
                if(tws.current_operation == APP_TWS_IDLE_OP)
                {
                    uint8_t status;
                    log_event_2(EVENT_SUSPEND_A2DP_STREAMING,  btif_a2dp_get_stream_type(src),
                        3& btif_a2dp_get_stream_device_cmgrhandler_remDev_hciHandle(src));
                    status = btif_a2dp_suspend_stream(src);
                    if(status == BT_STS_PENDING)
                    {
                        APP_TWS_SET_CURRENT_OP(APP_TWS_SUSPEND_STREAM);
                    }
                    else
                    {
                        TRACE("%s, suspend stream error state = %d",__FUNCTION__,status);
                    }
                    APP_TWS_SET_NEXT_OP(APP_TWS_IDLE_OP);
                }
                else
                {
                    APP_TWS_SET_NEXT_OP(APP_TWS_SUSPEND_STREAM);
                }   
            }
        }
    }
    else if (app_tws_is_slave_mode())
    {
    }
    else
    {
        if (( btif_a2dp_get_stream_state(streamToSuspend) == BTIF_AVDTP_STRM_STATE_STREAMING ||
            btif_a2dp_get_stream_state(streamToSuspend) == BTIF_AVDTP_STRM_STATE_OPEN) &&
            (btif_a2dp_get_stream_devic_cmgrHandler_remdev(streamToSuspend)))
        {
            log_event_2(EVENT_SUSPEND_A2DP_STREAMING, btif_a2dp_get_stream_type(streamToSuspend), 
                        3& btif_a2dp_get_stream_device_cmgrhandler_remDev_hciHandle(streamToSuspend));
            btif_a2dp_suspend_stream(streamToSuspend);
        }       
    }
}

bool app_tws_close_tws_stream(void)
{
    bool isExistingA2dpStreamToClose = false;
    
    a2dp_stream_t *src;
    a2dp_stream_t* streamToClose = app_tws_get_mobile_sink_stream();

    if (app_tws_is_master_mode())
    {
        if ((btif_a2dp_get_stream_state(streamToClose) == BTIF_AVDTP_STRM_STATE_STREAMING ||
             btif_a2dp_get_stream_state(streamToClose)  == BTIF_AVDTP_STRM_STATE_OPEN) &&
            (btif_a2dp_get_stream_devic_cmgrHandler_remdev(streamToClose)))
        {
            btif_a2dp_close_stream(streamToClose);
            log_event_2(EVENT_DISCONNECT_A2DP, BT_FUNCTION_ROLE_SLAVE,
                3&btif_me_get_remote_device_hci_handle(slaveBtRemDev));
            isExistingA2dpStreamToClose = true;
        }
        else if (app_tws_get_source_stream_connect_status()){
            src = app_tws_get_tws_source_stream();
            if(btif_a2dp_get_stream_devic_cmgrHandler_remdev(src))
            {
                log_event_2(EVENT_DISCONNECT_A2DP, BT_FUNCTION_ROLE_SLAVE,
                                3&btif_me_get_remote_device_hci_handle(slaveBtRemDev));
                btif_a2dp_close_stream(src);
                isExistingA2dpStreamToClose = true;
            }
        }
    }
    else if (app_tws_is_slave_mode())
    {
         if (app_tws_get_sink_stream_connect_status()){
            src = app_tws_get_tws_sink_stream();
            if(btif_a2dp_get_stream_devic_cmgrHandler_remdev(src))
            {
                log_event_2(EVENT_DISCONNECT_A2DP, BT_FUNCTION_ROLE_MASTER,
                                3&  btif_me_get_remote_device_hci_handle(masterBtRemDev));
                btif_a2dp_close_stream(src);
                isExistingA2dpStreamToClose = true;
            }
        }       
    }
    else
    {
        if (( btif_a2dp_get_stream_state(streamToClose) == BTIF_AVDTP_STRM_STATE_STREAMING ||
           btif_a2dp_get_stream_state(streamToClose)  == BTIF_AVDTP_STRM_STATE_OPEN) &&
            btif_a2dp_get_stream_devic_cmgrHandler_remdev(streamToClose))
        {
            log_event_2(EVENT_DISCONNECT_A2DP, BT_FUNCTION_ROLE_MOBILE,
                                3&btif_me_get_remote_device_hci_handle(mobileBtRemDev));
            btif_a2dp_close_stream(streamToClose);
            isExistingA2dpStreamToClose = true;
        }       
    }

    return isExistingA2dpStreamToClose;
}

#ifdef __TWS_RECONNECT_USE_BLE__
//static CmgrHandler app_tws_cmgrHandler;

static void app_tws_CmgrCallback(void *cHandler, 
                              uint8_t    Event, 
                              bt_status_t     Status)
{
    TRACE("%s cHandler:%x Event:%d status:%d", __func__, cHandler, Event, Status);

}

void app_tws_create_acl_to_slave(bt_bdaddr_t *bdAddr)
{
    bt_status_t status;

    //tws_dev_t *twsd = &tws; 

    memcpy(tws.connecting_addr.addr,bdAddr->addr,6);
    btif_create_acl_to_slave(btif_me_get_cmgr_handler(), bdAddr,  app_tws_CmgrCallback)
    
//     tws.notify(&tws);

    TWSCON_DBLOG("\nexit: %s %d %d\n",__FUNCTION__,__LINE__, status);   
}
#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)
uint16_t app_tws_get_master_stream_state(void)
{
    return tws.mobile_sink.stream_state;
}
#endif
#endif

#endif

