/**
 ****************************************************************************************
 *
 * @file app_ble_cmd_handler.c
 *
 * @date 1 April 2017
 *
 * @brief The command handler framework over the BLE
 *
 * Copyright (C) 2017
 *
 *
 ****************************************************************************************
 */
#include "string.h"
#include "cmsis_os.h"
#include "hal_trace.h"
#include "hal_timer.h"
#include "stdbool.h"
#include "apps.h"
#include "rwapp_config.h"
#include "hal_cmu.h"
#include "hal_bootmode.h"

#include "app_ble_cmd_handler.h"
#include "app_ble_custom_cmd.h"

#if (BLE_APP_DATAPATH_SERVER)
extern void BLE_to_connect_tws_slave(uint32_t funcCode, uint8_t* ptrParam, uint32_t paramLen);
extern void BLE_to_connect_tws_slave_rsp_handler(BLE_CUSTOM_CMD_RET_STATUS_E retStatus, uint8_t* ptrParam, uint32_t paramLen);
extern void BLE_initial_handshake_handler(uint32_t funcCode, uint8_t* ptrParam, uint32_t paramLen);
extern void BLE_initial_handshake_rsp_handler(BLE_CUSTOM_CMD_RET_STATUS_E retStatus, uint8_t* ptrParam, uint32_t paramLen);

void BLE_dummy_handler(uint32_t funcCode, uint8_t* ptrParam, uint32_t paramLen)
{

}

void BLE_test_no_response_print_handler(uint32_t funcCode, uint8_t* ptrParam, uint32_t paramLen)
{
	TRACE("%s Get OP_TEST_NO_RESPONSE_PRINT command!!!", __FUNCTION__);
}

void BLE_test_with_response_print_handler(uint32_t funcCode, uint8_t* ptrParam, uint32_t paramLen)
{
	TRACE("%s Get OP_TEST_WITH_RESPONSE_PRINT command!!!", __FUNCTION__);

	uint32_t currentTicks = GET_CURRENT_TICKS();
	BLE_send_response_to_command(funcCode, NO_ERROR, (uint8_t *)&currentTicks, sizeof(currentTicks), true);
}

void BLE_test_with_response_print_rsp_handler(BLE_CUSTOM_CMD_RET_STATUS_E retStatus, uint8_t* ptrParam, uint32_t paramLen)
{
	if (NO_ERROR == retStatus)
	{
		TRACE("%s Get the response of OP_TEST_WITH_RESPONSE_PRINT command!!!", __FUNCTION__);
	}
	else if (TIMEOUT_WAITING_RESPONSE == retStatus)
	{
		TRACE("%s Timeout happens, doesn't get the response of OP_TEST_WITH_RESPONSE_PRINT command!!!", __FUNCTION__);
	}
}

static void app_otaMode_enter(void)
{
    TRACE("%s",__func__);
    hal_sw_bootmode_set(HAL_SW_BOOTMODE_ENTER_HIDE_BOOT);
    hal_cmu_reset_set(HAL_CMU_MOD_GLOBAL);
}

void BLE_enter_OTA_mode_handler(uint32_t funcCode, uint8_t* ptrParam, uint32_t paramLen)
{
	app_otaMode_enter();
}

#ifdef __SW_IIR_EQ_PROCESS__
int audio_config_eq_iir_via_config_structure(uint8_t *buf, uint32_t  len);
void BLE_iir_eq_handler(uint32_t funcCode, uint8_t* ptrParam, uint32_t paramLen)
{
	
	audio_config_eq_iir_via_config_structure(BLE_custom_command_raw_data_buffer_pointer(), BLE_custom_command_received_raw_data_size());
}

#endif


/** \brief  Instances list of BLE custom command handler, should be in the order of the
 *			command code definition in BLE_CUSTOM_CMD_CODE_E
 */
BLE_CUSTOM_CMD_INSTANCE_T     customCommandArray[] =
{
	// { command handler,  wait for response or not,  time out of waiting response in ms,  callback when the response is received } 
	// non-touchable instances
	{ BLE_get_response_handler,  			false,  0,  	NULL 										},
	{ BLE_raw_data_xfer_control_handler,  	true,  	2000,  	BLE_start_raw_data_xfer_control_rsp_handler },
	{ BLE_raw_data_xfer_control_handler,  	true,  	2000,  	BLE_stop_raw_data_xfer_control_rsp_handler 	},
	
	// TO ADD: the new custom command instance
	{ BLE_test_no_response_print_handler, 	false,	0,		NULL 										},
	{ BLE_test_with_response_print_handler,	true,	5000, 	BLE_test_with_response_print_rsp_handler 	},

	{ BLE_enter_OTA_mode_handler,			false,	0, 		NULL 	},

#ifdef __SW_IIR_EQ_PROCESS__
	{ BLE_iir_eq_handler,					false,	0, 		NULL	},
#else
	{ BLE_dummy_handler,					false,	0, 		NULL	},
#endif

	{ BLE_to_connect_tws_slave,				true,	5000,		BLE_to_connect_tws_slave_rsp_handler	},
	{ BLE_initial_handshake_handler,		true,	5000,		BLE_initial_handshake_rsp_handler	},

};
#endif

