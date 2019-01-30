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
#ifndef __APP_BT_H__
#define __APP_BT_H__
#include "me_api.h"
#ifdef __cplusplus
extern "C" {
#endif
#define IS_ENABLE_BT_SNIFF_MODE		1

#define RECONNECT_MODE   0
#define DISCONNECT_MODE  1
#define NONE_MODE        2

enum APP_BT_REQ_T {
    APP_BT_REQ_ACCESS_MODE_SET,
    APP_BT_REQ_CONNECT_PROFILE,
    APP_BT_REQ_CUSTOMER_CALL,
    APP_BT_REQ_BT_THREAD_CALL,
    APP_BT_REQ_NUM
};

typedef enum
{
    BT_PROFILE_A2DP = 0,
    BT_PROFILE_HFP,
    BT_PROFILE_HSP
} BT_PROFILE_TYPE_E;


typedef void (*APP_BT_REQ_CUSTOMER_CALl_CB_T)(void *, void *);


typedef void (*APP_BT_REQ_CONNECT_PROFILE_FN_T)(void *, void *);
#define app_bt_accessmode_set_req(accmode) do{app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, accmode, 0, 0);}while(0)
#define app_bt_connect_profile_req(fn,param0,param1) do{app_bt_send_request(APP_BT_REQ_CONNECT_PROFILE, (uint32_t)param0, (uint32_t)param1, (uint32_t)fn);}while(0)

void PairingTransferToConnectable(void);

void app_bt_golbal_handle_init(void);

void app_bt_opening_reconnect(void);

void app_bt_accessmode_set(btif_accessible_mode_t mode);

void app_bt_set_linkpolicy(btif_remote_device_t *remDev, btif_link_policy_t policy);

void app_bt_accessmode_get(btif_accessible_mode_t *mode);

void app_bt_accessmode_set_scaninterval(uint32_t scan_intv);

void app_bt_send_request(uint32_t message_id, uint32_t param0, uint32_t param1, uint32_t ptr);

void app_bt_init(void);
void app_bt_disconnect_all_mobile_devces(void);
void app_bt_disconnect_sco_link(void);
int app_bt_state_checker(void);

void *app_bt_profile_active_store_ptr_get(uint8_t *bdAddr);

void app_bt_profile_connect_manager_open(void);

void app_bt_profile_connect_manager_opening_reconnect(void);

bt_bdaddr_t* app_bt_get_peer_remote_device_bd_addr(uint8_t deviceId);

bt_status_t app_bt_execute_connecting_mobile_device(uint8_t* pBdAddr);

void app_bt_open_mobile_a2dp_stream(uint32_t deviceId);

void app_bt_open_mobile_hfp_stream(uint32_t deviceId);

void app_bt_set_peer_remote_device_bd_addr(uint8_t deviceId, bt_bdaddr_t* pBtAddr);

void app_bt_get_mobile_device_active_profiles(uint8_t* bdAddr);

void app_bt_suspend_mobile_a2dp_stream(uint32_t deviceId);

void app_bt_cancel_connecting(uint32_t deviceId);

void app_tws_reconnect_stop_music(void);

void app_bt_hcireceived_data_clear(uint16_t conhdl);

void app_bt_disconnect_link(btif_remote_device_t* remDev);

void app_bt_connect_hfp(uint32_t deviceId, bt_bdaddr_t *addr);

void app_bt_connect_avrcp(uint32_t deviceId, bt_bdaddr_t *addr);

bool app_bt_is_hfp_audio_on(void);

bool app_bt_is_source_avrcp_connected(void);

bt_status_t app_bt_disconnect_source_avrcp(void);

//bt_status_t app_bt_disconnect_hfp(void);

void app_start_custom_function_in_app_thread(uint32_t param0, uint32_t param1, uint32_t ptr);

void app_start_custom_function_in_bt_thread(uint32_t param0, uint32_t param1, uint32_t ptr);

void app_bt_cleanup_mobile_cmgrhandler(void);

bool app_bt_is_avrcp_operation_available(void);

void app_check_pending_stop_sniff_op(void);

void app_tws_stop_sniff(void);

bool app_bt_is_tws_role_switch_timeout(void);

bool app_is_custom_func_in_app_thread_ready(void);

void app_bt_start_tws_reconnect_hfp_timer(void);

void app_bt_stop_tws_reconnect_hfp_timer(void);
void app_connect_a2dp_mobile(uint8_t *pBdAddr);

void app_acl_sniff_sco_conflict_check(void);

void fast_pair_enter_pairing_mode_handler(void);

uint8_t app_bt_get_num_of_connected_dev(void);

bool app_bt_is_device_connected(uint8_t deviceId);

bool app_bt_get_device_bdaddr(uint8_t deviceId, uint8_t* btAddr);

void hfp_service_connected_set(bool on);
bool app_is_hfp_service_connected(void);
void a2dp_service_connected_set(bool on);
bool a2dp_service_is_connected(void);
bool app_is_link_connected();bool btapp_hfp_is_sco_active(void);
bool btapp_is_bt_active(void);

#ifdef __cplusplus
}
#endif
#endif /* BESBT_H */
