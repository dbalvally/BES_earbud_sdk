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
#ifndef __APP_BT_CONN_MGR_H__
#define __APP_BT_CONN_MGR_H__
#include "nvrecord_env.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CONNECTION_CREATION_SUPERVISOR_RETRY_LIMIT (1)
#define CONNECTING_SLAVE_TIMEOUT_RECONNECT_IN_MS                        (5000 + 2000 + CONNECTION_CREATION_SUPERVISOR_RETRY_LIMIT*5000 + CONNECTION_CREATION_SUPERVISOR_RETRY_LIMIT*2000)
// in charger box, it's interleaved "reconnect slave + pending for ble adv/scan window"
#define CONNECTING_SLAVE_TIMEOUT_RECONNECT_IN_CHARGER_BOX_MS            (5000 + 2000 + CONNECTION_CREATION_SUPERVISOR_RETRY_LIMIT*5000 + CONNECTION_CREATION_SUPERVISOR_RETRY_LIMIT*2000)
//only for FPGA pairing
#define CONNECTING_SLAVE_TIMEOUT_FOR_FPGA  (600000)
#define WAITING_CONNECTION_FROM_TWS_MASTER_FOR_FPGA    (600000)

#define WAITING_CONNECTION_FROM_TWS_MASTER_TIMEOUT_IN_MS    10000
// the whole connecting lasting time is 10 min
#define CONNECTING_MOBILE_DEV_TIMEOUT_IN_MS                 600000  
#define ONE_MINITUE_IN_MS                                   60000
// after 5s connecting try, will enter the idle mode for 20s
#define CONNECTING_MOBILE_IDLE_TIME_IN_MS                   20000

// if a2dp is not opened within 8s after link is created, disconnect the mobile by force
#define CONNECTING_MOBILE_SUPERVISOR_TIMEOUT_IN_MS          8000

#define RECONNECT_MOBILE_TRY_TIMES                          8

#define APP_TWS_CONNECTING_BLE_TIME_OUT_IN_MS               8000
#define TWS_SHARING_TWS_INFO_SUPERVISOR_TIMEOUT_IN_MS       5000
#define TWS_SHARING_PAIRING_INFO_SUPERVISOR_TIMEOUT_IN_MS   5000

#define ROLE_SWITCH_TIMEOUT_IN_MS                           15000

#define DEV_OPERATION_OPEN_CHECK_MS                           100
#define DEV_OPERATION_CLOSE_CHECK_MS                           100

#define BOX_OPERATION_OPEN_CHECK_MS                           100
#define BOX_OPERATION_CLOSE_CHECK_MS                           3000

#define BOX_OPERATION_DELAY_CHECK_MS                           300

typedef enum
{
    BT_FUNCTION_ROLE_MOBILE = 0,
    BT_FUNCTION_ROLE_MASTER,
    BT_FUNCTION_ROLE_SLAVE
} BT_FUNCTION_ROLE_E;

typedef enum
{
    BT_SOURCE_CONN_MOBILE_DEVICE = 0,
    BT_SOURCE_CONN_MASTER,
} BT_CONNECTION_REQ_SOURCE_E;

typedef enum
{
    CONN_OP_STATE_MACHINE_NORMAL,
    CONN_OP_STATE_MACHINE_DISCONNECT_ALL,
    CONN_OP_STATE_MACHINE_DISCONNECT_ALL_BEFORE_RE_PAIRING,    
    CONN_OP_STATE_MACHINE_DISCONNECT_SLAVE,
    CONN_OP_STATE_MACHINE_RE_PAIRING,
    CONN_OP_STATE_MACHINE_ROLE_SWITCH,
    CONN_OP_STATE_MACHINE_DISCONNECT_ALL_BEFORE_CHANGE_ROLE,
    CONN_OP_STATE_MACHINE_DISCONNECT_MOBILE_BEFORE_SIMPLE_REPAIRING,
    CONN_OP_STATE_MACHINE_SIMPLE_REPAIRING,
    
} CONN_OP_STATE_MACHINE_TYPE_E;

typedef enum
{
    CONN_OP_STATE_IDLE = 0,
    CONN_OP_STATE_WAITING_FOR_MOBILE_CONNECTION,
    CONN_OP_STATE_WAITING_FOR_MASTER_CONNECTION,
    CONN_OP_STATE_CONNECTING_SLAVE,
    CONN_OP_STATE_CONNECTING_MASTER,
    CONN_OP_STATE_CONNECTING_MOBILE,
    
} CONN_OP_STATE_E;

#define IS_WAITING_FOR_MOBILE_CONNECTION() (conn_op_state == CONN_OP_STATE_WAITING_FOR_MOBILE_CONNECTION)
#define IS_WAITING_FOR_MASTER_CONNECTION() (conn_op_state == CONN_OP_STATE_WAITING_FOR_MASTER_CONNECTION)
#define IS_CONNECTING_SLAVE()       (conn_op_state == CONN_OP_STATE_CONNECTING_SLAVE)
#define IS_CONNECTING_MASTER()       (conn_op_state == CONN_OP_STATE_CONNECTING_MASTER)
#define IS_CONNECTING_MOBILE()      (conn_op_state == CONN_OP_STATE_CONNECTING_MOBILE)
#define IS_NO_PENDING_CONNECTION()  (conn_op_state == CONN_OP_STATE_IDLE)

#define CONN_STATE_LINK_BIT_OFFSET  0
#define CONN_STATE_A2DP_BIT_OFFSET  1
#define CONN_STATE_HFP_BIT_OFFSET   2
#define CONN_STATE_HSP_BIT_OFFSET   3

#define CLEAR_CONN_STATE_LINK(conn_state)   do { ((conn_state) &= (~(1 << CONN_STATE_LINK_BIT_OFFSET))); TRACE("conn_state %d at line %d", conn_state, __LINE__); } while (0);
#define CLEAR_CONN_STATE_A2DP(conn_state)   do { ((conn_state) &= (~(1 << CONN_STATE_A2DP_BIT_OFFSET))); TRACE("conn_state %d at line %d", conn_state, __LINE__); } while (0);
#define CLEAR_CONN_STATE_HFP(conn_state)    do { ((conn_state) &= (~(1 << CONN_STATE_HFP_BIT_OFFSET))); TRACE("conn_state %d at line %d", conn_state, __LINE__); } while (0);
#define CLEAR_CONN_STATE_HSP(conn_state)    do { ((conn_state) &= (~(1 << CONN_STATE_HSP_BIT_OFFSET))); TRACE("conn_state %d at line %d", conn_state, __LINE__); } while (0);

#define SET_CONN_STATE_LINK(conn_state)     do { ((conn_state) |= (1 << CONN_STATE_LINK_BIT_OFFSET)); TRACE("conn_state %d at line %d", conn_state, __LINE__); } while (0);
#define SET_CONN_STATE_A2DP(conn_state)     do { ((conn_state) |= ((1 << CONN_STATE_A2DP_BIT_OFFSET) | (1 << CONN_STATE_LINK_BIT_OFFSET))); TRACE("conn_state %d at line %d", conn_state, __LINE__); } while (0);
#define SET_CONN_STATE_HFP(conn_state)      do { ((conn_state) |= ((1 << CONN_STATE_HFP_BIT_OFFSET) | (1 << CONN_STATE_LINK_BIT_OFFSET))); TRACE("conn_state %d at line %d", conn_state, __LINE__); } while (0);
#define SET_CONN_STATE_HSP(conn_state)      do { ((conn_state) |= ((1 << CONN_STATE_HSP_BIT_OFFSET) | (1 << CONN_STATE_LINK_BIT_OFFSET))); TRACE("conn_state %d at line %d", conn_state, __LINE__); } while (0);

#define IS_JUST_LINK_ON(conn_state)           ((1 << CONN_STATE_LINK_BIT_OFFSET) == (conn_state))
#define IS_CONN_STATE_LINK_ON(conn_state)     ((conn_state) & (1 << CONN_STATE_LINK_BIT_OFFSET))
#define IS_CONN_STATE_A2DP_ON(conn_state)     ((conn_state) & (1 << CONN_STATE_A2DP_BIT_OFFSET))
#define IS_CONN_STATE_HFP_ON(conn_state)      ((conn_state) & (1 << CONN_STATE_HFP_BIT_OFFSET))
#define IS_CONN_STATE_HSP_ON(conn_state)      ((conn_state) & (1 << CONN_STATE_HSP_BIT_OFFSET))

#define IS_CONNECTED_WITH_MOBILE_PROFILE()  (mobileDevConnectionState > (1 << CONN_STATE_LINK_BIT_OFFSET))
#define IS_CONNECTED_ONLY_MOBILE()          (IS_CONN_STATE_LINK_ON(mobileDevConnectionState) && !IS_CONN_STATE_LINK_ON(twsConnectionState))
            
#define CONN_STATE_TWS_ALL_CONNECTED  ((1 << CONN_STATE_LINK_BIT_OFFSET) | (1 << CONN_STATE_A2DP_BIT_OFFSET))
#define IS_CONNECTED_WITH_TWS_PROFILE()     (twsConnectionState > (1 << CONN_STATE_LINK_BIT_OFFSET))
#define IS_CONNECTED_WITH_ALL_TWS_PROFILES()    (CONN_STATE_TWS_ALL_CONNECTED == twsConnectionState)

#define IS_CONNECTED_WITH_MOBILE()          (mobileDevConnectionState > 0)
#define IS_CONNECTED_WITH_TWS()             (twsConnectionState > 0)

#define CONN_STATE_MOBILE_ALL_CONNECTED  ((1 << CONN_STATE_LINK_BIT_OFFSET) | (1 << CONN_STATE_A2DP_BIT_OFFSET) |   \
                                            (1 << CONN_STATE_HFP_BIT_OFFSET))

#define IS_CONNECTED_WITH_ALL_MOBILE_PROFILES() (CONN_STATE_MOBILE_ALL_CONNECTED == mobileDevConnectionState)

#define CURRENT_TWS_CONN_STATE()            twsConnectionState
#define CURRENT_MOBILE_DEV_CONN_STATE()     mobileDevConnectionState
#define CURRENT_CONN_OP_STATE_MACHINE()     conn_op_state_machine_type

#define CLEAR_MOBILE_DEV_ACTIVE_PROFILE()           (mobileDevActiveProfiles = 0)
#define SET_MOBILE_DEV_ACTIVE_PROFILE(profileType)  ((mobileDevActiveProfiles) |= (1 << (profileType)))
#define IS_MOBILE_DEV_PROFILE_ACTIVE(profileType)   ((mobileDevActiveProfiles) & (1 << (profileType)))

extern CONN_OP_STATE_MACHINE_TYPE_E conn_op_state_machine_type;
extern CONN_OP_STATE_E              conn_op_state;

extern uint32_t twsConnectionState;
extern uint32_t mobileDevConnectionState;
extern uint32_t mobileDevActiveProfiles;

extern bt_bdaddr_t                   mobileDevBdAddr;
extern bool                         isMobileDevPaired;
extern bool switcherBetweenReconnMobileAndTws;
// no connection, no pairing/inquiry scan
extern uint8_t isInMobileBtSilentState; 

extern btif_remote_device_t* masterBtRemDev;
extern btif_remote_device_t* slaveBtRemDev;
extern btif_remote_device_t* mobileBtRemDev;


typedef void (*TWS_CONNECTION_CREATION_SUPERVISOR_TIMER_HANDLER)(void);

typedef void (*MOBILE_CONNECTION_CREATION_SUPERVISOR_TIMER_HANDLER)(void);

inline void set_conn_op_state_machine(CONN_OP_STATE_MACHINE_TYPE_E type)
{
    TRACE("Connection operation state machine is switched from %d to type %d", conn_op_state_machine_type, type);
    conn_op_state_machine_type = type;
}

inline void set_conn_op_state(CONN_OP_STATE_E state)
{
    TRACE("Connection operation state is switched from %d to %d", conn_op_state, state);
    conn_op_state = state;
    
}

void conn_mgr_init(void);
void tws_connection_state_updated_handler(uint8_t errorType);
void mobile_device_connection_state_updated_handler(uint8_t errorType);

void conn_start_connecting_mobile_device(void);
void conn_start_connecting_slave(uint32_t timeoutInMs, bool firstConnect);
void conn_start_connectable_only_pairing(BT_CONNECTION_REQ_SOURCE_E conn_req_source);
void conn_start_connecting_master(uint32_t timeoutInMs);

void conn_system_boot_up_handler(void);
void conn_start_tws_connection_creation_supvervisor_timer(uint32_t timerIntervalInMs, TWS_CONNECTION_CREATION_SUPERVISOR_TIMER_HANDLER callback);
void conn_stop_tws_connection_creation_supvervisor_timer(void);
void conn_start_mobile_connection_creation_supvervisor_timer(
    uint32_t timerIntervalInMs, MOBILE_CONNECTION_CREATION_SUPERVISOR_TIMER_HANDLER callback);
void conn_stop_mobile_connection_creation_supvervisor_timer(void);
void conn_stop_tws_ble_connection_creation_supvervisor_timer(void);
void conn_stop_discoverable_and_connectable_state(void);
void conn_start_tws_ble_connection_creation_supvervisor_timer(uint32_t timerIntervalInMs);
void conn_remove_all_history_paired_devices(void);
void conn_remove_tws_history_paired_devices(void);
bool app_bt_conn_is_any_connection_on(void);
void app_bt_cancel_ongoing_connecting(void);
void start_tws_sharing_tws_info_timer(void);
void stop_tws_sharing_tws_info_timer(void);
void start_tws_sharing_pairing_info_timer(void);
void stop_tws_sharing_pairing_info_timer(void);
bool is_tws_connection_creation_supervisor_need_retry(void);
void tws_connection_creation_supervisor_retry_cnt_reset(void);
bool is_tws_connection_creation_supervisor_timer_on_going(void);
bool is_mobile_connection_creation_supervisor_timer_on_going(void);

void conn_bt_disconnect_all(void);

void conn_tws_player_stopped_handler(void);
void conn_ble_reconnection_machanism_handler(bool isForceStartMasterAdv);
bool conn_tws_is_initiative_music_pause_not_allowed(void);
void conn_start_role_switch_supvervisor_timer(uint32_t timerIntervalInMs);
void conn_stop_role_switch_supvervisor_timer(void);
bool conn_isA2dpVolConfiguredByMaster(void);
void conn_set_flagofA2dpVolConfiguredByMaster(bool isDone);
void conn_start_reconnecting_on_mobile_disconnection(void);
void conn_get_mobile_dev_addr(void);
void tws_slave_update_sniffer_in_sco_state(void);
void tws_slave_stop_sco_sniffering(void);
void tws_master_send_max_slot_req_to_slave(void);
void conn_connecting_mobile_handler(void);

typedef void (*BOX_OPERATION_STATUS_CHECK_HANDLER)(void);
void box_operation_stop_check_timer(void);
void box_operation_start_check_timer(int open,int delaycheck,BOX_OPERATION_STATUS_CHECK_HANDLER cb);
void dev_operation_stop_check_timer(void);
void dev_operation_start_check_timer(int dir,int delaycheck,BOX_OPERATION_STATUS_CHECK_HANDLER cb);

bool app_conn_check_and_restore_op_state(void);
bool app_conn_check_current_operation_state(void);
void connecting_tws_slave_timeout_handler(void);
void connecting_mobile_timeout_handler(void);
bool execute_connecting_slave(uint32_t timeoutInMs);
bool execute_connecting_mobile(uint8_t* pBdAddr);
bool app_conn_is_pending_for_op_state_check(void);

void conn_start_mobile_connection_creation_idle_timer(void);
void conn_stop_mobile_connection_creation_idle_timer(void);
bool is_connecting_mobie_idle_timer_on(void);
bool box_operation_timer_on(void);

bool app_conn_is_mobile_connection_down_initiatively(void);
void app_conn_set_mobile_connection_down_reason(bool isDownInitiatively);
bool app_conn_is_to_reset_mobile_connection(void);
void app_conn_set_reset_mobile_connection_flag(bool isToReset);
void app_pending_for_idle_state_timeout_handler(bool isAllowToContinue);

void conn_start_connecting_mobile_supervising(void);
void conn_stop_connecting_mobile_supervising(void);
void app_resume_mobile_air_path_and_notify_switch_success(void);
void app_tws_config_master_wsco_by_connect_state(bool state, uint8_t mode);
 btif_remote_device_t*   app_bt_conn_mgr_get_mobileBtRemDev(void);
#ifdef __cplusplus
}
#endif

#endif // __APP_BT_CONN_MGR_H__

