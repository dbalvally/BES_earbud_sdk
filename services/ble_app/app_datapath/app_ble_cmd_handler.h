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
#ifndef __APP_BLE_CMD_HANDLER_H__
#define __APP_BLE_CMD_HANDLER_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief The BLE custom command handling return status
 *
 */
typedef enum
{
	NO_ERROR = 0,
	INVALID_CMD_CODE,
	PARAMETER_LENGTH_OUT_OF_RANGE,
	PARAMETER_LENGTH_TOO_SHORT,
	HANDLING_FAILED,	
	TIMEOUT_WAITING_RESPONSE,
	// TO ADD: new return status
} BLE_CUSTOM_CMD_RET_STATUS_E;

/**
 * @brief Format of the custom command handler function, called when the command is received
 *
 * @param cmdCode	Custom command code, from BLE_CUSTOM_CMD_CODE_E
 * @param ptrParam 	Pointer of the received parameter
 * @param paramLen 	Length of the recevied parameter
 * 
 */
typedef void (*BLE_CustomCmdHandler_t)(uint32_t cmdCode, uint8_t* ptrParam, uint32_t paramLen);

/**
 * @brief Format of the custom command response handler function, 
 *	called when the response to formerly sent command is received
 *
 * @param retStatus	Handling return status of the command
 * @param ptrParam 	Pointer of the received parameter
 * @param paramLen 	Length of the recevied parameter
 * 
 */
typedef void (*BLE_CustomCmd_Response_Handler_t)(BLE_CUSTOM_CMD_RET_STATUS_E retStatus, uint8_t* ptrParam, uint32_t paramLen);

/**
 * @brief Format of the raw data xfer received handler function
 *
 * @param ptrData	Pointer of the received raw data
 * @param dataLen 	Length of the received raw data
 * 
 */
typedef void (*BLE_RawDataReceived_Handler_t)(uint8_t* ptrData, uint32_t dataLen);


/**
 * @brief Custom command definition data structure
 *
 */
typedef struct
{
    BLE_CustomCmdHandler_t 	cmdHandler;  			/**< command handler function */
    uint8_t 				isNeedResponse;     	/**< true if needs the response from the peer device */
	uint16_t				timeoutWaitingRspInMs;	/**< time-out of waiting for response in milli-seconds */
	BLE_CustomCmd_Response_Handler_t	cmdRspHandler;	/**< command response handler function */
} BLE_CUSTOM_CMD_INSTANCE_T;

/**< maximum payload size of one BLE custom command */
#define BLE_CUSTOM_CMD_MAXIMUM_PAYLOAD_SIZE		20	// assure that one BLE packet can include all the data 

/**
 * @brief BLE custom command playload
 *
 */
typedef struct
{
    uint16_t 	cmdCode;    	/**< command code, from BLE_CUSTOM_CMD_CODE_E */
	uint16_t 	paramLen;		/**< length of the following parameter */
    uint8_t 	param[BLE_CUSTOM_CMD_MAXIMUM_PAYLOAD_SIZE - 2*sizeof(uint16_t)];
} BLE_CUSTOM_CMD_PAYLOAD_T;

/**
 * @brief Command response parameter structure
 *
 */
typedef struct
{
    uint16_t 	cmdCodeToRsp;  	/**< tell which command code to response */
    uint16_t 	cmdRetStatus;   /**< handling result of the command, from BLE_CUSTOM_CMD_RET_STATUS_E */
	uint16_t 	rspDataLen;		/**< length of the response data */
    uint8_t 	rspData[BLE_CUSTOM_CMD_MAXIMUM_PAYLOAD_SIZE - 5*sizeof(uint16_t)];
} BLE_CUSTOM_CMD_RSP_T;

void 						BLE_get_response_handler(uint32_t funcCode, uint8_t* ptrParam, uint32_t paramLen);
void 						BLE_raw_data_xfer_control_handler(uint32_t funcCode, uint8_t* ptrParam, uint32_t paramLen);
void 						BLE_control_raw_data_xfer(bool isStartXfer);
void 						BLE_set_raw_data_xfer_received_callback(BLE_RawDataReceived_Handler_t callback);
void 						BLE_start_raw_data_xfer_control_rsp_handler(BLE_CUSTOM_CMD_RET_STATUS_E retStatus, uint8_t* ptrParam, uint32_t paramLen);
void 						BLE_stop_raw_data_xfer_control_rsp_handler(BLE_CUSTOM_CMD_RET_STATUS_E retStatus, uint8_t* ptrParam, uint32_t paramLen);
uint8_t* 					BLE_custom_command_raw_data_buffer_pointer(void);
uint16_t 					BLE_custom_command_received_raw_data_size(void);
BLE_CUSTOM_CMD_RET_STATUS_E BLE_send_response_to_command(uint32_t responsedCmdCode, BLE_CUSTOM_CMD_RET_STATUS_E returnStatus, 
								uint8_t* rspData, uint32_t rspDataLen, bool isViaNotification);
BLE_CUSTOM_CMD_RET_STATUS_E BLE_send_custom_command(uint32_t cmdCode, uint8_t* ptrParam, uint32_t paramLen, bool isViaNotification);
BLE_CUSTOM_CMD_RET_STATUS_E BLE_custom_command_receive_data(uint8_t* ptrData, uint32_t dataLength);
void 						BLE_custom_command_init(void);


#ifdef __cplusplus
	}
#endif

#endif // #ifndef __APP_BLE_CMD_HANDLER_H__

