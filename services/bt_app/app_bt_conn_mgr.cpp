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
#include "stdlib.h"
#include "string.h"
#include "cmsis_os.h"
#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_chipid.h"
#include "app_status_ind.h"
#include "app_bt_stream.h"
#include "nvrecord.h"
#include "nvrecord_env.h"

#include "mei_api.h"
#include "a2dp_api.h"

#include "bt_drv_reg_op.h"

#include "rtos.h"
#include "besbt.h"

#include "cqueue.h"
#include "btapp.h"
#include "app_bt.h"
#include "app_hfp.h"

#include "apps.h"
#include "resources.h"

#include "app_bt_media_manager.h"
#ifdef __TWS__
#include "app_tws.h"
#include "app_tws_if.h"
#include "tws_api.h"
#endif

#include "app_utils.h"
#include "app_bt_conn_mgr.h"
#include "app_tws_role_switch.h"
#include "ble_tws.h"
#include "bt_drv_interface.h"
#include "log_section.h"
#ifdef BES_OTA_TWS
#include "ota_control.h"
#endif

CONN_OP_STATE_MACHINE_TYPE_E    conn_op_state_machine_type = CONN_OP_STATE_MACHINE_NORMAL;
CONN_OP_STATE_E                 conn_op_state = CONN_OP_STATE_IDLE;

uint32_t        twsConnectionState = 0;
uint32_t        mobileDevConnectionState = 0;
uint32_t        mobileDevActiveProfiles= 0;

bt_bdaddr_t mobileDevBdAddr;
bool            isMobileDevPaired = false;

uint8_t         isInMobileBtSilentState = false; 

uint8_t connection_creation_supervisor_retry_cnt = 0;

osTimerId   tws_connection_creation_supervisor_timer_id; 
bool        is_tws_connection_creation_supervisor_timer_on = false;
static void tws_connection_creation_supervisor_timer_cb(void const *n);
osTimerDef (APP_TWS_CONNECTION_SUPVERVISOR_TIMER, tws_connection_creation_supervisor_timer_cb); 

static TWS_CONNECTION_CREATION_SUPERVISOR_TIMER_HANDLER custom_tws_connection_creation_supvervisor_timer_cb;

osTimerId   mobile_connection_creation_supervisor_timer_id; 
uint8_t     passedSupervisingMinutes = 0;
uint8_t     supervisingMinutes = 0;
bool        is_mobile_connection_creation_supervisor_timer_on = false;
static void mobile_connection_creation_supervisor_timer_cb(void const *n);
osTimerDef (APP_MOBILE_CONNECTION_SUPVERVISOR_TIMER, mobile_connection_creation_supervisor_timer_cb); 

static MOBILE_CONNECTION_CREATION_SUPERVISOR_TIMER_HANDLER custom_mobile_connection_creation_supervisor_timer_cb;

osTimerId   tws_connecting_ble_time_out_timer_id;
bool        is_tws_connecting_ble_time_out_timer_on = false;
static void connecting_ble_time_out_cb(void const *n);
osTimerDef (APP_TWS_CONNECTING_BLE_TIMEOUT_TIMER, connecting_ble_time_out_cb);

osTimerId   tws_sharing_tws_info_time_out_timer_id;
static void tws_sharing_tws_info_time_out_cb(void const *n);
osTimerDef (APP_TWS_SHARING_TWS_INFO_TIMEOUT_TIMER, tws_sharing_tws_info_time_out_cb);

osTimerId   tws_sharing_pairing_info_time_out_timer_id;
static void tws_sharing_pairing_info_time_out_cb(void const *n);
osTimerDef (APP_TWS_SHARING_PAIRING_INFO_TIMEOUT_TIMER, tws_sharing_pairing_info_time_out_cb);

osTimerId   role_switch_supervisor_timer_id; 
static void role_switch_supervisor_timer_cb(void const *n);
osTimerDef (APP_TWS_ROLE_SWITCH_SUPVERVISOR_TIMER, role_switch_supervisor_timer_cb); 

osTimerId   connecting_tws_delay_timer_id; 
static void connecting_tws_delay_timer_cb(void const *n);
osTimerDef (APP_TWS_CONNECTING_TWS_DELAY_TIMER, connecting_tws_delay_timer_cb);

osTimerId   connecting_mobile_delay_timer_id; 
static void connecting_mobile_delay_timer_cb(void const *n);
osTimerDef (APP_TWS_CONNECTING_MOBILE_DELAY_TIMER, connecting_mobile_delay_timer_cb);

osTimerId   connecting_mobile_supervisor_timer_id; 
static void connecting_mobile_supervisor_timer_cb(void const *n);
osTimerDef (APP_CONNECTING_MOBILE_SUPERVISOR_TIMER, connecting_mobile_supervisor_timer_cb);


// connecting mobile and keep in the idle mode by interleave, to assure
// that when "device connecting mobile" and "mobile connecting device" happen in the
// meantime
osTimerId   connecting_mobile_idle_timer_id; 
static void connecting_mobile_idle_timer_cb(void const *n);
osTimerDef (APP_TWS_CONNECTING_MOBILE_IDLE_TIMER, connecting_mobile_idle_timer_cb);
bool isConnectingMobileIdleTimerOn = false;

//OPEN/CLOSE BOX
static BOX_OPERATION_STATUS_CHECK_HANDLER box_operation_status_check_timer_cb;
static BOX_OPERATION_STATUS_CHECK_HANDLER dev_operation_status_check_timer_cb;
bool dev_operation_timer_on(void);

static void box_operation_timer_cb(void const *n);
osTimerId   box_operation_status_check_timer_id;
bool        box_operation_status_timer_on = false ;
osTimerDef (BOX_OPERATION_STATUS_CHECK_TIMER, box_operation_timer_cb);

//IN/OUT BOX
static void dev_operation_timer_cb(void const *n);
osTimerId   dev_operation_status_check_timer_id;
bool        dev_operation_status_timer_on = false ;
osTimerDef (DEV_OPERATION_STATUS_CHECK_TIMER, dev_operation_timer_cb);

extern void a2dp_start_stream_handler(a2dp_stream_t *Stream);

btif_remote_device_t* masterBtRemDev = NULL;
btif_remote_device_t* slaveBtRemDev = NULL;
btif_remote_device_t* mobileBtRemDev = NULL;

bool switcherBetweenReconnMobileAndTws = false;

extern uint8_t bt_addr[BTIF_BD_ADDR_SIZE];
extern uint8_t ble_addr[BTIF_BD_ADDR_SIZE];


extern "C" void app_ble_update_adv_connect_with_mobile(uint8_t isConnectedWithMobile);

static uint8_t isMobileConnectionDownInitiatively = false;
static uint8_t isA2dpVolConfiguredByMaster = false;
static uint8_t isToResetMobileConnection = false;

bool app_conn_is_mobile_connection_down_initiatively(void)
{
    return isMobileConnectionDownInitiatively;
}

void app_conn_set_mobile_connection_down_reason(bool isDownInitiatively)
{
    isMobileConnectionDownInitiatively = isDownInitiatively;
}

bool app_conn_is_to_reset_mobile_connection(void)
{
    return isToResetMobileConnection;
}

void app_conn_set_reset_mobile_connection_flag(bool isToReset)
{
    isToResetMobileConnection = isToReset;
}


bool conn_isA2dpVolConfiguredByMaster(void)
{
    return isA2dpVolConfiguredByMaster;
}

void conn_set_flagofA2dpVolConfiguredByMaster(bool isDone)
{
    isA2dpVolConfiguredByMaster = isDone;
}

void start_tws_sharing_tws_info_timer(void)
{
    osTimerStart(tws_sharing_tws_info_time_out_timer_id, 
        TWS_SHARING_TWS_INFO_SUPERVISOR_TIMEOUT_IN_MS);
}

void stop_tws_sharing_tws_info_timer(void)
{
    osTimerStop(tws_sharing_tws_info_time_out_timer_id);
}

static void tws_sharing_tws_info_time_out_cb(void const *n)
{
    if (IS_CONNECTED_WITH_TWS())
    {
        // send the sharing tws info again
        app_tws_send_shared_tws_info();

        start_tws_sharing_tws_info_timer();
    }
}

void start_tws_sharing_pairing_info_timer(void)
{
    osTimerStart(tws_sharing_pairing_info_time_out_timer_id, 
        TWS_SHARING_PAIRING_INFO_SUPERVISOR_TIMEOUT_IN_MS);
}

void stop_tws_sharing_pairing_info_timer(void)
{
    osTimerStop(tws_sharing_pairing_info_time_out_timer_id);
}

static void tws_sharing_pairing_info_time_out_cb(void const *n)
{
    // send the sharing info again
    app_tws_send_shared_mobile_dev_info();

    start_tws_sharing_pairing_info_timer();
#ifdef __TWS_ROLE_SWITCH__
    if(is_tws_master_conn_slave_state())
    {
        TRACE("sync tws volume3");
        app_tws_set_slave_volume(a2dp_volume_get_tws());
    }
#endif
}

bool is_tws_connection_creation_supervisor_need_retry(void)
{
    bool nRet = false;
    if (connection_creation_supervisor_retry_cnt < CONNECTION_CREATION_SUPERVISOR_RETRY_LIMIT){
        connection_creation_supervisor_retry_cnt++;
        nRet = true;
    }else{
        nRet = false;
    }
    LOG_PRINT_BT_CONN_MGR("is_tws_connection_creation_supervisor_need_retry nRet:%d cnt:%d", nRet, connection_creation_supervisor_retry_cnt);
    
    return nRet;
}

void tws_connection_creation_supervisor_retry_cnt_reset(void)
{
    LOG_PRINT_BT_CONN_MGR("tws_connection_creation_supervisor_retry_cnt_reset");
    connection_creation_supervisor_retry_cnt = 0;
}

void tws_connection_creation_supervisor_retry_disable(void)
{
    LOG_PRINT_BT_CONN_MGR("tws_connection_creation_supervisor_retry_disable");
    connection_creation_supervisor_retry_cnt = CONNECTION_CREATION_SUPERVISOR_RETRY_LIMIT;
}

bool is_tws_connection_creation_supervisor_timer_on_going(void)
{
    return is_tws_connection_creation_supervisor_timer_on;
}

static void tws_connection_creation_supervisor_timer_cb(void const *n)
{
    is_tws_connection_creation_supervisor_timer_on = false;
    app_start_custom_function_in_bt_thread(0, 0, 
        (uint32_t)custom_tws_connection_creation_supvervisor_timer_cb);
}

void conn_start_tws_connection_creation_supvervisor_timer(uint32_t timerIntervalInMs, TWS_CONNECTION_CREATION_SUPERVISOR_TIMER_HANDLER callback)
{
    if (!is_tws_connection_creation_supervisor_timer_on)
    {
        custom_tws_connection_creation_supvervisor_timer_cb = callback;
        osTimerStart(tws_connection_creation_supervisor_timer_id, timerIntervalInMs);  
        is_tws_connection_creation_supervisor_timer_on = true;
    }
}

void conn_stop_tws_connection_creation_supvervisor_timer(void)
{
    if (is_tws_connection_creation_supervisor_timer_on)
    {
        is_tws_connection_creation_supervisor_timer_on = false;
        osTimerStop(tws_connection_creation_supervisor_timer_id);
    }    
    tws_connection_creation_supervisor_retry_disable();
}

static void connecting_tws_slave_timeout_callback(void)
{
    LOG_PRINT_BT_CONN_MGR("Connecting TWS slave time-out.");
    app_tws_config_master_wsco_by_connect_state(false, NONE_MODE);

    if (slaveBtRemDev)
    {
        if (btif_me_get_remote_device_state(slaveBtRemDev) == BTIF_BDS_DISCONNECTED){
            //do nothing
            LOG_PRINT_BT_CONN_MGR("WARNING!!! slaveBtRemDev is null");
        }else if (btif_me_get_remote_device_state(slaveBtRemDev) >= BTIF_BDS_CONNECTED){
            app_bt_disconnect_link(slaveBtRemDev);
        }else{
            LOG_PRINT_BT_CONN_MGR("WARNING!!! slaveBtRemDevState:%d so try again", btif_me_get_remote_device_state(slaveBtRemDev));
            conn_start_tws_connection_creation_supvervisor_timer(500, connecting_tws_slave_timeout_callback);
            return;
        }
    }
    else
    {
        // app_bt_cancel_ongoing_connecting();
    }
    
    set_conn_op_state(CONN_OP_STATE_IDLE);
    conn_ble_reconnection_machanism_handler(false);
}

void connecting_tws_slave_timeout_handler(void)
{    
    LOG_PRINT_BT_CONN_MGR("%s Connecting TWS slave time-out.", __FUNCTION__);
    app_tws_exit_role_switching(APP_TWS_ROLESWITCH_TIMEOUT);
    LOG_PRINT_BT_CONN_MGR("current state machine is %d", CURRENT_CONN_OP_STATE_MACHINE());
    set_conn_op_state(CONN_OP_STATE_IDLE);
#ifdef __ENABLE_IN_BOX_STATE_HANDLE__
    if (!app_is_charger_box_closed())
#endif
    {
        // TWS master connecting to slave timeout, start ble reconnecting procedure
        if (IS_CONNECTED_WITH_MOBILE())
        {
        #if FPGA==0
            conn_ble_reconnection_machanism_handler(false);
        #endif
        }  
        else
        {           
            if (!app_conn_is_mobile_connection_down_initiatively())
            {
                // start connecting mobile device
                app_start_custom_function_in_bt_thread(0, 0,(uint32_t)conn_connecting_mobile_handler);
            }
            else
            {
                conn_start_connectable_only_pairing(BT_SOURCE_CONN_MOBILE_DEVICE);
            }
        }  
    }
}

static void waiting_for_connection_from_tws_master_timeout_handler(void)
{
    LOG_PRINT_BT_CONN_MGR("Waiting for connection from TWS master time-out");

    app_tws_exit_role_switching(APP_TWS_ROLESWITCH_TIMEOUT);

    app_bt_accessmode_set_scaninterval(0x80);

#if FPGA==0
    conn_ble_reconnection_machanism_handler(false);
#endif
}

bool is_mobile_connection_creation_supervisor_timer_on_going(void)
{
    return is_mobile_connection_creation_supervisor_timer_on;
}

static void mobile_connection_creation_supervisor_timer_cb(void const *n)
{
    passedSupervisingMinutes++;
    if (passedSupervisingMinutes >= supervisingMinutes)
    {
        is_mobile_connection_creation_supervisor_timer_on = false;
        app_start_custom_function_in_bt_thread(0, 0, 
            (uint32_t)custom_mobile_connection_creation_supervisor_timer_cb);
    }
    else
    {
        osTimerStart(mobile_connection_creation_supervisor_timer_id, ONE_MINITUE_IN_MS);
    }
}

void conn_start_mobile_connection_creation_supvervisor_timer(uint32_t timerIntervalInMs, TWS_CONNECTION_CREATION_SUPERVISOR_TIMER_HANDLER callback)
{
    if (!is_mobile_connection_creation_supervisor_timer_on)
    {
        passedSupervisingMinutes = 0;
        supervisingMinutes = timerIntervalInMs/ONE_MINITUE_IN_MS;
        custom_mobile_connection_creation_supervisor_timer_cb = callback;
        osTimerStart(mobile_connection_creation_supervisor_timer_id, ONE_MINITUE_IN_MS);   
        is_mobile_connection_creation_supervisor_timer_on = true;
    }
}

void conn_stop_mobile_connection_creation_supvervisor_timer(void)
{
    if (is_mobile_connection_creation_supervisor_timer_on)
    {
        is_mobile_connection_creation_supervisor_timer_on = false;
        passedSupervisingMinutes = 0;
        supervisingMinutes = 0;
        osTimerStop(mobile_connection_creation_supervisor_timer_id);
    }
}

void conn_start_mobile_connection_creation_idle_timer(void)
{
    isConnectingMobileIdleTimerOn = true;
    set_conn_op_state(CONN_OP_STATE_WAITING_FOR_MOBILE_CONNECTION);
    app_start_custom_function_in_bt_thread(BTIF_BAM_CONNECTABLE_ONLY, 
                            0, (uint32_t)app_bt_accessmode_set);

    osTimerStart(connecting_mobile_idle_timer_id, CONNECTING_MOBILE_IDLE_TIME_IN_MS);
}

static void connecting_mobile_idle_timer_cb(void const *n)
{
    isConnectingMobileIdleTimerOn = false;
    if (!IS_CONNECTED_WITH_MOBILE())
    {
        app_start_custom_function_in_bt_thread((uint32_t)mobileDevBdAddr.address, 0, \
            (uint32_t)execute_connecting_mobile);  
    }
    else
    {
        conn_start_mobile_connection_creation_idle_timer();
    }
}

void conn_stop_mobile_connection_creation_idle_timer(void)
{
    isConnectingMobileIdleTimerOn = false;
    osTimerStop(connecting_mobile_idle_timer_id);
}

bool is_connecting_mobie_idle_timer_on(void)
{
    return isConnectingMobileIdleTimerOn;
}

static void connecting_ble_time_out_cb(void const *n)
{
    is_tws_connecting_ble_time_out_timer_on = false;

    if (app_tws_is_master_mode())
    {
        // if it's TWS master and the mobile connection is not created yet
        conn_ble_reconnection_machanism_handler(false);
    } 
}

void conn_start_tws_ble_connection_creation_supvervisor_timer(uint32_t timerIntervalInMs)
{
    if (!is_tws_connecting_ble_time_out_timer_on)
    {
        is_tws_connecting_ble_time_out_timer_on = true;
        osTimerStart(tws_connecting_ble_time_out_timer_id, timerIntervalInMs);
    }
}

void conn_stop_tws_ble_connection_creation_supvervisor_timer(void)
{
    if (is_tws_connecting_ble_time_out_timer_on)
    {
        is_tws_connecting_ble_time_out_timer_on = false;
        osTimerStop(tws_connecting_ble_time_out_timer_id);
    }
}

static void connecting_mobile_timeout_callback(void)
{   
    if ((0 != mobileBtRemDev) && IS_CONNECTED_WITH_MOBILE())
    {
        app_bt_disconnect_link(mobileBtRemDev);
    }
    else
    {
        // app_bt_cancel_ongoing_connecting();
    }
    
    if (is_connecting_mobie_idle_timer_on())
    {
        conn_stop_mobile_connection_creation_idle_timer();
        app_start_custom_function_in_bt_thread(0, 0, \
            (uint32_t)connecting_mobile_timeout_handler);
    }
    
    LOG_PRINT_BT_CONN_MGR("%s Connecting mobile dev timeout.", __func__);
}

void connecting_mobile_timeout_handler(void)
{        
    LOG_PRINT_BT_CONN_MGR("%s Connecting mobile dev timeout.", __func__);

    app_tws_exit_role_switching(APP_TWS_ROLESWITCH_TIMEOUT);
    set_conn_op_state(CONN_OP_STATE_IDLE); 
#ifdef __ENABLE_IN_BOX_STATE_HANDLE__
    if (!app_is_charger_box_closed())
#endif
    {
        if (!IS_CONNECTED_WITH_TWS() && !app_conn_is_pending_for_op_state_check())
        {
            conn_ble_reconnection_machanism_handler(false);
        }
        
        // enter page scan mode
        conn_start_connectable_only_pairing(BT_SOURCE_CONN_MOBILE_DEVICE);
    }
}

static void role_switch_supervisor_timer_cb(void const *n)
{
    LOG_PRINT_BT_CONN_MGR("Role switch time-out.");
    // if role switch cannot be completed and the time-out happens, come out of the role switch state machine
    // and back to normal state machine
    if (CONN_OP_STATE_MACHINE_ROLE_SWITCH == CURRENT_CONN_OP_STATE_MACHINE())
    {
        set_conn_op_state_machine(CONN_OP_STATE_MACHINE_NORMAL);
    }

    app_tws_exit_role_switching(APP_TWS_ROLESWITCH_TIMEOUT);
}

void conn_start_role_switch_supvervisor_timer(uint32_t timerIntervalInMs)
{
    osTimerStart(role_switch_supervisor_timer_id, timerIntervalInMs);   
}

void conn_stop_role_switch_supvervisor_timer(void)
{
    osTimerStop(role_switch_supervisor_timer_id);
}

static uint8_t isConnectingMobileTimeout = false;




static void connecting_mobile_supervisor_timer_cb(void const *n)
{
    LOG_PRINT_BT_CONN_MGR("[%s]", __func__);

    if ( mobileBtRemDev &&
        (btif_me_get_remote_device_auth_state(mobileBtRemDev) != BTIF_BAS_NOT_AUTHENTICATED) &&
        (btif_me_get_remote_device_auth_state(mobileBtRemDev) != BTIF_BAS_AUTHENTICATED)){
        //if AUTHENTICATE in process wait for user cancle forever
        LOG_PRINT_BT_CONN_MGR("[%s] authState:%d reset  connecting_mobile_supervisor_timer", __func__, btif_me_get_remote_device_auth_state(mobileBtRemDev));
        conn_start_connecting_mobile_supervising();
    }else{
        LOG_PRINT_BT_CONN_MGR("[%s] authState:%d timer out try disconnect it", __func__, btif_me_get_remote_device_auth_state(mobileBtRemDev));
        isConnectingMobileTimeout = true;
        app_start_custom_function_in_bt_thread(0,
                    0, (uint32_t)app_bt_disconnect_all_mobile_devces);
    }
}

void conn_start_connecting_mobile_supervising(void)
{
    osTimerStart(connecting_mobile_supervisor_timer_id, CONNECTING_MOBILE_SUPERVISOR_TIMEOUT_IN_MS);
}

void conn_stop_connecting_mobile_supervising(void)
{
    osTimerStop(connecting_mobile_supervisor_timer_id);
}

void conn_mgr_init(void)
{
    // create connecting tws supervisor timer
    tws_connection_creation_supervisor_timer_id = 
        osTimerCreate(osTimer(APP_TWS_CONNECTION_SUPVERVISOR_TIMER), osTimerOnce, NULL);
    
    mobile_connection_creation_supervisor_timer_id = 
        osTimerCreate (osTimer(APP_MOBILE_CONNECTION_SUPVERVISOR_TIMER), osTimerOnce, NULL);

    tws_connecting_ble_time_out_timer_id = 
        osTimerCreate(osTimer(APP_TWS_CONNECTING_BLE_TIMEOUT_TIMER), osTimerOnce, NULL);

    tws_sharing_pairing_info_time_out_timer_id = 
        osTimerCreate(osTimer(APP_TWS_SHARING_PAIRING_INFO_TIMEOUT_TIMER), osTimerOnce, NULL);

    tws_sharing_tws_info_time_out_timer_id = 
        osTimerCreate(osTimer(APP_TWS_SHARING_TWS_INFO_TIMEOUT_TIMER), osTimerOnce, NULL);

    role_switch_supervisor_timer_id =
        osTimerCreate (osTimer(APP_TWS_ROLE_SWITCH_SUPVERVISOR_TIMER), osTimerOnce, NULL);

    connecting_tws_delay_timer_id =
        osTimerCreate (osTimer(APP_TWS_CONNECTING_TWS_DELAY_TIMER), osTimerOnce, NULL);

    connecting_mobile_delay_timer_id =
        osTimerCreate (osTimer(APP_TWS_CONNECTING_MOBILE_DELAY_TIMER), osTimerOnce, NULL);

    connecting_mobile_idle_timer_id = 
        osTimerCreate (osTimer(APP_TWS_CONNECTING_MOBILE_IDLE_TIMER), osTimerOnce, NULL);

    box_operation_status_check_timer_id =
        osTimerCreate (osTimer(BOX_OPERATION_STATUS_CHECK_TIMER), osTimerOnce, NULL);

    dev_operation_status_check_timer_id =
        osTimerCreate (osTimer(DEV_OPERATION_STATUS_CHECK_TIMER), osTimerOnce, NULL);

    connecting_mobile_supervisor_timer_id =
        osTimerCreate (osTimer(APP_CONNECTING_MOBILE_SUPERVISOR_TIMER), osTimerOnce, NULL);

    conn_get_mobile_dev_addr();

}

uint8_t pendingSnifferControl;
void tws_slave_update_sniffer_in_sco_state(void)
{
    if (BTIF_DBG_SNIFFER_SCO_STOP == pendingSnifferControl)
    {
        LOG_PRINT_BT_CONN_MGR("Clear the in-sco sniffer state.");
#ifndef CHIP_BEST2300    
        bt_drv_reg_op_ld_sniffer_env_monitored_dev_state_set(0);
#endif
    }
}


#ifdef __TWS_CALL_DUAL_CHANNEL__
extern uint8_t slave_sco_active;
#endif

void tws_slave_stop_sco_sniffering(void)
{
#ifdef __TWS_CALL_DUAL_CHANNEL__
    slave_sco_active = 0;
#endif

#ifndef CHIP_BEST2300    
    if (bt_drv_reg_op_ld_sniffer_env_monitored_dev_state_get())
 #endif       
    {        
        pendingSnifferControl =  BTIF_DBG_SNIFFER_SCO_STOP;
        btif_me_dbg_sniffer_interface( btif_me_get_remote_device_hci_handle(masterBtRemDev), BTIF_DBG_SNIFFER_SCO_STOP); 
        app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_STOP,BT_STREAM_VOICE,BT_DEVICE_ID_1,MAX_RECORD_NUM,0,0);
    }    
}

void tws_master_send_max_slot_req_to_slave(void)
{
    //The 2M packet transmission should be used between TWS when LBRT
#if defined (__3M_PACK__) && !defined(LBRT)
    btdrv_current_packet_type_hacker(btif_me_get_remote_device_hci_handle(slaveBtRemDev),BT_TX_3M);
#else
    btdrv_current_packet_type_hacker(btif_me_get_remote_device_hci_handle(slaveBtRemDev),BT_TX_2M);
#endif
    //ME_DbgSnifferInterface(slaveBtRemDev->hciHandle, DBG_SEND_MAX_SLOT_REQ);
}




static void tws_connection_cleaned_up_callback(uint8_t errorType)
{
    if (IS_CONNECTING_SLAVE() || IS_WAITING_FOR_MASTER_CONNECTION() || IS_CONNECTING_MASTER())
    {
        set_conn_op_state(CONN_OP_STATE_IDLE);
    }
    
    reset_tws_acl_data_path();

    if (app_tws_is_slave_mode())
    {
        conn_set_flagofA2dpVolConfiguredByMaster(false);
        if (!is_simulating_reconnecting_in_progress())
        {
            tws_slave_stop_sco_sniffering();
        }
    }

    if (!is_simulating_reconnecting_in_progress())
    {
        uint8_t nullAddr[BD_ADDR_LEN] = {0}; 
        btif_me_set_sniffer_env(0, 0, nullAddr, nullAddr);
        app_tws_ui_reset_state();
        app_tws_clean_up_emsack();
    }
    
    slaveBtRemDev  = NULL;
    masterBtRemDev = NULL;
    tws_reset_saved_codectype();
    LOG_PRINT_BT_CONN_MGR("All connections of the TWS have been cleaned up.");
    if (app_tws_is_master_mode() && !is_simulating_reconnecting_in_progress())
    {
        uint8_t nullAddr[BD_ADDR_LEN] = {0}; 
        TRACE("tws_connection clean sniffer nulladdr !");
        btif_me_set_sniffer_env(0, 0, nullAddr, nullAddr);
    
    }
    stop_tws_sharing_pairing_info_timer();
    stop_tws_sharing_tws_info_timer();
    
    switch (CURRENT_CONN_OP_STATE_MACHINE())
    {        
        case CONN_OP_STATE_MACHINE_DISCONNECT_ALL:
        case CONN_OP_STATE_MACHINE_DISCONNECT_ALL_BEFORE_RE_PAIRING:
        case CONN_OP_STATE_MACHINE_DISCONNECT_ALL_BEFORE_CHANGE_ROLE:
            if (app_tws_is_master_mode())
            {
                app_start_custom_function_in_bt_thread(0, 0, (uint32_t)app_bt_disconnect_all_mobile_devces);
            }
            break;
        case CONN_OP_STATE_MACHINE_DISCONNECT_SLAVE:
            if (app_tws_is_master_mode())
            {
                set_conn_op_state_machine(CONN_OP_STATE_MACHINE_NORMAL);
            }
            break;  
        case CONN_OP_STATE_MACHINE_DISCONNECT_MOBILE_BEFORE_SIMPLE_REPAIRING:
            app_start_custom_function_in_bt_thread(0, 0, (uint32_t)app_tws_start_reset_simple_pairing);
            break;
        default:
            break;
    }

    if (app_tws_is_role_switching_on())
    {
        return;
    }

    if (BTIF_BEC_NO_ERROR != errorType)
    {
        switch (CURRENT_CONN_OP_STATE_MACHINE())
        {  
            case CONN_OP_STATE_MACHINE_NORMAL:
            case CONN_OP_STATE_MACHINE_RE_PAIRING:
            case CONN_OP_STATE_MACHINE_SIMPLE_REPAIRING:
                if (app_tws_is_slave_mode())
                {
                    conn_start_connectable_only_pairing(BT_SOURCE_CONN_MASTER);      
                }
                else
                {
                    /*
                     if (!((BEC_USER_TERMINATED == errorType) ||
                        (BEC_LOCAL_TERMINATED == errorType)))
                     {                        
                        app_start_custom_function_in_bt_thread(CONNECTING_SLAVE_TIMEOUT_RECONNECT_IN_MS, 0, 
                            (uint32_t)conn_start_connecting_slave);
                     }
                     */
                }
                
                conn_ble_reconnection_machanism_handler(true);
                break;
            default:
                break;
        } 
    }       
}

extern btif_remote_device_t* authenticate_failure_remote_dev;

static void mobile_device_connection_cleaned_up_callback(uint8_t errorType)
{   
    btif_remote_device_t* mobileBtRemDev_op = NULL;

    LOG_PRINT_BT_CONN_MGR("%s enter  errorType:%d", __func__, errorType); 
    if (IS_CONNECTING_MOBILE() || IS_WAITING_FOR_MOBILE_CONNECTION())
    {
        set_conn_op_state(CONN_OP_STATE_IDLE);
    }

    conn_stop_connecting_mobile_supervising();

    mobileBtRemDev_op = mobileBtRemDev;
    
    mobileBtRemDev = NULL;

    app_bt_cleanup_mobile_cmgrhandler();

    app_ble_update_adv_connect_with_mobile(0);
    
    LOG_PRINT_BT_CONN_MGR("All connections of the mobile device have been cleaned up.");

    app_conn_set_mobile_connection_down_reason(false);
    if(app_tws_is_master_mode() && !is_simulating_reconnecting_in_progress())
    {
        uint8_t nullAddr[BD_ADDR_LEN] = {0}; 
        btif_me_set_sniffer_env(0, 0, nullAddr, nullAddr);  
        LOG_PRINT_BT_CONN_MGR("mobile_device clear sniffer £¡");
    }


    if (app_conn_is_to_reset_mobile_connection())
    {
        LOG_PRINT_BT_CONN_MGR("Re-connect the mobile.");
        app_conn_set_reset_mobile_connection_flag(false);
        conn_start_reconnecting_on_mobile_disconnection();
        return;
    }
    LOG_PRINT_BT_CONN_MGR("%s:%d",__func__,CURRENT_CONN_OP_STATE_MACHINE());
    switch (CURRENT_CONN_OP_STATE_MACHINE())
    {
        case CONN_OP_STATE_MACHINE_NORMAL:
        case CONN_OP_STATE_MACHINE_RE_PAIRING:
        case CONN_OP_STATE_MACHINE_SIMPLE_REPAIRING:
        {
            // when the mobile initiates the remote disconnection, both of these
            // error codes will possibly be received
            if ((!isConnectingMobileTimeout) && 
                ((BTIF_BEC_USER_TERMINATED == errorType) ||
                 (BTIF_BEC_LOCAL_TERMINATED == errorType)||
                 (BTIF_BEC_AUTHENTICATE_FAILURE == errorType)))
            {
                app_conn_set_mobile_connection_down_reason(true);
                conn_start_connectable_only_pairing(BT_SOURCE_CONN_MOBILE_DEVICE);
                break;
            }
            else if (authenticate_failure_remote_dev == mobileBtRemDev_op)
            {
                app_conn_set_mobile_connection_down_reason(true);
                conn_start_connectable_only_pairing(BT_SOURCE_CONN_MOBILE_DEVICE);
                break;
            }

            isConnectingMobileTimeout = false;
            conn_start_reconnecting_on_mobile_disconnection();
            break;
        }
        case CONN_OP_STATE_MACHINE_ROLE_SWITCH:
            return;
        default:            
            break;
    } 

    conn_ble_reconnection_machanism_handler(false);
}

static void tws_connection_all_created_callback(void)
{
    LOG_PRINT_BT_CONN_MGR("All connections of the TWS have been created.");

    bt_drv_reg_op_set_swagc_mode(0);
    if (app_tws_is_master_mode())
    {
        tws_master_send_max_slot_req_to_slave();
    }
    if (IS_CONNECTING_SLAVE() || IS_WAITING_FOR_MASTER_CONNECTION() || IS_CONNECTING_MASTER())
    {
        set_conn_op_state(CONN_OP_STATE_IDLE);
    }
#ifdef __ENABLE_IN_BOX_STATE_HANDLE__
    app_tws_charger_box_opened_connect_slave_timer_stop();
#endif
#ifdef __TWS_PAIR_DIRECTLY__
    app_tws_reconnect_slave_timer_stop();
    app_tws_reconnect_master_timer_stop();
#endif
    conn_stop_tws_connection_creation_supvervisor_timer();

    if (CONN_OP_STATE_MACHINE_RE_PAIRING == CURRENT_CONN_OP_STATE_MACHINE() ||
        CONN_OP_STATE_MACHINE_SIMPLE_REPAIRING == CURRENT_CONN_OP_STATE_MACHINE() ||
        app_tws_ui_is_in_repairing())
    {
        if (app_tws_is_master_mode())
        {
            log_event_2(EVENT_TWS_PAIRED, TWSMASTER,  btif_me_get_remote_device_bdaddr(slaveBtRemDev)->address);
            app_tws_send_shared_tws_info();
            start_tws_sharing_tws_info_timer();
        }
        else
        {
            log_event_2(EVENT_TWS_PAIRED, TWSSLAVE, btif_me_get_remote_device_bdaddr(masterBtRemDev)->address);
        }
        
        ble_tws_clear_the_tws_pairing_state();  
        app_tws_pairing_done();
    }

    app_tws_ui_set_reboot_to_pairing(false);
#ifdef BES_OTA_TWS
    if (app_tws_is_master_mode())
    {
	ota_get_peer_firmrev();
    }
#endif

    if (is_simulating_reconnecting_in_progress())
    {
        app_tws_restore_mobile_a2dp_codec_type();
        if (app_tws_is_master_mode())
        {
           //LOG_PRINT_BT_CONN_MGR("Simulation of the TWS slave connecting has been completed,resume a2dp directly");
            resume_a2dp_directly();
           
           //   start_simulate_reconnecting_mobile();
             // app_start_custom_function_in_bt_thread(0, 0, (uint32_t)conn_start_connecting_mobile_device); 
        }
        else
        {            
            restore_avdtp_l2cap_channel_info();
        }
    }
    else
    {
#ifdef __ENABLE_IN_BOX_STATE_HANDLE__
        if (!app_is_in_charger_box())
        {
            ble_tws_stop_all_activities();
        }
        else
        {        
            // restore the BLE adv in the charger box
            app_tws_start_normal_ble_activities_in_chargerbox();
        }
#else
        ble_tws_stop_all_activities();
#endif
        if (app_tws_is_master_mode())
        {                    
            // if it's TWS master and the mobile connection is not created yet
            if (!IS_CONN_STATE_LINK_ON(mobileDevConnectionState))
            {
                switch (CURRENT_CONN_OP_STATE_MACHINE())
                {
                    case CONN_OP_STATE_MACHINE_NORMAL:
                    case CONN_OP_STATE_MACHINE_RE_PAIRING:
                    case CONN_OP_STATE_MACHINE_SIMPLE_REPAIRING:
                        if (!app_conn_is_mobile_connection_down_initiatively())
                        {
                            // start connecting mobile device
                            app_start_custom_function_in_bt_thread(0, 
                                0, (uint32_t)conn_start_connecting_mobile_device);
                        }
                        else
                        {
                            conn_start_connectable_only_pairing(BT_SOURCE_CONN_MOBILE_DEVICE);
                        }                                        
                        break;
                    default:
                        break;
                } 
            }
        } 
    }
}

void mobile_device_connection_all_created_handler(void)
{
    if (IS_CONNECTING_MOBILE() || IS_WAITING_FOR_MOBILE_CONNECTION())
    {
        set_conn_op_state(CONN_OP_STATE_IDLE);
    }

    LOG_PRINT_BT_CONN_MGR("app_tws_get_mode():%d",app_tws_get_mode());
    if (app_tws_is_master_mode())
    {
        // if it's TWS master and the TWS connection is not created yet
        if (!IS_CONN_STATE_LINK_ON(twsConnectionState))
        {             
            set_conn_op_state(CONN_OP_STATE_IDLE);
            conn_ble_reconnection_machanism_handler(false);
        }
    }    
}

static void mobile_device_connection_all_created_callback(void)
{
    LOG_PRINT_BT_CONN_MGR("enter mobile_device_connection_all_created_callback, state machine is %d", CURRENT_CONN_OP_STATE_MACHINE());    
    conn_stop_mobile_connection_creation_supvervisor_timer();
    conn_stop_mobile_connection_creation_idle_timer();
    
    LOG_PRINT_BT_CONN_MGR("All connections of the mobile device have been created.");

    mobile_device_connection_all_created_handler();
}

static void all_connections_cleaned_up_callback(uint8_t errorType)
{    
    if (!app_tws_is_role_switching_on())
    {
        app_tws_role_switch_reset_bt_info();
    }
    
    LOG_PRINT_BT_CONN_MGR("All connections have been down. Current state machine %d",
        CURRENT_CONN_OP_STATE_MACHINE());
#ifdef __TWS_PAIR_DIRECTLY__      
    slaveInReconMasterFlag=false;
#endif
    nv_record_flash_flush();
    switch (CURRENT_CONN_OP_STATE_MACHINE())
    {
#ifdef __TWS_ROLE_SWITCH__
        case CONN_OP_STATE_MACHINE_ROLE_SWITCH:
            app_start_custom_function_in_bt_thread(0, 
                        0, (uint32_t)app_tws_switch_role_and_reconnect_for_tws_roleswitch);
            return;
#endif
        case CONN_OP_STATE_MACHINE_DISCONNECT_ALL_BEFORE_RE_PAIRING:
            app_start_custom_function_in_bt_thread(0, 
                        0, (uint32_t)app_tws_pending_pairing_req_handler);
            return;
#ifdef __TWS_ROLE_SWITCH__
        case CONN_OP_STATE_MACHINE_DISCONNECT_ALL_BEFORE_CHANGE_ROLE:
            app_start_custom_function_in_bt_thread(0, 
                        0, (uint32_t)app_tws_switch_role_and_reconnect_for_tws_forcing_roleswitch);
            return;
#endif
        case CONN_OP_STATE_MACHINE_DISCONNECT_ALL:
        {
#if  defined(_AUTO_TEST_)
             AUTO_TEST_SEND("Disconnect ok.");
#endif
            // come to un-accessible mode
            set_conn_op_state_machine(CONN_OP_STATE_MACHINE_NORMAL);
            app_start_custom_function_in_bt_thread(BTIF_BAM_NOT_ACCESSIBLE, 
                        0, (uint32_t)app_bt_accessmode_set);
            return;
        }
        default:
            break;
    }

    if ((BTIF_BEC_USER_TERMINATED == errorType) ||
                (BTIF_BEC_LOCAL_TERMINATED == errorType))
    {
        return;
    }
#ifdef __ENABLE_IN_BOX_STATE_HANDLE__
    if (!app_tws_is_role_switching_on())
    {
        // check the charger box open state
        app_start_custom_function_in_bt_thread(0, 
                        0, (uint32_t)app_tws_check_chargerbox_state_after_operation);   
    }
#endif
}

static void all_connections_created_callback(void)
{
    LOG_PRINT_BT_CONN_MGR("All connections have been created. mode is %d isMobileDevPaired is %d", app_tws_get_mode(), isMobileDevPaired);

    app_start_custom_function_in_bt_thread(BTIF_BAM_NOT_ACCESSIBLE, 
                            0, (uint32_t)app_bt_accessmode_set);

    if ((!app_tws_is_exchanging_roleswitch_info_on_going()) &&
        (is_simulating_reconnecting_in_progress() || 
        app_tws_is_roleswitch_in_idle_state()))
    {
        app_tws_exit_role_switching(APP_TWS_ROLESWITCH_SUCCESSFUL);
    }
    else
    {
        // if connection is created right before the contoller side role switch is started,
        // don't do following handlings and return directly
        return;
    }    
    
    if (app_tws_is_master_mode())
    {
        LOG_PRINT_BT_CONN_MGR("Mobile BT role is %d", btif_me_get_remote_device_role(mobileBtRemDev));
        if (BTIF_BCR_SLAVE != btif_me_get_remote_device_role(mobileBtRemDev))
        {
            LOG_PRINT_BT_CONN_MGR("Need to switch the mobile BT role to the slave"); 
            btif_me_switch_role(mobileBtRemDev);
        }
        app_tws_sniffer_magic_config_channel_map(); 

#if IS_ENABLE_BT_SNIFF_MODE
        app_bt_set_linkpolicy(slaveBtRemDev, BTIF_BLP_MASTER_SLAVE_SWITCH|BTIF_BLP_SNIFF_MODE);
#else
        app_bt_set_linkpolicy(slaveBtRemDev, BTIF_BLP_MASTER_SLAVE_SWITCH);
#endif
        LOG_PRINT_BT_CONN_MGR("app_tws_get_env_pointer()->isOnGoingPlayInterruptedByRoleSwitch is %d", 
            app_tws_get_env_pointer()->isOnGoingPlayInterruptedByRoleSwitch);
#ifdef __ENABLE_WEAR_STATUS_DETECT__
        // sync the run-time wear status with the slave
        app_tws_syn_wearing_status();
#endif
#ifdef __ENABLE_IN_BOX_STATE_HANDLE__
        // sync the inbox state with the slave
        app_tws_inform_in_box_state();
#endif
    }
    else if (app_tws_is_slave_mode())
    {        
        if (isMobileDevPaired)
        {
#ifdef CHIP_BEST2300
            ASSERT(masterBtRemDev, "The ptr of master remote device error,please check!");
            if(masterBtRemDev){
                LOG_PRINT_BT_CONN_MGR("%s,DUMP before set sniffer role env ",__func__);
                LOG_PRINT_BT_CONN_MGR_DUMP8("%02x ", (uint8_t *)btif_me_get_remote_device_bdaddr(masterBtRemDev)->address, BD_ADDR_LEN);
                btif_me_set_sniffer_env(1, SNIFFER_ROLE, (uint8_t *)btif_me_get_remote_device_bdaddr(masterBtRemDev)->address, tws_mode_ptr()->slaveAddr.address);
            }
#elif defined(CHIP_BEST2000)
            bt_drv_reg_op_afh_set_default();
            btif_me_set_sniffer_env(1, SNIFFER_ROLE, mobileDevBdAddr.address, 
                tws_mode_ptr()->slaveAddr.address);         
#endif
        }
#ifdef __TWS_PAIR_DIRECTLY__      
        slaveInReconMasterFlag=false;
#endif   
#if IS_ENABLE_BT_SNIFF_MODE
        app_bt_set_linkpolicy(masterBtRemDev, BTIF_BLP_MASTER_SLAVE_SWITCH|BTIF_BLP_SNIFF_MODE);
#else
        app_bt_set_linkpolicy(masterBtRemDev, BTIF_BLP_MASTER_SLAVE_SWITCH);
#endif
    }

    app_stop_supervisor_timers();
    
    switch (CURRENT_CONN_OP_STATE_MACHINE())
    {
        case CONN_OP_STATE_MACHINE_RE_PAIRING:
        case CONN_OP_STATE_MACHINE_SIMPLE_REPAIRING:
            log_event_1(EVENT_MOBILE_CONNECTED_DURING_PAIRING, btif_me_get_remote_device_bdaddr(mobileBtRemDev)->address);
            
            // only send shared dev info while the paring is being done in the charger box
            if (app_tws_is_master_mode())
            {
                app_tws_send_shared_mobile_dev_info();
                start_tws_sharing_pairing_info_timer();
                nv_record_flash_flush();
#ifdef __TWS_ROLE_SWITCH__
                TRACE("sync tws volume4");
                app_tws_set_slave_volume(a2dp_volume_get_tws());
#endif
            }            
            else if (app_tws_is_freeman_mode())
            {      
                log_event_2(EVENT_TWS_PAIRED, TWSFREE, btif_me_get_remote_device_bdaddr(mobileBtRemDev)->address);
                app_tws_store_info_to_nv(TWSFREE, NULL);    
                nv_record_flash_flush();

                #if defined(TEST_OVER_THE_AIR_ENANBLED)
                // stop adv to close the anc tool connecting
                ble_tws_stop_all_activities();
                #endif
            }
#ifndef FPGA 
            app_stop_10_second_timer(APP_PAIR_TIMER_ID);
#endif            
            set_conn_op_state_machine(CONN_OP_STATE_MACHINE_NORMAL);
            break;
        case CONN_OP_STATE_MACHINE_NORMAL:
            if (app_tws_is_master_mode())
            {
                app_tws_send_shared_mobile_dev_info();
                start_tws_sharing_pairing_info_timer();
#ifdef __TWS_ROLE_SWITCH__
                TRACE("sync tws volume5");
                app_tws_set_slave_volume(a2dp_volume_get_tws());
#endif
            }
            break;
        default:
            set_conn_op_state_machine(CONN_OP_STATE_MACHINE_NORMAL);
            break;
    }

#ifdef __ENABLE_IN_BOX_STATE_HANDLE__
    if (app_is_in_charger_box())
    {
        // restore the BLE adv in the charger box
        app_tws_start_normal_ble_activities_in_chargerbox();
    }
#endif

    app_start_connectable_ble_adv(BLE_ADVERTISING_INTERVAL);
}

void app_resume_mobile_air_path_and_notify_switch_success(void)
{
    LOG_PRINT_BT_CONN_MGR("Resume mobile conn handle 0x%x", btif_me_get_remote_device_hci_handle(mobileBtRemDev)); 
    bt_drv_reg_op_resume_xfer_with_mobile(btif_me_get_remote_device_hci_handle(mobileBtRemDev));
    mobile_device_connection_all_created_callback();

    if ((CONN_STATE_TWS_ALL_CONNECTED == twsConnectionState) || \
        (app_tws_is_freeman_mode()))
    {
        all_connections_created_callback();
    }

    LOG_PRINT_BT_CONN_MGR("tws mode is %d",app_tws_get_mode());
    if ((app_tws_is_master_mode()) &&(true == get_hfp_connected_before_role_switch()))
    {
         LOG_PRINT_BT_CONN_MGR("app_resume_hfp_after_switch %d");
      set_hfp_connected_before_role_switch(false);
         hfp_handle_key(HFP_KEY_ADD_TO_EARPHONE);
    }
}

void tws_connection_state_updated_handler(uint8_t errorType)
{
    LOG_PRINT_BT_CONN_MGR("mobileDevActiveProfiles is 0x%x, mobileDevConnectionState is 0x%x twsConnectionState is 0x%x", 
        mobileDevActiveProfiles, mobileDevConnectionState, twsConnectionState);

    LOG_PRINT_BT_CONN_MGR("errorType %d", errorType);
    
    if ((0 == twsConnectionState) && !is_simulating_reconnecting_in_progress())
    {
        if (app_tws_is_slave_mode())
        {
        #if FPGA==0
            app_start_auto_power_off_supervising();
        #endif
            app_tws_role_switch_remove_bt_info_via_bd_addr(tws_mode_ptr()->masterAddr.address);
        }
        else if (app_tws_is_master_mode())
        {
            app_tws_role_switch_remove_bt_info_via_bd_addr(tws_mode_ptr()->slaveAddr.address);
        }
    }

    static uint32_t formerTwsConnState = 0;
    if (twsConnectionState == formerTwsConnState)
    {
        return;
    }
    formerTwsConnState = twsConnectionState;

    if (is_simulating_reconnecting_in_progress())
    {
        if (IS_CONN_STATE_LINK_ON(twsConnectionState) && !IS_CONN_STATE_A2DP_ON(twsConnectionState))
        {
            app_tws_roleswitch_restore_tws_enc_state();
        }
    }
        
    if (0 == twsConnectionState)
    {
        tws_connection_cleaned_up_callback(errorType);

        if (0 == mobileDevConnectionState)
        {
            all_connections_cleaned_up_callback(errorType);
        }        
    }
    else if (CONN_STATE_TWS_ALL_CONNECTED == twsConnectionState)
    {
        tws_connection_all_created_callback();
        if (app_tws_is_slave_mode())
        {
            all_connections_created_callback();
        }
        else if (IS_CONN_STATE_A2DP_ON(mobileDevConnectionState) &&
             IS_CONN_STATE_HFP_ON(mobileDevConnectionState) &&
            (IS_MOBILE_DEV_PROFILE_ACTIVE(BT_PROFILE_HSP)?IS_CONN_STATE_HSP_ON(mobileDevConnectionState):true))
        {
            all_connections_created_callback();
        }
    }
}

#if (_GFPS_)
extern "C" void app_exit_fastpairing_mode(void);
#endif
extern HCI_SIMULATE_RECONNECT_STATE_E currentSimulateReconnectionState;
void mobile_device_connection_state_updated_handler(uint8_t errorType)
{
    LOG_PRINT_BT_CONN_MGR("mobileDevActiveProfiles is 0x%x, mobileDevConnectionState is 0x%x twsConnectionState is 0x%x", 
        mobileDevActiveProfiles, mobileDevConnectionState, twsConnectionState);

    LOG_PRINT_BT_CONN_MGR("errorType %d", errorType);

    if ((0 == mobileDevConnectionState) && !is_simulating_reconnecting_in_progress())
    {
    #if FPGA==0    
        app_start_auto_power_off_supervising();
    #endif
        app_tws_role_switch_remove_bt_info_via_bd_addr(mobileDevBdAddr.address);
    }
        
    static uint32_t formerMobileConnState = 0;
    if (mobileDevConnectionState == formerMobileConnState)
    {
        return;
    }

    if (mobileDevConnectionState > 0)
    {
#ifndef FPGA    
        app_stop_10_second_timer(APP_RECONNECT_TIMER_ID); 
#endif
#if (_GFPS_)
        app_exit_fastpairing_mode();
#endif
    }

    bool isToStartConnectionSupervising = false;

    if (0 == formerMobileConnState)
    {
        if (IS_WAITING_FOR_MASTER_CONNECTION() && 
                    ((1 << CONN_STATE_LINK_BIT_OFFSET) == mobileDevConnectionState))
        {
            isToStartConnectionSupervising = true;
        }
    }
    
    formerMobileConnState = mobileDevConnectionState;
    
    if (0 == mobileDevConnectionState)
    {
        mobile_device_connection_cleaned_up_callback(errorType);
        if (0 == twsConnectionState)
        {
            all_connections_cleaned_up_callback(errorType);
        }  
    }
    else 
    {
        if (!is_simulating_reconnecting_in_progress())
        {
            if (IS_CONNECTED_WITH_MOBILE_PROFILE())
            {
                app_ble_update_adv_connect_with_mobile(1);
            }
            
            if (IS_CONN_STATE_A2DP_ON(mobileDevConnectionState) &&
                IS_CONN_STATE_HFP_ON(mobileDevConnectionState) &&
                (IS_MOBILE_DEV_PROFILE_ACTIVE(BT_PROFILE_HSP)?IS_CONN_STATE_HSP_ON(mobileDevConnectionState):true))
            { 
                mobile_device_connection_all_created_callback();

                if ((CONN_STATE_TWS_ALL_CONNECTED == twsConnectionState) || \
                    (app_tws_is_freeman_mode()))
                {
                    all_connections_created_callback();
                }
            }

            if (isToStartConnectionSupervising)
            {
                conn_start_mobile_connection_creation_supvervisor_timer(
                    CONNECTING_MOBILE_DEV_TIMEOUT_IN_MS, connecting_mobile_timeout_callback);
            }
        }
        else
        {
            if (IS_CONN_STATE_LINK_ON(mobileDevConnectionState))
            {
                if (IS_CONN_STATE_A2DP_ON(mobileDevConnectionState))
                {
                    if (TO_CONNECT_WITH_MOBILE == currentSimulateReconnectionState)
                    {
                        restore_avdtp_l2cap_channel_info();
                        set_current_simulated_reconnecting_state(SIMULATE_RECONNECT_STATE_IDLE);

                        //app_start_custom_function_in_bt_thread(0, 
                        //    0, (uint32_t)app_connect_op_after_role_switch);    
                    }
                }
                else
                {
                    app_tws_roleswitch_restore_mobile_enc_state();           
                }
            }
        }
        
    }
}

void conn_stop_discoverable_and_connectable_state(void)
{
    app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, BTIF_BAM_NOT_ACCESSIBLE,0,0);
}

void conn_remove_all_history_paired_devices(void)
{
    int paired_dev_count = nv_record_get_paired_dev_count();
    bt_status_t retStatus;
    btif_device_record_t record;
    for (int32_t index = 0;index < paired_dev_count;index++)
    {
        retStatus = nv_record_enum_dev_records(0, &record);
        if (BT_STS_SUCCESS == retStatus)
        {
            retStatus = nv_record_ddbrec_delete(&record.bdAddr);
        }   
    }
}

void conn_remove_tws_history_paired_devices(void)
{
    bt_status_t retStatus;
    btif_device_record_t record;    
    struct nvrecord_env_t *nvrecord_env;
    int paired_dev_count = nv_record_get_paired_dev_count();

    nv_record_env_get(&nvrecord_env);

    LOG_PRINT_BT_CONN_MGR("Remove all history tws nv records.");
    LOG_PRINT_BT_CONN_MGR("Master addr:");
    LOG_PRINT_BT_CONN_MGR_DUMP8("%02x ", nvrecord_env->tws_mode.masterAddr.address, BTIF_BD_ADDR_SIZE);
    LOG_PRINT_BT_CONN_MGR("Slave addr:");
    LOG_PRINT_BT_CONN_MGR_DUMP8("%02x ", nvrecord_env->tws_mode.slaveAddr.address, BTIF_BD_ADDR_SIZE);
    
    for (int32_t index = paired_dev_count - 1;index >= 0;index--){
        retStatus = nv_record_enum_dev_records(index, &record);
        if (BT_STS_SUCCESS == retStatus){
            LOG_PRINT_BT_CONN_MGR("The index %d of nv records:", index);
            LOG_PRINT_BT_CONN_MGR_DUMP8("%02x ", record.bdAddr.address, BTIF_BD_ADDR_SIZE);
            if (!memcmp(record.bdAddr.address, nvrecord_env->tws_mode.masterAddr.address, BTIF_BD_ADDR_SIZE) ||
                !memcmp(record.bdAddr.address, nvrecord_env->tws_mode.slaveAddr.address, BTIF_BD_ADDR_SIZE)){
                nv_record_ddbrec_delete(&record.bdAddr);
                LOG_PRINT_BT_CONN_MGR("Delete the nv record entry %d", index);
            }                        
        }   
    }
    
#ifdef __TWS_PAIR_DIRECTLY__
    nvrecord_env->tws_mode.mode=0;
#endif
    memset(nvrecord_env->tws_mode.masterAddr.address, 0, BTIF_BD_ADDR_SIZE);
    memset(nvrecord_env->tws_mode.slaveAddr.address, 0, BTIF_BD_ADDR_SIZE);
    memset(nvrecord_env->tws_mode.peerBleAddr.address, 0, BTIF_BD_ADDR_SIZE);
}

void conn_ble_reconnection_machanism_handler(bool isForceStartMasterAdv)
{
    LOG_PRINT_BT_CONN_MGR("%s enter connTWS:%d rolesw_onproc:%d",__func__, IS_CONNECTED_WITH_TWS(), app_tws_is_role_switching_on());

    if (app_tws_is_freeman_mode())
    {
        return;
    }
#ifdef __ENABLE_IN_BOX_STATE_HANDLE__
    if (app_is_charger_box_closed())
    {
        return;
    }
#endif  
    switch (CURRENT_CONN_OP_STATE_MACHINE())
    {        
        // for disconnect all state machine, won't start the ble re-connecting
        case CONN_OP_STATE_MACHINE_DISCONNECT_ALL:
        case CONN_OP_STATE_MACHINE_DISCONNECT_ALL_BEFORE_RE_PAIRING:
        case CONN_OP_STATE_MACHINE_DISCONNECT_ALL_BEFORE_CHANGE_ROLE:
        case CONN_OP_STATE_MACHINE_DISCONNECT_SLAVE:
        case CONN_OP_STATE_MACHINE_DISCONNECT_MOBILE_BEFORE_SIMPLE_REPAIRING:
            return;
        default:
            break;
    }

    if (!IS_CONNECTED_WITH_TWS())
    {
        if (!app_tws_is_role_switching_on())
        {
#if FPGA==0     
            if (app_tws_is_slave_mode())
            {   
                log_event_1(EVENT_TARGET_BLE_ADV_SCANNED, appm_get_current_ble_addr());
#ifdef __ENABLE_IN_BOX_STATE_HANDLE__
                if (!app_is_in_charger_box())
#else
                if(!slaveDisconMobileFlag)
#endif
                {
                    app_start_ble_tws_reconnect_scanning();
                }
                else
                {
#if BLE_TWS_USE_BLE_AS_INBOX_COMMUNICATION_PATH                
                    app_tws_start_interactive_reconn_ble_activities();
#endif
                }
            }
            else
            {
                if (IS_CONNECTING_MOBILE() || IS_CONNECTING_SLAVE())
                {
                    //do nothing
                }
                else
                {
#ifdef __TWS_PAIR_DIRECTLY__
                    if (isForceStartMasterAdv || IS_CONNECTED_WITH_MOBILE())
#else
                    if (isForceStartMasterAdv || 
#ifdef __ENABLE_IN_BOX_STATE_HANDLE__
                      app_is_in_charger_box() || IS_CONNECTED_WITH_MOBILE())
#else
                      IS_CONNECTED_WITH_MOBILE())
#endif
#endif
                    {
                        app_start_ble_tws_reconnect_adv();
                    }
                }
            }
#endif
        }
    }
    
    LOG_PRINT_BT_CONN_MGR("%s exit",__func__);
}

bool no_mobile_flag = 0;
void conn_get_mobile_dev_addr(void)
{
    int historyPairedMobileDeviceCount = 0;
    btif_device_record_t* pRecord;
    btif_device_record_t record;
    struct nvrecord_env_t *nvrecord_env;
    nv_record_env_get(&nvrecord_env);

    // find the latest two paired mobile device 
    int paired_dev_count = nv_record_get_paired_dev_count();
    LOG_PRINT_BT_CONN_MGR("%d paired devices in NV", paired_dev_count);
    if (paired_dev_count > 0)
    {
        if (IS_CONNECTING_MOBILE())
        {
            return;
        }
        
        bt_status_t retStatus;
        int leftCount = paired_dev_count;
        
        historyPairedMobileDeviceCount = 0;
        pRecord = &record;
        do
        {
            retStatus = nv_record_enum_dev_records(leftCount-1, pRecord);
            
            if (BT_STS_SUCCESS == retStatus)
            {
                if ((memcmp(nvrecord_env->tws_mode.slaveAddr.address,pRecord->bdAddr.address,BTIF_BD_ADDR_SIZE)) &&
                    (memcmp(nvrecord_env->tws_mode.masterAddr.address,pRecord->bdAddr.address,BTIF_BD_ADDR_SIZE)))
                {
                    historyPairedMobileDeviceCount++;
                    if (1 == historyPairedMobileDeviceCount)
                    {
                        // connect the latest paired mobile device 
                        break;
                    }     
                }
            }

            leftCount--;
        } while (leftCount > 0);
    }

    LOG_PRINT_BT_CONN_MGR("%d mobile devices used to be paired.", historyPairedMobileDeviceCount);

    if(historyPairedMobileDeviceCount > 0)
    {
        LOG_PRINT_BT_CONN_MGR("BD address of the mobile devices are:");
        LOG_PRINT_BT_CONN_MGR_DUMP8("%02x ", &record.bdAddr, BTIF_BD_ADDR_SIZE);   
        memcpy(&mobileDevBdAddr.address, &record.bdAddr, BTIF_BD_ADDR_SIZE);
        isMobileDevPaired = true;
        no_mobile_flag = 1;
    }
    else
    {
        LOG_PRINT_BT_CONN_MGR("No mobile device used to be paired.");
        isMobileDevPaired = false;
        no_mobile_flag = 0;
    }
}

static void connecting_mobile_delay_timer_cb(void const *n)
{
    app_start_custom_function_in_bt_thread((uint32_t)mobileDevBdAddr.address, 0, \
                (uint32_t)conn_connecting_mobile_handler);
}

bool execute_connecting_mobile(uint8_t* pBdAddr)
{
    static uint32_t connecting_mobile_pending_cnt = 0;
    LOG_PRINT_BT_CONN_MGR(" %s MEC(pendCons) is %d", __func__,  btif_me_get_pendCons());
    if ( btif_me_get_pendCons() > 0)
    {        
        LOG_PRINT_BT_CONN_MGR("pending connection op is on-going, so wait for some time. cnt:%d", connecting_mobile_pending_cnt);
        connecting_mobile_pending_cnt++;
        if ((connecting_mobile_pending_cnt*500) > 40*1000){
            ASSERT(0, "execute_connecting_mobile failed cnt:%d", connecting_mobile_pending_cnt);
        }
        osTimerStart(connecting_mobile_delay_timer_id, 500);       
        return false;
    }
    connecting_mobile_pending_cnt = 0;

    ble_tws_stop_all_activities();
    bt_status_t retStatus = app_bt_execute_connecting_mobile_device(pBdAddr);
    if ((BT_STS_NO_RESOURCES == retStatus) || (BT_STS_IN_PROGRESS == retStatus))
    {
        LOG_PRINT_BT_CONN_MGR("connection instance is occupied, so wait for some time.");
        osTimerStart(connecting_mobile_delay_timer_id, 500);   
        return false;
    }    

    osTimerStop(connecting_mobile_delay_timer_id); 

    set_conn_op_state(CONN_OP_STATE_CONNECTING_MOBILE);

    return true;
}

void conn_connecting_mobile_handler(void)
{
    if (app_conn_is_pending_for_op_state_check())
    {
        return;
    }

    if (IS_CONNECTING_SLAVE())
    {
        LOG_PRINT_BT_CONN_MGR("Connecting slave is in progress! So won't start connecting mobile.");
        return;
    }

#ifdef __TWS_PAIR_DIRECTLY__
    if (IS_CONNECTING_MASTER())
    {
        LOG_PRINT_BT_CONN_MGR("Connecting master is in progress! So won't start connecting mobile.");
        return;
    }
#endif
    
    // get the active profiles
    app_bt_get_mobile_device_active_profiles(mobileDevBdAddr.address);

    btif_device_record_t mobileRecord;
    bt_status_t retStatus = nv_record_ddbrec_find(&mobileDevBdAddr, &mobileRecord);
    if (BT_STS_SUCCESS == retStatus)
    {
        bool isConnectingStarted = execute_connecting_mobile(mobileRecord.bdAddr.address);
        if (!isConnectingStarted)
        {
            return;
        }

        conn_start_mobile_connection_creation_supvervisor_timer(
            CONNECTING_MOBILE_DEV_TIMEOUT_IN_MS, connecting_mobile_timeout_callback);
    }
    else
    {
        LOG_PRINT_BT_CONN_MGR("The mobile info is not in the record yet!");
        conn_start_connectable_only_pairing(BT_SOURCE_CONN_MOBILE_DEVICE);
    }    
}

#if (_GFPS_)
extern "C" void app_enter_fastpairing_mode(void);
#endif

void conn_start_connecting_mobile_device(void)
{            
    isInMobileBtSilentState = false;
    LOG_PRINT_BT_CONN_MGR("Try to connect mobile, current conn_op_state is 0x%x", conn_op_state); 
    
#if FPGA==1
    btif_device_record_t record; 
    bt_status_t ret = btif_sec_find_device_record(&mobileDevBdAddr, &record);
    if (BT_STS_SUCCESS == ret)
    {
        set_conn_op_state(CONN_OP_STATE_CONNECTING_MOBILE);
        app_bt_execute_connecting_mobile_device(record.bdAddr.address);
    }
#else

    if (isMobileDevPaired)
    {
        LOG_PRINT_BT_CONN_MGR("BD address of the mobile devices is:");
        LOG_PRINT_BT_CONN_MGR_DUMP8("%02x ", mobileDevBdAddr.address, BTIF_BD_ADDR_SIZE);          
        conn_connecting_mobile_handler();
    }
#endif
    else
    {
        CLEAR_MOBILE_DEV_ACTIVE_PROFILE();

        if (!app_tws_is_freeman_mode())
        {
            // TWS slave or master, it means the tws have been paired,can start public pairing to wait for
            // the connection from the mobile
            LOG_PRINT_BT_CONN_MGR("No history device paired, start public pairing...\n");
            app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, BTIF_BAM_GENERAL_ACCESSIBLE, 0,0);
            set_conn_op_state(CONN_OP_STATE_WAITING_FOR_MOBILE_CONNECTION);
            set_conn_op_state_machine(CONN_OP_STATE_MACHINE_RE_PAIRING); 

            #if (_GFPS_)
            app_enter_fastpairing_mode();
            #endif
        }
        else
        {
            // TWS freeman, it means the device is a blank one just programmed with image, so put it 
            // into the idle mode
            LOG_PRINT_BT_CONN_MGR("No history device paired, enter non-accessible mode...\n");
            app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, BTIF_BAM_NOT_ACCESSIBLE, 0,0);
        }
    }   
}

static uint32_t backupConnectingSlaveTimeoutInMs;
static bool backupFirstConnect;

static void connecting_tws_delay_timer_cb(void const *n)
{
    app_start_custom_function_in_bt_thread(backupConnectingSlaveTimeoutInMs, 
                        backupFirstConnect, (uint32_t)conn_start_connecting_slave);
}

bool execute_connecting_slave(uint32_t timeoutInMs, bool firstConnect)
{
    static uint32_t connecting_slave_pending_cnt = 0;

    LOG_PRINT_BT_CONN_MGR("Try to connect slave, current conn_op_state is 0x%x", conn_op_state);
    if (app_conn_is_pending_for_op_state_check())
    {
        ASSERT(!is_simulating_reconnecting_in_progress(), "in simulating progress");
        return false;
    }

    if (IS_CONNECTING_MOBILE())
    {
        LOG_PRINT_BT_CONN_MGR("Connecting mobile is in progress! So won't start connecting slave.");        
        ASSERT(!is_simulating_reconnecting_in_progress(), "in simulating progress");
        return false;
    }
    
    LOG_PRINT_BT_CONN_MGR("MEC(pendCons) is %d cnt:%d",  btif_me_get_pendCons(), connecting_slave_pending_cnt);
    if ( btif_me_get_pendCons() > 0)
    {
        //app_bt_cancel_ongoing_connecting();
        if (timeoutInMs){
            backupConnectingSlaveTimeoutInMs = timeoutInMs;            
            backupFirstConnect = firstConnect;
        }
        connecting_slave_pending_cnt++;
        if ((connecting_slave_pending_cnt*200) > 40*1000){
            ASSERT(0, "execute_connecting_mobile failed cnt:%d", connecting_slave_pending_cnt);
        }
        osTimerStart(connecting_tws_delay_timer_id, 200);
        return false;
    }
    connecting_slave_pending_cnt = 0;

    // stop being accessible
    app_bt_accessmode_set(BTIF_BAM_NOT_ACCESSIBLE);

    btif_device_record_t record; 
    btif_sec_find_device_record((bt_bdaddr_t*)&(tws_mode_ptr()->slaveAddr), &record);

    tws_dev_t *twsd = app_tws_get_env_pointer(); 
    a2dp_stream_t *src = twsd->tws_source.stream;

    log_event_2(EVENT_START_BT_CONNECTING, BT_FUNCTION_ROLE_SLAVE, tws_mode_ptr()->slaveAddr.address);
    log_event_1(EVENT_START_CONNECTING_A2DP, BT_FUNCTION_ROLE_SLAVE);
    ble_tws_stop_all_activities();
    bt_status_t ret = btif_a2dp_open_stream(src, &(tws_mode_ptr()->slaveAddr));  

    LOG_PRINT_BT_CONN_MGR("\nStart connecting TWS slave returns %d:\n", ret);
    LOG_PRINT_BT_CONN_MGR_DUMP8("0x%02x ", tws_mode_ptr()->slaveAddr.address, BTIF_BD_ADDR_SIZE);

    if ((BT_STS_NO_RESOURCES == ret) || (BT_STS_IN_PROGRESS == ret))
    {
        if (timeoutInMs){
            backupConnectingSlaveTimeoutInMs = timeoutInMs;            
            backupFirstConnect = firstConnect;
        }
        osTimerStart(connecting_tws_delay_timer_id, 200);
        return false;
    }   
    app_tws_config_master_wsco_by_connect_state(true, RECONNECT_MODE);
    twsd->notify(twsd);
    
    set_conn_op_state(CONN_OP_STATE_CONNECTING_SLAVE);
    return true;
}

void conn_start_connecting_slave(uint32_t timeoutInMs, bool firstConnect)
{         
    bool isConnectingStarted = execute_connecting_slave(timeoutInMs, firstConnect);

    if (isConnectingStarted)
    {
        bt_drv_reg_op_set_swagc_mode(2);
        if (!is_simulating_reconnecting_in_progress())
        {
            if (firstConnect){
                tws_connection_creation_supervisor_retry_cnt_reset();
            }
            if (timeoutInMs){
                // start a supervisor timer
                LOG_PRINT_BT_CONN_MGR("conn_start_tws_connection_creation_supvervisor_timer:%d", timeoutInMs);
                conn_start_tws_connection_creation_supvervisor_timer(timeoutInMs, connecting_tws_slave_timeout_callback);
            }
        }
    }
}

void conn_start_connecting_master(uint32_t timeoutInMs)
{             
    LOG_PRINT_BT_CONN_MGR("Try to connect master, current conn_op_state is 0x%x", conn_op_state);
        
    // stop being accessible
    app_bt_accessmode_set(BTIF_BAM_NOT_ACCESSIBLE);

    btif_device_record_t record; 
    btif_sec_find_device_record((bt_bdaddr_t*)&(tws_mode_ptr()->masterAddr), &record);

    tws_dev_t *twsd = app_tws_get_env_pointer(); 
    twsd->tws_sink.stream = app_tws_get_main_sink_stream();
    a2dp_stream_t *src = twsd->tws_sink.stream;

    LOG_PRINT_BT_CONN_MGR("\nStart connecting TWS master:\n");
    DUMP8("0x%02x ", tws_mode_ptr()->masterAddr.address, BTIF_BD_ADDR_SIZE);

    log_event_2(EVENT_START_BT_CONNECTING, BT_FUNCTION_ROLE_MASTER, tws_mode_ptr()->masterAddr.address);
    log_event_1(EVENT_START_CONNECTING_A2DP, BT_FUNCTION_ROLE_MASTER);

    btif_a2dp_open_stream(src, &(tws_mode_ptr()->masterAddr));  
    twsd->notify(twsd);

    set_conn_op_state(CONN_OP_STATE_CONNECTING_MASTER);

}

void conn_start_connectable_only_pairing(BT_CONNECTION_REQ_SOURCE_E conn_req_source)
{    
    LOG_PRINT_BT_CONN_MGR("The device starts connectable only paring");

    if (BT_SOURCE_CONN_MOBILE_DEVICE == conn_req_source)
    {        
        isInMobileBtSilentState = false;
        set_conn_op_state(CONN_OP_STATE_WAITING_FOR_MOBILE_CONNECTION);
#ifndef FPGA        
        app_start_10_second_timer(APP_RECONNECT_TIMER_ID); 
#endif
        log_event_1(EVENT_START_WAITING_FOR_BT_CONNECTING, BT_FUNCTION_ROLE_MOBILE);
    }
    else
    {              
        app_tws_get_env_pointer()->tws_sink.stream = app_tws_get_main_sink_stream();

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
        
        log_event_1(EVENT_START_WAITING_FOR_BT_CONNECTING, BT_FUNCTION_ROLE_MASTER);
        
        set_conn_op_state(CONN_OP_STATE_WAITING_FOR_MASTER_CONNECTION);

        bt_drv_reg_op_set_swagc_mode(2);
         
        uint32_t timeout = 0;
#ifdef FPGA
        timeout = WAITING_CONNECTION_FROM_TWS_MASTER_FOR_FPGA;
#else
        timeout = WAITING_CONNECTION_FROM_TWS_MASTER_TIMEOUT_IN_MS;
#endif
        conn_start_tws_connection_creation_supvervisor_timer(
                timeout , waiting_for_connection_from_tws_master_timeout_handler);
    }

    // set the access mode as connectable only
#ifdef __TWS_PAIR_DIRECTLY__
    if (BT_SOURCE_CONN_MOBILE_DEVICE == conn_req_source)
    {
        app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, BTIF_BAM_GENERAL_ACCESSIBLE,0,0);
        set_conn_op_state_machine(CONN_OP_STATE_MACHINE_RE_PAIRING); 

        LOG_PRINT_BT_CONN_MGR("#%s",__func__);
        conn_stop_mobile_connection_creation_supvervisor_timer();
        conn_stop_mobile_connection_creation_idle_timer();
    }
    else
    {
        app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, BTIF_BAM_CONNECTABLE_ONLY,0,0);
    }
#else
    app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, BTIF_BAM_CONNECTABLE_ONLY,0,0);
#endif
}

void conn_start_reconnecting_on_mobile_disconnection(void)
{
    // interleave the re-connecting to mobile and TWS, to avoid the case that
    // both TWS are connected with the mobile at the same-time with the same BT addr and link addr
    LOG_PRINT_BT_CONN_MGR("%s",__func__);
    if (!IS_CONNECTED_WITH_TWS() && switcherBetweenReconnMobileAndTws)
    {
        app_start_custom_function_in_bt_thread(CONNECTING_SLAVE_TIMEOUT_RECONNECT_IN_MS, true, 
                    (uint32_t)conn_start_connecting_slave);
    }
    else
    {
        app_start_custom_function_in_bt_thread(0, 
            0, (uint32_t)conn_start_connecting_mobile_device); 
    }

    switcherBetweenReconnMobileAndTws = !switcherBetweenReconnMobileAndTws;
}

void app_start_ble_tws_reconnect_adv(void);

void conn_system_boot_up_handler(void)
{        
    LOG_PRINT_BT_CONN_MGR("%s twsmode:%d connTws:%d connMobile:%d",__func__, app_tws_get_mode(), IS_CONNECTED_WITH_TWS(), IS_CONNECTED_WITH_MOBILE());

    set_conn_op_state_machine(CONN_OP_STATE_MACHINE_NORMAL);
    app_conn_set_mobile_connection_down_reason(false);

    switch (app_tws_get_mode())
    {
        case TWSMASTER:
            if (!IS_CONNECTED_WITH_TWS())
            {
                conn_ble_reconnection_machanism_handler(true);
            }
            else if (!IS_CONNECTED_WITH_MOBILE())
            {                
                // reconnect mobile
                app_start_custom_function_in_bt_thread(0, 0, (uint32_t)conn_start_connecting_mobile_device);
            }
            break;
        case TWSSLAVE:
            if (!IS_CONNECTED_WITH_TWS())
            {
                conn_start_connectable_only_pairing(BT_SOURCE_CONN_MASTER);

                conn_ble_reconnection_machanism_handler(true);
            }
            break;
        case TWSFREE:
            if (!IS_CONNECTED_WITH_MOBILE())
            {
                // reconnect mobile
                app_start_custom_function_in_bt_thread(0, 0, (uint32_t)conn_start_connecting_mobile_device);
            }
            break;
        default:
            break;                 
    }    
}

bool app_bt_conn_is_any_connection_on(void)
{
    return (0 != mobileDevConnectionState)||(0 != twsConnectionState);
}

void app_bt_cancel_ongoing_connecting(void)
{
    LOG_PRINT_BT_CONN_MGR("Cancel on-going connecting slave.");
    tws_dev_t *twsd = app_tws_get_env_pointer();
    a2dp_stream_t *src = twsd->tws_source.stream;
    
    if(btif_a2dp_get_stream_device(src) &&  btif_a2dp_get_stream_devic_cmgrHandler_remdev(src))
    {
        btif_me_cancel_create_link( (btif_handler*)btif_a2dp_get_stream_devic_cmgrHandler_bt_handler(src), btif_a2dp_get_stream_devic_cmgrHandler_remdev(src));
    }     
   
    LOG_PRINT_BT_CONN_MGR("Cancel on-going connecting mobile.");
    app_bt_cancel_connecting(BT_DEVICE_ID_1);
}

void conn_bt_disconnect_all(void)
{
    LOG_PRINT_BT_CONN_MGR("%s", __FUNCTION__);

    if (app_bt_conn_is_any_connection_on())
    {
        set_conn_op_state_machine(CONN_OP_STATE_MACHINE_DISCONNECT_ALL);
        bool isExistingA2dpStreamToClose = app_tws_close_tws_stream();
        if (!isExistingA2dpStreamToClose)
        {              
            app_tws_clean_up_connections();
        }
    }
}

void conn_tws_player_stopped_handler(void)
{
    LOG_PRINT_BT_CONN_MGR("TWS player is stopped, current state machine is %d", CURRENT_CONN_OP_STATE_MACHINE());
    
    if ((CONN_OP_STATE_MACHINE_DISCONNECT_ALL == CURRENT_CONN_OP_STATE_MACHINE()) ||
        (CONN_OP_STATE_MACHINE_DISCONNECT_ALL_BEFORE_RE_PAIRING == CURRENT_CONN_OP_STATE_MACHINE()) ||
        (CONN_OP_STATE_MACHINE_DISCONNECT_ALL_BEFORE_CHANGE_ROLE == CURRENT_CONN_OP_STATE_MACHINE()))
    {
        app_start_custom_function_in_bt_thread(0, 
            0, (uint32_t)app_tws_clean_up_connections);
    }    
}

void app_stop_supervisor_timers(void)
{
    osTimerStop(connecting_tws_delay_timer_id);
    osTimerStop(connecting_mobile_delay_timer_id);
    conn_stop_mobile_connection_creation_idle_timer();
    conn_stop_tws_ble_connection_creation_supvervisor_timer();
    conn_stop_tws_connection_creation_supvervisor_timer();
    conn_stop_mobile_connection_creation_supvervisor_timer();
}

void app_tws_clean_up_connections(void)
{
    app_stop_supervisor_timers();

    if (app_tws_is_master_mode())
    {
        if (IS_CONNECTED_WITH_TWS())
        {
            // disconnect all links with tws slave
            app_tws_disconnect_slave();   
        }
        else
        {
            app_bt_disconnect_all_mobile_devces();
        }
    }
    else if (app_tws_is_freeman_mode())
    {
        app_bt_disconnect_all_mobile_devces();
    }
    else
    {
        if (IS_CONNECTED_WITH_TWS())
        {
            // disconnect all links with tws master
            app_tws_disconnect_master();  
        }
    }
}

// done before doing the charger box open&close handling, or
// before doing the inbox pairing
#define CONN_STATE_CHECK_TIMEOUT_IN_MS      15000
static void conn_state_check_timeout_timer_cb(void const *n);
osTimerDef (CONN_STATE_CHECK_TIMEOUT_TIMER, conn_state_check_timeout_timer_cb);  
static osTimerId conn_state_check_timeout_timer = NULL;
static bool isConnCheckTimerStarted = false;

static void conn_state_check_timeout_timer_cb(void const *n)
{
    LOG_PRINT_BT_CONN_MGR("Conn state machine %d op state %d role switch state %d", 
        CURRENT_CONN_OP_STATE_MACHINE(), conn_op_state, app_tws_is_role_switching_on());
    LOG_PRINT_BT_CONN_MGR("Tws conn state %d Mobile conn state %d",
        twsConnectionState, mobileDevConnectionState);
    if (app_tws_is_role_switching_on()){
        if (app_tws_get_role_switch_target_role() != 
            app_tws_get_mode())
        {
            app_tws_switch_role_in_nv();
        }
    }
    ASSERT(0, "conn state check time-out happens!");
}

bool app_conn_check_current_operation_state(void)
{
    if (IS_CONNECTING_SLAVE())
    {
        LOG_PRINT_BT_CONN_MGR("Stop tws connecting timer.");
        conn_stop_tws_connection_creation_supvervisor_timer();
        return false;
    }
    else if (IS_CONNECTING_MOBILE())
    {
        LOG_PRINT_BT_CONN_MGR("Stop mobile connecting timer.");
        conn_stop_mobile_connection_creation_supvervisor_timer();
        if (is_connecting_mobie_idle_timer_on())
        {
            conn_stop_mobile_connection_creation_idle_timer();
            set_conn_op_state(CONN_OP_STATE_IDLE); 
        }
        return false;
    }
    else if (IS_CONNECTING_MASTER())
    {
        ASSERT(0, "Connecting master state should be involved in the role switch!");
    }
    else if (IS_WAITING_FOR_MASTER_CONNECTION() || IS_WAITING_FOR_MOBILE_CONNECTION())
    {
        LOG_PRINT_BT_CONN_MGR("IS_WAITING_FOR_MASTER_CONNECTION or IS_WAITING_FOR_MOBILE_CONNECTION ");
#ifndef FPGA            
        app_stop_10_second_timer(APP_RECONNECT_TIMER_ID); 
#endif
        app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, BTIF_BAM_NOT_ACCESSIBLE, 0, 0);
    } 

    return true;
}

bool app_conn_is_pending_for_op_state_check(void)
{
    return isConnCheckTimerStarted;
}

void app_pending_for_idle_state_timeout_handler(bool isAllowToContinue)
{
    if (NULL == conn_state_check_timeout_timer)
    {            
        conn_state_check_timeout_timer =
            osTimerCreate (osTimer(CONN_STATE_CHECK_TIMEOUT_TIMER), osTimerOnce, NULL);
    }

    if (!isAllowToContinue)
    {
        if (!isConnCheckTimerStarted)
        {
            osTimerStart(conn_state_check_timeout_timer, CONN_STATE_CHECK_TIMEOUT_IN_MS);
            isConnCheckTimerStarted = true;
        }
    }
    else
    {
        osTimerStop(conn_state_check_timeout_timer);
        isConnCheckTimerStarted = false;
    }    
}

bool app_conn_check_and_restore_op_state(void)
{
    bool isAllowToContinue = true;
    
    LOG_PRINT_BT_CONN_MGR("Enter %s", __FUNCTION__);
    if (app_tws_is_role_switching_on())
    {
        LOG_PRINT_BT_CONN_MGR("Return and wait until the role switch is done.");
        isAllowToContinue = false;
        goto exit;
    }

    if ((CONN_OP_STATE_MACHINE_DISCONNECT_ALL == CURRENT_CONN_OP_STATE_MACHINE()) || 
        (CONN_OP_STATE_MACHINE_DISCONNECT_ALL_BEFORE_RE_PAIRING == CURRENT_CONN_OP_STATE_MACHINE()) ||
        (CONN_OP_STATE_MACHINE_DISCONNECT_ALL_BEFORE_CHANGE_ROLE == CURRENT_CONN_OP_STATE_MACHINE()) ||
        (CONN_OP_STATE_MACHINE_DISCONNECT_SLAVE == CURRENT_CONN_OP_STATE_MACHINE()) ||
        (CONN_OP_STATE_MACHINE_DISCONNECT_MOBILE_BEFORE_SIMPLE_REPAIRING == CURRENT_CONN_OP_STATE_MACHINE()))
    {
        LOG_PRINT_BT_CONN_MGR("Return and wait until state machine %d is done.", CURRENT_CONN_OP_STATE_MACHINE());
        isAllowToContinue = false;
        goto exit;
    }

    isAllowToContinue = app_conn_check_current_operation_state();
    if (!isAllowToContinue)
    {
        LOG_PRINT_BT_CONN_MGR("no AllowToContinue !");
        goto exit;
    }
    
    if (CONN_OP_STATE_MACHINE_RE_PAIRING == CURRENT_CONN_OP_STATE_MACHINE() ||
        CONN_OP_STATE_MACHINE_SIMPLE_REPAIRING == CURRENT_CONN_OP_STATE_MACHINE())
    {            
        set_conn_op_state_machine(CONN_OP_STATE_MACHINE_NORMAL);  
    }

exit:

    app_pending_for_idle_state_timeout_handler(isAllowToContinue);

    return isAllowToContinue;
}

/*************************************
*open box/close box
*open box/out box
*open box/in box
****************************************/

bool box_operation_timer_on(void)
{
    return box_operation_status_timer_on;
}

void box_operation_stop_check_timer(void)
{
    if(box_operation_status_timer_on)
        osTimerStop(box_operation_status_check_timer_id);
    box_operation_status_timer_on = false ;
}

void box_operation_start_check_timer(int open,int delaycheck,BOX_OPERATION_STATUS_CHECK_HANDLER cb)
{
    uint32_t delay_ms = 0;

    box_operation_stop_check_timer();

    if(dev_operation_timer_on())
    {
        LOG_PRINT_BT_CONN_MGR(" [%s] wait in/out box complete",__func__);
        delaycheck  = 1;
    }

    if(delaycheck)   //delay check
    {
        delay_ms = BOX_OPERATION_DELAY_CHECK_MS;
    }
    else
    {
        if( open != 0)//open
        {
            delay_ms = BOX_OPERATION_OPEN_CHECK_MS;
        }
        else
        {
            delay_ms = BOX_OPERATION_CLOSE_CHECK_MS;
        }
    }

    box_operation_status_check_timer_cb = cb;
    box_operation_status_timer_on = true;
    osTimerStart(box_operation_status_check_timer_id,
                 BOX_OPERATION_DELAY_CHECK_MS);

    LOG_PRINT_BT_CONN_MGR(" [%s] open %d delay %d timer:%d ", __func__,open,delaycheck,delay_ms);

}

bool dev_operation_timer_on(void)
{
    return dev_operation_status_timer_on;
}

void dev_operation_stop_check_timer(void)
{
    if(dev_operation_status_timer_on)
        osTimerStop(dev_operation_status_check_timer_id);
    dev_operation_status_timer_on = false ;
}

void dev_operation_start_check_timer(int dir,int delaycheck,BOX_OPERATION_STATUS_CHECK_HANDLER cb)
{
    uint32_t delay_ms = 0;

    dev_operation_stop_check_timer();

    if(box_operation_timer_on())
    {
        LOG_PRINT_BT_CONN_MGR(" [%s] wait open/close box complete",__func__);
        delaycheck  = 1;
    }

    if(delaycheck)   //delay check
    {
        delay_ms = BOX_OPERATION_DELAY_CHECK_MS;
    }
    else
    {
        if( dir != 0)//out
        {
            delay_ms = DEV_OPERATION_OPEN_CHECK_MS;
        }
        else
        {
            delay_ms = DEV_OPERATION_CLOSE_CHECK_MS;
        }
    }

    dev_operation_status_check_timer_cb = cb;
    dev_operation_status_timer_on = true;
    osTimerStart(dev_operation_status_check_timer_id,
                 BOX_OPERATION_DELAY_CHECK_MS);

    LOG_PRINT_BT_CONN_MGR(" [%s] open %d delay %d timer:%d ", __func__,dir,delaycheck,delay_ms);
}

void box_operation_exec_callback(void *p1,void *p2)
{
    LOG_PRINT_BT_CONN_MGR(" [%s] ",__func__);

    if(box_operation_status_check_timer_cb)
        box_operation_status_check_timer_cb();
}

void box_operation_timer_cb(void const *n)
{
    LOG_PRINT_BT_CONN_MGR(" [%s] ",__func__);
    box_operation_status_timer_on = false;
    app_start_custom_function_in_bt_thread(0,0,(uint32_t)box_operation_exec_callback);
}

void dev_operation_exec_callback(void *p1,void *p2)
{
    LOG_PRINT_BT_CONN_MGR(" [%s] ",__func__);
    if(dev_operation_status_check_timer_cb)
        dev_operation_status_check_timer_cb();
}

void dev_operation_timer_cb(void const *n)
{
    LOG_PRINT_BT_CONN_MGR(" [%s] ",__func__);
    dev_operation_status_timer_on = false;
    app_start_custom_function_in_bt_thread(0,0,(uint32_t)dev_operation_exec_callback);

}
extern uint8_t esco_retx_nb;
/*when master eSCO exists,meanwhile master restart to connect slave,close eSCO retransmission window(wesco)*/
void app_tws_config_master_wsco_by_connect_state(bool state, uint8_t mode)
{
    if ((app_tws_is_master_mode()) &&btapp_hfp_call_is_present())
    {
        if(state)
        {
            LOG_PRINT_BT_CONN_MGR(" [%s] close wesco",__func__);
            if(mode == RECONNECT_MODE)
            {
                esco_retx_nb = bt_drv_reg_op_retx_att_nb_manager(RETX_NB_0);
            }
            else if(mode == DISCONNECT_MODE)
            {
                esco_retx_nb = bt_drv_reg_op_retx_att_nb_manager(RETX_NB_1);
            }    
            
            if (TWS_MASTER_CONN_SLAVE == app_tws_get_conn_state())
            {
                tws_send_esco_retx_nb(esco_retx_nb);
            }
         }
         else
         {
            LOG_PRINT_BT_CONN_MGR(" [%s] open wesco",__func__);
            bt_drv_reg_op_retx_att_nb_manager(RETX_NEGO);
         }
    }
}

btif_remote_device_t*   app_bt_conn_mgr_get_mobileBtRemDev(void)
{
    return (btif_remote_device_t*)mobileBtRemDev;

}

