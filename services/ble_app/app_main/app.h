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
#ifndef APP_H_
#define APP_H_

/**
 ****************************************************************************************
 * @addtogroup APP
 * @ingroup RICOW
 *
 * @brief Application entry point.
 *
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "rwip_config.h"     // SW configuration

#if (BLE_APP_PRESENT)

#include <stdint.h>          // Standard Integer Definition
#include <co_bt.h>           // Common BT Definitions
#include "arch.h"            // Platform Definitions
#include "gapc.h"            // GAPC Definitions
#include "gapm_task.h"
#if (NVDS_SUPPORT)
#include "nvds.h"
#endif // (NVDS_SUPPORT)


/// Advertising channel map - 37, 38, 39
#define APP_ADV_CHMAP           (0x07)
/// Advertising minimum interval - 40ms (64*0.625ms)
#define APP_ADV_INT_MIN         (64)
/// Advertising maximum interval - 40ms (64*0.625ms)
#define APP_ADV_INT_MAX         (64)
/// Fast advertising interval
#define APP_ADV_FAST_INT        (32)

#define APP_ADV_INT_MIN_IN_MS	(40)
/// Advertising maximum interval - 40ms (64*0.625ms)
#define APP_ADV_INT_MAX_IN_MS   (40)


#if defined(CFG_APP_SEC)
#if defined(CFG_SEC_CON)
#define BLE_AUTHENTICATION_LEVEL		GAP_AUTH_REQ_SEC_CON_BOND
#else
#define BLE_AUTHENTICATION_LEVEL		GAP_AUTH_REQ_MITM_BOND
#endif
#else
#define BLE_AUTHENTICATION_LEVEL		GAP_AUTH_REQ_NO_MITM_NO_BOND
#endif

/*
 * DEFINES
 ****************************************************************************************
 */
/// Maximal length of the Device Name value
#define APP_DEVICE_NAME_MAX_LEN      (24)

// Advertising mode
#define APP_FAST_ADV_MODE  (1)
#define APP_SLOW_ADV_MODE  (2)
#define APP_STOP_ADV_MODE  (3)
#define APP_MAX_TX_OCTETS	251
#define APP_MAX_TX_TIME		2120		//400

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */

#if (NVDS_SUPPORT)
/// List of Application NVDS TAG identifiers
enum app_nvds_tag
{
    /// BLE Application Advertising data
    NVDS_TAG_APP_BLE_ADV_DATA           = 0x0B,
    NVDS_LEN_APP_BLE_ADV_DATA           = 32,

    /// BLE Application Scan response data
    NVDS_TAG_APP_BLE_SCAN_RESP_DATA     = 0x0C,
    NVDS_LEN_APP_BLE_SCAN_RESP_DATA     = 32,

    /// Mouse Sample Rate
    NVDS_TAG_MOUSE_SAMPLE_RATE          = 0x38,
    NVDS_LEN_MOUSE_SAMPLE_RATE          = 1,
    /// Peripheral Bonded
    NVDS_TAG_PERIPH_BONDED              = 0x39,
    NVDS_LEN_PERIPH_BONDED              = 1,
    /// Mouse NTF Cfg
    NVDS_TAG_MOUSE_NTF_CFG              = 0x3A,
    NVDS_LEN_MOUSE_NTF_CFG              = 2,
    /// Mouse Timeout value
    NVDS_TAG_MOUSE_TIMEOUT              = 0x3B,
    NVDS_LEN_MOUSE_TIMEOUT              = 2,
    /// Peer Device BD Address
    NVDS_TAG_PEER_BD_ADDRESS            = 0x3C,
    NVDS_LEN_PEER_BD_ADDRESS            = 7,
    /// Mouse Energy Safe
    NVDS_TAG_MOUSE_ENERGY_SAFE          = 0x3D,
    NVDS_LEN_MOUSE_SAFE_ENERGY          = 2,
    /// EDIV (2bytes), RAND NB (8bytes),  LTK (16 bytes), Key Size (1 byte)
    NVDS_TAG_LTK                        = 0x3E,
    NVDS_LEN_LTK                        = 28,
    /// PAIRING
    NVDS_TAG_PAIRING                    = 0x3F,
    NVDS_LEN_PAIRING                    = 54,
};

enum app_loc_nvds_tag
{
    /// Audio mode 0 task
    NVDS_TAG_AM0_FIRST                  = NVDS_TAG_APP_SPECIFIC_FIRST,
    NVDS_TAG_AM0_LAST                   = NVDS_TAG_APP_SPECIFIC_FIRST+16,

    /// Local device Identity resolving key
    NVDS_TAG_LOC_IRK,
    /// Peer device Resolving identity key (+identity address)
    NVDS_TAG_PEER_IRK,



    /// size of local identity resolving key
    NVDS_LEN_LOC_IRK                    = KEY_LEN,
    /// size of Peer device identity resolving key (+identity address)
    NVDS_LEN_PEER_IRK                   = sizeof(struct gapc_irk),
};
#endif // (NVDS_SUPPORT)

/// Application environment structure
typedef struct
{
    /// Connection handle
    uint16_t conhdl;
	uint8_t isConnected;
    /// Bonding status
    uint8_t bonded;
	uint8_t peerAddrType;
	uint8_t isBdAddrResolvingInProgress;
	uint8_t isGotSolvedBdAddr;
	uint8_t bdAddr[BD_ADDR_LEN];
	uint8_t solvedBdAddr[BD_ADDR_LEN];

	// to avoid the glitch of the a2dp streaming caused by the
	// ble connection parameter update negotiation lauched by the peer device,
	// right after the ble connection, use this flag to reject the connection parameter
	// reques. And let the device trigger the connection parameter update initiatively
	uint8_t	isToRejectConnParamUpdateReqFromPeerDev;

} APP_BLE_CONN_CONTEXT_T;

/// Application environment structure
struct app_env_tag
{
	uint8_t conn_cnt;
    /// Last initialized profile
    uint8_t next_svc;
	
    /// Device Name length
    uint8_t dev_name_len;
    /// Device Name
    uint8_t dev_name[APP_DEVICE_NAME_MAX_LEN];

    /// Local device IRK
    uint8_t loc_irk[KEY_LEN];

	APP_BLE_CONN_CONTEXT_T context[BLE_CONNECTION_MAX];
};

/*
 * GLOBAL VARIABLE DECLARATION
 ****************************************************************************************
 */

/// Application environment
extern struct app_env_tag app_env;

/*
 * FUNCTION DECLARATIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Initialize the BLE demo application.
 ****************************************************************************************
 */
void appm_init(void);

/**
 ****************************************************************************************
 * @brief Add a required service in the database
 ****************************************************************************************
 */
bool appm_add_svc(void);

/**
 ****************************************************************************************
 * @brief Put the device in general discoverable and connectable mode
 ****************************************************************************************
 */
void appm_start_advertising(uint8_t advType, uint8_t advChMap, uint32_t advMaxIntervalInMs, uint32_t advMinIntervalInMs, \
	uint8_t* ptrManufactureData, uint32_t manufactureDataLen);

/**
 ****************************************************************************************
 * @brief Put the device in non discoverable and non connectable mode
 ****************************************************************************************
 */
void appm_stop_advertising(void);

/**
 ****************************************************************************************
 * @brief Send to request to update the connection parameters
 ****************************************************************************************
 */
void appm_update_param(uint8_t conidx, struct gapc_conn_param *conn_param);

/**
 ****************************************************************************************
 * @brief Send a disconnection request
 ****************************************************************************************
 */
void appm_disconnect(uint8_t conidx);

/**
 ****************************************************************************************
 * @brief Retrieve device name
 *
 * @param[out] device name
 *
 * @return name length
 ****************************************************************************************
 */
uint8_t appm_get_dev_name(uint8_t* name);

uint8_t appm_is_connected();

/**
 ****************************************************************************************
 * @brief Return if the device is currently bonded
 ****************************************************************************************
 */
bool app_sec_get_bond_status(void);

void app_connected_evt_handler(const uint8_t* pPeerBdAddress);

void app_disconnected_evt_handler(void);

void l2cap_update_param(uint8_t conidx, struct gapc_conn_param *conn_param);

void appm_start_connecting(struct gap_bdaddr* ptBdAddr);

void appm_stop_connecting(void);

void appm_start_scanning(uint16_t intervalInMs, uint16_t windowInMs, uint32_t filtPolicy);

void appm_stop_scanning(void);

void appm_create_advertising(void);

void appm_create_connecting(void);

void app_advertising_stopped(uint8_t state);

void app_advertising_starting_failed(void);

void app_scanning_stopped(void);

void app_connecting_stopped(void);

void appm_exchange_mtu(uint8_t conidx);

void app_ble_system_ready(void);

void app_adv_reported_scanned(struct gapm_adv_report_ind* ptInd);

void appm_set_private_bd_addr(uint8_t* bdAddr);

void appm_add_dev_into_whitelist(struct gap_bdaddr* ptBdAddr);

void app_scanning_started(void);

void app_advertising_started(void);

void app_connecting_stopped(void);

void app_connecting_started(void);

void appm_update_adv_data(uint8_t* pAdvData, uint32_t dataLength);

bool appm_resolve_random_ble_addr_from_nv(uint8_t conidx, uint8_t* randomAddr);

void appm_resolve_random_ble_addr_with_sepcific_irk(uint8_t conidx, uint8_t* randomAddr, uint8_t* pIrk);

void appm_random_ble_addr_solved(bool isSolvedSuccessfully, uint8_t* irkUsedForSolving);
uint8_t app_ble_connection_count(void);

bool app_is_arrive_at_max_ble_connections(void);

bool app_is_resolving_ble_bd_addr(void);

#if (BLE_APP_GFPS)
void app_enter_fastpairing_mode(void);
void app_exit_fastpairing_mode(void);

bool app_is_in_fastparing_mode(void);
void app_set_in_fastpairing_mode_flag(bool isEnabled);
#endif
uint16_t appm_get_conhdl_from_conidx(uint8_t conidx);

void appm_check_and_resolve_ble_address(uint8_t conidx);

void appm_disconnect_all(void);

uint8_t* appm_get_current_ble_addr(void);

/// @} APP

#endif //(BLE_APP_PRESENT)

#endif // APP_H_
