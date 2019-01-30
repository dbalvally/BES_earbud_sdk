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
#ifndef __BLE_TWS_H__
#define __BLE_TWS_H__

#ifdef __cplusplus
extern "C" {
#endif

#define BEST_MAGICCODE					0x54534542
#define TWS_BLE_ADV_MANU_DATA_MAX_LEN	(2*6 + 4)

#define BLE_TWS_FAST_ADV_INTERVAL_IN_MS			25	// used for master and slave earphone pairing mode
#define BLE_TWS_FAST_PAIR_ADV_INTERVAL_IN_MS	96	// used for fast pairing mode
#define BLE_TWS_ADV_SLOW_INTERVAL_IN_MS			200	// used for normal mode
#define BLE_TWS_OUT_OF_BOX_ADV_INTERVAL_IN_MS   1000
#define BLE_ADV_SCAN_SWITCH_PERIOD_IN_MS        (320)
#define BLE_RECONN_ADV_SCAN_SWITCH_PERIOD_IN_MS 1000

#define BLE_ACTIVITY_IDLE_TIME_PERIOD_IN_MS     6000
#define BLE_INTERACTIVE_ACTIVITY_TIMEOUT_PERIO  60000
#define BLE_ADV_DATA_UPDATE_PERIOD_IN_MS        (3000)

#define BLE_HIGH_SPEED_RECONNECT_ADV_ENCURANCE_TIME_IN_MS       (30000)

#ifdef __TWS_PAIR_DIRECTLY__
#define BLE_HIGH_SPEED_RECONNECT_ADV_INTERVAL_IN_MS             (30)
#else
#define BLE_HIGH_SPEED_RECONNECT_ADV_INTERVAL_IN_MS             (100)
#endif
#define BLE_LOW_SPEED_RECONNECT_ADV_INTERVAL_IN_MS              (100)

#define BLE_TWS_SCANNING_WINDOW_IN_MS                           (100)
#define BLE_TWS_SCANNING_INTERVAL_IN_MS                         (100)


#ifdef __TWS_PAIR_DIRECTLY__
#define BLE_TWS_RECONNECT_HIGH_DUTYCYCLE_SCANNING_WINDOW_IN_MS      (60)
#define BLE_TWS_RECONNECT_HIGH_DUTYCYCLE_SCANNING_INTERVAL_IN_MS    (1000)
#else
#define BLE_TWS_RECONNECT_HIGH_DUTYCYCLE_SCANNING_WINDOW_IN_MS      (300)
#define BLE_TWS_RECONNECT_HIGH_DUTYCYCLE_SCANNING_INTERVAL_IN_MS    (500)
#endif
#define BLE_TWS_RECONNECT_LOW_DUTYCYCLE_SCANNING_WINDOW_IN_MS       (300)
#define BLE_TWS_RECONNECT_LOW_DUTYCYCLE_SCANNING_INTERVAL_IN_MS     (500)

#define BLE_ACTIVITY_IDLE_PERIOD_DURING_INBOX_PAIRING_IN_MS     (5000)

#define TWS_BT_RECONNECTING_TIMEOUT_IN_MS						(5000)

#define TWS_BLE_HANDSHAKE_TIMEOUT_IN_MS							(5000)

#define TWS_COUNT_PER_PAIR      (2)

#define TWS_TIME_INTERVAL_WAITING_PENDING_OP_DONE_BEFORE_REPAIRING_IN_MS    (200)

// TODO: should be changed based on the realistic environment
#define BLE_ADVERTISING_INTERVAL          (874) // 546.25ms (874*0.625ms)
#define BLE_FAST_ADVERTISING_INTERVAL     (96) // 60ms (96*0.625ms)

#define BLE_ADV_MANU_FLAG		0xFF

#define TWS_BOX_RSSI_THRESHOLD	(-70)
#define BLE_ADV_MANU_FLAG		0xFF

#ifndef BLE_TWS_USE_BLE_AS_INBOX_COMMUNICATION_PATH
#define BLE_TWS_USE_BLE_AS_INBOX_COMMUNICATION_PATH		0
#endif

#define TWS_DUAL_PAIRING	0
#define TWS_FREEMAN_PAIRING	1
#define TWS_SLAVE_ENTER_PAIRING	2
#define TWS_MASTER_CON_SLAVE	3

#ifndef BD_ADDR_SIZE
#define BD_ADDR_SIZE    6
#endif
/**
 * @brief The states of the ble tws.
 *
 */
typedef enum
{
    TWS_BLE_STATE_IDLE = 0,
    TWS_BLE_STATE_ADVERTISING = 1,
    TWS_BLE_STATE_STARTING_ADV = 2,
    TWS_BLE_STATE_STOPPING_SCANNING = 3,
    TWS_BLE_STATE_SCANNING = 4,
    TWS_BLE_STATE_STARTING_SCANNING = 5,
    TWS_BLE_STATE_STOPPING_ADV = 6,
    TWS_BLE_STATE_CONNECTING = 7,
    TWS_BLE_STATE_STARTING_CONNECTING = 8,
    TWS_BLE_STATE_STOPPING_CONNECTING = 9,
    TWS_BLE_STATE_CONNECTED = 10,
} TWS_BLE_STATE_E;

typedef enum
{
	TWS_BLE_OP_IDLE = 0,
	TWS_BLE_OP_START_ADV,
	TWS_BLE_OP_START_SCANNING,
	TWS_BLE_OP_START_CONNECTING,
	TWS_BLE_OP_STOP_ADV,
	TWS_BLE_OP_STOP_SCANNING,
	TWS_BLE_OP_STOP_CONNECTING,
} TWS_BLE_OP_E;

typedef enum
{
	TWS_FREEMAN = 0,
	TWS_MASTER,
	TWS_SLAVE,
	TWS_CHARGER_BOX
} TWS_BOX_FUNC_ROLE_E;

typedef enum
{
	TWS_LEFT_SIDE = 0,
	TWS_RIGHT_SIDE
} TWS_EAR_SIDE_E;

#define TWS_EARPHONE_BOX_STATE_BIT_OFFSET					0

#define TWS_EARPHONE_BOX_STATE_OUT							0
#define TWS_EARPHONE_BOX_STATE_IN							1

#define IS_TWS_EARHPONE_ADV_STATE_IN_BOX(state)				((state) & (1 << TWS_EARPHONE_BOX_STATE_BIT_OFFSET))

#if BLE_TWS_USE_BLE_AS_INBOX_COMMUNICATION_PATH
#define TWS_EARPHONE_PAIRING_STATE_BIT_OFFSET				1
#define TWS_EARPHONE_MOBILE_CONNECTION_STATE_BIT_OFFSET		2

#define TWS_EARPHONE_PAIRING_STATE_IDLE						0	
#define TWS_EARPHONE_PAIRING_STATE_IN_PAIRING				1

#define TWS_EARPHONE_MOBILE_CONNECTION_STATE_DISCONNECTED	0
#define TWS_EARPHONE_MOBILE_CONNECTION_STATE_CONNECTED		1	

#define IS_TWS_EARHPONE_ADV_STATE_IN_PAIRING(state)				((state) & (1 << TWS_EARPHONE_PAIRING_STATE_BIT_OFFSET))
#define IS_TWS_EARHPONE_ADV_STATE_CONNECTED_WITH_MOBILE(state)		((state) & (1 << TWS_EARPHONE_MOBILE_CONNECTION_STATE_BIT_OFFSET))

#endif	// #if BLE_TWS_USE_BLE_AS_INBOX_COMMUNICATION_PATH

typedef enum
{
	TWS_BOX_IDLE = 0,
	TWS_BOX_SCANNING,
	TWS_BOX_REQUESTING_TWS_PAIRING,
	TWS_BOX_REQUESTING_FREEMAN_PAIRING,
	TWS_BOX_REQUESTING_ENTERING_OTA,
} TWS_BOX_FUNC_STATE_E;

typedef enum
{
	TWS_NORMAL_ADV,
	TWS_INTERACTIVE_ADV,
} TWS_ADV_MODE_E;

typedef struct
{
	uint8_t	currentState;	
	uint8_t	reqCounter;
	uint8_t	btAddr[BTIF_BD_ADDR_SIZE];
} __attribute__ ((__packed__))TWS_EARPHONE_BLE_ADV_INFO_T;

typedef struct
{
	uint8_t funcState;	// functionality state, should be TWS_BOX_REQUESTING_FREEMAN_PAIRING
	uint8_t	reqCounter;	// the counter of the operation request, increased by 1 per operation request
	uint8_t btAddrFreeman[BTIF_BD_ADDR_SIZE];	
} __attribute__ ((__packed__))TWS_CHARGERBOX_FREEMAN_PAIRING_REQ_T;

typedef struct
{
	uint8_t funcState;	// functionality state, should be TWS_BOX_REQUESTING_TWS_PAIRING
	uint8_t	reqCounter;	// the counter of the operation request, increased by 1 per operation request
	uint8_t btAddrMaster[BTIF_BD_ADDR_SIZE];
	uint8_t btAddrSlave[BTIF_BD_ADDR_SIZE];		
} __attribute__ ((__packed__))TWS_CHARGERBOX_TWS_PAIRING_REQ_T;

typedef struct
{
	uint8_t funcState;	// functionality state, should be TWS_BOX_REQUESTING_ENTERING_OTA
	uint8_t	reqCounter;	// the counter of the operation request, increased by 1 per operation request
	uint8_t	countOfDevices;	// the counter of the BT devices to enter OTA
	uint8_t btAddr[TWS_COUNT_PER_PAIR][BTIF_BD_ADDR_SIZE];	
} __attribute__ ((__packed__))TWS_CHARGERBOX_ENTER_OTA_REQ_T;


typedef struct
{
	uint8_t		dataLengthFollowing;
	uint8_t		manufactureDataFlag;	// 0xFF
	uint8_t		selfDefinedRegion;		// For debugging purpose, to fill current status of the device
	uint32_t 	magicCode;	// BEST_MAGICCODE: 0x54534542
	uint8_t		role : 2;		// role from TWS_BOX_FUNC_ROLE_E
	uint8_t     isConnectedWithMobile : 1;
	uint8_t     isConnectedWithTws : 1;
	uint8_t		earSide: 4;		// side of the earphone from TWS_EAR_SIDE_E	
	uint8_t 	batteryLevel;	// battery level in percentage
	uint8_t		pairCode;	// A unique identifier used in pairs
	uint8_t		swVersion1;	    // The first version of the software version number  
	uint8_t		swVersion2;	    // The second version of the software version number	
	uint8_t		data[TWS_BLE_ADV_MANU_DATA_MAX_LEN];
} __attribute__ ((__packed__))TWS_BLE_ADV_MANU_DATA_SECTION_T;

/**
 * @brief The environment data structure of the ble tws application.
 *
 */
typedef struct
{
    TWS_BLE_STATE_E state;
	TWS_BOX_FUNC_STATE_E funcState;

	// used to delay 5s for starting ble adv if the device is just powered up and in-chargerbox
	uint8_t		advType;	
	uint16_t	advIntervalInMs;
	uint16_t	scanWindowInMs;
	uint16_t	scanIntervalInMs;
	uint8_t		scannedTwsCount;
	uint8_t		indexForNextScannedTws;
	uint8_t		twsScannedBleAddr[BTIF_BD_ADDR_SIZE];
	uint8_t     twsModeToStart;
	uint8_t		twsBtBdAddr[TWS_COUNT_PER_PAIR][BTIF_BD_ADDR_SIZE];	

	uint8_t		pendingPairedBtAddr[TWS_COUNT_PER_PAIR][BTIF_BD_ADDR_SIZE];	
	uint8_t		pendingPairingMode;

	uint8_t		advMode;
	uint8_t		currentAdvChannel;	// adv channel bit 0,1,2
	
	uint8_t		operationRequestCounter;	
    uint8_t     pairCode;    
	TWS_BLE_ADV_MANU_DATA_SECTION_T adv_manufacture_data_section;
} __attribute__ ((__packed__))TWS_BOX_ENV_T;

typedef void(*ble_tws_initialized_callback)(void);

#if !BLE_TWS_USE_BLE_AS_INBOX_COMMUNICATION_PATH
typedef void(*ble_tws_pairing_preparation_done_callback)(void);
typedef void(*ble_tws_pairing_done_callback)(void);
#endif

typedef struct
{
	uint8_t		earSide : 1;	// from TWS_EAR_SIDE_E, the earside
	uint8_t		earSideAsMasterByDefault : 1;	// which earside is chosen as the master by default
	uint8_t		reserve : 6;
	int8_t		rssiThreshold;	// rssi threshold to clarify whether the earphone is in box or not
	ble_tws_initialized_callback ble_tws_initialized_cb;
} BLE_TWS_CONFIG_T;

#define BLE_INITIAL_HANDSHAKE_STR_LEN	6
typedef struct
{
	// sum of master bt and slave bt per byte
	uint8_t 	ble_initial_handshake_str[BLE_INITIAL_HANDSHAKE_STR_LEN];
} TWS_BLE_INITIAL_HANDSHAKE_T;

extern TWS_BOX_ENV_T twsBoxEnv;

void ble_tws_config(BLE_TWS_CONFIG_T* ptConfig);

bool app_freeman_pairing_request_handler(uint8_t* btAddr);
bool app_tws_pairing_request_handler(uint8_t* masterBtAddr, uint8_t* slaveBtAddr);
void app_tws_enter_ota_mode(void);

// use this api to check whether the ble tws has been ready during the system init, 
// if some operations need to be done at that time
// if the ble tws initialization is not done yet, use the registered BLE_TWS_CONFIG_T.ble_tws_initialized_cb to do that
bool ble_tws_is_initialized(void);
void ble_tws_set_bt_ready_flag(void);

// for debug purpose
void app_ble_fill_debug_status_region(uint8_t debugStatus);

// internal used APIs
void ble_tws_stop_all_activities(void);
void app_start_ble_tws_reconnect_adv(void);
void app_start_ble_tws_reconnect_scanning(void);
void app_tws_start_interactive_ble_activities_in_chargerbox(void);
void app_tws_pending_pairing_req_handler(void);
void app_tws_start_normal_ble_activities_in_chargerbox(void);
void app_tws_start_normal_ble_activities_in_chargerbox_led(void);
void app_start_ble_tws_out_of_box_adv(void);
void app_tws_start_pairing_inbox(uint32_t pairingMode, uint8_t* pMasterAddr, uint8_t* SlaveAddr);
void ble_tws_clear_the_tws_pairing_state(void);
void app_tws_start_interactive_reconn_ble_activities(void);
void app_tws_stop_pairing_in_chargerbox(void);
bool app_is_ble_adv_allowed(void);
void app_tws_start_reset_simple_pairing(void);
void app_tws_start_pairing_in_chargerbox(void);
void app_tws_start_simple_repairing_inbox(uint8_t pairCode);
TWS_BOX_ENV_T* app_tws_get_env(void);
void app_tws_restart_operation(void);
void ble_tws_config_init_done_callback(ble_tws_initialized_callback cb);

#ifdef __TWS_PAIR_DIRECTLY__
extern bool slaveDisconMobileFlag;
extern bool slaveInReconMasterFlag;

bool app_is_ble_adv_allowed_without_chargerbox(void);
void app_tws_start_normal_ble_activities_without_chargerbox(void);
void app_start_ble_tws_connecting(void);

#endif

void app_start_ble_tws_advertising(uint8_t advType, uint16_t adv_interval_in_ms);
void app_start_ble_tws_advertising_in_bt_thread(uint8_t advType, uint16_t adv_interval_in_ms);

bool app_ble_tws_is_connected_with_peer_earbud(const uint8_t* connectedBleAddr);
void app_tws_disconnected_evt_handler(uint8_t conidx);
void app_tws_connected_evt_handler(uint8_t conidx);

bool is_ble_stack_initialized(void);
void app_tws_stop_ble_handshake_supervising(void);
bool app_tws_is_connecting_ble(void);

void app_start_connectable_ble_adv(uint16_t adv_interval_in_ms);


#ifdef __cplusplus
	}
#endif

#endif // #ifndef __BLE_TWS_H__

