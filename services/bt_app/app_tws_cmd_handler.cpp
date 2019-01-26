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
#include "hal_trace.h"


#include "rtos.h"
#include "besbt.h"
#include "cqueue.h"
#include "app_bt.h"
#include "nvrecord.h"

#include "apps.h"
#include "resources.h"
#include "a2dp_api.h"
#include "hci_api.h"
#include "l2cap_api.h"

#ifdef __TWS__
#include "app_tws.h"
#include "app_tws_if.h"
#endif

#include "app_utils.h"
#include "app_bt_queue.h"
#include "app_bt_conn_mgr.h"
#include "app_tws_role_switch.h"
#include "app_tws_cmd_handler.h"
#include "os_api.h"
#define BUF_SIZE_FOR_TWS_REC_ACL_DATA_PATH    672
static uint8_t bufForTwsReceivedAclDataPath[BUF_SIZE_FOR_TWS_REC_ACL_DATA_PATH];

#define BUF_SIZE_FOR_TWS_TRANS_ACL_DATA_PATH    672*4
static uint8_t bufForTwsTransmittedAclDataPath[BUF_SIZE_FOR_TWS_TRANS_ACL_DATA_PATH];

static CQueue receivedTwsAclDataQueue;
static CQueue transmittedTwsAclDataQueue;

APP_TWS_CMD_CODE_E cmdNotExitSniff[]={
    APP_TWS_CMD_REQ_BATTERY_LEVEL,
    APP_TWS_CMD_RSP_BATTERY_LEVEL,
    APP_TWS_CMD_SEND_RUNTIME_STATUS,
    APP_TWS_CMD_SET_HFP_VOLUME,
    APP_TWS_CMD_TELL_IN_BOX_STATE,
    APP_TWS_CMD_SET_EMSACK_MODE,
    APP_TWS_CMD_SHARE_TWS_ESCO_RETX_NB,
};

osSemaphoreDef(app_tws_wait_cmd_rsp);
osSemaphoreId app_tws_wait_cmd_rsp = NULL;

extern "C" void register_tws_data_recevied_callback(btif_tws_data_received_callback_func func);

static void send_data_to_peer_tws_via_acl_data_path(uint8_t* data, uint16_t dataLen);

void app_tws_inform_waiting_cmd_rsp_thread(void)
{
    tws_dev_t *twsd = app_tws_get_env_pointer();
    LOG_PRINT_TWS("%s tws cmd rsp is received", __FUNCTION__);
    if(twsd->isWaitingCmdRsp && app_tws_wait_cmd_rsp)
    {                 
        twsd->isWaitingCmdRsp = false;    
        osSemaphoreRelease(app_tws_wait_cmd_rsp);                
    }    
}

static APP_TWS_CMD_INSTANCE_T* app_tws_get_entry_pointer_from_cmdcode(uint16_t cmdCode)
{
	for (uint32_t index = 0;
		index < ((uint32_t)__tws_custom_handler_table_end-(uint32_t)__tws_custom_handler_table_start)/sizeof(APP_TWS_CMD_INSTANCE_T);index++)
	{
		if (TWS_CUSTOM_COMMAND_PTR_FROM_ENTRY_INDEX(index)->cmdCode == cmdCode)
		{
			return TWS_CUSTOM_COMMAND_PTR_FROM_ENTRY_INDEX(index);
		}			
	}

	return NULL;
}

static void app_tws_receivd_cmd_handler(uint8_t* pDataBuf, uint16_t dataLength)
{    
    APP_TWS_CMD_DATA_FORMAT_T* pDataPacket = (APP_TWS_CMD_DATA_FORMAT_T *)pDataBuf;
    LOG_PRINT_TWS("app tws received cmd 0x%x", pDataPacket->cmdCode);
    LOG_PRINT_TWS("cmd data:");
    LOG_PRINT_TWS_DUMP8("0x%02x ", (uint8_t *)&(pDataPacket->content), dataLength-APP_TWS_CMD_CODE_SIZE);

    APP_TWS_CMD_INSTANCE_T* pCmdInstance = app_tws_get_entry_pointer_from_cmdcode(pDataPacket->cmdCode);
    if (pCmdInstance)
    {
        pCmdInstance->cmdHandler(pDataPacket->content, dataLength-APP_TWS_CMD_CODE_SIZE);
    }
}

bool app_tws_send_cmd_with_rsp(uint16_t cmdCode, uint8_t *pDataBuf, uint16_t dataLength, bool lbrt)
{
    if (IS_CONNECTED_WITH_TWS())
    {
        APP_TWS_CMD_DATA_FORMAT_T cmdData = {cmdCode};
        memcpy(cmdData.content, pDataBuf, dataLength);
#if IS_ENABLE_BT_DRIVER_REG_DEBUG_READING
        bt_drv_reg_op_connection_checker()
#endif
        send_data_to_peer_tws_via_acl_data_path((uint8_t *)&cmdData, dataLength + APP_TWS_CMD_CODE_SIZE);

        if (app_tws_wait_cmd_rsp){
            tws_dev_t *twsd = app_tws_get_env_pointer();
            twsd->isWaitingCmdRsp = true;
            LOG_PRINT_TWS("Waiting for TWS Cmd Rsp...");
#ifdef LBRT
			if(lbrt)
			{
				int32_t returnValue = osSemaphoreWait(app_tws_wait_cmd_rsp, APP_TWS_LBRT_TIMEOUT_IN_MS);
				if ((0 == returnValue) || (-1 == returnValue))
				{
					twsd->isWaitingCmdRsp = false;
					app_tws_exit_lbrt_mode();
					LOG_PRINT_TWS("LBRT transfer time out!");
					return false;
				}
			}
			else
#endif	
		    {
				int32_t returnValue = osSemaphoreWait(app_tws_wait_cmd_rsp, APP_TWS_WAITING_CMD_RESPONSE_TIMEOUT_IN_MS);
				if ((0 == returnValue) || (-1 == returnValue))
				{
					twsd->isWaitingCmdRsp = false;
					LOG_PRINT_TWS("Waiting TWS Cmd Rsp time out!");
					return false;
				}
		    }
        }

        return true;
    }
    else
    {
        return false;
    }
}
    
bool app_tws_send_cmd_without_rsp(uint16_t cmdCode, uint8_t *pDataBuf, uint16_t dataLength)
{
    if (IS_CONNECTED_WITH_TWS_PROFILE() && app_tws_is_roleswitch_in_idle_state())
    {
        APP_TWS_CMD_DATA_FORMAT_T cmdData = {cmdCode};
        memcpy(cmdData.content, pDataBuf, dataLength);
#if IS_ENABLE_BT_DRIVER_REG_DEBUG_READING
        bt_drv_reg_op_connection_checker();
#endif
        send_data_to_peer_tws_via_acl_data_path((uint8_t *)&cmdData, dataLength + APP_TWS_CMD_CODE_SIZE);
        return true;
    }
    else
    {
        return false;
    }
}

bool app_tws_force_send_cmd_without_rsp(uint16_t cmdCode, uint8_t *pDataBuf, uint16_t dataLength)
{
    if (IS_CONNECTED_WITH_TWS_PROFILE())
    {
        APP_TWS_CMD_DATA_FORMAT_T cmdData = {cmdCode};
        memcpy(cmdData.content, pDataBuf, dataLength);
#if IS_ENABLE_BT_DRIVER_REG_DEBUG_READING
        bt_drv_reg_op_connection_checker();
#endif
        send_data_to_peer_tws_via_acl_data_path((uint8_t *)&cmdData, dataLength + APP_TWS_CMD_CODE_SIZE);
        return true;
    }
    else
    {
        return false;
    }
}

bool app_tws_send_cmd_rsp(uint8_t *pRspData, uint16_t rspDataLen)
{
    if (IS_CONNECTED_WITH_TWS_PROFILE() && app_tws_is_roleswitch_in_idle_state())
    {
        APP_TWS_CMD_DATA_FORMAT_T cmdData = {APP_TWS_CMD_RSP};
        memcpy(cmdData.content, pRspData, rspDataLen);
        send_data_to_peer_tws_via_acl_data_path((uint8_t *)&cmdData, rspDataLen + APP_TWS_CMD_CODE_SIZE);
        return true;
    }
    else
    {
        return false;
    }
}

static void app_tws_cmd_rsp_received_handler(uint8_t* ptrParam, uint32_t paramLen)
{
    app_tws_inform_waiting_cmd_rsp_thread();
}

TWS_CUSTOM_COMMAND_TO_ADD(APP_TWS_CMD_RSP,   app_tws_cmd_rsp_received_handler);

void tws_init_cmd_handler(void)
{
    reset_tws_acl_data_path();

    app_tws_wait_cmd_rsp = osSemaphoreCreate(osSemaphore(app_tws_wait_cmd_rsp), 0);
}

void data_from_peer_tws_received(uint8_t* data, uint16_t dataLen)
{
    // push the received data into the queue
    app_bt_queue_push(&receivedTwsAclDataQueue, data, dataLen);
}

static void send_data_to_peer_tws_via_acl_data_path(uint8_t* data, uint16_t dataLen)
{
    // push the data pending for transmission into the queue
    app_bt_queue_push(&transmittedTwsAclDataQueue, data, dataLen);
}

void reset_tws_acl_data_path(void)
{
    // register the tws data received callback function
    register_tws_data_recevied_callback(data_from_peer_tws_received);

    // initialize the tws received acl data queue    
    app_bt_queue_init(&receivedTwsAclDataQueue, bufForTwsReceivedAclDataPath, 
        BUF_SIZE_FOR_TWS_REC_ACL_DATA_PATH);

    // initialize the tws transmitted acl data queue    
    app_bt_queue_init(&transmittedTwsAclDataQueue, bufForTwsTransmittedAclDataPath, 
        BUF_SIZE_FOR_TWS_TRANS_ACL_DATA_PATH);
}

void tws_received_acl_data_handler(void)
{
    if (IS_CONNECTED_WITH_TWS() && LengthOfCQueue(&receivedTwsAclDataQueue) > 0)
    {
        uint8_t tmpBufForReceivedAclData[BUF_SIZE_FOR_TWS_REC_ACL_DATA_PATH];
        uint16_t nextEntryLen = 0;
        do
        {
            nextEntryLen = app_bt_queue_get_next_entry_length(&receivedTwsAclDataQueue);            
            if (nextEntryLen > 0)
            {
                app_bt_queue_pop(&receivedTwsAclDataQueue, 
                    tmpBufForReceivedAclData, nextEntryLen);
                LOG_PRINT_TWS("[Received ACL data is:]");
                uint32_t length = nextEntryLen;
                uint32_t index = 0;
                if(length > 16)
                {
                    LOG_PRINT_TWS_DUMP8("%02x ", &tmpBufForReceivedAclData[index], 16);
                    index += 16;
                    length -= 16;
                }
                else
                {
                    LOG_PRINT_TWS_DUMP8("%02x ", &tmpBufForReceivedAclData[index], length);
                }
                
				app_tws_receivd_cmd_handler(tmpBufForReceivedAclData, nextEntryLen);
            }
            else
            {
                DeCQueue(&receivedTwsAclDataQueue, 0, 1);
            }
        }
        while (0);
        
        if (LengthOfCQueue(&receivedTwsAclDataQueue) > 0)
        {
            osapi_notify_evm();
        }
    }      
}

static void tws_all_acl_data_transmitted(void)
{
    app_tws_role_switch_handling_post_req_transmission();
}


void tws_transmitted_acl_data_handler(void)
{
    if (IS_CONNECTED_WITH_TWS() && LengthOfCQueue(&transmittedTwsAclDataQueue) > 0)
    {
        uint8_t tmpBufForTransmittingAclData[BUF_SIZE_FOR_TWS_TRANS_ACL_DATA_PATH];
        uint16_t nextEntryLen = 0;
        do
        {
            nextEntryLen = app_bt_queue_get_next_entry_length(&transmittedTwsAclDataQueue);            
            if (nextEntryLen > 0)
            {
            #if IS_USE_INTERNAL_ACL_DATA_PATH
                if (btif_me_is_sending_data_to_peer_dev_pending())
                {
                    break;
                }
            #else
                if (btif_l2cap_is_tws_reserved_channel_in_use())
                {
                    //TRACE("TWS reserved channel is in use!");
                    break;
                }
            #endif
            
                app_bt_queue_pop(&transmittedTwsAclDataQueue, 
                    tmpBufForTransmittingAclData, nextEntryLen);
                LOG_PRINT_TWS("[Transmitted ACL data is:]");
                uint32_t length = nextEntryLen;
                uint32_t index = 0;
                if (length > 16)
                {
                    LOG_PRINT_TWS_DUMP8("%02x ", &tmpBufForTransmittingAclData[index], 16);
                    index += 16;
                    length -= 16;
                }
                else
                {
                    LOG_PRINT_TWS_DUMP8("%02x ", &tmpBufForTransmittingAclData[index], length);
                }
                
                if (app_tws_is_master_mode())
                {
                #if IS_USE_INTERNAL_ACL_DATA_PATH
                    btif_me_send_data_to_peer_dev(btif_me_get_remote_device_hci_handle(slaveBtRemDev), nextEntryLen, tmpBufForTransmittingAclData);
                #else
                    btif_l2cap_send_data_to_peer_tws(btif_me_get_remote_device_hci_handle(slaveBtRemDev), nextEntryLen, tmpBufForTransmittingAclData);
                #endif
                }
                else if (app_tws_is_slave_mode())
                {
                #if IS_USE_INTERNAL_ACL_DATA_PATH    
                    btif_me_send_data_to_peer_dev(btif_me_get_remote_device_hci_handle(masterBtRemDev), nextEntryLen, tmpBufForTransmittingAclData);
                #else
                    btif_l2cap_send_data_to_peer_tws(btif_me_get_remote_device_hci_handle(masterBtRemDev), nextEntryLen,tmpBufForTransmittingAclData);
                #endif
                }
            }
            else
            {
                DeCQueue(&transmittedTwsAclDataQueue, 0, 1);
            }
        }
        while (0);
        
        if (LengthOfCQueue(&transmittedTwsAclDataQueue) > 0)
        {
            osapi_notify_evm();
        }
        else
        {
            tws_all_acl_data_transmitted();
        }
    }    
}

BOOL is_cmd_in_not_exit_sniff_list(uint16_t cmd_code)
{
    BOOL ret_status = false;
    for(uint8_t i = 0;i<sizeof(cmdNotExitSniff);i++)
    {
        if(cmdNotExitSniff[i] == cmd_code)
        {
            ret_status = true;
        }
    }
    return ret_status;
}


