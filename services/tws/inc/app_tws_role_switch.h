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
#ifndef __APP_TWS_ROLE_SWITCH_H__
#define __APP_TWS_ROLE_SWITCH_H__

#include "co_bt_defines.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "app_tws_ui.h"

#define TWS_ROLE_SWITCH_DEBUG   1
#define APP_SYSFREQ_APP_ROLE_SWITCH            APP_SYSFREQ_USER_APP_7

#define TWS_PAIRING_INFO_MAGIC_NUMBER_INIT_XFER		0x12341234
#define TWS_PAIRING_INFO_MAGIC_NUMBER_FEEDBACK_XFER	0x56785678

#define HCI_INQUIRY_COMP_EVT                0x01
#define HCI_INQUIRY_RESULT_EVT              0x02
#define HCI_CONNECTION_COMP_EVT             0x03
#define HCI_DISCONNECTION_COMP_EVT          0x05
#define HCI_AUTHENTICATION_COMP_EVT         0x06
#define HCI_RMT_NAME_REQUEST_COMP_EVT       0x07
#define HCI_ENCRYPTION_CHANGE_EVT           0x08
#define HCI_CHANGE_CONN_LINK_KEY_EVT        0x09
#define HCI_MASTER_LINK_KEY_COMP_EVT        0x0A
#define HCI_READ_RMT_FEATURES_COMP_EVT      0x0B
#define HCI_READ_RMT_VERSION_COMP_EVT       0x0C
#define HCI_QOS_SETUP_COMP_EVT              0x0D
#define HCI_COMMAND_COMPLETE_EVT            0x0E
#define HCI_COMMAND_STATUS_EVT              0x0F
#define HCI_HARDWARE_ERROR_EVT              0x10
#define HCI_FLUSH_OCCURED_EVT               0x11
#define HCI_ROLE_CHANGE_EVT                 0x12
#define HCI_NUM_COMPL_DATA_PKTS_EVT         0x13
#define HCI_MODE_CHANGE_EVT                 0x14
#define HCI_RETURN_LINK_KEYS_EVT            0x15
#define HCI_PIN_CODE_REQUEST_EVT            0x16
#define HCI_LINK_KEY_REQUEST_EVT            0x17
#define HCI_LINK_KEY_NOTIFICATION_EVT       0x18
#define HCI_LOOPBACK_COMMAND_EVT            0x19
#define HCI_DATA_BUF_OVERFLOW_EVT           0x1A
#define HCI_MAX_SLOTS_CHANGED_EVT           0x1B
#define HCI_READ_CLOCK_OFF_COMP_EVT         0x1C
#define HCI_CONN_PKT_TYPE_CHANGE_EVT        0x1D
#define HCI_QOS_VIOLATION_EVT               0x1E
#define HCI_PAGE_SCAN_MODE_CHANGE_EVT       0x1F
#define HCI_PAGE_SCAN_REP_MODE_CHNG_EVT     0x20
#define HCI_FLOW_SPECIFICATION_COMP_EVT     0x21
#define HCI_INQUIRY_RSSI_RESULT_EVT         0x22
#define HCI_READ_RMT_EXT_FEATURES_COMP_EVT  0x23
#define HCI_ESCO_CONNECTION_COMP_EVT        0x2C
#define HCI_ESCO_CONNECTION_CHANGED_EVT     0x2D
#define HCI_SNIFF_SUB_RATE_EVT              0x2E
#define HCI_EXTENDED_INQUIRY_RESULT_EVT     0x2F
#define HCI_ENCRYPTION_KEY_REFRESH_COMP_EVT 0x30
#define HCI_IO_CAPABILITY_REQUEST_EVT       0x31
#define HCI_IO_CAPABILITY_RESPONSE_EVT      0x32
#define HCI_USER_CONFIRMATION_REQUEST_EVT   0x33
#define HCI_USER_PASSKEY_REQUEST_EVT        0x34
#define HCI_REMOTE_OOB_DATA_REQUEST_EVT     0x35
#define HCI_SIMPLE_PAIRING_COMPLETE_EVT     0x36
#define HCI_LINK_SUPER_TOUT_CHANGED_EVT     0x38
#define HCI_ENHANCED_FLUSH_COMPLETE_EVT     0x39
#define HCI_USER_PASSKEY_NOTIFY_EVT         0x3B
#define HCI_KEYPRESS_NOTIFY_EVT             0x3C
#define HCI_RMT_HOST_SUP_FEAT_NOTIFY_EVT    0x3D

/*#define HCI_GENERIC_AMP_LINK_KEY_NOTIF_EVT  0x3E Removed from spec */
#define HCI_PHYSICAL_LINK_COMP_EVT          0x40
#define HCI_CHANNEL_SELECTED_EVT            0x41
#define HCI_DISC_PHYSICAL_LINK_COMP_EVT     0x42
#define HCI_PHY_LINK_LOSS_EARLY_WARNING_EVT 0x43
#define HCI_PHY_LINK_RECOVERY_EVT           0x44
#define HCI_LOGICAL_LINK_COMP_EVT           0x45
#define HCI_DISC_LOGICAL_LINK_COMP_EVT      0x46
#define HCI_FLOW_SPEC_MODIFY_COMP_EVT       0x47
#define HCI_NUM_COMPL_DATA_BLOCKS_EVT       0x48
#define HCI_SHORT_RANGE_MODE_COMPLETE_EVT   0x4C
#define HCI_AMP_STATUS_CHANGE_EVT           0x4D
#define HCI_SET_TRIGGERED_CLOCK_CAPTURE_EVT 0x4E

#define HCI_DBG_EVT							0xFF

#define MAX_ROLE_SWITCH_BT_INFO_COUNT       2 

#define BD_ADDRESS_LENGTH			6

#define HCI_PACKET_COUNT 			5
#define HCI_CONN_HANDLER_INDEX_MASK 0x7F
#define HCI_EVENT_HEADER_LEN    	3

#define HCI_MAX_AVDTP_EP_NUM		3

typedef enum
{
    SIMULATE_RECONNECT_STATE_IDLE = 0,	
	TO_DISCONNECT_BEFORE_ROLE_SWITCH,
	TO_PREPARE_ROLE_SWITCH,

	/* simulation phases */
	TO_CONNECT_WITH_SLAVE,
	TO_CONNECT_WITH_MOBILE,
	TO_CONNECT_WITH_MASTER,
	
} HCI_SIMULATE_RECONNECT_STATE_E;

typedef enum
{
    LINK_CONN_REQ_RECEIVED,
	AVDTP_CONN_REQ_RECEIVED,
	AVDTP_DISCOVER_REQ_RECEIVED,
	AVDTP_GET_ALL_CAP_REQ_RECEIVED,
	AVDTP_SET_CONFIG_REQ_RECEIVED,
	AVDTP_OPEN_REQ_RECEIVED
	
} HCI_SIMULATE_RECONNECT_MASTER_STATE_E;


/// LC command complete event structure for create connection
typedef struct 
{
    /// Status
    uint8_t             status;
    ///Connection handle
    uint16_t            conhdl;
    ///Bluetooth Device address
    uint8_t     		bd_addr[BD_ADDRESS_LENGTH];
    ///Link type
    uint8_t             link_type;
    ///Encryption state
    uint8_t             enc_en;
} __attribute__((__packed__)) lc_con_cmp_evt_t;

/// LC disconnect complete event structure
typedef struct 
{
    ///Status of received command
    uint8_t     status;
    ///Connection Handle
    uint16_t    conhdl;
    ///Reason for disconnection
    uint8_t     reason;
} __attribute__((__packed__)) lc_disc_evt_complete_t;

/// LC max slot change event structure
typedef struct 
{
    ///Connection handle
    uint16_t conhdl;
    ///Max slot
    uint8_t max_slot;
} __attribute__((__packed__)) lc_max_slot_chg_evt_t;

/// LC role change event parameters structure
typedef struct 
{
    ///Status
    uint8_t status;
    ///BD address
    uint8_t bd_addr[BD_ADDRESS_LENGTH];
    ///New role
    uint8_t new_role;
} __attribute__((__packed__)) lc_role_chg_evt_t;

/// LC command complete event structure for the read remote information version command
typedef struct 
{
    ///Status for command reception
    uint8_t status;
    ///Connection handle
    uint16_t conhdl;
    ///LMP version
    uint8_t     vers;
    ///Manufacturer name
    uint16_t    compid;
    ///LMP subversion
    uint16_t    subvers;
} __attribute__((__packed__)) lc_rd_rem_info_ver_cmp_evt_t;

/// LC command complete event structure for the read remote supported features command
typedef struct 
{
    ///Status for command reception
    uint8_t status;
    ///Connection handle
    uint16_t conhdl;
    ///Remote features
    struct features rem_feats;

} __attribute__((__packed__)) lc_rd_rem_supp_feats_cmp_evt_t;

/// LC read remote extended features complete event parameters structure
typedef struct 
{
    ///Status for command reception
    uint8_t status;
    ///Connection handle
    uint16_t conhdl;
    ///page number
    uint8_t pg_nb;
    ///page number max
    uint8_t pg_nb_max;
    ///ext LMP features
    struct features ext_feats;

} __attribute__((__packed__)) lc_rd_rem_ext_feats_cmp_evt_t;

/// LC link supervision timeout change event structure
typedef struct 
{
    ///Connection handle
    uint16_t    conhdl;
    ///Link supervision timeout
    uint16_t    lsto_val;
} __attribute__((__packed__)) lc_link_sup_to_chg_evt_t;


/// LC link key request event structure
typedef struct 
{
    ///BD address
    uint8_t 	bd_addr[BD_ADDRESS_LENGTH];
} __attribute__((__packed__)) lc_lk_key_req_evt_t;

/// LC encryption change event structure
typedef struct 
{
    ///Status for command reception
    uint8_t status;
    ///Connection handle
    uint16_t conhdl;
    ///Encryption enabled information
    uint8_t     enc_stat;
} __attribute__((__packed__)) lc_enc_change_evt_t;

/// LC Authentication complete event structure
typedef struct 
{
    ///Status for command reception
    uint8_t status;
    ///Connection handle
    uint16_t    conhdl;
} __attribute__((__packed__)) lc_auth_cmp_evt_t;

/// LC number of packet complete event structure
typedef struct 
{
    uint8_t     nb_of_hdl;
    uint16_t    conhdl[1];
    uint16_t    nb_comp_pkt[1];
} __attribute__((__packed__)) lc_nb_of_pkt_cmp_evt_t;

typedef struct
{
    uint8_t     isValid 					: 1;
    uint8_t     isAvdtpPsmPacketInParsing	: 1;
	uint8_t     isAvctpPsmPacketInParsing	: 1;
	uint8_t     isSdpPsmPacketInParsing		: 1;
    uint8_t     isAvctpConnected			: 1;
	uint8_t		isSdpConnected				: 1;
    uint8_t     isAvdtpConnected			: 1;
	uint8_t		reserve 					: 1;

	uint8_t		endpointNum;
	uint8_t		respondedEndpointCnt;
    uint8_t     maxSlot;
    uint8_t     role;
	uint16_t    connHandle;
    uint16_t    linkSupervisonTimeout;  // for tws, assign it as the default value
    uint8_t     bdAddr[BD_ADDRESS_LENGTH];

    uint8_t     avdtpIdentifier;
	uint8_t     avctpIdentifier;
	uint8_t     sdpIdentifier;
    uint8_t     signalingPacketCode;

    uint16_t  	avdtpLocalCId;
	uint16_t    avdtpRemoteCId;

    uint16_t    avctpLocalCId;
	uint16_t    avctpRemoteCId;

	uint16_t	sdpSrcCId;
	uint16_t	sdpDstCId;
	uint16_t	sdpAudioType;
	uint8_t		recordedAvdtpChannelPairsCnt;
	uint8_t		allocatedAvdtpChannelCount;
	uint16_t    avdtpLocalChannelForSignaling;
	uint16_t    avdtpRemoteChannelForSignaling;
	uint16_t    avdtpLocalChannelForMedia;
	uint16_t    avdtpRemoteChannelForMedia;
	uint8_t		localA2dpStreamId;
	uint8_t		remoteA2dpStreamId;

	uint8_t		backupAcpA2dpStreamId;
	uint8_t		backupIntA2dpStreamId;
	
	uint8_t		audioSrcSdpRsp[128];
	uint8_t		audioSrcSdpRspLen;
	uint8_t		audioSinkSdpRsp[128];
	uint8_t		audioSinkSdpRspLen;
	uint8_t     avdtp_get_cap_rsp[HCI_MAX_AVDTP_EP_NUM][28];
    uint8_t     avdtp_get_cap_rsp_len[HCI_MAX_AVDTP_EP_NUM];
	uint8_t     avdtp_get_cap_rsp_endpoint[HCI_MAX_AVDTP_EP_NUM];
	uint8_t		avdtp_get_cap_rsp_endpoint_cnt;
    uint8_t     avdtp_set_config_rsp[32];
    uint8_t     avdtp_set_config_rsp_len;
	uint8_t     avdtp_set_config_req[32];
    uint8_t     avdtp_set_config_req_len;
    uint8_t     avdtp_open_rsp[32];
    uint8_t     avdtp_open_rsp_len;
    uint8_t     avdtp_delayreport_rsp[32];
    uint8_t     avdtp_delayreport_rsp_len;
    lc_rd_rem_info_ver_cmp_evt_t        rd_rem_info_ver_cmp_evt;
    lc_rd_rem_supp_feats_cmp_evt_t      rd_rem_supp_feats_cmp_evt;
    lc_rd_rem_ext_feats_cmp_evt_t       rd_rem_ext_feats_cmp_evt;
} __attribute__((__packed__)) TWS_ROLE_SWITCH_BT_INFO_T;

typedef enum
{
    RESTORE_THE_ORIGINAL_PLAYING_STATE = 0,
    START_PLAYING,
    STOP_PLAYING,
} PLAYING_STATE_AFTER_ROLE_SWITCH_E;

typedef struct
{
	BOOL               newLinkKey;          /* TRUE means attempting to get a new link key */
    btif_sec_access_state_t   secAccessState;
    btif_auth_state_t        authState;
    btif_authorize_state_t   authorizeState;
    btif_encrypt_state_t     encryptState;
    U8                 pairingType;         /* Uses the BtPairingType type */
    U8                 pinLen;              /* length of pin used in generating a key */
    U8                 ioType;              /* Track the kind of I/O transaction */
    U32                numeric;             /* Numeric value to display */
    uint8_t   oobDataPresent;      /* Out of band data present on the remote device */
    btif_auth_requirements_t curAuthRequirements; /* Current auth requirements of local device */
    btif_link_key_type_t      keyType;             /* Type of link key for comparison */
} BT_ENC_STATE_T;

typedef struct
{
	// should be TWS_PAIRING_INFO_MAGIC_NUMBER_INIT_XFER or TWS_PAIRING_INFO_MAGIC_NUMBER_FEEDBACK_XFER
	uint32_t				magicNumber;	
	app_tws_shared_info_t	sharedInfo;
	uint8_t					isOnGoingPlayInterruptedByRoleSwitch : 1;
	uint8_t					reserve 			: 7;
	//int8_t					currentLocalVolume;
	uint8_t					currentA2dpSampleRate;
	uint8_t					a2dpCodecInfo;
	uint8_t					mobileBdAddr[BTIF_BD_ADDR_SIZE];
	BT_ENC_STATE_T			mobileEncState;
	BT_ENC_STATE_T			twsEncState;
	
	TWS_ROLE_SWITCH_BT_INFO_T	btInfo[MAX_ROLE_SWITCH_BT_INFO_COUNT];
} app_tws_role_switch_info_t;

typedef struct
{
	PLAYING_STATE_AFTER_ROLE_SWITCH_E	playingStateAfterRoleSwitch;
} app_tws_role_switch_slave_req_t;

#define APP_TWS_ROLESWITCH_SUCCESSFUL 	0
#define APP_TWS_ROLESWITCH_FAILED		1
#define APP_TWS_ROLESWITCH_TIMEOUT		2
#define APP_TWS_ROLESWITCH_DISCONN_HAPPENED	3

typedef void(*app_tws_role_switch_started_callback)(void);
typedef void(*app_tws_role_switch_done_callback)(uint8_t retStatus);

typedef struct
{
	app_tws_role_switch_started_callback	role_switch_started_callback;
	app_tws_role_switch_done_callback 		role_switch_done_callback;
} APP_TWS_ROLESWITCH_CONFIG_T;

extern HCI_SIMULATE_RECONNECT_STATE_E currentSimulateReconnectionState;

void set_current_simulated_reconnecting_state(HCI_SIMULATE_RECONNECT_STATE_E state);
bool is_simulating_reconnecting_in_progress(void);
void simulated_reconnecting_post_handler_after_connection_is_created(void);
void start_simulate_reconnecting_mobile(void);
void hci_simulate_reconnect_state_machine_handler(void);
int hci_rx_data_parser(uint8_t packetType, uint8_t* ptr, uint32_t length);
void hci_save_rx_data_during_simulated_reconnecting(uint8_t packetType, uint8_t* ptr, uint32_t length);
void hci_data_packet_parse(bool isReceived, uint8_t* ptr, uint32_t length);
void hci_simulate_tx_handler(uint8_t* ptr, uint32_t length);
void update_role_switch_runtime_channel_id(uint8_t* pBdAddr, uint16_t psm, uint16_t CId);
bool is_need_skip_reserved_channel_before_role_switch(uint16_t CId);
void app_tws_role_switch_remove_bt_info_via_bd_addr(uint8_t* pBdAddr);
void app_tws_role_switch_add_bt_info(uint16_t connHandle, uint8_t* pBdAddr);
bool is_role_switch_preparation_in_progress(void);
void reset_tws_acl_data_path(void);
void tws_received_acl_data_handler(void);
void tws_transmitted_acl_data_handler(void);
void send_data_to_peer_tws_via_acl_data_path(uint8_t dataLen, uint8_t* data);
void restore_avdtp_l2cap_channel_info(void);
void app_tws_enter_role_switching(void);
void app_tws_exit_role_switching(uint8_t ret);
void app_tws_role_switch_handling_post_req_transmission(void);
void app_tws_switch_role_further_handler(void);
bool app_tws_is_lowlevel_roleswitch_successful(void);
void app_tws_role_switch_clear_indexes(void);
bool app_tws_is_to_update_sample_rate_after_role_switch(void);
void app_tws_clear_flag_of_update_sample_rate_after_role_switch(void);
void app_tws_preparation_for_roleswich_after_hf_disconnection(void);
void app_tws_preparation_for_roleswich_after_maxslotchanged(uint16_t connHandle, uint8_t maxSlot);
uint8_t app_tws_get_previous_sample_rate(void);
bool app_tws_is_roleswitch_in_idle_state(void);
void app_tws_roleswitch_restore_mobile_enc_state(void);
void app_tws_roleswitch_restore_tws_enc_state(void);
void app_tws_further_handling_for_role_switch(void);
void app_tws_switch_role_with_slave(void);
void app_tws_restore_mobile_a2dp_codec_type(void);
void app_tws_start_saving_msg_during_simulated_reconn(void);
void app_tws_check_received_event(uint8_t* pBuf, uint32_t len);
bool app_tws_is_to_save_msg_during_simulated_reconn(void);
void app_resume_mobile_air_path_and_connect_hfp(void);
bool app_tws_roleswitch_is_pending_for_post_handling_after_role_switch(void);
void app_tws_role_switch_cleanup_a2dp(uint16_t connHandle);

// external APIs
void app_tws_role_switch_init(APP_TWS_ROLESWITCH_CONFIG_T* ptConfig);
bool app_tws_is_role_switching_on(void);
void app_tws_kickoff_switching_role(PLAYING_STATE_AFTER_ROLE_SWITCH_E postPlayingState);
void app_tws_roleswitch_post_handling_after_slave_role_switched(void);
void app_tws_role_switch_reset_bt_info(void);
bool app_tws_is_to_save_msg_during_simulated_reconn(void);
void app_tws_check_max_slot_setting(void);
void app_tws_sniffer_magic_config_channel_map(void);
#ifdef __ENABLE_WEAR_STATUS_DETECT__
void app_tws_check_if_need_to_switch_mic(void);
#endif
uint8_t app_tws_get_role_switch_target_role(void);
bool app_tws_is_reconnecting_hfp_in_progress(void);
bool app_tws_is_exchanging_roleswitch_info_on_going(void);
void app_tws_set_role_switching_on_flag(bool isOn);
void start_simulate_reconnect_master(void);
void start_simulate_reconnect_slave(void);
void resume_a2dp_directly(void);
//void app_show_tws_switch_time(uint8_t step);
extern bool role_switch_force_error_flag;

bool check_conditions_not_send_data_to_controller(uint16_t connection_handle);
void app_tws_preparation_for_roleswich_after_hf_audio_disconnection(void);
bool get_hfp_connected_before_role_switch(void);
void set_hfp_connected_before_role_switch(bool status);
bool no_role_switch_in_progress(void);
void app_show_tws_switch_time(uint8_t step);
uint16_t app_get_local_channelForMedia(uint16_t connHandle);

uint16_t app_tws_get_conhdl_after_role_switch(void);

#ifdef __cplusplus
}
#endif

#endif	// #ifndef __APP_TWS_ROLE_SWITCH_H__


