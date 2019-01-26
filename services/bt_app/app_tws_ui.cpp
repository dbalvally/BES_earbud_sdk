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
#include "hal_timer.h"
#include "hal_trace.h"
#include "app_status_ind.h"
#include "app_bt_stream.h"
#include "nvrecord.h"
#include "nvrecord_env.h"

#include "me_api.h"
#include "a2dp_api.h"
#include "avdtp_api.h"
#include "hci_api.h"


#include "rtos.h"
#include "besbt.h"

#include "cqueue.h"
#include "btapp.h"
#include "app_bt.h"

#include "nvrecord.h"

#include "apps.h"
#include "resources.h"

#include "app_bt_media_manager.h"
#ifdef __TWS__
#include "app_tws.h"
#include "app_tws_if.h"
#include "factory_section.h"
#endif

#include "app_utils.h"
#include "app_tws_cmd_handler.h"
#include "app_bt_conn_mgr.h"
#include "app_tws_role_switch.h"
#include "app_tws_if.h"
#include "app_battery.h"
#include "app_hfp.h"
#include "tws_switch_param.h"
#include "ble_tws.h"

#include "log_section.h"
#include "bt_drv_interface.h"
#include "bt_drv_reg_op.h"

#include "hfp_api.h"

extern int app_bt_stream_local_volume_get(void);
extern int a2dp_volume_set(U8 vol);
extern void a2dp_resume_mobile_stream(void);
static void app_tws_switch_source_mic(bool isMute);
int hfp_volume_set(int vol);
int hfp_volume_get(void);
extern void hfp_handle_key(uint8_t hfp_key);
int avrcp_callback_getplaystatus(void);
extern "C" uint8_t* appm_get_current_ble_addr(void);

extern uint8_t bt_addr[BD_ADDR_SIZE];
extern uint8_t ble_addr[BD_ADDR_SIZE];
extern nv_record_struct nv_record_config;
extern struct BT_DEVICE_T  app_bt_device;

#define ROLE_SWITCH_FAILURE_HANDLING_PENDING_TIME_IN_MS 500
#define ROLE_SWITCH_FAILURE_TIMEOUT_TO_RESET_IN_MS  15000
static uint8_t  roleswitch_failure_check_count = 0;
static void check_before_roleswitch_failure_handling_timer_cb(void const *n);
osTimerDef (CHECK_BEFORE_ROLESWITCH_FAILURE_HANDLING_TIMER, check_before_roleswitch_failure_handling_timer_cb);
static osTimerId check_before_roleswitch_failure_handling_timer;

#ifdef __ENABLE_IN_BOX_STATE_HANDLE__
static uint8_t slave_inbox_and_closed_flag;
#define CHARGERBOX_CLOSED_EVENT_EXCHANGE_TIMEOUT_IN_MS  (12000)
static void chargerbox_closed_event_exchange_supervisor_timer_cb(void const *n);
osTimerDef (CHARGERBOX_CLOSED_EVENT_EXCHANGE_SUPERVISOR_TIMER,
    chargerbox_closed_event_exchange_supervisor_timer_cb);
static osTimerId chargerbox_closed_event_exchange_supervisor_timer;
#endif

#define WAITING_BT_ROLESWITCH_DONE_SIGNAL_FROM_MASTER_TIMEOUT_IN_MS         3900
#define WAITING_BT_ROLESWITCH_DONE_SIGNAL_FROM_MASTER_TIMER_PERIOD_IN_MS    300
#ifdef __TWS_ROLE_SWITCH__
static void waiting_bt_roleswitch_done_signal_from_master_timer_cb(void const *n);
osTimerDef (WAITING_BT_ROLESWITCH_DONE_SIGNAL_FROM_MASTER_TIMER,
    waiting_bt_roleswitch_done_signal_from_master_timer_cb);
static osTimerId waiting_bt_roleswitch_done_signal_from_master_timer;
static uint8_t  waiting_bt_roleswitch_done_timer_triggered_count = 0;

static void app_tws_role_switch_failure_handler(uint8_t reason);
#endif
#ifdef __ENABLE_IN_BOX_STATE_HANDLE__
static void app_tws_charger_box_opened_connect_slave_timeout_handler(void const *param);
osTimerDef (CHARGER_BOX_OPENED_CONNECT_SLAVE_TIMER, app_tws_charger_box_opened_connect_slave_timeout_handler);
static osTimerId charger_box_opened_connect_slave_timer = NULL;
#endif
#ifdef __TWS_PAIR_DIRECTLY__
static void app_tws_reconnect_slave_timeout_handler(void const *param);
osTimerDef (APP_TWS_RECONNECT_SLAVE_TIMER, app_tws_reconnect_slave_timeout_handler);
static osTimerId app_tws_reconnect_slave_timer = NULL;

static void app_tws_reconnect_master_timeout_handler(void const *param);
osTimerDef (APP_TWS_RECONNECT_MASTER_TIMER, app_tws_reconnect_master_timeout_handler);
static osTimerId app_tws_reconnect_master_timer = NULL;

#endif

static void app_tws_pause_music_timeout_handler(void const *param);
osTimerDef (PAUSE_MUSIC_TIMER, app_tws_pause_music_timeout_handler);
static osTimerId pause_music_timer = NULL;


#ifdef __ENABLE_IN_BOX_STATE_HANDLE__
static void app_tws_execute_charger_box_bt_closed_operation_delay_timeout_handler(void const *param);
osTimerDef (BOX_CLOSED_OPERATION_DELAY_TIMER, app_tws_execute_charger_box_bt_closed_operation_delay_timeout_handler);
static osTimerId box_closed_operation_delay_timer = NULL;
#endif

#ifdef LBRT
static void app_tws_lbrt_ping_to_cb(void const *param);
osTimerDef (TWS_LBRT_PING, app_tws_lbrt_ping_to_cb);
static osTimerId tws_lbrt_ping_timer = NULL;

static void app_tws_set_lbrt_to_cb(void const *param);
osTimerDef (TWS_SERT_LBRT_TIMER, app_tws_set_lbrt_to_cb);
static osTimerId tws_set_lbrt_timer = NULL;

#endif

typedef struct
{
    app_tws_runtime_status_t runtimeStatus;
    uint8_t isPutInChargerBoxCalled         : 1;
    uint8_t isOutOfChargerBoxCalled         : 1;
    uint8_t isChargerBoxOpenedCalled        : 1;
    uint8_t isChargerBoxClosedCalled        : 1;
    uint8_t isInChargerBox                  : 1;
    uint8_t isChargerBoxClosed              : 1;
    uint8_t isPeerDevRoleSwitchInProgress   : 1;
    uint8_t isRebootToPairing               : 1;

    uint8_t isBtRoleSwitchDuringRoleSwitchOn: 1;
    uint8_t isLbrtEnable                    : 1;
    uint8_t isFixTxPwr                      : 1;
    uint8_t reserve                         : 5;
    uint8_t lbrt_tx_pwr;
    uint32_t lbrt_ping_ticks;
    uint32_t lbrt_switch_ticks;
    app_tws_pairing_preparation_done_callback app_tws_pairing_preparation_done_cb;
    app_tws_pairing_done_callback   app_tws_pairing_done_cb;
    uint8_t roleSwitchFailureReason;
} APP_TWS_UI_ENV_T;

static APP_TWS_UI_ENV_T app_tws_ui_env =
{
    {
        0,
        0,
        0,
        0,
    },
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,

    0,
    0,
    0,      //isFixTxPwr
    0,      //reserve
    0,      //lbrt_tx_pwr
    0,      //lbrt_ping_ticks
    0,      //lbrt_switch_ticks
    NULL,   // fill app_tws_pairing_preparation_done_cb here
    NULL,   // fill app_tws_pairing_done_cb
    0,      //roleSwitchFailureReason
};

void app_tws_ui_init(void)
{
#ifdef __TWS_ROLE_SWITCH__
    APP_TWS_ROLESWITCH_CONFIG_T tConfig = {app_tws_role_switch_started, app_tws_roleswitch_done};
    app_tws_role_switch_init(&tConfig);

    check_before_roleswitch_failure_handling_timer =
        osTimerCreate(osTimer(CHECK_BEFORE_ROLESWITCH_FAILURE_HANDLING_TIMER),
            osTimerOnce, NULL);
#endif
#ifdef __ENABLE_IN_BOX_STATE_HANDLE__
    chargerbox_closed_event_exchange_supervisor_timer =
        osTimerCreate(osTimer(CHARGERBOX_CLOSED_EVENT_EXCHANGE_SUPERVISOR_TIMER),
            osTimerOnce, NULL);
#endif
#ifdef __TWS_ROLE_SWITCH__
    waiting_bt_roleswitch_done_signal_from_master_timer =
        osTimerCreate(osTimer(WAITING_BT_ROLESWITCH_DONE_SIGNAL_FROM_MASTER_TIMER),
            osTimerOnce, NULL);
#endif
#ifdef __ENABLE_IN_BOX_STATE_HANDLE__
    charger_box_opened_connect_slave_timer =
        osTimerCreate(osTimer(CHARGER_BOX_OPENED_CONNECT_SLAVE_TIMER),
            osTimerOnce, NULL);
#endif
#ifdef __TWS_PAIR_DIRECTLY__
    app_tws_reconnect_slave_timer =
        osTimerCreate(osTimer(APP_TWS_RECONNECT_SLAVE_TIMER),
            osTimerOnce, NULL);
    app_tws_reconnect_master_timer =
    osTimerCreate(osTimer(APP_TWS_RECONNECT_MASTER_TIMER),
        osTimerOnce, NULL);

#endif

    pause_music_timer =
        osTimerCreate(osTimer(PAUSE_MUSIC_TIMER),osTimerOnce, NULL);

#ifdef __ENABLE_IN_BOX_STATE_HANDLE__
    box_closed_operation_delay_timer =
        osTimerCreate(osTimer(BOX_CLOSED_OPERATION_DELAY_TIMER),osTimerOnce, NULL);
#endif
#ifdef LBRT
    if (tws_lbrt_ping_timer == NULL)
        tws_lbrt_ping_timer = osTimerCreate (osTimer(TWS_LBRT_PING), osTimerOnce, NULL);

    if (tws_set_lbrt_timer == NULL)
        tws_set_lbrt_timer = osTimerCreate (osTimer(TWS_SERT_LBRT_TIMER), osTimerOnce, NULL);
#endif //LBRT
    // TODO: can config app_tws_ui_env here
}

static void app_tws_prepare_for_pairing(void)
{
    TRACE(" [ %s ] curr mode %d ", __func__,app_tws_get_mode());
    if (app_tws_is_freeman_mode())
    {
        app_bt_disconnect_all_mobile_devces();
    }
    else
    {
        if (a2dp_is_music_ongoing())
        {
            app_tws_close_tws_stream();
        }
        else
        {
            app_tws_clean_up_connections();
        }
    }
}

void app_tws_do_preparation_before_paring(void)
{
    set_conn_op_state_machine(CONN_OP_STATE_MACHINE_DISCONNECT_ALL_BEFORE_RE_PAIRING);

    app_stop_supervisor_timers();

    app_start_custom_function_in_bt_thread(0, 0, (uint32_t)app_tws_prepare_for_pairing);
}

void app_tws_pairing_done(void)
{
    app_tws_ui_set_reboot_to_pairing(false);
    nv_record_flash_flush();
    if (app_tws_ui_env.app_tws_pairing_done_cb)
    {
        app_start_custom_function_in_bt_thread(0, 0,
            (uint32_t)app_tws_ui_env.app_tws_pairing_done_cb);
    }
}

void app_tws_pairing_preparation_done(void)
{
    if (app_tws_ui_env.app_tws_pairing_preparation_done_cb)
    {
        app_start_custom_function_in_bt_thread(0, 0,
            (uint32_t)app_tws_ui_env.app_tws_pairing_preparation_done_cb);
    }
}

#ifdef __TWS_ROLE_SWITCH__
void app_tws_force_change_role(void)
{
    // change role to master
    app_tws_switch_role_in_nv();

    // change BT address
    if (app_tws_is_master_mode())
    {
        //app_bt_cancel_ongoing_connecting();
        btif_me_set_bt_address(tws_mode_ptr()->masterAddr.address) ;// ME_SetBtAddress(tws_mode_ptr()->masterAddr.addr);
    }
    else if (app_tws_is_slave_mode())
    {
        btif_me_set_bt_address(tws_mode_ptr()->slaveAddr.address);
    }
}

void app_tws_switch_role_and_reconnect_for_tws_roleswitch(void)
{
    app_tws_role_switch_clear_indexes();
    app_tws_switch_role_in_nv();
    set_conn_op_state_machine(CONN_OP_STATE_MACHINE_NORMAL);
    if (app_tws_is_master_mode())
    {
        conn_start_connecting_slave(CONNECTING_SLAVE_TIMEOUT_RECONNECT_IN_MS, true);
    }
    else if (app_tws_is_slave_mode())
    {
        conn_start_connecting_master(CONNECTING_SLAVE_TIMEOUT_RECONNECT_IN_MS);
    }
}

void app_tws_switch_role_and_reconnect_for_tws_forcing_roleswitch(void)
{
    app_tws_role_switch_clear_indexes();
    app_tws_force_change_role();
    set_conn_op_state_machine(CONN_OP_STATE_MACHINE_NORMAL);
    if (app_tws_is_master_mode())
    {
        conn_start_connecting_slave(CONNECTING_SLAVE_TIMEOUT_RECONNECT_IN_MS, true);
    }
    else if (app_tws_is_slave_mode())
    {
        conn_start_connectable_only_pairing(BT_SOURCE_CONN_MASTER);
    }
}
#endif

//#define __DEBUG_MODE_SIMPLEPAIRING_DISABLE__ (1)

void app_tws_send_shared_tws_info(void)
{
    app_tws_shared_tws_info_t sharedInfo;

    memcpy(sharedInfo.bleBdAddr, appm_get_current_ble_addr(), BTIF_BD_ADDR_SIZE);

    char key[BTIF_BD_ADDR_SIZE+1];
    int getint;
    memcpy(key, tws_mode_ptr()->slaveAddr.address, BTIF_BD_ADDR_SIZE);
    getint = nvrec_config_get_int(nv_record_config.config, section_name_ddbrec, (char *)key, 0);
#ifdef __DEBUG_MODE_SIMPLEPAIRING_DISABLE__
    if (!getint){
        btif_device_record_t record;
        memcpy(record.bdAddr.address, tws_mode_ptr()->slaveAddr.address, BTIF_BD_ADDR_SIZE);
        btif_sec_add_device_record(&record);
        getint = nvrec_config_get_int(nv_record_config.config, section_name_ddbrec, (char *)key, 0);
    }
#endif
    ASSERT(getint != 0, "The tws slave is not in NV!");

    int masterDevRecGetInt;
    memcpy(key, tws_mode_ptr()->masterAddr.address, BTIF_BD_ADDR_SIZE);
    masterDevRecGetInt = nvrec_config_get_int(nv_record_config.config,section_name_ddbrec,(char *)key,0);

    bool isDeviceInNv = true;
    if (0 == masterDevRecGetInt)
    {
        isDeviceInNv = false;
    }
    else
    {
        // bd address exists but the link key doesn't match
        if (memcmp(((btif_device_record_t *)masterDevRecGetInt)->linkKey,
                ((btif_device_record_t *)getint)->linkKey, 16))
        {
            isDeviceInNv = false;
        }
    }

    if (!isDeviceInNv)
    {
        TRACE("The TWS master device is not in the nv record, save it!");
        btif_device_record_t twsMasterDevRecord;
        twsMasterDevRecord = *(btif_device_record_t *)getint;
        memcpy(twsMasterDevRecord.bdAddr.address, tws_mode_ptr()->masterAddr.address, BTIF_BD_ADDR_SIZE);
        btif_sec_add_device_record(&twsMasterDevRecord);//  SEC_AddDeviceRecord(&twsMasterDevRecord);

        nv_record_flash_flush();
    }

    TRACE("Send tws info to the peer TWS slave.");

    app_tws_send_cmd_without_rsp(APP_TWS_CMD_SHARE_TWS_INFO, (uint8_t *)&sharedInfo, sizeof(sharedInfo));
}

bool app_tws_is_mobile_dev_in_nv(uint8_t* pMobileAddr, int* ptrGetInt)
{
    char key[BD_ADDR_SIZE+1];
    int getint;

    memcpy(key, pMobileAddr, BTIF_BD_ADDR_SIZE);
    getint = nvrec_config_get_int(nv_record_config.config, section_name_ddbrec, (char *)key, 0);

    if (0 == getint)
    {
        TRACE("The mobile dev is not in the nv record:");
        DUMP8("0x%02x ", pMobileAddr, BTIF_BD_ADDR_SIZE);
        *ptrGetInt = 0;
        return false;
    }
    else
    {
        *ptrGetInt= getint;
        return true;
    }
}

void app_tws_send_shared_mobile_dev_info(void)
{
    app_tws_shared_mobile_info_t sharedInfo;

    if (IS_CONNECTED_WITH_MOBILE_PROFILE())
    {
        int getint;
        if (!app_tws_is_mobile_dev_in_nv(mobileDevBdAddr.address, &getint))
        {
            TRACE("Cannot find the peer remote device in the nv record!");
            return;
        }

        sharedInfo.peerRemoteDevInfo = *(nvrec_btdevicerecord *)getint;
        TRACE("loaded key is:");
        DUMP8("%x ", sharedInfo.peerRemoteDevInfo.record.linkKey, 16);
    }

    char key[BD_ADDR_SIZE+1];
    int getint;
    memcpy(key, tws_mode_ptr()->slaveAddr.address, BTIF_BD_ADDR_SIZE);
    getint = nvrec_config_get_int(nv_record_config.config, section_name_ddbrec, (char *)key, 0);
#ifdef __DEBUG_MODE_SIMPLEPAIRING_DISABLE__
    if (!getint){
        btif_device_record_t record;
        memcpy(record.bdAddr.address, tws_mode_ptr()->slaveAddr.address, BTIF_BD_ADDR_SIZE);
        btif_sec_add_device_record(&record);
        getint = nvrec_config_get_int(nv_record_config.config, section_name_ddbrec, (char *)key, 0);
    }
#endif
    ASSERT(getint != 0, "The tws slave is not in NV!");

    int masterDevRecGetInt;
    memcpy(key, tws_mode_ptr()->masterAddr.address, BTIF_BD_ADDR_SIZE);
    masterDevRecGetInt = nvrec_config_get_int(nv_record_config.config,section_name_ddbrec,(char *)key,0);

    bool isDeviceInNv = true;
    if (0 == masterDevRecGetInt)
    {
        isDeviceInNv = false;
    }
    else
    {
        // bd address exists but the link key doesn't match
        if (memcmp(((btif_device_record_t *)masterDevRecGetInt)->linkKey,
                ((btif_device_record_t *)getint)->linkKey, 16))
        {
            isDeviceInNv = false;
        }
    }

    if (!isDeviceInNv)
    {
        TRACE("The TWS master device is not in the nv record, save it!");
        btif_device_record_t twsMasterDevRecord;
        twsMasterDevRecord = *(btif_device_record_t *)getint;
        memcpy(twsMasterDevRecord.bdAddr.address, tws_mode_ptr()->masterAddr.address, BTIF_BD_ADDR_SIZE);
       // SEC_AddDeviceRecord(&twsMasterDevRecord);
        btif_sec_add_device_record(&twsMasterDevRecord);
        app_audio_switch_flash_flush_req();

        nv_record_flash_flush();
    }

    TRACE("Send paired mobile device's info to peer TWS slave.");

    app_tws_send_cmd_without_rsp(APP_TWS_CMD_SHARE_MOBILE_DEV_INFO, (uint8_t *)&sharedInfo, sizeof(sharedInfo));
}

#ifdef __ENABLE_IN_BOX_STATE_HANDLE__
void app_tws_inform_in_box_state(void)
{
    TRACE("Send inbox state to peer TWS device.");
    app_tws_inbox_state_t inBoxState;
    inBoxState.isInBox = app_is_in_charger_box();
    inBoxState.isBoxOpened = !app_is_charger_box_closed();
    app_tws_send_cmd_without_rsp(APP_TWS_CMD_TELL_IN_BOX_STATE, (uint8_t *)&inBoxState, sizeof(inBoxState));
}

//trigger TWS role switch
void app_tws_simu_master_box_close()
{
    if (app_tws_is_master_mode())
    {
            app_set_in_charger_box_flag(true);
            app_set_charger_box_closed_flag(true);

            app_tws_inform_in_box_state();
    }
}

static void app_tws_inbox_state_received_handler(uint8_t* ptrParam, uint32_t paramLen)
{
    app_tws_inbox_state_t* inBoxState = (app_tws_inbox_state_t *)ptrParam;
    TRACE("Get peer device inbox info: inbox %d boxOpened:%d",
        inBoxState->isInBox, inBoxState->isBoxOpened);
    if(inBoxState->isInBox && !inBoxState->isBoxOpened)
    {
        slave_inbox_and_closed_flag= 1;
    }
    else
    {
        slave_inbox_and_closed_flag = 0;
    }
    TRACE("slave_inbox_and_closed = %d",slave_inbox_and_closed_flag);

#if !CLOSE_BOX_TO_TRIGGER_ROLE_SWITCH
    if (inBoxState->isInBox ^ app_is_in_charger_box())
    {
        if (app_is_in_charger_box() && (app_tws_is_master_mode()))
        {
            TRACE("The peer TWS slave is out of charger box while the master is inbox.");
            TRACE("Start role switch.");
            app_start_custom_function_in_bt_thread(RESTORE_THE_ORIGINAL_PLAYING_STATE,
                    0, (uint32_t)app_tws_kickoff_switching_role);
        }
        else if (!app_is_in_charger_box() && (app_tws_is_slave_mode()))
        {
            TRACE("The peer TWS master is put in charger box while the slave is out of box.");
            app_tws_inform_in_box_state();
            TRACE("Tell master that the slave is out of box, to let it start the role-switch.");
        }
    }
#else
    //TODO:use the tws_is_slave_mode()?
    if (app_tws_is_slave_mode())
    {
        app_tws_inform_in_box_state();
    }
    else
    {
        osTimerStop(chargerbox_closed_event_exchange_supervisor_timer);

        if (app_is_charger_box_closed())
        {
            if (!(inBoxState->isInBox)||(inBoxState->isBoxOpened))
            {
                TRACE("The peer TWS slave is out of charger box while the master is inbox and the box is closed.");
                TRACE("Start role switch.");
                app_start_custom_function_in_bt_thread(RESTORE_THE_ORIGINAL_PLAYING_STATE,
                        0, (uint32_t)app_tws_kickoff_switching_role);
            }
            else
            {
                if (!app_tws_ui_env.isPeerDevRoleSwitchInProgress &&
                    !app_tws_is_role_switching_on())
                {
                    if ((CONN_OP_STATE_MACHINE_NORMAL != CURRENT_CONN_OP_STATE_MACHINE()) &&
                        (CONN_OP_STATE_MACHINE_DISCONNECT_ALL != CURRENT_CONN_OP_STATE_MACHINE())){
                        TRACE("!!WARNING!! NOT CONN_OP_STATE_MACHINE_NORMAL currstate:%d", CURRENT_CONN_OP_STATE_MACHINE());
                    }
                    if (CONN_OP_STATE_MACHINE_DISCONNECT_ALL != CURRENT_CONN_OP_STATE_MACHINE()){
                        TRACE("try to app_tws_execute_charger_box_closed_operation");
                        app_tws_execute_charger_box_closed_operation();
                    }
                }
            }
        }else{
            if (!inBoxState->isBoxOpened){
                bool direct_disconnect_slave = true;
                a2dp_stream_t *src;
                if (app_tws_is_role_switching_on() || app_tws_ui_env.isPeerDevRoleSwitchInProgress){
                    TRACE("!!WARNING!! The peer TWS slave RoleSwitchInProgress");
                }
                if ((CONN_OP_STATE_MACHINE_NORMAL != CURRENT_CONN_OP_STATE_MACHINE()) &&
                    (CONN_OP_STATE_MACHINE_DISCONNECT_SLAVE != CURRENT_CONN_OP_STATE_MACHINE())){
                    TRACE("!!WARNING!! NOT CONN_OP_STATE_MACHINE_NORMAL currstate:%d", CURRENT_CONN_OP_STATE_MACHINE());
                }

                if ((IS_CONNECTED_WITH_TWS()) &&
                    (CONN_OP_STATE_MACHINE_DISCONNECT_SLAVE != CURRENT_CONN_OP_STATE_MACHINE()))
                {
                    set_conn_op_state_machine(CONN_OP_STATE_MACHINE_DISCONNECT_SLAVE);
                    TRACE("try to close slave source_status:%d src:%02x remDev:%08x", app_tws_get_source_stream_connect_status(),
                                                                                        app_tws_get_tws_source_stream(),
                                                                                        btif_a2dp_get_stream_devic_cmgrHandler_remdev(app_tws_get_tws_source_stream()));
                    if (app_tws_get_source_stream_connect_status())
                    {
                        src = app_tws_get_tws_source_stream();
                        if(src && btif_a2dp_get_stream_devic_cmgrHandler_remdev(src))
                        {
                            TRACE("try to close slave by A2DP_CloseStream");
                            direct_disconnect_slave = false;
                            log_event_2(EVENT_DISCONNECT_A2DP, BT_FUNCTION_ROLE_SLAVE,
                                3& btif_me_get_remote_device_hci_handle(slaveBtRemDev));
                            btif_a2dp_close_stream(src);
                        }
                    }

                    if (direct_disconnect_slave)
                    {
                        TRACE("try to close slave by disconnect_link");
                        app_tws_disconnect_slave();
                    }
                }
            }
        }
    }
#endif
}

TWS_CUSTOM_COMMAND_TO_ADD(APP_TWS_CMD_TELL_IN_BOX_STATE,   app_tws_inbox_state_received_handler);
#endif//__ENABLE_IN_BOX_STATE_HANDLE__

static void app_tws_slave_receive_tws_info_handler(uint8_t* ptrParam, uint32_t paramLen)
{
    app_tws_shared_tws_info_t* ptInfo = (app_tws_shared_tws_info_t *)ptrParam;
    bool isFlushNv = false;
    char key[BD_ADDR_SIZE+1] = {0,};
    int getint;

    memcpy(key, tws_mode_ptr()->masterAddr.address, BTIF_BD_ADDR_SIZE);
    getint = nvrec_config_get_int(nv_record_config.config,section_name_ddbrec,(char *)key,0);
#ifdef __DEBUG_MODE_SIMPLEPAIRING_DISABLE__
    if (!getint){
        btif_device_record_t record;
        memcpy(record.bdAddr.address, tws_mode_ptr()->masterAddr.address, BTIF_BD_ADDR_SIZE);
        btif_sec_add_device_record(&record);
        getint = nvrec_config_get_int(nv_record_config.config, section_name_ddbrec, (char *)key, 0);
    }
#endif
    if (0 == getint)
    {
        ASSERT(0, "The received peer TWS master device is not in the nv record!");
    }
    else
    {
        int slaveDevRecGetInt;
        memcpy(key, tws_mode_ptr()->slaveAddr.address, BTIF_BD_ADDR_SIZE);
        slaveDevRecGetInt = nvrec_config_get_int(nv_record_config.config,section_name_ddbrec,(char *)key,0);

        bool isDeviceInNv = true;
        if (0 == slaveDevRecGetInt)
        {
            isDeviceInNv = false;
        }
        else
        {
            // bd address exists but the link key doesn't match
            if (memcmp(((btif_device_record_t *)slaveDevRecGetInt)->linkKey,
                    ((btif_device_record_t *)getint)->linkKey, 16))
            {
                isDeviceInNv = false;
            }
        }

        if (!isDeviceInNv)
        {
            TRACE("The TWS slave device is not in the nv record, save it!");
            btif_device_record_t twsSlaveDevRecord;
            twsSlaveDevRecord = *(btif_device_record_t *)getint;
            memcpy(twsSlaveDevRecord.bdAddr.address, tws_mode_ptr()->slaveAddr.address, BTIF_BD_ADDR_SIZE);
            btif_sec_add_device_record(&twsSlaveDevRecord);
            TRACE("Store peer ble addr to nv");
            app_tws_store_peer_ble_addr_to_nv(ptInfo->bleBdAddr);
            isFlushNv = true;
        }
    }

    if (isFlushNv)
    {
        TRACE("Flush nv recorder.");
        struct nvrecord_env_t *nvrecord_env;
        nv_record_env_get(&nvrecord_env);
        nv_record_env_set(nvrecord_env);

    #if FPGA==0
        app_audio_switch_flash_flush_req();
        nv_record_flash_flush();
    #endif
    }

    app_tws_ble_addr_t rsp;
    memcpy(rsp.bleBdAddr, appm_get_current_ble_addr(), BTIF_BD_ADDR_SIZE);

    TRACE("Send ble addr of the slave.");

    app_tws_send_cmd_without_rsp(APP_TWS_CMD_RESPONSE_BLE_ADDR, (uint8_t *)&rsp, sizeof(rsp));
}

TWS_CUSTOM_COMMAND_TO_ADD(APP_TWS_CMD_SHARE_TWS_INFO, app_tws_slave_receive_tws_info_handler);

extern btif_remote_device_t* mobileBtRemDev;

static void app_tws_slave_receive_mobile_dev_info_handler(uint8_t* ptrParam, uint32_t paramLen)
{
    app_tws_shared_mobile_info_t* ptInfo = (app_tws_shared_mobile_info_t *)ptrParam;
    bool isFlushNv = false;
    char key[BD_ADDR_SIZE+1] = {0,};
    int getint;

    TRACE("Mobile's BT BD address is:");
    DUMP8("0x%02x ", ptInfo->peerRemoteDevInfo.record.bdAddr.address, BTIF_BD_ADDR_SIZE);

    memcpy(key, ptInfo->peerRemoteDevInfo.record.bdAddr.address,
        BTIF_BD_ADDR_SIZE);
    getint = nvrec_config_get_int(nv_record_config.config,section_name_ddbrec,(char *)key,0);

    bool isDeviceInNv = true;
    if (0 == getint)
    {
        isDeviceInNv = false;
    }
    else
    {
        // bd address exists but the link key doesn't match
        if (memcmp(ptInfo->peerRemoteDevInfo.record.linkKey,
                ((btif_device_record_t *)getint)->linkKey, 16))
        {
            isDeviceInNv = false;
        }
    }

    if (!isDeviceInNv)
    {
        TRACE("The received peer remote device is not in the nv record, save it!");
        btif_sec_add_device_record(&(ptInfo->peerRemoteDevInfo.record));
        isFlushNv = true;

        getint = nvrec_config_get_int(nv_record_config.config,section_name_ddbrec,(char *)key,0);
    }

    nvrec_btdevicerecord* nvrec_source = &(ptInfo->peerRemoteDevInfo);
    nvrec_btdevicerecord* nvrec_pool_record = (nvrec_btdevicerecord *)getint;

    if (conn_isA2dpVolConfiguredByMaster())
    {
        nvrec_source->device_vol.a2dp_vol = app_bt_stream_local_volume_get();
        nvrec_source->device_vol.hfp_vol = app_bt_stream_local_volume_get();
    }

    if (memcmp(&(nvrec_pool_record->device_vol), &(nvrec_source->device_vol), sizeof(nvrec_source->device_vol)) ||
        memcmp(&(nvrec_pool_record->device_plf), &(nvrec_source->device_plf), sizeof(nvrec_source->device_plf)))
    {
        isFlushNv = true;
    }

    nvrec_pool_record->device_vol = nvrec_source->device_vol;
    nvrec_pool_record->device_plf = nvrec_source->device_plf;

    TRACE("%s hfp_act:%d hsp_act:%d a2dp_act:0x%x ", __func__, nvrec_pool_record->device_plf.hfp_act,
        nvrec_pool_record->device_plf.hsp_act, nvrec_pool_record->device_plf.a2dp_act);

    memcpy(key, tws_mode_ptr()->masterAddr.address, BTIF_BD_ADDR_SIZE);
    getint = nvrec_config_get_int(nv_record_config.config,section_name_ddbrec,(char *)key,0);
#ifdef __DEBUG_MODE_SIMPLEPAIRING_DISABLE__
    if (!getint){
        btif_device_record_t record;
        memcpy(record.bdAddr.addr, tws_mode_ptr()->masterAddr.address, BTIF_BD_ADDR_SIZE);
        btif_sec_add_device_record(&record);
        getint = nvrec_config_get_int(nv_record_config.config, section_name_ddbrec, (char *)key, 0);
    }
#endif
    if (0 == getint)
    {
        ASSERT(0, "The received peer TWS master device is not in the nv record!");
    }
    else
    {
        int slaveDevRecGetInt;
        memcpy(key, tws_mode_ptr()->slaveAddr.address, BD_ADDR_SIZE);
        slaveDevRecGetInt = nvrec_config_get_int(nv_record_config.config,section_name_ddbrec,(char *)key,0);

        bool isDeviceInNv = true;
        if (0 == slaveDevRecGetInt)
        {
            isDeviceInNv = false;
        }
        else
        {
            // bd address exists but the link key doesn't match
            if (memcmp(((btif_device_record_t *)slaveDevRecGetInt)->linkKey,
                    ((btif_device_record_t *)getint)->linkKey, 16))
            {
                isDeviceInNv = false;
            }
        }

        if (!isDeviceInNv)
        {
            TRACE("The TWS slave device is not in the nv record, save it!");
            btif_device_record_t twsSlaveDevRecord;
            twsSlaveDevRecord = *(btif_device_record_t *)getint;
            memcpy(twsSlaveDevRecord.bdAddr.address, tws_mode_ptr()->slaveAddr.address, BTIF_BD_ADDR_SIZE);
            btif_sec_add_device_record(&twsSlaveDevRecord);
            isFlushNv = true;
        }
    }

    isMobileDevPaired = true;
    memcpy(mobileDevBdAddr.address, ptInfo->peerRemoteDevInfo.record.bdAddr.address, BTIF_BD_ADDR_SIZE);
#ifdef CHIP_BEST2300
    ASSERT(masterBtRemDev, "The ptr of master remote device error,please check!");
    if(masterBtRemDev){
        TRACE("%s,DUMP before set sniffer role env ",__func__);
        DUMP8("%02x ", (uint8_t *) btif_me_get_remote_device_bdaddr(masterBtRemDev)->address, BTIF_BD_ADDR_SIZE);
        btif_me_set_sniffer_env(1, SNIFFER_ROLE, (uint8_t *)btif_me_get_remote_device_bdaddr(masterBtRemDev)->address, tws_mode_ptr()->slaveAddr.address);
    }
#elif defined(CHIP_BEST2000)
    bt_drv_reg_op_afh_set_default();
    btif_me_set_sniffer_env(1, SNIFFER_ROLE, btif_me_get_remote_device_bdaddr(mobileBtRemDev)->address,
        tws_mode_ptr()->slaveAddr.address);
#endif

    if (isFlushNv)
    {
        TRACE("Flush nv recorder.");
        struct nvrecord_env_t *nvrecord_env;
        nv_record_env_get(&nvrecord_env);
        nv_record_env_set(nvrecord_env);

    #if FPGA==0
        app_audio_switch_flash_flush_req();
    #endif
    }

    app_tws_ble_addr_t rsp;
    rsp.isToUpdateNv = isFlushNv;
    memcpy(rsp.bleBdAddr, appm_get_current_ble_addr(), BD_ADDR_SIZE);

    TRACE("Send ble addr of the slave.");

    app_tws_send_cmd_without_rsp(APP_TWS_CMD_RESPONSE_BLE_ADDR, (uint8_t *)&rsp, sizeof(rsp));
}

TWS_CUSTOM_COMMAND_TO_ADD(APP_TWS_CMD_SHARE_MOBILE_DEV_INFO, app_tws_slave_receive_mobile_dev_info_handler);

static void app_tws_responsed_ble_addr_received_handler(uint8_t* ptrParam, uint32_t paramLen)
{
    stop_tws_sharing_tws_info_timer();
    stop_tws_sharing_pairing_info_timer();

    app_tws_ble_addr_t* ptInfo = (app_tws_ble_addr_t *)ptrParam;

    if (ptInfo->isToUpdateNv)
    {
        TRACE("Store peer ble addr to nv");
        app_tws_store_peer_ble_addr_to_nv(ptInfo->bleBdAddr);
    }
}

TWS_CUSTOM_COMMAND_TO_ADD(APP_TWS_CMD_RESPONSE_BLE_ADDR,   app_tws_responsed_ble_addr_received_handler);

#ifdef FPGA
static TWS_BOX_ENV_T twsBoxEnv_debug;
// left side
static uint8_t left_bdaddr[BTIF_BD_ADDR_SIZE] = {0x11, 0x66, 0x95, 0x33, 0x33, 0x33};
// right side
static uint8_t right_bdaddr[BTIF_BD_ADDR_SIZE] = {0x22, 0x66, 0x95, 0x33, 0x33, 0x33};


void app_tws_start_reset_pairing_debug(void)
{
    set_conn_op_state_machine(CONN_OP_STATE_MACHINE_RE_PAIRING);

    if (TWSFREE != twsBoxEnv_debug.twsModeToStart)
    {
        memcpy(tws_mode_ptr()->masterAddr.address, twsBoxEnv_debug.twsBtBdAddr[0], BTIF_BD_ADDR_SIZE);
        memcpy(tws_mode_ptr()->slaveAddr.address, twsBoxEnv_debug.twsBtBdAddr[1], BTIF_BD_ADDR_SIZE);
    }

    conn_remove_all_history_paired_devices();
    conn_stop_tws_connection_creation_supvervisor_timer();
    conn_stop_mobile_connection_creation_supvervisor_timer();

    set_conn_op_state(CONN_OP_STATE_IDLE);

    if (TWSMASTER == twsBoxEnv_debug.twsModeToStart)
    {
        memcpy(bt_addr, twsBoxEnv_debug.twsBtBdAddr[0], BTIF_BD_ADDR_SIZE);
        btif_me_set_bt_address(bt_addr);    
        nv_record_update_tws_mode(TWSMASTER);
        conn_start_connecting_slave(CONNECTING_SLAVE_TIMEOUT_FOR_FPGA,true);
    }
    else if (TWSSLAVE == twsBoxEnv_debug.twsModeToStart)
    {
        memcpy(bt_addr, twsBoxEnv_debug.twsBtBdAddr[1], BTIF_BD_ADDR_SIZE);
        btif_me_set_bt_address(bt_addr);
        nv_record_update_tws_mode(TWSSLAVE);
        conn_start_connectable_only_pairing(BT_SOURCE_CONN_MASTER);
    }
    else
    {
        btif_me_set_bt_address(bt_addr); 
        app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, BTIF_BAM_GENERAL_ACCESSIBLE,0,0);
        set_conn_op_state(CONN_OP_STATE_WAITING_FOR_MOBILE_CONNECTION);
        nv_record_update_tws_mode(TWSFREE);
    }
}

bool app_tws_pairing_request_handler_debug(uint8_t type)
{

    if (app_bt_conn_is_any_connection_on())
    {
        TRACE("%s, app_bt_conn_is_any_connection_on==true", __func__);
        return false;
    }
    
    if (type == TWSMASTER)
    {
        TRACE("Pairing request from charger box is received, start as the master.");
        twsBoxEnv_debug.twsModeToStart = TWSMASTER;
    }
    else if(type == TWSSLAVE)
    {
        TRACE("Pairing request from charger box is received, start as the slave.");
        twsBoxEnv_debug.twsModeToStart = TWSSLAVE;
    }
    else if(type == TWSFREE)
    {
        TRACE("Pairing request from charger box is received, start as the freeman.");
        twsBoxEnv_debug.twsModeToStart = TWSFREE;
    }

    if(type != TWSFREE)
    {
        memcpy(twsBoxEnv_debug.twsBtBdAddr[0], left_bdaddr, BD_ADDR_LEN);
        memcpy(twsBoxEnv_debug.twsBtBdAddr[1], right_bdaddr, BD_ADDR_LEN);
    }

    app_tws_start_reset_pairing_debug();

    return true;
}

void app_tws_simulate_pairing(uint8_t type)
{
    bool ret = app_tws_pairing_request_handler_debug(type);
    if (ret)
    {
        TRACE("Pairing is started");
    }
}

void simulate_hci_role_switch(void)
{
    btif_me_start_tws_role_switch(btif_me_get_remote_device_hci_handle(slaveBtRemDev),
        btif_me_get_remote_device_hci_handle(mobileBtRemDev));
}
#else
uint8_t left_bdaddr[BD_ADDR_SIZE] = {0x51, 0x33, 0x33, 0x22, 0x11, 0x11};
// right side
uint8_t right_bdaddr[BD_ADDR_SIZE] = {0x50, 0x33, 0x33, 0x22, 0x11, 0x11};

#if 1
void app_tws_simulate_pairing(void)
{
    TRACE("app_tws_simulate_pairing bt_addr[0]=0x%x",bt_addr[0]);
    if (bt_addr[0] %2 == 0)
    {
        memcpy(right_bdaddr,bt_addr,6);
        bt_addr[0] += 1;
        memcpy(left_bdaddr,bt_addr,6);
        bt_addr[0] -= 1;
    }
    else
    {
        memcpy(left_bdaddr,bt_addr,6);
        bt_addr[0] -= 1;
        memcpy(right_bdaddr,bt_addr,6);
        bt_addr[0] += 1;
    }
    DUMP8("%02x ", right_bdaddr, 6);
    DUMP8("%02x ", left_bdaddr, 6);
    app_tws_ui_set_reboot_to_pairing(true);
    //app_tws_start_pairing_inbox(TWS_FREEMAN_PAIRING, right_bdaddr, NULL);
    app_tws_start_pairing_inbox(TWS_DUAL_PAIRING, right_bdaddr, left_bdaddr);
    TRACE("Pairing is started");
}
#else
void app_tws_simulate_pairing(void)
{
    app_tws_ui_set_reboot_to_pairing(true);
    //app_tws_start_pairing_inbox(TWS_FREEMAN_PAIRING, right_bdaddr, NULL);
    app_tws_start_pairing_inbox(TWS_FREEMAN_PAIRING, left_bdaddr, NULL);
    TRACE("Freeman Pairing is started");
}
#endif

#ifdef LBRT
void app_tws_lbrt_simulate_pairing(void)
{
    app_tws_ui_set_reboot_to_pairing(true);
    app_tws_start_pairing_inbox(TWS_DUAL_PAIRING, right_bdaddr, left_bdaddr);
    app_tws_config_lbrt(0x80,1);
    app_set_lbrt_enable(true);

    TRACE("LBRT Pairing is started");
}
#endif

void app_tws_freeman_simulate_pairing(void)
{
    app_tws_ui_set_reboot_to_pairing(true);
    app_tws_start_pairing_inbox(TWS_FREEMAN_PAIRING, bt_addr, NULL);
    TRACE("Freeman Pairing is started");
}
#ifdef __TWS_PAIR_DIRECTLY__
void app_tws_slave_enter_pairing(void)
{
    app_tws_ui_set_reboot_to_pairing(true);
    app_tws_start_pairing_inbox(TWS_SLAVE_ENTER_PAIRING, NULL,bt_addr);
    TRACE("SLAVE is waiting for connec !");
}

void app_tws_master_enter_pairing(uint8_t* SlaveAddr)
{
    uint8_t original_btAddr[BD_ADDR_LEN];
    factory_section_original_btaddr_get(original_btAddr);
    app_tws_ui_set_reboot_to_pairing(true);
    app_tws_start_pairing_inbox(TWS_MASTER_CON_SLAVE, original_btAddr,SlaveAddr);
    TRACE("master connect slave !");
}
#endif//__TWS_PAIR_DIRECTLY__

#endif

#ifdef __ENABLE_WEAR_STATUS_DETECT__
//when sco start link the mic source will reset , recheck the weared status
void app_tws_check_if_need_to_switch_mic(void)
{
    if(app_tws_is_master_mode())
    {
        // send the wear status anyway in case this function is called right after the role switch
        app_tws_send_peer_earphones_wearing_status(true);
   }
}

typedef enum
{
    EARPHONE_WEARSTATUS_PUTON,
    EARPHONE_WEARSTATUS_PUTOFF,
    EARPHONE_WEARSTATUS_JUSTPUTON,
    EARPHONE_WEARSTATUS_JUSTPUTOFF,
} APP_EARPHONE_WEARSTATUS_E;

int app_tws_earphone_wearstatus_get(uint8_t isEarphoneweared)
{
    static uint8_t previous_isEarphoneweared = false;
    APP_EARPHONE_WEARSTATUS_E wearstatus;
    bool update = false;

    if (previous_isEarphoneweared != isEarphoneweared){
        update = true;
    }
    previous_isEarphoneweared = isEarphoneweared;

    if (update){
        if (previous_isEarphoneweared){
            wearstatus = EARPHONE_WEARSTATUS_JUSTPUTON;
        }else{
            wearstatus = EARPHONE_WEARSTATUS_JUSTPUTOFF;
        }
    }else{
        if (previous_isEarphoneweared){
            wearstatus = EARPHONE_WEARSTATUS_PUTON;
        }else{
            wearstatus = EARPHONE_WEARSTATUS_PUTOFF;
        }
    }

    return wearstatus;
}

int app_tws_peerearphone_wearstatus_get(uint8_t isPeerphoneweared)
{
    static uint8_t previous_isPeerphoneweared = false;
    APP_EARPHONE_WEARSTATUS_E wearstatus;
    bool update = false;

    if (previous_isPeerphoneweared != isPeerphoneweared){
        update = true;
    }
    previous_isPeerphoneweared = isPeerphoneweared;

    if (update){
        if (previous_isPeerphoneweared){
            wearstatus = EARPHONE_WEARSTATUS_JUSTPUTON;
        }else{
            wearstatus = EARPHONE_WEARSTATUS_JUSTPUTOFF;
        }
    }else{
        if (previous_isPeerphoneweared){
            wearstatus = EARPHONE_WEARSTATUS_PUTON;
        }else{
            wearstatus = EARPHONE_WEARSTATUS_PUTOFF;
        }
    }

    return wearstatus;
}

static void app_tws_runtime_status_received_handler(uint8_t* ptrParam, uint32_t paramLen)
{
    app_tws_runtime_status_t* pRuntimeStatus = (app_tws_runtime_status_t *)ptrParam;

    TRACE("Get the run-time status from the peer earphone. Wear %d role-switch %d inbox %d",
                                                            pRuntimeStatus->isEarphoneweared,
                                                            pRuntimeStatus->isRoleSwitchInProgress,
                                                            pRuntimeStatus->isInChargerBox);

    uint8_t formerWearStatusOfPeerDev = app_tws_ui_env.runtimeStatus.isPeerphoneweared;

    TRACE("Former peer wear status %d device wear status %d",
        formerWearStatusOfPeerDev, app_tws_ui_env.runtimeStatus.isWearedPreviously);

    app_tws_ui_env.isPeerDevRoleSwitchInProgress = pRuntimeStatus->isRoleSwitchInProgress;
    app_tws_ui_env.runtimeStatus.isPeerphoneweared = pRuntimeStatus->isEarphoneweared;

    bool peer_wearon_status = false;
    bool earphone_wearon_status = false;

    peer_wearon_status = app_tws_ui_env.runtimeStatus.isPeerphoneweared;
    earphone_wearon_status = app_tws_ui_env.runtimeStatus.isEarphoneweared;

    TRACE("isEarphoneweared:%d isPeerphoneweared:%d", earphone_wearon_status, peer_wearon_status);
    TRACE("app_tws_get_mode()=%d", app_tws_get_mode());
    int earphone_wearstatus = app_tws_earphone_wearstatus_get(app_tws_ui_env.runtimeStatus.isEarphoneweared);
    int peerearphone_wearstatus = app_tws_peerearphone_wearstatus_get(pRuntimeStatus->isEarphoneweared);

    if(app_tws_is_master_mode())
    {
        TRACE("%s mobile:%d,  isInMobileBtSilentState:%d", __func__, IS_CONNECTED_WITH_MOBILE(), isInMobileBtSilentState);
        if (isInMobileBtSilentState && app_tws_ui_env.runtimeStatus.isEarphoneweared &&
            (!IS_CONNECTING_MOBILE()) && (!IS_CONNECTED_WITH_MOBILE()))
        {
            app_start_custom_function_in_bt_thread(0,
                0, (uint32_t)conn_start_connecting_mobile_device);
        }
    }

    // 1. both the master and the slave are weared, master on and slave off
    if (app_tws_ui_env.runtimeStatus.isEarphoneweared &&
        app_tws_ui_env.runtimeStatus.isPeerphoneweared)
    {
#ifdef __ENABLE_IN_BOX_STATE_HANDLE__
        if (app_is_in_charger_box())
        {
            app_tws_switch_source_mic(true);
        }
        else
        {
            if (pRuntimeStatus->isInChargerBox)
            {
                app_tws_switch_source_mic(false);
            }
            else
            {
                if (app_tws_is_master_mode())
                {
                    app_tws_switch_source_mic(false);
                }
                else
                {
                    app_tws_switch_source_mic(true);
                }
            }
        }
#else
        if (app_tws_is_master_mode())
        {
            app_tws_switch_source_mic(false);
        }
        else
        {
            app_tws_switch_source_mic(true);
        }
#endif
    }
    // 2. both the master and the slave are not weared, both off
    else if (!app_tws_ui_env.runtimeStatus.isEarphoneweared &&
        !app_tws_ui_env.runtimeStatus.isPeerphoneweared)
    {
        if (app_tws_is_master_mode())
        {
            app_tws_switch_source_mic(false);
        }
        else
        {
            app_tws_switch_source_mic(true);
        }
    }
    // 3. one of the tws is weared, turn on the one weared
    else
    {
        if (app_tws_ui_env.runtimeStatus.isEarphoneweared)
        {
            TRACE("I am wear on, mic switch me");
            app_tws_switch_source_mic(false);
        }
        else
        {
            TRACE("I am wear off, mic switch peer");
            app_tws_switch_source_mic(true);
        }
    }

    if (app_tws_is_master_mode())
    {
        TRACE("loc_ep_weared:%d/%d rmt_ep_weared:%d/%d",app_tws_ui_env.runtimeStatus.isEarphoneweared, earphone_wearstatus,
                                                        pRuntimeStatus->isEarphoneweared, peerearphone_wearstatus);

        if (app_tws_ui_env.runtimeStatus.isEarphoneweared && pRuntimeStatus->isEarphoneweared)
        {
            //TRACE("dual earphone wear on , play!!!!!!!!!!!!!!!");
            //app_tws_control_music_playing(TWS_MASTER_MUSIC_CONTROL_TYPE_PLAY);
        }
        else if ((earphone_wearstatus == EARPHONE_WEARSTATUS_JUSTPUTOFF) || (peerearphone_wearstatus == EARPHONE_WEARSTATUS_JUSTPUTOFF))
        {
            TRACE("dual earphone not wear on , pause!!!!!!!!!!!!!!!");
            app_tws_control_music_playing(TWS_MASTER_MUSIC_CONTROL_TYPE_PAUSE);
        }
    }

    if (pRuntimeStatus->isInitialReq)
    {
        app_tws_send_peer_earphones_wearing_status(false);
    }
}

TWS_CUSTOM_COMMAND_TO_ADD(APP_TWS_CMD_SEND_RUNTIME_STATUS, app_tws_runtime_status_received_handler);
#endif//__ENABLE_WEAR_STATUS_DETECT__

void app_tws_control_music_playing(TWS_MASTER_MUSIC_CONTROL_TYPE_E controlType)
{
    if (IS_CONN_STATE_A2DP_ON(mobileDevConnectionState) && (app_tws_is_master_mode()) && (!btapp_hfp_call_is_present()))
    {
        if ((TWS_MASTER_MUSIC_CONTROL_TYPE_PAUSE == controlType))
        {
            app_tws_control_avrcp_media(BTIF_AVRCP_MEDIA_PAUSED);
        }
        else if ((TWS_MASTER_MUSIC_CONTROL_TYPE_PLAY == controlType))
        {
            app_tws_control_avrcp_media(BTIF_AVRCP_MEDIA_PLAYING);
        }
    }
}

static void app_tws_music_playing_control_handler(uint8_t* ptrParam, uint32_t paramLen)
{
    app_tws_music_playing_control_t* pReq = (app_tws_music_playing_control_t *)ptrParam;
    if (pReq->isEnableMusicPlaying)
    {
        app_tws_control_music_playing(TWS_MASTER_MUSIC_CONTROL_TYPE_PLAY);
    }
    else
    {
        app_tws_control_music_playing(TWS_MASTER_MUSIC_CONTROL_TYPE_PAUSE);
    }
}
TWS_CUSTOM_COMMAND_TO_ADD(APP_TWS_CMD_CONTROL_MUSIC_PLAYING, app_tws_music_playing_control_handler);

void app_tws_let_master_control_music_playing(bool isEnable)
{
    app_tws_music_playing_control_t req;
    req.isEnableMusicPlaying = isEnable;
    app_tws_send_cmd_without_rsp(APP_TWS_CMD_CONTROL_MUSIC_PLAYING, (uint8_t *)&req, sizeof(req));
}
#ifdef __ENABLE_WEAR_STATUS_DETECT__
void app_tws_syn_wearing_status(void)
{
    app_tws_send_peer_earphones_wearing_status(true);
}

bool app_tws_send_peer_earphones_wearing_status(bool isInitialReq)
{
    TRACE("Tell the peer earphone its wearing status.");

    if (app_tws_is_role_switching_on()){
        return true;
    }

    app_tws_runtime_status_t req;
    req.isEarphoneweared = app_tws_ui_env.runtimeStatus.isEarphoneweared;
    req.isRoleSwitchInProgress = app_tws_is_role_switching_on();
    req.isInitialReq = isInitialReq;
#ifdef __ENABLE_IN_BOX_STATE_HANDLE__
    req.isInChargerBox = app_is_in_charger_box();
#endif
    app_tws_send_cmd_without_rsp(APP_TWS_CMD_SEND_RUNTIME_STATUS, (uint8_t *)&req, sizeof(req));
    return true;
}
#endif//__ENABLE_WEAR_STATUS_DETECT__

static void app_tws_role_switch_done_received_handler(uint8_t* ptrParam, uint32_t paramLen)
{
    TRACE("Get siwtch done signal from the peer Dev.");
    app_tws_role_switch_done_t* pReq = (app_tws_role_switch_done_t *)ptrParam;

    app_tws_ui_env.isPeerDevRoleSwitchInProgress = pReq->isRoleSwitchInProgress;

    if (app_tws_is_slave_mode())
    {
        app_tws_inform_role_switch_done();
        bt_drv_reg_op_lsto_hack( btif_me_get_remote_device_hci_handle(masterBtRemDev)-0x80);
    }

    if (!app_tws_is_role_switching_on() && !pReq->isRoleSwitchInProgress)
    {
        TRACE("Both sides' role switch have been done!");

        if (app_tws_is_master_mode())
        {
#ifdef __ENABLE_WEAR_STATUS_DETECT__
            app_tws_syn_wearing_status();
#endif
            app_bt_get_mobile_device_active_profiles(mobileDevBdAddr.address);
            if(IS_MOBILE_DEV_PROFILE_ACTIVE(BT_PROFILE_A2DP)){
                SET_CONN_STATE_A2DP(mobileDevConnectionState);
            }
            if(IS_MOBILE_DEV_PROFILE_ACTIVE(BT_PROFILE_HFP)){
                SET_CONN_STATE_HFP(mobileDevConnectionState);
            }
            bt_drv_reg_op_lsto_hack(btif_me_get_remote_device_hci_handle(slaveBtRemDev)-0x80);
        }
#ifdef __ENABLE_IN_BOX_STATE_HANDLE__
        app_start_custom_function_in_bt_thread(0, 0,
                (uint32_t)app_tws_check_chargerbox_state_after_operation);
#endif
    }
    else
    {
        //inform again, if slave still in role switch progress
        if (app_tws_is_master_mode())
        {
            app_tws_inform_role_switch_done();
        }
    }
}

TWS_CUSTOM_COMMAND_TO_ADD(APP_TWS_INFORM_ROLE_SWITCH_DONE, app_tws_role_switch_done_received_handler);

bool app_tws_inform_role_switch_done(void)
{
    TRACE("Tell the peer earphone the role switch has been done.");

    app_tws_role_switch_done_t req;
    req.isEarphoneweared = app_tws_ui_env.runtimeStatus.isEarphoneweared;
    req.isRoleSwitchInProgress = app_tws_is_role_switching_on();

    app_tws_send_cmd_without_rsp(APP_TWS_INFORM_ROLE_SWITCH_DONE, (uint8_t *)&req, sizeof(req));
    return true;
}


static uint8_t app_tws_dest_AvrcpMediaStatus = BTIF_AVRCP_MEDIA_ERROR;
bool app_tws_control_avrcp_media_remote_status_onprocess(void)
{
    if (app_tws_dest_AvrcpMediaStatus == BTIF_AVRCP_MEDIA_ERROR)
     return false;
    else
     return true;
}

void app_tws_control_avrcp_media_remote_status_ind(uint8_t play_status)
{
    TRACE("%s, curr:%d dest:%d",__func__, play_status, app_tws_dest_AvrcpMediaStatus);

    if (app_tws_is_role_switching_on())
    {
        return;
    }

    if (play_status == BTIF_AVRCP_MEDIA_STOPPED)
    {
        return;
    }

    if (app_tws_dest_AvrcpMediaStatus == BTIF_AVRCP_MEDIA_ERROR){
        return;
    }

    if (btapp_hfp_call_is_present()){
        app_tws_dest_AvrcpMediaStatus = BTIF_AVRCP_MEDIA_ERROR;
        return;
    }

    if (play_status == BTIF_AVRCP_MEDIA_ERROR){
        app_tws_dest_AvrcpMediaStatus = BTIF_AVRCP_MEDIA_ERROR;
        return;
    }

    if ((play_status == BTIF_AVRCP_MEDIA_PLAYING) ||
        (play_status == BTIF_AVRCP_MEDIA_FWD_SEEK)||
        (play_status == BTIF_AVRCP_MEDIA_REV_SEEK)){
        play_status = BTIF_AVRCP_MEDIA_PLAYING;
    }


    if (app_tws_dest_AvrcpMediaStatus != play_status && play_status !=BTIF_AVRCP_MEDIA_STOPPED) {
        if (app_tws_dest_AvrcpMediaStatus == BTIF_AVRCP_MEDIA_PLAYING){
            a2dp_handleKey(AVRCP_KEY_PLAY);
        }else if (app_tws_dest_AvrcpMediaStatus == BTIF_AVRCP_MEDIA_PAUSED){
            a2dp_handleKey(AVRCP_KEY_PAUSE);
        }
    }else{
        app_tws_dest_AvrcpMediaStatus = BTIF_AVRCP_MEDIA_ERROR;
    }

    TRACE("%s,exit",__func__);
}

uint8_t avrcp_ctrl_music_flag = 0;
extern uint8_t avrcp_get_media_status(void);
extern void avrcp_set_media_status(uint8_t status);
static uint32_t avrcp_media_handler_prev_ticks = 0;
static uint32_t avrcp_media_handler_prev_AvrcpMediaStatus = 0;
extern bool is_sbc_play(void);
static void app_tws_ctrl_avrcp_music(uint8_t status)
{
    static uint8_t play_count = 0;
    static uint8_t pause_count = 0;

    TRACE("%s %d",__func__, status);
    if (status == AVRCP_KEY_PLAY)
    {
        play_count++;
        pause_count = 0;
        avrcp_ctrl_music_flag = 1;
    }
    else if (status == AVRCP_KEY_PAUSE)
    {
        pause_count++;
        play_count = 0;
        avrcp_ctrl_music_flag = 2;
    }
    a2dp_handleKey(status);
    if (pause_count < 5 && play_count < 5)
        osTimerStart(pause_music_timer, 2000);
}

static void app_tws_pause_music_timeout_handler(void const *param)
{
    uint8_t curr_media_status = avrcp_get_media_status();
    osTimerStop(pause_music_timer);
    TRACE("%s %d %d %d",__func__,avrcp_ctrl_music_flag, curr_media_status, is_sbc_play());
    if (curr_media_status == BTIF_AVRCP_MEDIA_PLAYING && !is_sbc_play())
    {
        avrcp_set_media_status(BTIF_AVRCP_MEDIA_PAUSED);
        curr_media_status = avrcp_get_media_status();
    }

    if (curr_media_status == BTIF_AVRCP_MEDIA_PLAYING && avrcp_ctrl_music_flag == 2)
        app_tws_ctrl_avrcp_music(AVRCP_KEY_PAUSE);
    else if (curr_media_status == BTIF_AVRCP_MEDIA_PAUSED && avrcp_ctrl_music_flag == 1)
        app_tws_ctrl_avrcp_music(AVRCP_KEY_PLAY);
}

int app_tws_control_avrcp_media_handler(uint8_t AvrcpMediaStatus)
{
    bool need_send_cmd = false;
    uint32_t curr_ticks = hal_sys_timer_get();
    uint32_t diff_ticks = hal_timer_get_passed_ticks(curr_ticks, avrcp_media_handler_prev_ticks);
    uint8_t media_status = avrcp_get_media_status();

    TRACE("************%s, dest:%d curr:%d pre:%d************",__func__, AvrcpMediaStatus, media_status, app_tws_dest_AvrcpMediaStatus);
    ASSERT(((AvrcpMediaStatus ==BTIF_AVRCP_MEDIA_PLAYING) || (AvrcpMediaStatus == BTIF_AVRCP_MEDIA_PAUSED)), "INVALID AvrcpMediaStatus:%d", AvrcpMediaStatus);

    if (diff_ticks < MS_TO_TICKS(1000) && (avrcp_media_handler_prev_AvrcpMediaStatus == AvrcpMediaStatus))
    {
        TRACE("skip 0\n");
        return 0;
    }
    avrcp_media_handler_prev_ticks = curr_ticks;
    avrcp_media_handler_prev_AvrcpMediaStatus = AvrcpMediaStatus;

    TRACE("%s(%d):%d", __func__, __LINE__, is_sbc_play());
    if (AvrcpMediaStatus == BTIF_AVRCP_MEDIA_PLAYING && is_sbc_play())
    {
        TRACE("skip 1");
        return 0;
    }

    if (app_tws_dest_AvrcpMediaStatus == AvrcpMediaStatus){
        TRACE("%s, skip 2",__func__);
        return 0;
    }

    if (btapp_hfp_call_is_present()){
        app_tws_dest_AvrcpMediaStatus = BTIF_AVRCP_MEDIA_ERROR;
        TRACE("%s, skip 3",__func__);
        return 0;
    }

    if ((media_status == BTIF_AVRCP_MEDIA_PLAYING) ||
        (media_status == BTIF_AVRCP_MEDIA_FWD_SEEK)||
        (media_status == BTIF_AVRCP_MEDIA_REV_SEEK)){
        media_status = BTIF_AVRCP_MEDIA_PLAYING;
    }


    if (media_status == AvrcpMediaStatus && media_status == BTIF_AVRCP_MEDIA_PLAYING)
    {
        if (is_sbc_play())
        {
            app_tws_dest_AvrcpMediaStatus = BTIF_AVRCP_MEDIA_ERROR;
            TRACE("%s, skip 4",__func__);
            return 0;
        }
    }

    if (media_status == AvrcpMediaStatus && media_status == BTIF_AVRCP_MEDIA_PAUSED)
    {
            app_tws_dest_AvrcpMediaStatus = BTIF_AVRCP_MEDIA_ERROR;
            TRACE("%s, skip 5",__func__);
            return 0;
    }

    if (media_status != 0xff){
        TRACE("get playstatus notify, direct op");
        need_send_cmd = true;
    }else{
        if (!avrcp_callback_getplaystatus()){
            TRACE("get playstatus pending, need more op");
            app_tws_dest_AvrcpMediaStatus = AvrcpMediaStatus;
        }else{
            TRACE("get playstatus not support, direct op");
            need_send_cmd = true;
        }
    }

    if (need_send_cmd){
        if (AvrcpMediaStatus == BTIF_AVRCP_MEDIA_PLAYING){
            TRACE("%s play media\n", __func__);
            app_tws_ctrl_avrcp_music(AVRCP_KEY_PLAY);
        }else if (AvrcpMediaStatus == BTIF_AVRCP_MEDIA_PAUSED){
            TRACE("%s pause media\n", __func__);
            app_tws_ctrl_avrcp_music(AVRCP_KEY_PAUSE);
        }
    }

    TRACE("%s,media_status=%d exit",__func__,app_tws_dest_AvrcpMediaStatus);

    return 0;
}

int app_tws_control_avrcp_media_reinit(void)
{
    avrcp_media_handler_prev_ticks = 0;
    avrcp_media_handler_prev_AvrcpMediaStatus = 0;
    app_tws_dest_AvrcpMediaStatus = BTIF_AVRCP_MEDIA_ERROR;

    return 0;
}

int app_tws_control_avrcp_media(uint8_t AvrcpMediaStatus)
{
    if (app_tws_is_role_switching_on())
    {
        return 0;
    }

    app_start_custom_function_in_bt_thread(AvrcpMediaStatus,
                            0, (uint32_t)app_tws_control_avrcp_media_handler);
    return 0;
}

#ifdef __ENABLE_WEAR_STATUS_DETECT__
void app_tws_update_wear_status(bool isWeared)
{
    TRACE("%s   %d  %d\n", __func__, isWeared, app_tws_ui_env.runtimeStatus.isEarphoneweared);
    if (isWeared != app_tws_ui_env.runtimeStatus.isEarphoneweared)
    {
        if (!isWeared)
        {
            log_event(EVENT_TAKEN_OFF);
        }
        else
        {
            log_event(EVENT_WEARED);
        }

        TRACE("Update wear status as %d", isWeared);
        app_tws_ui_env.runtimeStatus.isWearedPreviously =
            app_tws_ui_env.runtimeStatus.isEarphoneweared;

        app_tws_ui_env.runtimeStatus.isEarphoneweared = isWeared;

        if (app_tws_is_freeman_mode() || (app_tws_is_master_mode() && 1 == slave_inbox_and_closed_flag))
        {
            TRACE("I am free man or I am master but disconnt with slave");
            if (btapp_hfp_call_is_present())
            {
#ifdef __ENABLE_IN_BOX_STATE_HANDLE__
                if (!app_is_in_charger_box() && isWeared)
#else
                if (isWeared)
#endif
                {
                    TRACE("earphone wear on and out of chargerbox, mic switch to earphone");
                    hfp_handle_key(HFP_KEY_ADD_TO_EARPHONE);
                }
            }
            else
            {
                if (!isWeared)
                {
                    app_tws_control_avrcp_media(BTIF_AVRCP_MEDIA_PAUSED);
                }
            }
        }
        else
        {
            app_tws_send_peer_earphones_wearing_status(true);
        }
        if ((app_tws_is_freeman_mode()) || (app_tws_is_master_mode()))
        {
            TRACE("%s mobile:%d, isInMobileBtSilentState:%d", __func__,
                IS_CONNECTED_WITH_MOBILE(), isInMobileBtSilentState);
            if (isInMobileBtSilentState && isWeared &&
                (!IS_CONNECTING_MOBILE()) && (!IS_CONNECTED_WITH_MOBILE()))
            {
                app_start_custom_function_in_bt_thread(0,
                    0, (uint32_t)conn_start_connecting_mobile_device);
            }
        }
    }
}
#endif//__ENABLE_WEAR_STATUS_DETECT__

extern uint16_t sniff_sco_hcihandle;
static void app_tws_switch_source_mic(bool isMute)
{
#ifdef __TWS_CALL_DUAL_CHANNEL__
    if (app_tws_is_freeman_mode())
    {
        TRACE("%s TWSFREE", __func__);
        return;
    }

    uint16_t connHandle;

    if (app_tws_is_slave_mode())
    {
        if(masterBtRemDev)
        {
                connHandle = sniff_sco_hcihandle;
        }
        else
        {
            return;
        }
    }
    else
    {
        if(mobileBtRemDev)
        {
            connHandle = btif_me_get_remote_device_hci_handle(mobileBtRemDev);
        }
        else
        {
            return;
        }
    }

    if(connHandle<0x80 || connHandle>0x83)
        return;

    if (IS_CONNECTING_SLAVE()){
        if (isMute){
            TRACE("master connecting slaver so force TxSilence disable");
            isMute = false;
        }
    }
    TRACE("Mic isMute is %d,connHandle=%x", isMute,connHandle);

    log_event_1(EVENT_MIC_SWITCHED, !isMute);

   // ME_SetScoTxSilence(connHandle, isMute);
   btif_me_set_sco_tx_silence(connHandle, isMute);
#endif
}

static void app_tws_switch_source_mic_handler(uint8_t* ptrParam, uint32_t paramLen)
{
    app_tws_control_source_mic_t* req = (app_tws_control_source_mic_t *)ptrParam;
    app_tws_switch_source_mic(req->isMuteSourceMic);
}

TWS_CUSTOM_COMMAND_TO_ADD(APP_TWS_CMD_SWITCH_SOURCE_MIC, app_tws_switch_source_mic_handler);

static void app_tws_set_slave_volume_handler(uint8_t* ptrParam, uint32_t paramLen)
{
    app_tws_set_slave_volume_req_t* ptReq = (app_tws_set_slave_volume_req_t *)ptrParam;
    a2dp_volume_set(ptReq->a2dpVolume);
    hfp_volume_set(ptReq->hfpVolume);
}

TWS_CUSTOM_COMMAND_TO_ADD(APP_TWS_CMD_SET_SLAVE_VOLUME, app_tws_set_slave_volume_handler);

void app_tws_set_slave_volume(int32_t a2dpvolume)
{
    if (!app_tws_is_role_switching_on())
    {
        app_tws_set_slave_volume_req_t req;
        req.a2dpVolume = a2dpvolume;
        req.hfpVolume = hfp_volume_get();

        TRACE("Set slave's a2dp volume as %d, hfp volume as %d", req.a2dpVolume, req.hfpVolume);
        app_tws_send_cmd_without_rsp(APP_TWS_CMD_SET_SLAVE_VOLUME, (uint8_t *)&req, sizeof(req));
    }
}

#ifdef __ENABLE_IN_BOX_STATE_HANDLE__
void app_tws_put_in_charger_box_timer_process(void)
{
    TRACE("[IO] %s, isPutInChargerBoxCalled=%d, isOutOfChargerBoxCalled=%d",
          __func__, app_tws_ui_env.isPutInChargerBoxCalled, app_tws_ui_env.isOutOfChargerBoxCalled);

    if (!app_is_in_charger_box())
    {
        return;
    }

    TRACE("put in charger box.");

    if(!ble_tws_is_initialized())
    {
        TRACE("ble or bt not initialized! [%s] start timer ",__func__);
        dev_operation_start_check_timer(0,1,app_tws_put_in_charger_box_timer_process);
        return;
    }

    if (app_tws_is_role_switching_on())
    {
        TRACE("In role switch! [%s] start timer ",__func__);
        dev_operation_start_check_timer(0,1,app_tws_put_in_charger_box_timer_process);
        return;
    }

    if ((CONN_OP_STATE_MACHINE_DISCONNECT_ALL_BEFORE_CHANGE_ROLE == CURRENT_CONN_OP_STATE_MACHINE()) &&
        (CONN_OP_STATE_MACHINE_SIMPLE_REPAIRING == CURRENT_CONN_OP_STATE_MACHINE()))
    {
        TRACE("In state machine of %d", CURRENT_CONN_OP_STATE_MACHINE());
        dev_operation_start_check_timer(0,1,app_tws_put_in_charger_box_timer_process);
        return;
    }

    app_tws_inform_in_box_state();

    app_stop_auto_power_off_supervising();

    app_tws_start_normal_ble_activities_in_chargerbox();

    if (!app_is_charger_box_closed())
    {
        if (IS_NO_PENDING_CONNECTION())
        {
            conn_system_boot_up_handler();
        }
        else
        {
            TRACE("do something try to connect tws and mobile");
            if (!app_tws_is_freeman_mode())
            {
                if (!IS_CONNECTED_WITH_TWS_PROFILE())
                {
                    conn_ble_reconnection_machanism_handler(true);

                    // for slave, need to start connection creation supervisor timer to
                    // assure that it can switch to master and connect to mobile
                    if (app_tws_is_slave_mode())
                    {
                        conn_start_connectable_only_pairing(BT_SOURCE_CONN_MASTER);
                    }
                }
            }
        }
    }
}

void app_tws_put_in_charger_box(void)
{
    TRACE("[IO] --%s-- isPutInChargerBoxCalled=%d, isOutOfChargerBoxCalled=%d\n", __func__,
                                                                app_tws_ui_env.isPutInChargerBoxCalled,
                                                                app_tws_ui_env.isOutOfChargerBoxCalled);
    TRACE("isRebootToPairing %d", app_tws_ui_env.isRebootToPairing);

    log_event(EVENT_PUT_IN_BOX);

    if (app_tws_ui_env.isPutInChargerBoxCalled)
    {
        return;
    }

    if (app_tws_ui_env.isRebootToPairing)
    {
        app_set_in_charger_box_flag(true);
        return;
    }

    TRACE("put in charger box.");
    app_tws_ui_env.isPutInChargerBoxCalled = true;
    app_tws_ui_env.isOutOfChargerBoxCalled = false;

    app_set_in_charger_box_flag(true);

    TRACE(" [%s] start timer ",__func__);
    dev_operation_start_check_timer(0,0,app_tws_put_in_charger_box_timer_process);
}

void app_tws_out_of_charger_box_timer_process(void)
{
    TRACE("[IO] %s, isPutInChargerBoxCalled=%d, isOutOfChargerBoxCalled=%d",
          __func__, app_tws_ui_env.isPutInChargerBoxCalled, app_tws_ui_env.isOutOfChargerBoxCalled);

    if (app_is_in_charger_box())
    {
        return;
    }

    app_tws_earphone_wearstatus_get(false);
    app_tws_peerearphone_wearstatus_get(false);

    if(!ble_tws_is_initialized())
    {
        TRACE("ble or bt not initialized! [%s] start timer ",__func__);
        dev_operation_start_check_timer(1,1,app_tws_out_of_charger_box_timer_process);
        return;
    }

    if (app_tws_is_role_switching_on())
    {
        TRACE("In role switch! [%s] start timer ",__func__);
        dev_operation_start_check_timer(1,1,app_tws_out_of_charger_box_timer_process);
        return;
    }

    if ((CONN_OP_STATE_MACHINE_DISCONNECT_ALL_BEFORE_CHANGE_ROLE == CURRENT_CONN_OP_STATE_MACHINE()) &&
        (CONN_OP_STATE_MACHINE_SIMPLE_REPAIRING == CURRENT_CONN_OP_STATE_MACHINE()))
    {
        TRACE("In state machine of %d", CURRENT_CONN_OP_STATE_MACHINE());
        dev_operation_start_check_timer(1,1,app_tws_out_of_charger_box_timer_process);
        return;
    }

#if FPGA==0
    if ((app_tws_is_master_mode()) || \
        (app_tws_is_freeman_mode()))
    {
        if (!IS_CONNECTED_WITH_MOBILE())
        {
            app_start_custom_function_in_bt_thread(0, 0, (uint32_t)app_start_auto_power_off_supervising);
        }
    }
    else
    {
        if (!IS_CONNECTED_WITH_TWS())
        {
            app_start_custom_function_in_bt_thread(0, 0, (uint32_t)app_start_auto_power_off_supervising);
        }
    }
#endif

    app_tws_event_exchange_supervisor_timer_cancel();

    set_conn_op_state_machine(CONN_OP_STATE_MACHINE_NORMAL);

#if BLE_TWS_USE_BLE_AS_INBOX_COMMUNICATION_PATH
    ble_tws_stop_all_activities();
#else
    app_start_ble_tws_out_of_box_adv();
#endif

    // tell the peer tws that I am out
    app_tws_inform_in_box_state();

    if (IS_NO_PENDING_CONNECTION())
    {
        conn_system_boot_up_handler();
    }
    else
    {
        TRACE("do something try to connect tws and mobile");
        if (!app_tws_is_freeman_mode())
        {
            if (!IS_CONNECTED_WITH_TWS_PROFILE())
            {
                conn_ble_reconnection_machanism_handler(true);

                // for slave, need to start connection creation supervisor timer to
                // assure that it can switch to master and connect to mobile
                if (app_tws_is_slave_mode())
                {
                    conn_start_connectable_only_pairing(BT_SOURCE_CONN_MASTER);
                }
            }
        }
    }
}

void app_tws_out_of_charger_box(void)
{
    TRACE("[IO] %s, isPutInChargerBoxCalled=%d, isOutOfChargerBoxCalled=%d",
        __func__, app_tws_ui_env.isPutInChargerBoxCalled, app_tws_ui_env.isOutOfChargerBoxCalled);
    TRACE("conn_op_state is %d twsConnectionState is %d mobileDevConnectionState is %d",
        conn_op_state, twsConnectionState, mobileDevConnectionState);
    TRACE("isRebootToPairing %d", app_tws_ui_env.isRebootToPairing);

    log_event(EVENT_OUT_OF_BOX);

    if (app_tws_ui_env.isOutOfChargerBoxCalled)
    {
        return;
    }

    if (app_tws_ui_env.isRebootToPairing)
    {
        app_set_in_charger_box_flag(false);
        return;
    }

    app_tws_ui_env.isOutOfChargerBoxCalled = true;
    app_tws_ui_env.isPutInChargerBoxCalled = false;

    app_set_in_charger_box_flag(false);

    dev_operation_start_check_timer(1,0,app_tws_out_of_charger_box_timer_process);
}

static void app_tws_charger_box_opened_connect_slave_timeout_handler(void const *param)
{
    TRACE("%s connSlave:%d",__func__, IS_CONNECTING_SLAVE());
    if (!IS_CONNECTING_SLAVE()){
        connecting_tws_slave_timeout_handler();
    }
}

void app_tws_charger_box_opened_connect_slave_timer_stop(void)
{
    osTimerStop(charger_box_opened_connect_slave_timer);
}

void app_tws_charger_box_opened_timer_process(void)
{
    TRACE("[IO] %s", __func__);

#ifndef __TWS_PAIR_DIRECTLY__
    if (app_is_charger_box_closed())
    {
        return;
    }
#endif
    if (!app_conn_check_and_restore_op_state())
    {
        box_operation_start_check_timer(1,1,app_tws_charger_box_opened_timer_process);
        TRACE(" [%s] check status later ",__func__);
        return ;
    }

    if (app_tws_is_master_mode()){
        if (!IS_CONNECTED_WITH_TWS()){
            osTimerStop(charger_box_opened_connect_slave_timer);
            osTimerStart(charger_box_opened_connect_slave_timer, CONNECTING_SLAVE_TIMEOUT_RECONNECT_IN_MS);
        }
    }

    conn_system_boot_up_handler();
}

void app_tws_charger_box_opened(void)
{
    static uint8_t poweron_init = 0;
    TRACE("[IO] --%s--, isChargerBoxClosedCalledf=%d, isChargerBoxOpenedCalled=%d\n", __func__,
                                                                    app_tws_ui_env.isChargerBoxClosedCalled,
                                                                    app_tws_ui_env.isChargerBoxOpenedCalled);
    TRACE("isRebootToPairing=%d\n", app_tws_ui_env.isRebootToPairing);

    log_event(EVENT_BOX_OPENED);

    INCREASE_USER_HABIT_DATA(USER_HABIT_DATA_TYPE_IN_OUT_BOX_COUNT);

    if (app_tws_ui_env.isRebootToPairing)
    {
        app_set_in_charger_box_flag(true);
        return;
    }

    if (app_tws_ui_env.isChargerBoxOpenedCalled)
    {
        return;
    }

    app_tws_ui_env.isChargerBoxOpenedCalled = true;
    app_tws_ui_env.isChargerBoxClosedCalled = false;

    conn_get_mobile_dev_addr();

    if (poweron_init != 0)
        app_set_in_charger_box_flag(true);
    else
        poweron_init = 1;

    app_set_charger_box_closed_flag(false);
    app_stop_auto_power_off_supervising();

    app_tws_start_normal_ble_activities_in_chargerbox();
    app_tws_event_exchange_supervisor_timer_cancel();

    box_operation_start_check_timer(1,0,app_tws_charger_box_opened_timer_process);
}

static void app_tws_execute_charger_box_bt_closed_operation(void)
{
    TRACE("%s enter", __func__);

    osTimerStop(box_closed_operation_delay_timer);

    conn_stop_discoverable_and_connectable_state();

    if (app_bt_conn_is_any_connection_on()||
        IS_CONNECTED_WITH_TWS_PROFILE()){
        set_conn_op_state_machine(CONN_OP_STATE_MACHINE_DISCONNECT_ALL);
    }

    if  ((CONN_OP_STATE_MACHINE_DISCONNECT_ALL == CURRENT_CONN_OP_STATE_MACHINE()) &&
         ((app_tws_is_master_mode()) ||
          (app_tws_is_freeman_mode()))){
        if (app_bt_is_hfp_audio_on())
        {
            btif_hf_force_disconnect_sco(app_bt_device.hf_channel[BT_DEVICE_ID_1]);
            osTimerStart(box_closed_operation_delay_timer, 300);
            TRACE("%s diconnect_sco", __func__);
            goto Exit;
        }
    }else if((CONN_OP_STATE_MACHINE_DISCONNECT_ALL == CURRENT_CONN_OP_STATE_MACHINE())&&
        (app_tws_is_slave_mode())){
        tws_slave_stop_sco_sniffering();
    }

    if (IS_CONNECTED_WITH_TWS_PROFILE())
    {
        set_conn_op_state_machine(CONN_OP_STATE_MACHINE_DISCONNECT_ALL);
        app_tws_close_tws_stream();
    }
    else
    {
        conn_bt_disconnect_all();
    }
Exit:
    TRACE("%s exit", __func__);
}

static void app_tws_execute_charger_box_bt_closed_operation_delay_timeout_handler(void const *param)
{
    app_tws_execute_charger_box_bt_closed_operation();
}

void app_tws_execute_charger_box_closed_operation(void)
{
#if BLE_TWS_USE_BLE_AS_INBOX_COMMUNICATION_PATH
    ble_tws_stop_all_activities();
#endif

    app_start_custom_function_in_bt_thread(0, 0, (uint32_t)app_tws_execute_charger_box_bt_closed_operation);
}

static void chargerbox_closed_event_exchange_supervisor_timer_cb(void const *n)
{
    app_start_custom_function_in_bt_thread(0,
        0, (uint32_t)app_tws_execute_charger_box_closed_operation);
}

void app_tws_event_exchange_supervisor_timer_cancel(void)
{
    osTimerStop(chargerbox_closed_event_exchange_supervisor_timer);
}

static void app_tws_charger_box_closed_handling(void)
{
#if CLOSE_BOX_TO_TRIGGER_ROLE_SWITCH
    // The condition to start the role switch
    // 1. master
    // 2. master is connected with slave
    // 3. slave is out of chargerbox
    if (app_tws_is_master_mode())
    {
        if (IS_CONNECTED_WITH_TWS())
        {
            // - inform the chargerbox closed event to the slave
            app_tws_inform_in_box_state();

            // - start a 2s timer to force the close operation when time-out happenes
            //   but still don't receive the response from the slave
            osTimerStop(chargerbox_closed_event_exchange_supervisor_timer);
            osTimerStart(chargerbox_closed_event_exchange_supervisor_timer,
                CHARGERBOX_CLOSED_EVENT_EXCHANGE_TIMEOUT_IN_MS);
            return;
        }
    }
#endif
    if (app_tws_is_slave_mode())
    {
        if (IS_CONNECTED_WITH_TWS())
        {
            app_tws_inform_in_box_state();
            ble_tws_stop_all_activities();
            set_conn_op_state_machine(CONN_OP_STATE_MACHINE_DISCONNECT_ALL);
            osTimerStop(chargerbox_closed_event_exchange_supervisor_timer);
            osTimerStart(chargerbox_closed_event_exchange_supervisor_timer,
                CHARGERBOX_CLOSED_EVENT_EXCHANGE_TIMEOUT_IN_MS);
            return;
        }
    }
    // for free man
    app_tws_execute_charger_box_closed_operation();
}

void app_tws_charger_box_closed_stop_ble_activities(void)
{
    TRACE("--%s--", __FUNCTION__);

#if BLE_TWS_USE_BLE_AS_INBOX_COMMUNICATION_PATH
    ble_tws_stop_all_activities();
#endif
}

void app_tws_charger_box_closed_timer_process(void)
{
    TRACE("[IO] %s", __func__);

    if (!app_is_charger_box_closed())
    {
        return;
    }
#if CLOSE_BOX_TO_TRIGGER_ROLE_SWITCH
    if (app_tws_is_role_switching_on())
    {
        box_operation_start_check_timer(0,1,app_tws_charger_box_closed_timer_process);
        return;
    }

    if (is_simulating_reconnecting_in_progress())
    {
        box_operation_start_check_timer(0,1,app_tws_charger_box_closed_timer_process);
        return;
    }

    if (IS_CONNECTING_SLAVE() || IS_CONNECTING_MOBILE())
    {
        app_pending_for_idle_state_timeout_handler(false);
        box_operation_start_check_timer(0,1,app_tws_charger_box_closed_timer_process);
        return;
    }
    app_pending_for_idle_state_timeout_handler(true);
#endif

    if (!app_conn_check_and_restore_op_state())
    {
        box_operation_start_check_timer(0,1,app_tws_charger_box_closed_timer_process);
        TRACE(" [%s] check status later ",__func__);
        return;
    }

    osTimerStop(charger_box_opened_connect_slave_timer);

    app_start_custom_function_in_bt_thread(0,
                                           0, (uint32_t)app_tws_charger_box_closed_handling);
}

void app_tws_charger_box_closed(void)
{
    TRACE("[IO] --%s--, isChargerBoxClosedCalledf=%d, isChargerBoxOpenedCalled=%d\n", __func__,
                                                                    app_tws_ui_env.isChargerBoxClosedCalled,
                                                                    app_tws_ui_env.isChargerBoxOpenedCalled);
    TRACE("isRebootToPairing=%d\n", app_tws_ui_env.isRebootToPairing);

    log_event(EVENT_BOX_CLOSED);

    if (app_tws_ui_env.isRebootToPairing)
    {
        return;
    }

    if (app_tws_ui_env.isChargerBoxClosedCalled)
    {
        return;
    }

    app_tws_ui_env.isChargerBoxClosedCalled = true;
    app_tws_ui_env.isChargerBoxOpenedCalled = false;
    app_set_charger_box_closed_flag(true);
    app_set_in_charger_box_flag(true);

    box_operation_start_check_timer(0,0,app_tws_charger_box_closed_timer_process);

}

void app_tws_check_chargerbox_state_after_operation(void)
{
    if (app_is_in_charger_box())
    {
        if (app_is_charger_box_closed())
        {
            app_tws_charger_box_closed_handling();
        }
        else
        {
        #if BLE_TWS_USE_BLE_AS_INBOX_COMMUNICATION_PATH
            app_tws_start_normal_ble_activities_in_chargerbox();
        #endif
            conn_system_boot_up_handler();
        }
    }
}
#endif//__ENABLE_IN_BOX_STATE_HANDLE__


#ifdef __TWS_PAIR_DIRECTLY__
static void app_tws_reconnect_slave_timeout_handler(void const *param)
{
    TRACE("%s connSlave:%d",__func__, IS_CONNECTING_SLAVE());
    if (!IS_CONNECTING_SLAVE()){
        connecting_tws_slave_timeout_handler();
    }
}

void app_tws_reconnect_slave_timer_stop(void)
{
    osTimerStop(app_tws_reconnect_slave_timer);
}


void connecting_tws_master_timeout_handler(void)
{
    TRACE("%s Slave Connecting TWS master time-out !", __FUNCTION__);
    app_tws_exit_role_switching(APP_TWS_ROLESWITCH_TIMEOUT);
    TRACE("current state machine is %d", CURRENT_CONN_OP_STATE_MACHINE());
    set_conn_op_state(CONN_OP_STATE_IDLE);

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
#ifdef __TWS_PAIR_DIRECTLY__
            conn_start_connectable_only_pairing(BT_SOURCE_CONN_MASTER);
#else
            conn_start_connectable_only_pairing(BT_SOURCE_CONN_MOBILE_DEVICE);
#endif
        }
    }

}

static void app_tws_reconnect_master_timeout_handler(void const *param)
{
    TRACE("%s connSlave:%d",__func__, IS_CONNECTING_MASTER());
    if (!IS_CONNECTING_MASTER()){
        connecting_tws_master_timeout_handler();
    }
}

void app_tws_reconnect_master_timer_stop(void)
{
    osTimerStop(app_tws_reconnect_master_timer);
}

void app_tws_reconnect_timer_process(void)
{
    if (!app_conn_check_and_restore_op_state())
    {
        box_operation_start_check_timer(1,1,app_tws_reconnect_timer_process);
        TRACE(" [%s] check status later ",__func__);
        return ;
    }
    if (app_tws_is_master_mode()){
        if (!IS_CONNECTED_WITH_TWS()){
            osTimerStop(app_tws_reconnect_slave_timer);
            osTimerStart(app_tws_reconnect_slave_timer, CONNECTING_SLAVE_TIMEOUT_RECONNECT_IN_MS);
        }
    }
    if (TWSSLAVE == tws_mode_ptr()->mode){
        if (!IS_CONNECTED_WITH_TWS()){
            osTimerStop(app_tws_reconnect_master_timer);
            osTimerStart(app_tws_reconnect_master_timer, CONNECTING_SLAVE_TIMEOUT_RECONNECT_IN_MS);
            TRACE("start app_tws_reconnect_master_timer");
        }
    }

    conn_system_boot_up_handler();
}

void app_tws_reconnect_after_sys_startup(void)
{
    TRACE("##isRebootToPairing=%d\n", app_tws_ui_env.isRebootToPairing);
    conn_get_mobile_dev_addr();
    app_stop_auto_power_off_supervising();
    app_tws_start_normal_ble_activities_without_chargerbox();
    box_operation_start_check_timer(1,0,app_tws_reconnect_timer_process);
}
#endif

#define HFP_DISCONNECTION_CNT_BEFORE_RESET_MOBILE_CONNECTION    8
static uint8_t hfp_disconnection_count_during_role_switch = 0;
void app_tws_process_hfp_disconnection_event_during_role_switch(bt_bdaddr_t* bdAddr)
{
    hfp_disconnection_count_during_role_switch++;
    if (hfp_disconnection_count_during_role_switch <
        HFP_DISCONNECTION_CNT_BEFORE_RESET_MOBILE_CONNECTION)
    {
        TRACE("HFP disconnected %d times, retry.", hfp_disconnection_count_during_role_switch);
        app_start_custom_function_in_bt_thread(BT_DEVICE_ID_1,
            (uint32_t)bdAddr, (uint32_t)app_bt_connect_hfp);
    }
    else
    {
        TRACE("HFP reconnect tries exceed the maximum time %d, reset the mobile connection.",
            HFP_DISCONNECTION_CNT_BEFORE_RESET_MOBILE_CONNECTION);
        app_conn_set_reset_mobile_connection_flag(true);
        app_start_custom_function_in_bt_thread(0,
            0, (uint32_t)app_bt_disconnect_all_mobile_devces);
    }
}

#ifdef __TWS_ROLE_SWITCH__
void app_tws_role_switch_started(void)
{
    TRACE("The peer dev role switch is started!");
    hfp_disconnection_count_during_role_switch = 0;
    app_tws_ui_env.isPeerDevRoleSwitchInProgress = true;
}

static void check_before_roleswitch_failure_handling_timer_cb(void const *n)
{
    app_start_custom_function_in_bt_thread(app_tws_ui_env.roleSwitchFailureReason, 0,
        (uint32_t)app_tws_role_switch_failure_handler);
}

static void app_tws_role_switch_failure_handler(uint8_t reason)
{
    bool isAllowToContinue = app_conn_check_current_operation_state();
    if (!isAllowToContinue)
    {
        roleswitch_failure_check_count++;
        if (roleswitch_failure_check_count >=
            (ROLE_SWITCH_FAILURE_TIMEOUT_TO_RESET_IN_MS/ROLE_SWITCH_FAILURE_HANDLING_PENDING_TIME_IN_MS))
        {
            TRACE("Role switch failure handling time-out!");
            if (app_tws_get_role_switch_target_role() !=
                app_tws_get_mode())
            {
                app_tws_switch_role_in_nv();
            }

            ASSERT(0, "Role switch failure handling timeout!");
        }

        TRACE("Pending for role switch failure handling.");
        app_tws_ui_env.roleSwitchFailureReason = reason;
        app_tws_set_role_switching_on_flag(true);
        osTimerStart(check_before_roleswitch_failure_handling_timer,
            ROLE_SWITCH_FAILURE_HANDLING_PENDING_TIME_IN_MS);

        return;
    }

    roleswitch_failure_check_count = 0;

    app_tws_set_role_switching_on_flag(false);

    // if current role is not the target role, do following things
    // 1. Disconnect all links
    // 2. Change the BT addr and the tws role
    // assure that BT address is correct
    if (app_tws_is_master_mode())
    {
        btif_me_set_bt_address(tws_mode_ptr()->masterAddr.address);
    }
    else if (app_tws_is_slave_mode())
    {
        btif_me_set_bt_address(tws_mode_ptr()->slaveAddr.address);
    }

    // it means the controll tells that the role switch failed,
    // and the re-connection to the HFP will be done inside the role switch handler,
    // so we return here
    if (APP_TWS_ROLESWITCH_FAILED == reason)
    {
        return;
    }

    if (app_tws_get_role_switch_target_role() != app_tws_get_mode())
    {
        TRACE("Target role is %d current role is %d", app_tws_get_role_switch_target_role(), app_tws_get_mode());
        TRACE("Start forcing the role switch!");
        if (app_bt_conn_is_any_connection_on())
        {
            set_conn_op_state_machine(CONN_OP_STATE_MACHINE_DISCONNECT_ALL_BEFORE_CHANGE_ROLE);
            app_tws_clean_up_connections();
        }
        else
        {
            app_tws_switch_role_and_reconnect_for_tws_forcing_roleswitch();
        }
    }
    else
    {
        TRACE("conn_op_state is 0x%x", conn_op_state);
#ifdef __ENABLE_IN_BOX_STATE_HANDLE__
        if (!box_operation_timer_on() && IS_NO_PENDING_CONNECTION())
#else
        if (IS_NO_PENDING_CONNECTION())
#endif
        {
            conn_system_boot_up_handler();
        }
    }
}

bool app_tws_is_bt_roleswitch_in_progress(void)
{
    return app_tws_ui_env.isBtRoleSwitchDuringRoleSwitchOn;
}

static void waiting_bt_roleswitch_done_signal_from_master_timer_cb(void const *n)
{
    waiting_bt_roleswitch_done_timer_triggered_count++;

    if ((masterBtRemDev && (BTIF_BCR_SLAVE == btif_me_get_remote_device_role(masterBtRemDev)))||
        (waiting_bt_roleswitch_done_timer_triggered_count >=
        (WAITING_BT_ROLESWITCH_DONE_SIGNAL_FROM_MASTER_TIMEOUT_IN_MS/WAITING_BT_ROLESWITCH_DONE_SIGNAL_FROM_MASTER_TIMER_PERIOD_IN_MS)))
    {
        TRACE("Restore the inbox state operation! waiting_bt_roleswitch_done_timer_triggered_count %d",
            waiting_bt_roleswitch_done_timer_triggered_count);
        app_tws_ui_env.isBtRoleSwitchDuringRoleSwitchOn = false;
#ifdef __ENABLE_IN_BOX_STATE_HANDLE__
        if (app_is_in_charger_box() && !app_is_charger_box_closed())
        {
        #if BLE_TWS_USE_BLE_AS_INBOX_COMMUNICATION_PATH
            app_tws_start_normal_ble_activities_in_chargerbox();
        #endif
        }
#endif
    }
    else
    {
        osTimerStart(waiting_bt_roleswitch_done_signal_from_master_timer,
                WAITING_BT_ROLESWITCH_DONE_SIGNAL_FROM_MASTER_TIMER_PERIOD_IN_MS);
    }
}

void app_tws_roleswitch_done(uint8_t retStatus)
{
    set_conn_op_state_machine(CONN_OP_STATE_MACHINE_NORMAL);

    app_bt_stop_tws_reconnect_hfp_timer();

    app_conn_set_reset_mobile_connection_flag(false);

    if (app_tws_is_master_mode())
    {
        if (IS_CONNECTED_WITH_TWS())
        {
#if IS_ENABLE_BT_SNIFF_MODE
            app_bt_set_linkpolicy(slaveBtRemDev, BTIF_BLP_MASTER_SLAVE_SWITCH|BTIF_BLP_SNIFF_MODE);
#else
            app_bt_set_linkpolicy(slaveBtRemDev, BTIF_BLP_MASTER_SLAVE_SWITCH);
#endif
        }

        if (IS_CONNECTED_WITH_MOBILE())
        {
#if IS_ENABLE_BT_SNIFF_MODE
            app_bt_set_linkpolicy(mobileBtRemDev, BTIF_BLP_MASTER_SLAVE_SWITCH|BTIF_BLP_SNIFF_MODE);
#else
            app_bt_set_linkpolicy(mobileBtRemDev, BTIF_BLP_MASTER_SLAVE_SWITCH);
#endif
            app_bt_stream_volume_ptr_update(btif_me_get_remote_device_bdaddr(mobileBtRemDev)->address);
        }
    }
    else if (app_tws_is_slave_mode())
    {
        if (IS_CONNECTED_WITH_TWS())
        {
#if IS_ENABLE_BT_SNIFF_MODE
            app_bt_set_linkpolicy(masterBtRemDev, BTIF_BLP_MASTER_SLAVE_SWITCH|BTIF_BLP_SNIFF_MODE);
#else
            app_bt_set_linkpolicy(masterBtRemDev, BTIF_BLP_MASTER_SLAVE_SWITCH);
#endif
        }

        if (APP_TWS_ROLESWITCH_SUCCESSFUL == retStatus)
        {
            TRACE("Current role of the slave is %d", btif_me_get_remote_device_role(masterBtRemDev));
            app_tws_ui_env.isBtRoleSwitchDuringRoleSwitchOn = true;
            waiting_bt_roleswitch_done_timer_triggered_count = 0;
            osTimerStart(waiting_bt_roleswitch_done_signal_from_master_timer,
                WAITING_BT_ROLESWITCH_DONE_SIGNAL_FROM_MASTER_TIMER_PERIOD_IN_MS);
        }
    }

    app_tws_ui_env.isPeerDevRoleSwitchInProgress = false;

    if (APP_TWS_ROLESWITCH_SUCCESSFUL == retStatus)
    {
        if (!app_tws_is_role_switching_on() && !app_tws_ui_env.isPeerDevRoleSwitchInProgress)
        {
            TRACE("Both sides' role switch have been done!");
        }

        if (app_tws_is_master_mode())
        {
            app_tws_inform_role_switch_done();
        }
    }
    else
    {
        /*
        if (IS_CONNECTING_MASTER() || IS_CONNECTING_SLAVE() ||
            IS_CONNECTING_MOBILE())
        {
            conn_stop_tws_connection_creation_supvervisor_timer();
            conn_stop_mobile_connection_creation_supvervisor_timer();
            if (is_connecting_mobie_idle_timer_on())
            {
                conn_stop_mobile_connection_creation_idle_timer();
            }
            set_conn_op_state(CONN_OP_STATE_IDLE);
        }
        */

        TRACE("Role switch failed!");
        if (app_tws_get_role_switch_target_role() !=
            app_tws_get_mode())
        {
            app_tws_switch_role_in_nv();
        }

        ASSERT(0, "Role switch failed!");

        roleswitch_failure_check_count = 0;
        app_tws_role_switch_failure_handler(retStatus);
    }
}
#else
bool app_tws_is_bt_roleswitch_in_progress(void)
{
    return false;
}
#endif

#ifdef __ENABLE_IN_BOX_STATE_HANDLE__
bool app_is_in_charger_box(void)
{
    return app_tws_ui_env.isInChargerBox;
}

void app_set_in_charger_box_flag(bool isSet)
{
    TRACE("[IO] Set charger box in state as %d.", isSet);

    app_tws_ui_env.isInChargerBox = isSet;
}

bool app_is_charger_box_closed(void)
{
    return app_tws_ui_env.isChargerBoxClosed;
}

void app_set_charger_box_closed_flag(bool isSet)
{
    TRACE("[IO] Set charger box closed state as %d.", isSet);
    app_tws_ui_env.isChargerBoxClosed = isSet;
}
#endif

void app_tws_ui_reset_state(void)
{
    app_tws_ui_env.isPeerDevRoleSwitchInProgress = false;
}

void app_tws_ui_set_reboot_to_pairing(bool isRebootToPairing)
{
    TRACE("%s isRebootToPairing:%d", __func__, isRebootToPairing);
    app_tws_ui_env.isRebootToPairing = isRebootToPairing;
    bt_drv_reg_op_set_reboot_pairing_mode(isRebootToPairing);
}

bool app_tws_ui_is_in_repairing(void)
{
    return app_tws_ui_env.isRebootToPairing;
}

static void app_tws_cmd_voice_prompt_play_req_received_handler(uint8_t* ptrParam, uint32_t paramLen)
{
#ifdef MEDIA_PLAYER_SUPPORT
    app_tws_voice_prompt_to_play_t* ptPlayReq = (app_tws_voice_prompt_to_play_t *)ptrParam;
    TRACE("Receive the voice prompt play request to play %d",
        ptPlayReq->voicePrompt);

    app_voice_report((APP_STATUS_INDICATION_T)ptPlayReq->voicePrompt, 0);
#endif    
}

TWS_CUSTOM_COMMAND_TO_ADD(APP_TWS_CMD_LET_SLAVE_PLAY_MUSIC,   app_tws_cmd_voice_prompt_play_req_received_handler);

void app_tws_ask_slave_to_play_voice_prompt(uint32_t voicePrompt)
{
    if (app_tws_is_master_mode())
    {
        app_tws_voice_prompt_to_play_t tRequest = { voicePrompt };
        app_tws_send_cmd_without_rsp(APP_TWS_CMD_LET_SLAVE_PLAY_MUSIC,
            (uint8_t *)&tRequest, sizeof(tRequest));
    }
}

#if IS_WITH_SCO_PACKET_STATISTICS
static uint8_t double_click_times_within_5s = 0;
extern void print_msbc_packet_quality(void);
extern void print_msbc_packet_trend(void);

static void dump_trigger_check_timer_cb(void const *n);
osTimerDef (DUMP_TRIGGER_CHECK_TIMER, dump_trigger_check_timer_cb);
static osTimerId dump_trigger_check_timer = NULL;

static void dump_trigger_check_timer_cb(void const *n)
{
    double_click_times_within_5s = 0;
}

static void dump_msbc_quality_info(void)
{
    print_msbc_packet_quality();
    print_msbc_packet_trend();
}

static void app_tws_dump_trigger_handler(void)
{
    double_click_times_within_5s++;
    if (2 == double_click_times_within_5s)
    {
        hal_trace_crash_dump_register(dump_msbc_quality_info);
        ASSERT(false, "Print msbc quality");
    }
    else
    {
        if (NULL == dump_trigger_check_timer)
        {
            dump_trigger_check_timer =
                osTimerCreate(osTimer(DUMP_TRIGGER_CHECK_TIMER), osTimerOnce, NULL);
        }
        osTimerStart(dump_trigger_check_timer, 5000);
    }
}
#endif

#define LBRS_DATA_VALID_FLAG        (0xA5)

static uint8_t bat_req_cnt = 0;
static app_tws_low_battery_role_switch_status_t local_lbrs_status;
static app_tws_low_battery_role_switch_status_t another_lbrs_status;


static void app_tws_get_local_lbrs_status(app_tws_low_battery_role_switch_status_t *lbrs_status)
{
    ASSERT(lbrs_status != NULL, "lbrs_status == NULL");

    lbrs_status->data_valid = LBRS_DATA_VALID_FLAG;
    lbrs_status->bat_level = (uint8_t)app_battery_current_level();
#ifdef __ENABLE_IN_BOX_STATE_HANDLE__
    lbrs_status->isInBox = (uint8_t)app_is_in_charger_box();
#endif
    lbrs_status->isWearStatus = app_tws_ui_env.runtimeStatus.isEarphoneweared;
}

#if defined(__TWS_ROLE_SWITCH__) && defined(__ENABLE_ROLE_SWITCH_FOR_BATTERY_BALANCE__)
static void app_tws_cmd_req_battery_level_received_handler(uint8_t* ptrParam, uint32_t paramLen)
{
    TRACE("%s", __func__);

    if (app_tws_is_slave_mode())
    {
        memcpy((void *)&another_lbrs_status, (void *)ptrParam, paramLen);

        TRACE("%s i'm slave:", __func__);
        TRACE("%s local_level=%d, another_level=%d", __func__, local_lbrs_status.bat_level, another_lbrs_status.bat_level);
        TRACE("%s local_Isinbox=%d, another_Isinbox=%d", __func__, local_lbrs_status.isInBox, another_lbrs_status.isInBox);
        TRACE("%s local_isWeard=%d, another_isWeard=%d", __func__, local_lbrs_status.isWearStatus, another_lbrs_status.isWearStatus);

        app_tws_send_cmd_without_rsp(APP_TWS_CMD_RSP_BATTERY_LEVEL, (uint8_t *)&local_lbrs_status, sizeof(app_tws_low_battery_role_switch_status_t));
    }
}

static void app_tws_cmd_rsp_battery_level_received_handler(uint8_t* ptrParam, uint32_t paramLen)
{
    TRACE("%s", __func__);

    if (app_tws_is_master_mode())
    {
        memcpy((void *)&another_lbrs_status, (void *)ptrParam, paramLen);

        TRACE("%s i'm master:", __func__);
        TRACE("%s local_level=%d, another_level=%d", __func__, local_lbrs_status.bat_level, another_lbrs_status.bat_level);
        TRACE("%s local_Isinbox=%d, another_Isinbox=%d", __func__, local_lbrs_status.isInBox, another_lbrs_status.isInBox);
        TRACE("%s local_isWeard=%d, another_isWeard=%d", __func__, local_lbrs_status.isWearStatus, another_lbrs_status.isWearStatus);

#ifdef __TWS_PAIR_DIRECTLY__
        if((local_lbrs_status.bat_level < 3)
            && (another_lbrs_status.bat_level >= (local_lbrs_status.bat_level + 20)))
#else
        if((local_lbrs_status.bat_level < 3)
            && (another_lbrs_status.bat_level >= (local_lbrs_status.bat_level + 20))
            && (!another_lbrs_status.isInBox)
            && (another_lbrs_status.isWearStatus)
        )
#endif
        {
            app_start_custom_function_in_bt_thread(RESTORE_THE_ORIGINAL_PLAYING_STATE,
                    0, (uint32_t)app_tws_kickoff_switching_role);
        }
    }
}

uint8_t app_tws_get_report_level(void)
{
    uint8_t bat_level = 0xff;
    uint8_t which_on = 0;

    if (app_tws_is_master_mode())
    {
        if(LBRS_DATA_VALID_FLAG != local_lbrs_status.data_valid)
        {
            app_tws_get_local_lbrs_status(&local_lbrs_status);
        }

        if(LBRS_DATA_VALID_FLAG != another_lbrs_status.data_valid)
        {
            bat_level = local_lbrs_status.bat_level;
            which_on = 1;
        }
        else
        {
            if(local_lbrs_status.bat_level >= another_lbrs_status.bat_level)
            {
                bat_level = local_lbrs_status.bat_level;
                which_on = 1;
            }
            else
            {
                bat_level = another_lbrs_status.bat_level;
                which_on = 2;
            }
        }
    }

    TRACE("%s,  bat_level=%d, which_on=%d", __func__, bat_level, which_on);
    return bat_level;
}


TWS_CUSTOM_COMMAND_TO_ADD(APP_TWS_CMD_REQ_BATTERY_LEVEL,   app_tws_cmd_req_battery_level_received_handler);
TWS_CUSTOM_COMMAND_TO_ADD(APP_TWS_CMD_RSP_BATTERY_LEVEL,   app_tws_cmd_rsp_battery_level_received_handler);


void app_tws_cmd_req_battery_level(void)
{
    app_tws_get_local_lbrs_status(&local_lbrs_status);
    if (app_tws_is_master_mode())
    {
        if (!app_tws_is_role_switching_on())
        {
            TRACE("app_tws_cmd_req_battery_level bat_req_cnt=%d", bat_req_cnt);

            if(bat_req_cnt < 6)
            {
                bat_req_cnt++;
            }
            else
            {
                bat_req_cnt = 0;
                app_tws_send_cmd_without_rsp(APP_TWS_CMD_REQ_BATTERY_LEVEL, (uint8_t *)&local_lbrs_status, sizeof(app_tws_low_battery_role_switch_status_t));
            }
        }
    }
}
#endif

//share esco retx
bool app_tws_share_esco_retx_nb(uint8_t retx_nb)
{
    APP_TWS_SHARE_RETX_T tws_share_retx_nb;
    TRACE("APP tws master share esco retx nb = %d",retx_nb);
    tws_share_retx_nb.retx_nb= retx_nb;

    return app_tws_send_cmd_without_rsp(APP_TWS_CMD_SHARE_TWS_ESCO_RETX_NB, (uint8_t *)&tws_share_retx_nb, sizeof(tws_share_retx_nb));
}
uint8_t esco_retx_nb=0;
static void app_tws_share_esco_retx_nb_handler(uint8_t* ptrParam, uint32_t paramLen)
{
        APP_TWS_SHARE_RETX_T* pReq = (APP_TWS_SHARE_RETX_T *)ptrParam;
        TRACE("APP tws slave get  esco retx nb = %d",pReq->retx_nb);
        esco_retx_nb = pReq->retx_nb;
}

TWS_CUSTOM_COMMAND_TO_ADD(APP_TWS_CMD_SHARE_TWS_ESCO_RETX_NB, app_tws_share_esco_retx_nb_handler);

#ifdef  __EMSACK__
bool app_tws_set_emsack_mode(bool enable)
{
    APP_TWS_SET_EMSACK_T tws_emsack_mode_set;
    TRACE("APP tws set emsack mode = %d",enable);
    tws_emsack_mode_set.enable = enable;

    return app_tws_send_cmd_without_rsp(APP_TWS_CMD_SET_EMSACK_MODE, (uint8_t *)&tws_emsack_mode_set, sizeof(tws_emsack_mode_set));
}

static void app_tws_set_emsack_mode_handler(uint8_t* ptrParam, uint32_t paramLen)
{
    TRACE("%s",__func__);

    APP_TWS_SET_EMSACK_T* pReq = (APP_TWS_SET_EMSACK_T *)ptrParam;

    if(app_tws_is_master_mode())
    {
        bt_drv_reg_op_enable_emsack_mode(btif_me_get_remote_device_hci_handle(mobileBtRemDev),1, pReq->enable);
    }
}

TWS_CUSTOM_COMMAND_TO_ADD(APP_TWS_CMD_SET_EMSACK_MODE, app_tws_set_emsack_mode_handler);
#endif //__EMSACK__

#ifdef LBRT

bool app_get_lbrt_enable(void)
{
    return app_tws_ui_env.isLbrtEnable;
}

void app_set_lbrt_enable(bool enable)
{
    LBRT_PRINT("LBRT is enable = %d.", enable);

    app_tws_ui_env.isLbrtEnable = enable;
}

uint32_t app_get_lbrt_ping_ticks(void)
{
    return app_tws_ui_env.lbrt_ping_ticks;
}

void app_set_lbrt_ping_ticks(uint32_t ticks)
{
    app_tws_ui_env.lbrt_ping_ticks = ticks;
}

uint32_t app_get_lbrt_switch_ticks(void)
{
    return app_tws_ui_env.lbrt_switch_ticks;
}

void app_set_lbrt_switch_ticks(uint32_t ticks)
{
    if(app_tws_is_master_mode())
    {
        app_tws_ui_env.lbrt_switch_ticks = ticks;
    }
}

uint8_t app_lbrt_get_bt_tx_pwr(void)
{
    return app_tws_ui_env.lbrt_tx_pwr;
}

void app_lbrt_set_bt_tx_pwr(uint32_t pwr)
{
    TRACE("%s,pwr=%d",__func__,pwr);
    app_tws_ui_env.lbrt_tx_pwr = pwr;
}

bool app_lbrt_get_is_fix_tx_pwr(void)
{
    return app_tws_ui_env.isFixTxPwr;
}

void app_lbrt_set_is_fix_tx_pwr(bool fix)
{
    app_tws_ui_env.isFixTxPwr = fix;
}

void app_tws_compute_lbrt_switch_time(uint32_t ticks,uint8_t enable)
{
    if(app_tws_is_master_mode())
    {
        uint32_t duration_ticks = ticks - app_get_lbrt_switch_ticks();
        LBRT_PRINT("App tws switch LBRT time = %d ms,[enable =%d]",BT_CONTROLLER_TICKS_TO_MS(duration_ticks),enable);

        TRACE("duration_ticks= %d,dest ticks = %d, record ticks=%d",duration_ticks,ticks, app_get_lbrt_switch_ticks());
        app_set_lbrt_switch_ticks(0xFFFF);
    }
}

static void app_tws_start_set_lbrt_timer(uint32_t timeoutInMs)
{
    osTimerStart(tws_set_lbrt_timer, timeoutInMs);
}

static void app_tws_start_lbrt_ping_timer(uint32_t timeoutInMs)
{
    osTimerStart(tws_lbrt_ping_timer, timeoutInMs);
}

static void app_tws_stop_lbrt_ping_timer(void)
{
    osTimerStop(tws_lbrt_ping_timer);
}

void app_tws_config_lbrt_tx_pwr(uint16_t connHandle,uint8_t en)
{

    if(LBRT_ENABLE == en && !app_lbrt_get_is_fix_tx_pwr())
    {
        LBRT_PRINT("%s fix LBRT tx pwr level",__func__);
        uint8_t temp_tx_pwr = bt_drv_reg_op_get_tx_pwr(connHandle);
        app_lbrt_set_bt_tx_pwr(temp_tx_pwr);
        bt_drv_reg_op_fix_tx_pwr(connHandle);
        app_lbrt_set_is_fix_tx_pwr(true);
    }
    else if(LBRT_DISABLE == en && app_lbrt_get_is_fix_tx_pwr())
    {
        LBRT_PRINT("%s resume BT tx pwr level",__func__);
        bt_drv_reg_op_set_tx_pwr(connHandle, app_lbrt_get_bt_tx_pwr());
        app_lbrt_set_is_fix_tx_pwr(false);
    }
}


void app_tws_config_lbrt(uint16_t connHandle, uint8_t en)
{
    LBRT_PRINT("%s, hcihandle=0x%x,config[1:enable | 0:disable]=%d",__func__,connHandle,en);
#ifdef CHIP_BEST2300
    btif_me_set_lbrt_enable(connHandle, en);
#endif
    app_tws_config_lbrt_tx_pwr(connHandle, en);
}

void app_tws_exit_lbrt_mode(void)
{
    LBRT_PRINT("%s,enable=%d",__func__,app_get_lbrt_enable());
    if(app_get_lbrt_enable())
    {
        app_tws_config_lbrt(app_tws_get_tws_conhdl(), LBRT_DISABLE);
        app_tws_stop_lbrt_ping_timer();
        app_set_lbrt_enable(false);
        app_set_lbrt_ping_ticks(0xFFFF);
        app_set_lbrt_switch_ticks(0xFFFF);
    }
}

static void app_tws_lbrt_ping_to_cb(void const *param)
{
    LBRT_PRINT("%s",__func__);

    uint32_t curr_ticks = bt_syn_get_curr_ticks(app_tws_get_tws_conhdl());
    if(app_get_lbrt_enable())
    {
        if(app_tws_is_master_mode())
        {
            tws_lbrt_ping_req(curr_ticks);
            app_tws_start_lbrt_ping_timer(APP_TWS_LBRT_TIMEOUT_IN_MS);
        }
        else if(app_tws_is_slave_mode())
        {
            uint32_t diff_ticks = curr_ticks - app_get_lbrt_ping_ticks();
            if(diff_ticks > APP_TWS_LBRT_TIMEOUT_IN_BT_TICKS)
            {
                app_tws_exit_lbrt_mode();
                LBRT_PRINT("LBRT time out,change to 2.4G mode,last ping ticks=%d",diff_ticks);
            }
            else
            {
                app_tws_start_lbrt_ping_timer(APP_TWS_LBRT_TIMEOUT_IN_MS);
                LBRT_PRINT("Tws slave check LBRT Well,last ping ticks=%d",diff_ticks);
            }
        }
    }
    else
    {
        osTimerStop(tws_lbrt_ping_timer);
    }
}

static void app_tws_set_lbrt_to_cb(void const *param)
{
    LBRT_PRINT("%s, set LBRT mode=%d",__func__,app_get_lbrt_enable());

    if(app_get_lbrt_enable())
    {
#ifndef __LBRT_PAIR__
        if(app_tws_is_slave_mode())
        {
            app_tws_start_lbrt_ping_timer(2*APP_TWS_LBRT_TIMEOUT_IN_MS);
        }
        else if(app_tws_is_master_mode())
        {
            app_tws_start_lbrt_ping_timer(APP_TWS_LBRT_TIMEOUT_IN_MS);
        }
#endif
    }
    else
    {
        osTimerStop(tws_lbrt_ping_timer);
    }

    app_tws_config_lbrt(app_tws_get_tws_conhdl(), app_get_lbrt_enable());
}

//request LBRT
void app_tws_req_set_lbrt(uint8_t en, uint8_t initial_req, uint32_t ticks)
{
    APP_TWS_REQ_SET_LBRT_T pReq;
    pReq.lbrt_en = en;
    pReq.initial_req = initial_req;
    pReq.ticks = ticks;
    app_set_lbrt_enable(en);
    app_set_lbrt_switch_ticks(ticks);
    app_tws_send_cmd_without_rsp(APP_TWS_CMD_REQ_SET_LBRT,(uint8_t *)&pReq, sizeof(pReq));
}

static void app_tws_req_set_lbrt_handler(uint8_t* ptrParam, uint32_t paramLen)
{
    APP_TWS_REQ_SET_LBRT_T* pReq = (APP_TWS_REQ_SET_LBRT_T *)ptrParam;
    uint32_t current_ticks = bt_syn_get_curr_ticks(app_tws_get_tws_conhdl());

    if(pReq->initial_req&&(app_tws_is_slave_mode()))
    {
        uint32_t diff_tick = current_ticks - pReq->ticks;
        LBRT_PRINT("%s,recieve master tick=%d,current tick=%d, diff_ticks=%d\n",__func__,pReq->ticks,
            bt_syn_get_curr_ticks(app_tws_get_tws_conhdl()),diff_tick);

        if(diff_tick < APP_TWS_LBRT_TIMEOUT_IN_BT_TICKS)
        {
            app_tws_req_set_lbrt(pReq->lbrt_en,false,current_ticks +
                                    BT_CONTROLLER_MS_TO_TICKS(APP_TWS_LBRT_SWITCH_TIME_IN_MS));
            app_tws_start_set_lbrt_timer(APP_TWS_LBRT_SWITCH_TIME_IN_MS);
        }
        else
        {
            app_tws_req_set_lbrt(false,false,current_ticks);
            LBRT_PRINT("ERROR, TWS transfer time out!");
        }
    }
    else if(!pReq->initial_req&&(app_tws_is_master_mode()))
    {
        int diff_tick = pReq->ticks - current_ticks;
        LBRT_PRINT("%s,recieve slave dest tick=%d,current tick=%d, diff_ticks=%d\n",__func__,pReq->ticks,
            bt_syn_get_curr_ticks(app_tws_get_tws_conhdl()),diff_tick);

        if(diff_tick > 0)
        {
            app_set_lbrt_enable(pReq->lbrt_en);
            app_tws_compute_lbrt_switch_time(pReq->ticks,pReq->lbrt_en);
            app_tws_start_set_lbrt_timer(BT_CONTROLLER_TICKS_TO_MS(diff_tick));
        }
        else
        {
            app_set_lbrt_enable(false);
            LBRT_PRINT("ERROR, SET LBRT destination time is past!");
        }
    }
}


TWS_CUSTOM_COMMAND_TO_ADD(APP_TWS_CMD_REQ_SET_LBRT, app_tws_req_set_lbrt_handler);
//response LBRT ping
void app_tws_rsp_lbrt_ping(uint32_t curr_ticks)
{
    APP_TWS_RSP_LBRT_PING_T pRsp;
    pRsp.rsp_ticks = curr_ticks;

    LBRT_PRINT("%s,slave send ticks = %d\n",__func__,pRsp.rsp_ticks);

    app_tws_send_cmd_without_rsp(APP_TWS_CMD_RSP_LBRT_PING, (uint8_t *)&pRsp, sizeof(pRsp));
}


static void app_tws_rsp_lbrt_ping_handler(uint8_t* ptrParam, uint32_t paramLen)
{
    APP_TWS_RSP_LBRT_PING_T* pRsp = (APP_TWS_RSP_LBRT_PING_T *)ptrParam;
    uint32_t diff_tick = bt_syn_get_curr_ticks(app_tws_get_tws_conhdl())- pRsp->rsp_ticks;

    LBRT_PRINT("%s,recieve slave tick=%d,current tick=%d, diff_ticks=%d\n",__func__,pRsp->rsp_ticks,bt_syn_get_curr_ticks(app_tws_get_tws_conhdl()),diff_tick);

    if(diff_tick < APP_TWS_LBRT_TIMEOUT_IN_BT_TICKS)
    {
        LBRT_PRINT("Good ,LBRT well!");
    }
    else
    {
        app_tws_exit_lbrt_mode();
        LBRT_PRINT("ERROR ,LBRT maybe timeout!!!!!");
    }

    app_tws_inform_waiting_cmd_rsp_thread();
}

//request LBRT ping

TWS_CUSTOM_COMMAND_TO_ADD(APP_TWS_CMD_RSP_LBRT_PING, app_tws_rsp_lbrt_ping_handler);

bool app_tws_req_lbrt_ping(uint32_t current_ticks)
{
    APP_TWS_REQ_LBRT_PING_T tws_lbrt_ping_req;
    LBRT_PRINT("tws_lbrt_ping_req, current_ticks=%d",current_ticks);
    tws_lbrt_ping_req.req_ticks = current_ticks;

    return app_tws_send_cmd_with_rsp(APP_TWS_CMD_REQ_LBRT_PING, (uint8_t *)&tws_lbrt_ping_req, sizeof(tws_lbrt_ping_req), true);
}

static void app_tws_req_lbrt_ping_handler(uint8_t* ptrParam, uint32_t paramLen)
{
    LBRT_PRINT("%s",__func__);

    APP_TWS_REQ_LBRT_PING_T* pReq = (APP_TWS_REQ_LBRT_PING_T *)ptrParam;
    uint32_t current_ticks = bt_syn_get_curr_ticks(app_tws_get_tws_conhdl());

    uint32_t diff_tick = current_ticks - pReq->req_ticks;

    if((app_tws_is_slave_mode())&&(diff_tick < APP_TWS_LBRT_TIMEOUT_IN_BT_TICKS))
    {
        LBRT_PRINT("Recieve Master ping ticks=%d, current ticks=%d",pReq->req_ticks,current_ticks);
        app_set_lbrt_ping_ticks(pReq->req_ticks);
        app_tws_rsp_lbrt_ping(current_ticks);
    }
    else
    {
        LBRT_PRINT("ERROR,please check your TWS mode or LBRT RF");
    }

}

TWS_CUSTOM_COMMAND_TO_ADD(APP_TWS_CMD_REQ_LBRT_PING, app_tws_req_lbrt_ping_handler);

#endif// LBRT

#ifdef __TWS_ROLE_SWITCH__
extern struct BT_DEVICE_T  app_bt_device;
//extern BtCmgrContext cmgrContext;

static uint32_t app_tws_switch_send_bt_me_data(uint8_t *buf)
{
    uint32_t len;

    len = tws_switch_send_bt_me_data(buf);
    return len;
}

static uint32_t app_tws_switch_reset_bt_me_data(uint8_t *buf)
{
    uint32_t len;

    len = tws_switch_reset_bt_me_data(buf);
    return len;
}

static uint32_t app_tws_switch_send_bt_hci_data(uint8_t *buf)
{
    uint32_t len;

    len =  tws_switch_send_bt_hci_data(buf);
    return len;
}

static uint32_t app_tws_switch_reset_bt_hci_data(uint8_t *buf)
{
    uint32_t len;

    len = tws_switch_reset_bt_hci_data(buf);
    return len;
}

static uint32_t app_tws_switch_send_bt_l2cap_data(uint8_t *buf)
{
    uint32_t len;

    len = tws_switch_send_bt_l2cap_data(buf);
    return len;
}

static uint32_t app_tws_switch_reset_bt_l2cap_data(uint8_t *buf)
{
    uint32_t len;

    len = tws_switch_reset_bt_l2cap_data(buf);
    return len;
}

static uint32_t app_tws_switch_send_bt_rfc_data(uint8_t *buf)
{
    uint32_t len;

    len = tws_switch_send_bt_rfc_data(buf);
    return len;
}

static uint32_t app_tws_switch_reset_bt_rfc_data(uint8_t *buf)
{
    uint32_t len;

    len = tws_switch_reset_bt_rfc_data(buf);
    return len;
}

static uint32_t app_tws_switch_send_avdev_data(uint8_t *buf)
{
    uint32_t len;

    len = tws_switch_send_avdev_data(buf);
    return len;
}

static uint32_t app_tws_switch_reset_avdev_data(uint8_t *buf)
{
    uint32_t len;

    len = tws_switch_reset_avdev_data(buf);
    return len;
}

//extern A2dpStream * app_bt_get_mobile_a2dp_stream(uint32_t deviceId);
static uint32_t app_tws_switch_send_app_bt_device_data(uint8_t *buf)
{
    uint32_t len;

    len = tws_switch_send_app_bt_device_data(buf);
    return len;
}
//AvdtpStream         mobile_stream;
//AvdtpMediaHeader    mobile_mediaHeader;
//extern uint32_t RfAdvanceToOpenTimeoutHandlerAddr;
static uint32_t app_tws_switch_reset_app_bt_device_data(uint8_t *buf)
{
    uint32_t len;

    len = tws_switch_reset_app_bt_device_data(buf);
    return len;
}

static uint32_t app_tws_switch_send_cmgrcontext_data(uint8_t *buf)
{
    uint32_t len;

    len = tws_switch_send_cmgrcontext_data(buf);
    return len;
}
osTimerId POSSIBLY_UNUSED app_wait_data_send_to_peer_timer = NULL;

enum SIMULATE_RECONNECT_ROLE
{
    RECONNECT_SLAVE = 1,
    RECONNECT_MASTER,
    RECONNECT_NONE,
};
#define WAIT_DATA_SEND_TO_PEER_TIMER 20
static SIMULATE_RECONNECT_ROLE simulate_reconnect_role = RECONNECT_NONE;

uint32_t complete_packet_during_simulate_reconnect = 0;
void app_wait_data_send_to_peer_timeout_handler(void const *param)
{
    //app_show_tws_switch_time(6);
    TRACE("app_wait_data_send_to_peer_timeout_handler");

    uint16_t currentFreeAclBuf = bt_drv_reg_op_currentfreeaclbuf_get();
    TRACE("Current free acl buff %d", currentFreeAclBuf);

    if (btif_hci_Is_any_pending_hci_xfer() || (currentFreeAclBuf < btif_hci_get_acl_buffers_num()))
    {
        app_start_custom_function_in_bt_thread(0, 0,
            (uint32_t)btif_hci_get_PktsCompleteTimerHandler());
        osTimerStart(app_wait_data_send_to_peer_timer, WAIT_DATA_SEND_TO_PEER_TIMER);
        return;
    }
    if(simulate_reconnect_role == RECONNECT_SLAVE)
        start_simulate_reconnect_slave();
    else if(simulate_reconnect_role == RECONNECT_MASTER)
        start_simulate_reconnect_master();
    complete_packet_during_simulate_reconnect += btif_hci_current_rx_packet_complete();
    complete_packet_during_simulate_reconnect += btif_hci_current_rx_aclfreelist_cnt();
}

osTimerDef (APP_WAIT_DATA_SEND_TO_PEER, app_wait_data_send_to_peer_timeout_handler);



static void app_tws_switch_reset_cmgrcontext_data(uint8_t *buf)
{
    tws_switch_reset_cmgrcontext_data(buf);

    if(!app_wait_data_send_to_peer_timer) {
        app_wait_data_send_to_peer_timer = osTimerCreate(osTimer(APP_WAIT_DATA_SEND_TO_PEER), osTimerOnce, NULL);
    }
    simulate_reconnect_role = RECONNECT_SLAVE;
    osTimerStart(app_wait_data_send_to_peer_timer, WAIT_DATA_SEND_TO_PEER_TIMER);

    return;
}




static void tws_switch_receive_slave_saved_data_ok(uint8_t *data)
{
    switch(data[0])
    {
   // case BT_ME:tws_role_switch_exchange_profile_data(BT_HCI);break;
   // case BT_HCI:tws_role_switch_exchange_profile_data(BT_L2CAP);break;
   // case BT_L2CAP:tws_role_switch_exchange_profile_data(BT_RFC);break;
   // case BT_RFC:tws_role_switch_exchange_profile_data(AVDEV_CONTEXT);break;
   // case AVDEV_CONTEXT:tws_role_switch_exchange_profile_data(APP_BT_DEVICE);break;
   // case APP_BT_DEVICE:tws_role_switch_exchange_profile_data(CMGR_CONTEXT);break;
    case CMGR_CONTEXT:
        //app_show_tws_switch_time(6);
        if(!app_wait_data_send_to_peer_timer)
              {
                  app_wait_data_send_to_peer_timer = osTimerCreate(osTimer(APP_WAIT_DATA_SEND_TO_PEER), osTimerOnce, NULL);
              }
              simulate_reconnect_role = RECONNECT_MASTER;
              osTimerStart(app_wait_data_send_to_peer_timer, WAIT_DATA_SEND_TO_PEER_TIMER);
        break;//slave send data done , try reconnect new master
    default:break;
    }
}

static void tws_switch_send_slave_saved_data_ok(uint8_t *data)
{
    uint8_t send_data_buf[2];
    send_data_buf[0] = SLAVE_SAVE_DATA_OK;
    send_data_buf[1] = data[0];
     //now is in role switch state
    TRACE("FORCE SEND SLAVE_SAVE_DATA_OK :\n");
    DUMP8("%02x ",send_data_buf,2);
    app_tws_force_send_cmd_without_rsp(APP_TWS_CMD_PROFILE_DATA_EXCHANGE, send_data_buf, 2);
}

void tws_role_switch_exchange_profile_data(uint32_t arg)
{
    uint8_t send_data_buf[512];
    uint32_t length = 0;
    switch(arg)
    {
     case BT_ME:
        TRACE("send bt me data");
           //app_show_tws_switch_time(5);
           length = app_tws_switch_send_bt_me_data(send_data_buf);
        break;
     case BT_HCI:
        TRACE("send bt hci data");
              length =app_tws_switch_send_bt_hci_data(send_data_buf);
        break;
     case BT_L2CAP:
        TRACE("send bt l2cap data");
              length = app_tws_switch_send_bt_l2cap_data(send_data_buf);
        break;
     case BT_RFC:
        TRACE("send bt rfc data");
        length = app_tws_switch_send_bt_rfc_data(send_data_buf);
           break;
     case HF_CONTEXT:
        //jjjj TRACE("HF_CONTEXT_LENGTH = %d",sizeof(hfContext));
        break;
     case CMGR_CONTEXT:
        //jjjjj  TRACE("CMGR_CONTEXT_LENGTH = %d",sizeof(cmgrContext));
      length = app_tws_switch_send_cmgrcontext_data(send_data_buf);
      break;
     case AVRCP_CONTEXT:
        //jjjjTRACE("AVRCP_CONTEXT_LENGTH = %d",sizeof(avrcpContext));
        break;
     //case DEV_TABLE:TRACE("DEV_TABLE_LENGTH = %d",sizeof(devTable));break;
     case APP_BT_DEVICE:
        TRACE("send app bt device data");
        length = app_tws_switch_send_app_bt_device_data(send_data_buf);
        break;
     case AVDEV_CONTEXT:
        TRACE("send avdev data");
        length = app_tws_switch_send_avdev_data(send_data_buf);
        break;
     default:break;
    }
    //now is in role switch state
    TRACE("FORCE SEND PROFILE DATA:\n");
    DUMP8("%02x ",send_data_buf,length>20?20:length);
    app_tws_force_send_cmd_without_rsp(APP_TWS_CMD_PROFILE_DATA_EXCHANGE, send_data_buf, length);
}
void tws_role_switch_send_all_profile_data(void)
{
    tws_send_profile_data(BT_ME);
    tws_send_profile_data(BT_HCI);
    tws_send_profile_data(BT_L2CAP);
    tws_send_profile_data(BT_RFC);
    tws_send_profile_data(AVDEV_CONTEXT);
    tws_send_profile_data(APP_BT_DEVICE);
    tws_send_profile_data(CMGR_CONTEXT);
}
void tws_parse_profile_data(uint8_t* ptrParam, uint32_t paramLen)
{
    TRACE("receive data length %d:\n",paramLen);
    DUMP8("%02x",ptrParam,paramLen>20?20:paramLen);
    //return;
    if(ptrParam[0] == CMGR_CONTEXT) {
        tws_switch_send_slave_saved_data_ok(ptrParam);
    }
    switch(ptrParam[0]) {
    case BT_ME:
        TRACE("receive bt me data");
        //app_show_tws_switch_time(5);
        app_tws_switch_reset_bt_me_data(&ptrParam[1]);
       break;
    case BT_HCI:
        TRACE("receive bt hci data");
        app_tws_switch_reset_bt_hci_data(&ptrParam[1]);
        break;
    case BT_L2CAP:
        TRACE("receive bt l2cap data");
        app_tws_switch_reset_bt_l2cap_data(&ptrParam[1]);
        break;
    case BT_RFC:
        TRACE("receive bt rfc data");
        app_tws_switch_reset_bt_rfc_data(&ptrParam[1]);
        break;
    case HF_CONTEXT:
       //jjjj  TRACE("HF_CONTEXT_LENGTH = %d",sizeof(hfContext));
        break;
    case CMGR_CONTEXT:
        TRACE("receive cmgr data");
        app_tws_switch_reset_cmgrcontext_data(&ptrParam[1]);
        break;
    case AVRCP_CONTEXT:
        //jjjj TRACE("AVRCP_CONTEXT_LENGTH = %d",sizeof(avrcpContext));
        break;
    //case DEV_TABLE:TRACE("DEV_TABLE_LENGTH = %d",sizeof(devTable));break;
    case APP_BT_DEVICE:
        TRACE("receive app bt device");
        app_tws_switch_reset_app_bt_device_data(&ptrParam[1]);
        break;
    case AVDEV_CONTEXT:
        TRACE("receive avdev data");
        app_tws_switch_reset_avdev_data(&ptrParam[1]);
        break;
    case SLAVE_SAVE_DATA_OK:
        TRACE("slave save data ok");
        tws_switch_receive_slave_saved_data_ok(&ptrParam[1]);
        break;
    default:
        break;
    }
}

static void app_tws_role_switch_profile_exchange_handler(uint8_t* ptrParam, uint32_t paramLen)
{
    TRACE("%s",__func__);
    tws_parse_profile_data(ptrParam,paramLen);

}
TWS_CUSTOM_COMMAND_TO_ADD(APP_TWS_CMD_PROFILE_DATA_EXCHANGE, app_tws_role_switch_profile_exchange_handler);
#else
uint32_t complete_packet_during_simulate_reconnect = 0;
#endif

#if defined(__BQB_FUNCTION_CODE__)
void app_tws_free_src_stream(void)
{
    a2dp_stream_t *a2dp_source_stream;

    a2dp_source_stream = app_tws_get_tws_source_stream();
    if (a2dp_source_stream != NULL) {
        TRACE("%s goto a2dp source stream %d", __func__, a2dp_source_stream);
        btif_a2dp_deregister(a2dp_source_stream);
    } else {
        TRACE("%s goto a2dp source stream error %d", __func__, a2dp_source_stream);
    }
}
#endif

void app_tws_get_tws_slave_mobile_rssi(void)
{
    if ((app_tws_get_conn_state() != TWS_MASTER_CONN_SLAVE)||( !mobileBtRemDev))
    {
        TRACE("no connect mobile or slave,get slave2mobile rssi disallow");
        return;
    }
    btif_me_get_tws_slave_mobile_rssi(btif_me_get_remote_device_hci_handle(slaveBtRemDev));
}

int8_t app_tws_get_master_slave_rssi(void)
{
    uint8_t rssi=127;

    if ((app_tws_get_conn_state() == TWS_MASTER_CONN_SLAVE)&&app_tws_is_master_mode())
    {
         rssi = bt_drv_read_rssi_in_dbm(btif_me_get_remote_device_hci_handle(slaveBtRemDev));
         rssi =bt_drv_rssi_correction(rssi);
    }
    else
    {
        TRACE("no connect slave or I am slave,get slave rssi disallow");
    }
    
     return (int8_t)rssi;
}