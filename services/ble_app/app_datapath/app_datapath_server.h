/**
 ****************************************************************************************
 *
 * @file app_datapath_server.h
 *
 * @brief DataPath Server Application entry point
 *
 * Copyright (C) RivieraWaves 2009-2015
 *
 *
 ****************************************************************************************
 */

#ifndef APP_DATAPATH_SERVER_H_
#define APP_DATAPATH_SERVER_H_

/**
 ****************************************************************************************
 * @addtogroup APP
 * @ingroup RICOW
 *
 * @brief DataPath Server Application entry point.
 *
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "rwip_config.h"     // SW configuration

#if (BLE_APP_DATAPATH_SERVER)

#include <stdint.h>          // Standard Integer Definition
#include "ke_task.h"

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */


extern struct app_env_tag app_env;

/// health thermometer application environment structure
struct app_datapath_server_env_tag
{
    uint8_t connectionIndex;
	uint8_t	isNotificationEnabled;
};

typedef void(*app_datapath_server_tx_done_t)(void);

typedef void(*app_datapath_server_activity_stopped_t)(void);

/*
 * GLOBAL VARIABLES DECLARATIONS
 ****************************************************************************************
 */

/// Health Thermomter Application environment
extern struct app_datapath_server_env_tag app_datapath_server_env;

/// Table of message handlers
extern const struct ke_state_handler app_datapath_server_table_handler;

/*
 * FUNCTIONS DECLARATION
 ****************************************************************************************
 */
/**
 ****************************************************************************************
 * @brief Initialize DataPath Server Application
 ****************************************************************************************
 */
void app_datapath_server_init(void);

/**
 ****************************************************************************************
 * @brief Add a DataPath Server instance in the DB
 ****************************************************************************************
 */
void app_datapath_add_datapathps(void);

void app_datapath_server_connected_evt_handler(uint8_t conidx);

void app_datapath_server_disconnected_evt_handler(uint8_t conidx);

void app_datapath_server_send_data_via_notification(uint8_t* ptrData, uint32_t length);

void app_datapath_server_send_data_via_write_command(uint8_t* ptrData, uint32_t length);

void app_datapath_server_register_tx_done(app_datapath_server_tx_done_t callback);

void app_datapath_server_update_conn_parameter(uint32_t min_interval_in_ms, uint32_t max_interval_in_ms,
		uint32_t supervision_timeout_in_ms);

void app_datapath_server_control_notification(bool isEnable, uint8_t conidx);

void app_datapath_server_mtu_exchanged_handler(uint8_t conidx, uint16_t mtu);


#endif //(BLE_APP_DATAPATH_SERVER)

/// @} APP

#endif // APP_DATAPATH_SERVER_H_
