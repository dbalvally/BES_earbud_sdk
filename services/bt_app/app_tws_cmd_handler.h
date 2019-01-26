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
#ifndef __APP_TWS_CMD_HANDLER_H__
#define __APP_TWS_CMD_HANDLER_H__

#include "nvrecord.h"

#ifdef __cplusplus
extern "C" {
#endif

extern uint32_t __tws_custom_handler_table_start[];
extern uint32_t __tws_custom_handler_table_end[];

#define APP_TWS_MAXIMUM_CMD_DATA_SIZE       512
#define APP_TWS_CMD_CODE_SIZE               sizeof(uint16_t)
#define APP_TWS_WAITING_CMD_RESPONSE_TIMEOUT_IN_MS  5000

typedef enum
{
    APP_TWS_CMD_SET_CODEC_TYPE = 0x8001,
    APP_TWS_CMD_SET_HFP_VOLUME,
    APP_TWS_CMD_CTRL_SLAVE_KEY_NOTIFY,
    APP_SET_TWS_PLATER_RESTART,
    APP_TWS_CMD_SWITCH_ROLE,
    APP_TWS_CMD_SHARE_TWS_INFO,     // 0x8006
    APP_TWS_CMD_SHARE_MOBILE_DEV_INFO,
    APP_TWS_CMD_RESPONSE_BLE_ADDR,
    APP_TWS_CMD_SEND_RUNTIME_STATUS,
    APP_TWS_CMD_TELL_IN_BOX_STATE,
    APP_TWS_CMD_SWITCH_SOURCE_MIC,  // 0x800A
    APP_TWS_WAITING_FOR_SLAVE_READY_FOR_ROLESWITCH,
    APP_TWS_INFORM_MASTER_THAT_READY_FOR_ROLESWITCH,
    APP_TWS_CMD_SET_SLAVE_VOLUME,
    APP_TWS_INFORM_ROLE_SWITCH_DONE,
    APP_TWS_CMD_RSP,                // 0x800F
    APP_TWS_CMD_DOUBLE_CLICK,
    APP_TWS_CMD_CONTROL_MUSIC_PLAYING,
    APP_TWS_CMD_UPDATE_A2DP_STREAMING_STATUS,
    APP_TWS_CMD_LET_SLAVE_PLAY_MUSIC,   
    APP_TWS_CMD_REQ_BATTERY_LEVEL,  // 0x8014
    APP_TWS_CMD_RSP_BATTERY_LEVEL, 

    APP_TWS_CMD_UPDATE_A2DP_STREAMING_STATUS_RSP,
    APP_TWS_CMD_REQ_SET_LBRT,//0x8017
    APP_TWS_CMD_REQ_LBRT_PING,//0X8018
    APP_TWS_CMD_RSP_LBRT_PING,//0x8019
    APP_TWS_CMD_RING_SYNC,
    APP_TWS_CMD_SET_EMSACK_MODE,
#ifdef __TWS_ROLE_SWITCH__
    APP_TWS_CMD_PROFILE_DATA_EXCHANGE,
#endif
    APP_TWS_CMD_SHARE_TWS_ESCO_RETX_NB,
    APP_TWS_CMD_SIMULATE_DISCONNECT_DONE,
	APP_TWS_CMD_CTRL_ANC_SET_STATUS,
/*	
	APP_TWS_CMD_CTRL_ANC_NOTIFY_STATUS,
	APP_TWS_CMD_CTRL_ANC_GET_STATUS
*/	
} APP_TWS_CMD_CODE_E;

typedef void (*app_tws_cmd_handler_t)(uint8_t* ptrParam, uint32_t paramLen);

typedef struct
{
    uint32_t                cmdCode;
    app_tws_cmd_handler_t   cmdHandler;             /**< command handler function */

} __attribute__((packed)) APP_TWS_CMD_INSTANCE_T;

typedef struct
{
    uint16_t                cmdCode;
    uint8_t                 content[APP_TWS_MAXIMUM_CMD_DATA_SIZE];
        
} __attribute__((packed)) APP_TWS_CMD_DATA_FORMAT_T;


#define TWS_CUSTOM_COMMAND_TO_ADD(cmdCode, cmdHandler)  \
    static APP_TWS_CMD_INSTANCE_T cmdCode##_entry __attribute__((used, section(".tws_custom_handler_table"))) =     \
        {(cmdCode), (cmdHandler)};

#define TWS_CUSTOM_COMMAND_PTR_FROM_ENTRY_INDEX(index)  \
    ((APP_TWS_CMD_INSTANCE_T *)((uint32_t)__tws_custom_handler_table_start + (index)*sizeof(APP_TWS_CMD_INSTANCE_T)))

void app_tws_inform_waiting_cmd_rsp_thread(void);
void tws_init_cmd_handler(void);
bool app_tws_send_cmd_without_rsp(uint16_t cmdCode, uint8_t *pDataBuf, uint16_t dataLength);
bool app_tws_send_cmd_rsp(uint8_t *pRspData, uint16_t rspDataLen);
bool app_tws_send_cmd_with_rsp(uint16_t cmdCode, uint8_t *pDataBuf, uint16_t dataLength, bool lbrt);
void data_from_peer_tws_received(uint8_t* data, uint16_t dataLen);
bool app_tws_force_send_cmd_without_rsp(uint16_t cmdCode, uint8_t *pDataBuf, uint16_t dataLength);
BOOL is_cmd_in_not_exit_sniff_list(uint16_t cmd_code);

#ifdef __cplusplus
    }
#endif

#endif // #ifndef __APP_TWS_CMD_HANDLER_H__

