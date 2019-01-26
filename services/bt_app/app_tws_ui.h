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
#ifndef __APP_BT_ROLE_SWITCH_MGR_H__
#define __APP_BT_ROLE_SWITCH_MGR_H__
#include "bluetooth.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
	TWS_MASTER_MUSIC_CONTROL_TYPE_NO_OP = 0,
	TWS_MASTER_MUSIC_CONTROL_TYPE_PAUSE,
	TWS_MASTER_MUSIC_CONTROL_TYPE_PLAY,
} TWS_MASTER_MUSIC_CONTROL_TYPE_E;

typedef struct
{
	uint8_t					bleBdAddr[BTIF_BD_ADDR_SIZE];
} app_tws_shared_tws_info_t;

typedef struct
{
	nvrec_btdevicerecord	peerRemoteDevInfo;
} app_tws_shared_mobile_info_t;

typedef struct
{
	uint8_t					bleBdAddr[BTIF_BD_ADDR_SIZE];
	bool                    isRemoteMobileInfoValid;
	nvrec_btdevicerecord	peerRemoteDevInfo;
} app_tws_shared_info_t;

typedef struct
{
	uint8_t					isToUpdateNv;
	uint8_t					bleBdAddr[BTIF_BD_ADDR_SIZE];
} app_tws_ble_addr_t;

typedef struct
{
	uint8_t					isRoleSwitchInProgress	: 1;
	uint8_t					isEarphoneweared		: 1;
	uint8_t					isPeerphoneweared		: 1;
	uint8_t					isMusicPlayingPending	: 1;
	uint8_t					isInitialReq			: 1;
	uint8_t					isWearedPreviously		: 1;
	uint8_t					isInChargerBox			: 1;
	uint8_t					reserve					: 1;
} app_tws_runtime_status_t;

typedef struct
{
	uint8_t					isRoleSwitchInProgress	: 1;
	uint8_t					isEarphoneweared		: 1;
	uint8_t					reserve					: 6;
} app_tws_role_switch_done_t;

typedef struct
{
	int32_t					a2dpVolume;
	int32_t					hfpVolume;
} app_tws_set_slave_volume_req_t;

typedef struct
{
	uint8_t					isMuteSourceMic;
} app_tws_control_source_mic_t;

typedef struct
{
	uint8_t					isInBox;
	uint8_t					isBoxOpened;
} app_tws_inbox_state_t;

typedef struct
{
	uint8_t					isEnableMusicPlaying;
} app_tws_music_playing_control_t;

typedef struct
{
	uint32_t				voicePrompt;
} app_tws_voice_prompt_to_play_t;

typedef struct
{
	uint8_t					data_valid;
	uint8_t					bat_level;
	uint8_t					isWearStatus;
	uint8_t 				isInBox;
} app_tws_low_battery_role_switch_status_t;

typedef struct {
    bool enable;
}APP_TWS_SET_EMSACK_T;

typedef struct {
    uint8_t retx_nb;
}APP_TWS_SHARE_RETX_T;

typedef void(*app_tws_pairing_preparation_done_callback)(void);
typedef void(*app_tws_pairing_done_callback)(void);
#ifdef LBRT

#define LBRT_DEBUG  1
#if LBRT_DEBUG
#define LBRT_PRINT(fmt, ...) TRACE(fmt, ##__VA_ARGS__)
#define LBRT_DUMP(s,buff,len) DUMP8(s,buff,len)
#else
#define LBRT_PRINT(fmt, ...)
#define LBRT_DUMP(s,buff,len)
#endif

#define LBRT_ENABLE                           1
#define LBRT_DISABLE                          0
#define BT_CONTROLLER_TICKS_TO_MS(tick)              ((uint32_t)((tick) * 312.5/1000))
#define BT_CONTROLLER_MS_TO_TICKS(ms)                ((uint32_t)((ms) * 1000/312.5))
#define APP_TWS_LBRT_TIMEOUT_IN_MS            300
#define APP_TWS_LBRT_SWITCH_TIME_IN_MS        100
#define APP_TWS_LBRT_TIMEOUT_IN_BT_TICKS      BT_CONTROLLER_MS_TO_TICKS(APP_TWS_LBRT_TIMEOUT_IN_MS + 100)

typedef struct {
    uint32_t ticks;
    uint8_t lbrt_en;
    uint8_t initial_req;
}APP_TWS_REQ_SET_LBRT_T;

typedef struct {
    uint32_t rsp_ticks;
}APP_TWS_RSP_LBRT_PING_T;

typedef struct {
    uint32_t req_ticks;
}APP_TWS_REQ_LBRT_PING_T;

bool app_tws_req_lbrt_ping(uint32_t current_ticks);
void app_set_lbrt_enable(bool enable);
bool app_get_lbrt_enable(void);
void app_tws_exit_lbrt_mode(void);
void app_tws_config_lbrt(uint16_t connHandle, uint8_t en);
#endif

#ifdef TWS_RING_SYNC
typedef struct {
    U16 hf_event;
    U32 trigger_time;
}APP_TWS_CMD_RING_SYNC_T;
#endif

void app_tws_clean_up_connections(void);
void app_tws_switch_role_and_reconnect_for_tws_normal_reconnecting(void);
#ifdef __TWS_ROLE_SWITCH__
void app_tws_switch_role_and_reconnect_for_tws_roleswitch(void);
void app_tws_switch_role_and_reconnect_for_tws_forcing_roleswitch(void);
#endif
void app_tws_send_shared_tws_info(void);
void app_tws_send_shared_mobile_dev_info(void);
void app_stop_supervisor_timers(void);
void app_tws_inform_out_of_box_event(void);
#ifdef FPGA
void app_tws_simulate_pairing(uint8_t type);
#else
void app_tws_simulate_pairing(void);
void app_tws_freeman_simulate_pairing(void);

bool app_bt_pairing_request_handler(void);
#endif
#ifdef LBRT
void app_tws_lbrt_simulate_pairing(void);
void app_tws_req_set_lbrt(uint8_t en, uint8_t initial_req, uint32_t ticks);
#endif
#ifdef __TWS_PAIR_DIRECTLY__
void app_tws_slave_enter_pairing(void);
void app_tws_master_enter_pairing(uint8_t* SlaveAddr);
void app_tws_reconnect_after_sys_startup(void);

#endif//__TWS_PAIR_DIRECTLY__

void app_disconnection_handler_before_role_switch(void);
void app_connect_op_after_role_switch(void);

// external APIs
void app_tws_peer_out_of_box_event_received_handler(void);
void app_tws_role_switch_by_wear_status(uint8_t status);
void app_tws_do_preparation_before_paring(void);
void app_tws_pairing_done(void);
bool app_tws_send_peer_earphones_wearing_status(bool isInitialReq);
void app_tws_set_slave_volume(int32_t a2dpvolume);
void app_tws_start_switch_source_mic(void);

#ifdef __ENABLE_IN_BOX_STATE_HANDLE__
void app_tws_put_in_charger_box(void);
void app_tws_out_of_charger_box(void);
void app_tws_charger_box_opened(void);
void app_tws_charger_box_closed(void);
void app_tws_charger_box_opened_connect_slave_timer_stop(void);
#endif
#ifdef __TWS_PAIR_DIRECTLY__
void app_tws_reconnect_slave_timer_stop(void);
void app_tws_reconnect_master_timer_stop(void);

#endif
void app_tws_roleswitch_done(uint8_t retStatus);
void app_tws_role_switch_started(void);

#ifdef __ENABLE_IN_BOX_STATE_HANDLE__
bool app_is_in_charger_box(void);
void app_set_in_charger_box_flag(bool isSet);
bool app_is_charger_box_closed(void);
void app_set_charger_box_closed_flag(bool isSet);
#endif
void app_tws_ui_init(void);

#ifdef __ENABLE_WEAR_STATUS_DETECT__
void app_tws_update_wear_status(bool isWeared);
#endif
void app_tws_pairing_preparation_done(void);
bool app_tws_inform_role_switch_done(void);
void app_tws_ui_reset_state(void);
void app_tws_ui_set_reboot_to_pairing(bool isRebootToPairing);

#ifdef __ENABLE_IN_BOX_STATE_HANDLE__
void app_tws_check_chargerbox_state_after_operation(void);
#endif
void app_tws_send_cmd_double_click(void);
void app_tws_let_master_control_music_playing(bool isEnable);
int app_tws_control_avrcp_media(uint8_t AvrcpMediaStatus);
void app_tws_control_music_playing(TWS_MASTER_MUSIC_CONTROL_TYPE_E controlType);
void app_tws_control_music_based_on_play_status(uint8_t play_status);
void app_tws_control_avrcp_media_remote_status_ind(uint8_t play_status);
#ifdef __ENABLE_WEAR_STATUS_DETECT__
void app_tws_check_if_need_to_switch_mic(void);
#endif
bool app_tws_is_mobile_dev_in_nv(uint8_t* pMobileAddr, int* ptrGetInt);
bool app_is_earphone_weared(void);
#ifdef __TWS_ROLE_SWITCH__
void app_tws_force_change_role(void);
#endif
void app_tws_ask_slave_to_play_voice_prompt(uint32_t voicePrompt);
bool app_tws_ui_is_in_repairing(void);
void app_tws_check_pending_role_switch(void);
#ifdef __ENABLE_WEAR_STATUS_DETECT__
void app_tws_syn_wearing_status(void);
#endif
void app_tws_process_hfp_disconnection_event_during_role_switch(bt_bdaddr_t* bdAddr);
uint8_t app_tws_get_report_level(void);
#ifdef __ENABLE_IN_BOX_STATE_HANDLE__
void app_tws_inform_in_box_state(void);
void app_tws_execute_charger_box_closed_operation(void);
#endif
bool app_tws_is_bt_roleswitch_in_progress(void);
void app_tws_event_exchange_supervisor_timer_cancel(void);
bool app_tws_set_emsack_mode(bool enable);
void app_tws_set_emsack_mode_timer(uint32_t timeoutInMs);
bool app_tws_share_esco_retx_nb(uint8_t retx_nb);
void simulate_hci_get_tws_slave_mobile_rssi(void);
int8_t app_tws_get_master_slave_rssi(void);

#ifdef __TWS_ROLE_SWITCH__

void tws_role_switch_exchange_profile_data(uint32_t arg);
void tws_parse_profile_data(uint8_t* ptrParam, uint32_t paramLen);
void tws_reset_app_bt_device_streaming(void);
void tws_role_switch_send_all_profile_data(void);
#endif
#ifdef __cplusplus
}
#endif


#endif // __APP_BT_ROLE_SWITCH_MGR_H__

