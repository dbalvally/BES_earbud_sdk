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
/**
 ****************************************************************************************
 * @addtogroup APP
 * @{
 ****************************************************************************************
 */
/*
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "rwip_config.h"             // SW configuration

#if (BLE_APP_PRESENT)
#include <string.h>
#include "cmsis_os.h"

#include "app_task.h"                // Application task Definition
#include "app.h"                     // Application Definition
#include "gap.h"                     // GAP Definition
#include "gapm_task.h"               // GAP Manager Task API
#include "gapc_task.h"               // GAP Controller Task API

#include "gattc_task.h"

#include "co_bt.h"                   // Common BT Definition
#include "co_math.h"                 // Common Maths Definition

#include "l2cc.h"
#include "l2cc_pdu.h"

#include "nvrecord_ble.h"
#include "ble_tws.h"
#include "prf.h"
#include "gapm.h"

#include "nvrecord_env.h"
#include "factory_section.h"
#ifdef _AMA_
#include "ai_manager.h"
#include "app_ama_ble.h"                  // AMA Voice Application Definitions
#include "app_ama_handle.h"
#endif

#if (BLE_APP_SEC)
#include "app_sec.h"                 // Application security Definition
#endif // (BLE_APP_SEC)

#if (BLE_APP_HT)
#include "app_ht.h"                  // Health Thermometer Application Definitions
#endif //(BLE_APP_HT)

#if (BLE_APP_DATAPATH_SERVER)
#include "app_datapath_server.h"                  // Data Path Server Application Definitions
#endif //(BLE_APP_DATAPATH_SERVER)

#if (BLE_APP_DIS)
#include "app_dis.h"                 // Device Information Service Application Definitions
#endif //(BLE_APP_DIS)

#if (BLE_APP_BATT)
#include "app_batt.h"                // Battery Application Definitions
#endif //(BLE_APP_DIS)

#if (BLE_APP_HID)
#include "app_hid.h"                 // HID Application Definitions
#endif //(BLE_APP_HID)

#if (BLE_APP_VOICEPATH)
#include "app_voicepath_ble.h"         // Voice Path Application Definitions
#endif //(BLE_APP_VOICEPATH)

#if (BLE_APP_OTA)
#include "app_ota.h"                  // OTA Application Definitions
#endif //(BLE_APP_OTA)

#if (BLE_APP_ANCC)
#include "app_ancc.h"                  // ANCC Application Definitions
#endif //(BLE_APP_ANCC)

#if (BLE_APP_AMS)
#include "app_amsc.h"               // AMS Module Definition
#endif // (BLE_APP_AMS)

#if (BLE_APP_GFPS)
#include "app_gfps.h"               // Google Fast Pair Service Definitions
#endif
#if (DISPLAY_SUPPORT)
#include "app_display.h"             // Application Display Definition
#endif //(DISPLAY_SUPPORT)

#ifdef BLE_APP_AM0
#include "am0_app.h"                 // Audio Mode 0 Application
#endif //defined(BLE_APP_AM0)

#if (NVDS_SUPPORT)
#include "nvds.h"                    // NVDS Definitions
#endif //(NVDS_SUPPORT)

#include "ble_tws.h"
#include "cmsis_os.h"
#include "ke_timer.h"
#include "nvrecord.h"
#include "log_section.h"
#include "tgt_hardware.h"
#if VOICE_DATAPATH
#include "gsound_service.h"
#endif
#include "app_bt.h"

/*
 * DEFINES
 ****************************************************************************************
 */

/// Default Device Name if no value can be found in NVDS
#define APP_DFLT_DEVICE_NAME            (BLE_DEFAULT_NAME)
#define APP_DFLT_DEVICE_NAME_LEN        (strlen(APP_DFLT_DEVICE_NAME) + 1)


#if (BLE_APP_HID)
// HID Mouse
#define DEVICE_NAME        "Hid Mouse"
#else
#define DEVICE_NAME        "RW DEVICE"
#endif

#define DEVICE_NAME_SIZE    sizeof(DEVICE_NAME)

/**
 * UUID List part of ADV Data
 * --------------------------------------------------------------------------------------
 * x03 - Length
 * x03 - Complete list of 16-bit UUIDs available
 * x09\x18 - Health Thermometer Service UUID
 *   or
 * x12\x18 - HID Service UUID
 * --------------------------------------------------------------------------------------
 */

#if (BLE_APP_HT)
#define APP_HT_ADV_DATA_UUID        "\x03\x03\x09\x18"
#define APP_HT_ADV_DATA_UUID_LEN    (4)
#endif //(BLE_APP_HT)

#if (BLE_APP_HID)
#define APP_HID_ADV_DATA_UUID       "\x03\x03\x12\x18"
#define APP_HID_ADV_DATA_UUID_LEN   (4)
#endif //(BLE_APP_HID)
#if _GFPS_
#define APP_GFPS_ADV_FLAGS_UUID      "\x02\x01\x01"
#define APP_GFPS_ADV_FLAGS_UUID_LEN  (3)
#define APP_GFPS_ADV_GFPS_MODULE_ID_UUID  "\x06\x16\x2C\xFE\x00\x01\x04"
#define APP_GFPS_ADV_GFPS_MODULE_ID_UUID_LEN (7)
#define APP_GFPS_ADV_GFPS_1_0_SERVICE_DATA  "\x06\x16\x2C\xFE\x05\x02\xF0"

#define APP_GFPS_ADV_POWER_UUID  "\x02\x0a\xEC"
#define APP_GFPS_ADV_POWER_UUID_LEN (3)
#define APP_GFPS_ADV_APPEARANCE_UUID   "\x03\x19\xda\x96"
#define APP_GFPS_ADV_APPEARANCE_UUID_LEN (4)
#define APP_GFPS_ADV_MANU_SPE_UUID_TEST  "\x07\xFF\xe0\x00\x01\x5B\x32\x01"
#define APP_GFPS_ADV_MANU_SPE_UUID_LEN (8)
#endif
#if (BLE_APP_DATAPATH_SERVER)
/*
 * x11 - Length
 * x07 - Complete list of 16-bit UUIDs available
 * .... the 128 bit UUIDs
 */
#define APP_DATAPATH_SERVER_ADV_DATA_UUID        "\x11\x07\x9e\x34\x9B\x5F\x80\x00\x00\x80\x00\x10\x00\x00\x00\x01\x00\x01"
#define APP_DATAPATH_SERVER_ADV_DATA_UUID_LEN    (18)
#endif //(BLE_APP_HT)

#ifdef _AMA_
#define APP_AMA_UUID            "\x03\x16\x03\xFE"
#define APP_AMA_UUID_LEN        (4)
#endif
/**
 * Appearance part of ADV Data
 * --------------------------------------------------------------------------------------
 * x03 - Length
 * x19 - Appearance
 * x03\x00 - Generic Thermometer
 *   or
 * xC2\x04 - HID Mouse
 * --------------------------------------------------------------------------------------
 */

#if (BLE_APP_HT)
#define APP_HT_ADV_DATA_APPEARANCE    "\x03\x19\x00\x03"
#endif //(BLE_APP_HT)

#if (BLE_APP_HID)
#define APP_HID_ADV_DATA_APPEARANCE   "\x03\x19\xC2\x03"
#endif //(BLE_APP_HID)

#define APP_ADV_DATA_APPEARANCE_LEN  (4)

#if defined(__SW_IIR_EQ_PROCESS__) || defined(__HW_FIR_EQ_PROCESS__)
/**
 * Default Scan response data
 * --------------------------------------------------------------------------------------
 * x08                             - Length
 * xFF                             - Vendor specific advertising type
 * \xB0\x02\x45\x51\x5F\x54\x45\x53\x54 - "EQ_TEST"
 * --------------------------------------------------------------------------------------
 */
#define APP_SCNRSP_DATA         "\x0A\xFF\xB0\x02\x45\x51\x5F\x54\x45\x53\x54"
#define APP_SCNRSP_DATA_LEN     (11)
#else
#define APP_SCNRSP_DATA         ""
#define APP_SCNRSP_DATA_LEN     (0)
#endif

/**
 * Advertising Parameters
 */
#if (BLE_APP_HID)
/// Default Advertising duration - 30s (in multiple of 10ms)
#define APP_DFLT_ADV_DURATION   (3000)
#endif //(BLE_APP_HID)


#define BISTO_SERVICE_DATA_LEN 6
#define BISTO_SERVICE_DATA_UUID 0xFE26
#define DEVICE_MODEL_ID 0x00003E
#ifdef _AMA_
bool app_ama_is_ble_adv_uuid_enabled(void);
void bt_drv_restore_ble_control_timing(void);
#endif

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */

typedef void (*appm_add_svc_func_t)(void);

/*
 * ENUMERATIONS
 ****************************************************************************************
 */

/// List of service to add in the database
enum appm_svc_list
{
    #if (BLE_APP_HT)
    APPM_SVC_HTS,
    #endif //(BLE_APP_HT)
    #if (BLE_APP_DIS)
    APPM_SVC_DIS,
    #endif //(BLE_APP_DIS)
    #if (BLE_APP_BATT)
    APPM_SVC_BATT,
    #endif //(BLE_APP_BATT)
    #if (BLE_APP_HID)
    APPM_SVC_HIDS,
    #endif //(BLE_APP_HID)
    #ifdef BLE_APP_AM0
    APPM_SVC_AM0_HAS,
    #endif //defined(BLE_APP_AM0)
    #if (BLE_APP_HR)
    APPM_SVC_HRP,
    #endif
    #if (BLE_APP_DATAPATH_SERVER)
    APPM_SVC_DATAPATH_SERVER,
    #endif //(BLE_APP_DATAPATH_SERVER)
    #if (BLE_APP_VOICEPATH)
    APPM_SVC_VOICEPATH,
    #if GSOUND_ENABLED
    APPM_SVC_BMS,
    #endif
    #endif //(BLE_APP_VOICEPATH)
	#if (BLE_AMA_VOICE)
	APPM_AMA_SMARTVOICE,
	#endif //(BLE_APP_AMA)
    #if (ANCS_PROXY_ENABLE)
    APPM_SVC_ANCSP,
    APPM_SVC_AMSP,
    #endif
    #if (BLE_APP_ANCC)
    APPM_SVC_ANCC,
    #endif //(BLE_APP_ANCC)
    #if (BLE_APP_AMS)
    APPM_SVC_AMSC,
    #endif //(BLE_APP_AMS)
    #if (BLE_APP_OTA)
    APPM_SVC_OTA,
    #endif //(BLE_APP_OTA)
    #if (BLE_APP_GFPS)
    APPM_SVC_GFPS,
    #endif //(BLE_APP_GFPS)

    APPM_SVC_LIST_STOP,
};

/*
 * LOCAL VARIABLES DEFINITIONS
 ****************************************************************************************
 */
//gattc_msg_handler_tab
//#define KE_MSG_HANDLER_TAB(task)   __STATIC const struct ke_msg_handler task##_msg_handler_tab[] =
/// Application Task Descriptor

//static const struct ke_task_desc TASK_DESC_APP = {&appm_default_handler,
//                                                  appm_state, APPM_STATE_MAX, APP_IDX_MAX};
extern const struct ke_task_desc TASK_DESC_APP;

/// List of functions used to create the database
static const appm_add_svc_func_t appm_add_svc_func_list[APPM_SVC_LIST_STOP] =
{
    #if (BLE_APP_HT)
    (appm_add_svc_func_t)app_ht_add_hts,
    #endif //(BLE_APP_HT)
    #if (BLE_APP_DIS)
    (appm_add_svc_func_t)app_dis_add_dis,
    #endif //(BLE_APP_DIS)
    #if (BLE_APP_BATT)
    (appm_add_svc_func_t)app_batt_add_bas,
    #endif //(BLE_APP_BATT)
    #if (BLE_APP_HID)
    (appm_add_svc_func_t)app_hid_add_hids,
    #endif //(BLE_APP_HID)
    #ifdef BLE_APP_AM0
    (appm_add_svc_func_t)am0_app_add_has,
    #endif //defined(BLE_APP_AM0)
    #if (BLE_APP_HR)
    (appm_add_svc_func_t)app_hrps_add_profile,
    #endif
    #if (BLE_APP_DATAPATH_SERVER)
    (appm_add_svc_func_t)app_datapath_add_datapathps,
    #endif //(BLE_APP_DATAPATH_SERVER)
    #if (BLE_APP_VOICEPATH)
    (appm_add_svc_func_t)app_ble_voicepath_add_svc,
    #if GSOUND_ENABLED
    (appm_add_svc_func_t)app_ble_bms_add_svc,
    #endif 
    #endif //(BLE_APP_VOICEPATH)
	#if (BLE_APP_AMA_VOICE)
    (appm_add_svc_func_t)app_ama_add_ama,
	#endif //(BLE_APP_AMA_VOICE)
    #if (ANCS_PROXY_ENABLE)
    (appm_add_svc_func_t)app_ble_ancsp_add_svc,
    (appm_add_svc_func_t)app_ble_amsp_add_svc,
    #endif
    #if (BLE_APP_ANCC)
    (appm_add_svc_func_t)app_ancc_add_ancc,
    #endif
    #if (BLE_APP_AMS)
    (appm_add_svc_func_t)app_amsc_add_amsc,
    #endif
    #if (BLE_APP_OTA)
    (appm_add_svc_func_t)app_ota_add_ota,
    #endif //(BLE_APP_OTA)  
    #if (BLE_APP_GFPS)
    (appm_add_svc_func_t)app_gfps_add_gfps,
    #endif 
};

/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */

/// Application Environment Structure
struct app_env_tag app_env;

/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
 */
void appm_init()
{
    BLE_APP_FUNC_ENTER();

    // Reset the application manager environment
    memset(&app_env, 0, sizeof(app_env));

    // Create APP task
    ke_task_create(TASK_APP, &TASK_DESC_APP);

    // Initialize Task state
    ke_state_set(TASK_APP, APPM_INIT);

    // Load the device name from NVDS

    // Get the Device Name to add in the Advertising Data (Default one or NVDS one)
#ifdef _BLE_NVDS_
    const char* ble_name_in_nv = (const char*)factory_section_get_ble_name();
#else
    const char* ble_name_in_nv = BLE_DEFAULT_NAME;
#endif
    uint32_t nameLen = strlen(ble_name_in_nv) + 1;
    if (nameLen > APP_DEVICE_NAME_MAX_LEN)
    {
        nameLen = APP_DEVICE_NAME_MAX_LEN;
    }
    // Get default Device Name (No name if not enough space)
    memcpy(app_env.dev_name, ble_name_in_nv, nameLen);
    app_env.dev_name[nameLen - 1] = '\0';
    app_env.dev_name_len = nameLen;

#ifdef _BLE_NVDS_
    nv_record_blerec_init();

    nv_record_blerec_get_local_irk(app_env.loc_irk);
#else
    uint8_t counter;

    //avoid ble irk collision low probability
    uint32_t generatedSeed = hal_sys_timer_get();
    for (uint8_t index = 0; index < sizeof(bt_addr); index++)
    {
        generatedSeed ^= (((uint32_t)(bt_addr[index])) << (hal_sys_timer_get()&0xF));
    }
    srand(generatedSeed);

    // generate a new IRK
    for (counter = 0; counter < KEY_LEN; counter++)
    {
        app_env.loc_irk[counter]    = (uint8_t)co_rand_word();
    }
#endif

    /*------------------------------------------------------
     * INITIALIZE ALL MODULES
     *------------------------------------------------------*/

    // load device information:

    #if (DISPLAY_SUPPORT)
    app_display_init();
    #endif //(DISPLAY_SUPPORT)

    #if (BLE_APP_SEC)
    // Security Module
    app_sec_init();
    #endif // (BLE_APP_SEC)

    #if (BLE_APP_HT)
    // Health Thermometer Module
    app_ht_init();
    #endif //(BLE_APP_HT)

    #if (BLE_APP_DIS)
    // Device Information Module
    app_dis_init();
    #endif //(BLE_APP_DIS)

    #if (BLE_APP_HID)
    // HID Module
    app_hid_init();
    #endif //(BLE_APP_HID)

    #if (BLE_APP_BATT)
    // Battery Module
    app_batt_init();
    #endif //(BLE_APP_BATT)

    #ifdef BLE_APP_AM0
    // Audio Mode 0 Module
    am0_app_init();
    #endif // defined(BLE_APP_AM0)

    #if (BLE_APP_VOICEPATH)
    // Voice Path Module
    app_ble_voicepath_init();
    #endif //(BLE_APP_VOICEPATH)

    #if (BLE_APP_DATAPATH_SERVER)
    // Data Path Server Module
    app_datapath_server_init();
    #endif //(BLE_APP_DATAPATH_SERVER)
	#if (BLE_APP_AMA_VOICE)
    // AMA Voice Module
    app_ama_init();
	#endif //(BLE_APP_AMA_VOICE)

    #if (BLE_APP_OTA)
    // OTA Module
    app_ota_init();
    #endif //(BLE_APP_OTA)

    #if (BLE_APP_GFPS)
    app_gfps_init();
    #endif

#ifdef ADAPTIVE_BLE_CONN_PARAM_ENABLED
    app_init_ble_update_conn_param_state_machine();
#endif

    BLE_APP_FUNC_LEAVE();
}

bool appm_add_svc(void)
{
    // Indicate if more services need to be added in the database
    bool more_svc = false;

    // Check if another should be added in the database
    if (app_env.next_svc != APPM_SVC_LIST_STOP)
    {
        ASSERT_INFO(appm_add_svc_func_list[app_env.next_svc] != NULL, app_env.next_svc, 1);

        BLE_APP_DBG("appm_add_svc adds service");

        // Call the function used to add the required service
        appm_add_svc_func_list[app_env.next_svc]();

        // Select following service to add
        app_env.next_svc++;
        more_svc = true;
    }
    else
    {
        BLE_APP_DBG("appm_add_svc doesn't execute, next svc is %d", app_env.next_svc);
    }


    return more_svc;
}

uint16_t appm_get_conhdl_from_conidx(uint8_t conidx)
{
    return app_env.context[conidx].conhdl;
}

void appm_disconnect_all(void)
{
    for (uint8_t index = 0;index < BLE_CONNECTION_MAX;index++)
    {
        if (app_env.context[index].isConnected)
        {
            appm_disconnect(index);
        }
    }
}

uint8_t appm_is_connected(void)
{
    for (uint8_t index = 0;index < BLE_CONNECTION_MAX;index++)
    {
        if (app_env.context[index].isConnected)
        {
            return 1;
        }
    } 
    return 0;
}
void appm_disconnect(uint8_t conidx)
{
    log_event_1(EVENT_INITIATIVE_BLE_DISCONNECTING,conidx);
    struct gapc_disconnect_cmd *cmd = KE_MSG_ALLOC(GAPC_DISCONNECT_CMD,
                                                   KE_BUILD_ID(TASK_GAPC, conidx), 
                                                   TASK_APP,
                                                   gapc_disconnect_cmd);

    cmd->operation = GAPC_DISCONNECT;
    cmd->reason    = CO_ERROR_REMOTE_USER_TERM_CON;

    // Send the message
    ke_msg_send(cmd);
}

#if _GFPS_
static uint8_t app_is_in_fastpairing_mode = false;
#define APP_FAST_PAIRING_TIMEOUT_IN_MS  120000

osTimerId app_fast_pairing_timeout_timer = NULL;
static int app_fast_pairing_timeout_timehandler(void const *param);
osTimerDef (APP_FAST_PAIRING_TIMEOUT_TIMER, (void (*)(void const *))app_fast_pairing_timeout_timehandler);                      // define timers

bool app_is_in_fastparing_mode(void)
{
    return app_is_in_fastpairing_mode;
}

void app_set_in_fastpairing_mode_flag(bool isEnabled)
{
    app_is_in_fastpairing_mode = isEnabled;
}

void app_exit_fastpairing_mode(void)
{
    if (app_is_in_fastparing_mode())
    {
        if (NULL == app_fast_pairing_timeout_timer)
        {
            app_fast_pairing_timeout_timer = 
                osTimerCreate(osTimer(APP_FAST_PAIRING_TIMEOUT_TIMER), 
                osTimerOnce, NULL);
        }
        app_set_in_fastpairing_mode_flag(false);
        
        // reset ble adv
        app_start_ble_tws_advertising_in_bt_thread(GAPM_ADV_UNDIRECT, BLE_TWS_ADV_SLOW_INTERVAL_IN_MS); 
        LOG_PRINT_FP("exit fast pairing mode");
    }
}

static int app_fast_pairing_timeout_timehandler(void const *param)
{
    app_exit_fastpairing_mode();
    return 0;
}

void app_enter_fastpairing_mode(void)
{
    LOG_PRINT_FP("enter fast pairing mode");
    app_set_in_fastpairing_mode_flag(true);

    if (NULL == app_fast_pairing_timeout_timer)
    {
        app_fast_pairing_timeout_timer = 
            osTimerCreate(osTimer(APP_FAST_PAIRING_TIMEOUT_TIMER), 
            osTimerOnce, NULL);
    }

    app_start_ble_tws_advertising_in_bt_thread(GAPM_ADV_UNDIRECT, BLE_TWS_FAST_PAIR_ADV_INTERVAL_IN_MS);

    osTimerStart(app_fast_pairing_timeout_timer, APP_FAST_PAIRING_TIMEOUT_IN_MS);
}
#endif

/**
 ****************************************************************************************
 * Advertising Functions
 ****************************************************************************************
 */

void appm_update_adv_data(uint8_t* pAdvData, uint32_t dataLength)
{
    BLE_APP_FUNC_ENTER();
    struct gapm_update_advertise_data_cmd *cmd = KE_MSG_ALLOC(GAPM_UPDATE_ADVERTISE_DATA_CMD,
                                        TASK_GAPM, TASK_APP,
                                        gapm_update_advertise_data_cmd);    
    memcpy(cmd->adv_data, pAdvData, dataLength);
    cmd->adv_data_len = dataLength;
    //  Device Name Length
    int8_t avail_space;

    // Get remaining space in the Advertising Data - 2 bytes are used for name length/flag
    avail_space = (ADV_DATA_LEN - 3) - cmd->adv_data_len - 2;

    // Check if data can be added to the Advertising data
    if (avail_space > 2)
    {
    
        avail_space = co_min(avail_space, app_env.dev_name_len);

        cmd->adv_data[cmd->adv_data_len]     = avail_space + 1;
        // Fill Device Name Flag
        cmd->adv_data[cmd->adv_data_len + 1] = (avail_space == app_env.dev_name_len) ? '\x08' : '\x09';
        // Copy device name
        memcpy(&cmd->adv_data[cmd->adv_data_len + 2], app_env.dev_name, avail_space);

        // Update Advertising Data Length
        cmd->adv_data_len += (avail_space + 2);
    }

    ke_msg_send(cmd);
    BLE_APP_FUNC_LEAVE();
}

void appm_start_advertising(uint8_t advType, uint8_t advChMap, uint32_t advMaxIntervalInMs, uint32_t advMinIntervalInMs, \
    uint8_t* ptrManufactureData, uint32_t manufactureDataLen)
{
    BLE_APP_FUNC_ENTER();

    // Check if the advertising procedure is already is progress
    //if (ke_state_get(TASK_APP) == APPM_READY)
    {
        // Prepare the GAPM_START_ADVERTISE_CMD message
        struct gapm_start_advertise_cmd *cmd = KE_MSG_ALLOC(GAPM_START_ADVERTISE_CMD,
                                                            TASK_GAPM, TASK_APP,
                                                            gapm_start_advertise_cmd);
#if BLE_IS_USE_RPA
        cmd->op.addr_src    = GAPM_GEN_RSLV_ADDR;
        cmd->op.randomAddrRenewIntervalInSecond = 60*15*1.28;       
#else
        cmd->op.addr_src    = GAPM_STATIC_ADDR;
#endif
        cmd->channel_map    = advChMap;

        cmd->intv_min = advMinIntervalInMs*1000/625;
        cmd->intv_max = advMaxIntervalInMs*1000/625;

        cmd->op.code        = advType;
        cmd->info.host.mode = GAP_GEN_DISCOVERABLE;

        cmd->info.host.flags = GAP_LE_GEN_DISCOVERABLE_FLG | GAP_BR_EDR_NOT_SUPPORTED;
        /*-----------------------------------------------------------------------------------
         * Set the Advertising Data and the Scan Response Data
         *---------------------------------------------------------------------------------*/
#ifdef _AMA_
    uint8_t avail_space;

    // Scan Response Data
    cmd->info.host.scan_rsp_data_len = 0;
    
	if (app_ama_is_ble_adv_uuid_enabled())
	{
		memcpy(&cmd->info.host.scan_rsp_data[cmd->info.host.scan_rsp_data_len],
		  	APP_AMA_UUID, APP_AMA_UUID_LEN);
		cmd->info.host.scan_rsp_data_len += APP_AMA_UUID_LEN;
	}
	
    avail_space = (SCAN_RSP_DATA_LEN) - cmd->info.host.scan_rsp_data_len - 2;
    // Check if data can be added to the Scan response Data
    if (avail_space > 2)
    {
        avail_space = co_min(avail_space, app_env.dev_name_len);

        cmd->info.host.scan_rsp_data[cmd->info.host.scan_rsp_data_len]     = avail_space + 1;
        // Fill Device Name Flag
        cmd->info.host.scan_rsp_data[cmd->info.host.scan_rsp_data_len + 1] = (avail_space == app_env.dev_name_len) ? '\x08' : '\x09';
        // Copy device name
        memcpy(&cmd->info.host.scan_rsp_data[cmd->info.host.scan_rsp_data_len + 2], app_env.dev_name, avail_space);

        // Update Scan response Data Length
        cmd->info.host.scan_rsp_data_len += (avail_space + 2);
    }

#else
        // Flag value is set by the GAP

        // Scan Response Data
        cmd->info.host.scan_rsp_data_len = 0;
        if (manufactureDataLen > SCAN_RSP_DATA_LEN - 3)
        {
            manufactureDataLen = SCAN_RSP_DATA_LEN - 3;
        }

        memcpy(&cmd->info.host.scan_rsp_data[cmd->info.host.scan_rsp_data_len],
                       ptrManufactureData, manufactureDataLen);
        cmd->info.host.scan_rsp_data_len += manufactureDataLen;

        cmd->info.host.adv_data_len = 0;

#if _GFPS_              
#if BLE_APP_GFPS_VER==FAST_PAIR_REV_2_0
        if (app_is_in_fastparing_mode())
        {
            memcpy(&cmd->info.host.adv_data[cmd->info.host.adv_data_len],
                    APP_GFPS_ADV_GFPS_MODULE_ID_UUID, APP_GFPS_ADV_GFPS_MODULE_ID_UUID_LEN);
            cmd->info.host.adv_data_len +=            APP_GFPS_ADV_GFPS_MODULE_ID_UUID_LEN;

            memcpy(&cmd->info.host.adv_data[cmd->info.host.adv_data_len],
                    APP_GFPS_ADV_POWER_UUID, APP_GFPS_ADV_POWER_UUID_LEN);
            cmd->info.host.adv_data_len +=   APP_GFPS_ADV_POWER_UUID_LEN;
        }
        else
        {
            uint8_t serviceData[32];
            serviceData[0] = 0x03;
            serviceData[1] = 0x16;
            serviceData[2] = 0x2C;
            serviceData[3] = 0xFE;

            uint8_t dataLen = app_gfps_generate_accountkey_data(&serviceData[4]);
            serviceData[0] += dataLen;
            memcpy(&cmd->info.host.adv_data[cmd->info.host.adv_data_len],
                serviceData, serviceData[0]+1);
            cmd->info.host.adv_data_len += (serviceData[0]+1);

            memcpy(&cmd->info.host.adv_data[cmd->info.host.adv_data_len],
                    APP_GFPS_ADV_POWER_UUID, APP_GFPS_ADV_POWER_UUID_LEN);
            cmd->info.host.adv_data_len += APP_GFPS_ADV_POWER_UUID_LEN;
        }
#else
        if (app_is_in_fastparing_mode())
        {                   
            memcpy(&cmd->info.host.adv_data[cmd->info.host.adv_data_len],
                    APP_GFPS_ADV_GFPS_1_0_SERVICE_DATA, APP_GFPS_ADV_GFPS_MODULE_ID_UUID_LEN);
            cmd->info.host.adv_data_len += APP_GFPS_ADV_GFPS_MODULE_ID_UUID_LEN;

            memcpy(&cmd->info.host.adv_data[cmd->info.host.adv_data_len],
                    APP_GFPS_ADV_POWER_UUID, APP_GFPS_ADV_POWER_UUID_LEN);
            cmd->info.host.adv_data_len += APP_GFPS_ADV_POWER_UUID_LEN;
        }
#endif
        if (!app_is_in_fastparing_mode())
        {
            cmd->info.host.adv_data[cmd->info.host.adv_data_len++] = BISTO_SERVICE_DATA_LEN;
            cmd->info.host.adv_data[cmd->info.host.adv_data_len++] = 0x16; /* Service Data */
            cmd->info.host.adv_data[cmd->info.host.adv_data_len++] = (BISTO_SERVICE_DATA_UUID >> 0) & 0xFF;
            cmd->info.host.adv_data[cmd->info.host.adv_data_len++] = (BISTO_SERVICE_DATA_UUID >> 8) & 0xFF;
            cmd->info.host.adv_data[cmd->info.host.adv_data_len++] = (DEVICE_MODEL_ID >> 0) & 0xFF;
            cmd->info.host.adv_data[cmd->info.host.adv_data_len++] = (DEVICE_MODEL_ID >> 8) & 0xFF;
            cmd->info.host.adv_data[cmd->info.host.adv_data_len++] = (DEVICE_MODEL_ID >> 16) & 0xFF;
        }
#endif


        //  Device Name Length
        int8_t avail_space;

        #if 0
        // Get remaining space in the Advertising Data - 2 bytes are used for name length/flag
        avail_space = (ADV_DATA_LEN - 3) - cmd->info.host.scan_rsp_data_len - 2;
        
        // Check if data can be added to the Advertising data
        if (avail_space > 2)
        {
            avail_space = co_min(avail_space, app_env.dev_name_len);
        
            cmd->info.host.scan_rsp_data[cmd->info.host.scan_rsp_data_len]     = avail_space + 1;
            // Fill Device Name Flag
            cmd->info.host.scan_rsp_data[cmd->info.host.scan_rsp_data_len + 1] = (avail_space == app_env.dev_name_len) ? '\x08' : '\x09';
            // Copy device name
            memcpy(&cmd->info.host.scan_rsp_data[cmd->info.host.scan_rsp_data_len + 2], app_env.dev_name, avail_space);
        
            // Update Advertising Data Length
            cmd->info.host.scan_rsp_data_len += (avail_space + 2);
        }
        #else
        // Get remaining space in the Advertising Data - 2 bytes are used for name length/flag
        avail_space = (ADV_DATA_LEN - 3) - cmd->info.host.adv_data_len - 2;

        // Check if data can be added to the Advertising data
        if (avail_space > 2)
        {
        
            avail_space = co_min(avail_space, app_env.dev_name_len);

            cmd->info.host.adv_data[cmd->info.host.adv_data_len]     = avail_space + 1;
            // Fill Device Name Flag
            cmd->info.host.adv_data[cmd->info.host.adv_data_len + 1] = (avail_space == app_env.dev_name_len) ? '\x08' : '\x09';
            // Copy device name
            memcpy(&cmd->info.host.adv_data[cmd->info.host.adv_data_len + 2], app_env.dev_name, avail_space);

            // Update Advertising Data Length
            cmd->info.host.adv_data_len += (avail_space + 2);
        }
        #endif
#endif //_AMA_


        LOG_PRINT_BLE("adv data:");
        LOG_PRINT_BLE_DUMP8("%02x ", &cmd->info.host.adv_data[0], cmd->info.host.adv_data_len);
        LOG_PRINT_BLE("scan resp:");
        LOG_PRINT_BLE_DUMP8("%02x ", &cmd->info.host.scan_rsp_data[0], cmd->info.host.scan_rsp_data_len);  

        // Send the message
        ke_msg_send(cmd);

        // Set the state of the task to APPM_ADVERTISING
        ke_state_set(TASK_APP, APPM_ADVERTISING);
    }
    // else ignore the request

    BLE_APP_FUNC_LEAVE();
}

void appm_stop_advertising(void)
{
    BLE_APP_FUNC_ENTER();

    LOG_PRINT_BLE("app status: %d", ke_state_get(TASK_APP));
    if (ke_state_get(TASK_APP) == APPM_ADVERTISING)
    {
        #if (BLE_APP_HID)
        // Stop the advertising timer if needed
        if (ke_timer_active(APP_ADV_TIMEOUT_TIMER, TASK_APP))
        {
            ke_timer_clear(APP_ADV_TIMEOUT_TIMER, TASK_APP);
        }
        #endif //(BLE_APP_HID)


        // Go in ready state
        ke_state_set(TASK_APP, APPM_READY);

        // Prepare the GAPM_CANCEL_CMD message
        struct gapm_cancel_cmd *cmd = KE_MSG_ALLOC(GAPM_CANCEL_CMD,
                                                   TASK_GAPM, TASK_APP,
                                                   gapm_cancel_cmd);

        cmd->operation = GAPM_CANCEL;

        // Send the message
        ke_msg_send(cmd);

        #if (DISPLAY_SUPPORT)
        // Update advertising state screen
        app_display_set_adv(false);
        #endif //(DISPLAY_SUPPORT)
    }
    // else ignore the request
    BLE_APP_FUNC_LEAVE();
}

void appm_start_scanning(uint16_t intervalInMs, uint16_t windowInMs, uint32_t filtPolicy)
{
    BLE_APP_FUNC_ENTER();

    struct gapm_start_scan_cmd* cmd = KE_MSG_ALLOC(GAPM_START_SCAN_CMD,
                                                                 TASK_GAPM, TASK_APP,
                                                                 gapm_start_scan_cmd);


    //cmd->op.code = GAPM_SCAN_PASSIVE;
    cmd->op.code = GAPM_SCAN_ACTIVE;
#if BLE_IS_USE_RPA  
    cmd->op.addr_src = GAPM_GEN_RSLV_ADDR;
#else
    cmd->op.addr_src = GAPM_STATIC_ADDR;
#endif
    cmd->interval = intervalInMs*1000/625;
    cmd->window = windowInMs*1000/625;
    cmd->mode = GAP_OBSERVER_MODE; //GAP_GEN_DISCOVERY;
    cmd->filt_policy = filtPolicy;
    cmd->filter_duplic = SCAN_FILT_DUPLIC_DIS;

    ke_state_set(TASK_APP, APPM_SCANNING);

    ke_msg_send(cmd);
    BLE_APP_FUNC_LEAVE();
}

void appm_stop_scanning(void)
{
    BLE_APP_FUNC_ENTER();

    if (ke_state_get(TASK_APP) == APPM_SCANNING)
    {
        // Go in ready state
        ke_state_set(TASK_APP, APPM_READY);

        // Prepare the GAPM_CANCEL_CMD message
        struct gapm_cancel_cmd *cmd = KE_MSG_ALLOC(GAPM_CANCEL_CMD,
                                                   TASK_GAPM, TASK_APP,
                                                   gapm_cancel_cmd);

        cmd->operation = GAPM_CANCEL;

        // Send the message
        ke_msg_send(cmd);
    }
    BLE_APP_FUNC_LEAVE();
}

void appm_add_dev_into_whitelist(struct gap_bdaddr* ptBdAddr)
{
    BLE_APP_FUNC_ENTER();
    struct gapm_white_list_mgt_cmd* cmd = KE_MSG_ALLOC_DYN(GAPM_WHITE_LIST_MGT_CMD,
                                                                 TASK_GAPM, TASK_APP,
                                                                 gapm_white_list_mgt_cmd, sizeof(struct gap_bdaddr));
    cmd->operation = GAPM_ADD_DEV_IN_WLIST;
    cmd->nb = 1;

    memcpy(cmd->devices, ptBdAddr, sizeof(struct gap_bdaddr));

    ke_msg_send(cmd);
    BLE_APP_FUNC_LEAVE();
}

void appm_start_connecting(struct gap_bdaddr* ptBdAddr)
{
    BLE_APP_FUNC_ENTER();
    struct gapm_start_connection_cmd* cmd = KE_MSG_ALLOC_DYN(GAPM_START_CONNECTION_CMD,
                                                                 TASK_GAPM, TASK_APP,
                                                                 gapm_start_connection_cmd, sizeof(struct gap_bdaddr));
    cmd->ce_len_max = 200;
    cmd->ce_len_min = 100;
    cmd->con_intv_max = 70; // in the unit of 1.25ms
    cmd->con_intv_min = 45; // in the unit of 1.25ms
    cmd->con_latency = 0;
    cmd->superv_to = 1000;  // in the unit of 10ms
    cmd->op.code = GAPM_CONNECTION_DIRECT;
    cmd->op.addr_src = ptBdAddr->addr_type;
    cmd->nb_peers = 1;
    cmd->scan_interval = ((600) * 1000 / 625);
    cmd->scan_window = ((300) * 1000 / 625);

    memcpy(cmd->peers, ptBdAddr, sizeof(struct gap_bdaddr));

    ke_state_set(TASK_APP, APPM_CONNECTING);

    ke_msg_send(cmd);
    BLE_APP_FUNC_LEAVE();
}

void appm_stop_connecting(void)
{
    BLE_APP_FUNC_ENTER();
    if (ke_state_get(TASK_APP) == APPM_CONNECTING)
    {
        // Go in ready state
        ke_state_set(TASK_APP, APPM_READY);

        // Prepare the GAPM_CANCEL_CMD message
        struct gapm_cancel_cmd *cmd = KE_MSG_ALLOC(GAPM_CANCEL_CMD,
                                                   TASK_GAPM, TASK_APP,
                                                   gapm_cancel_cmd);

        cmd->operation = GAPM_CANCEL;

        // Send the message
        ke_msg_send(cmd);
    }
    BLE_APP_FUNC_LEAVE();
}

#ifdef ADAPTIVE_BLE_CONN_PARAM_ENABLED
#define APP_BLE_UPDATE_CONN_PARAM_TRY_TIMES     1
#define APP_BLE_UPDATE_CONN_PARAM_CHECK_PERIOD_IN_MS    2000
static void app_ble_update_conn_param_timer_handler(void const *param);
osTimerDef (APP_BLE_UPDATE_CONN_PARAM_TIMER, app_ble_update_conn_param_timer_handler);
osTimerId app_ble_update_conn_param_timer = NULL;

#define APP_BLE_LOW_SPEED_MIN_CONN_INTVEL_DURING_CALL   (uint16_t)(300/1.25)
#define APP_BLE_LOW_SPEED_MAX_CONN_INTVEL_DURING_CALL   (uint16_t)(340/1.25)

#define APP_BLE_LOW_SPEED_MIN_CONN_INTVEL_NORMAL        (uint16_t)(50/1.25)
#define APP_BLE_LOW_SPEED_MAX_CONN_INTVEL_NORMAL        (uint16_t)(80/1.25)

#define APP_BLE_HIGH_SPEED_MIN_CONN_INTVEL  (uint16_t)(20/1.25)
#define APP_BLE_HIGH_SPEED_MAX_CONN_INTVEL  (uint16_t)(40/1.25)

#define APP_BLE_UPDATE_CONN_PARAM_IDLE              0
#define APP_BLE_UPDATE_CONN_PARAM_TO_LOW_SPEED      1
#define APP_BLE_UPDATE_CONN_PARAM_TO_HIGH_SPEED     2
static uint8_t app_ble_update_conn_param_times = 0;

static uint8_t app_ble_update_conn_param_op = APP_BLE_UPDATE_CONN_PARAM_IDLE;

static void app_ble_check_conn_param_status_peridically(void);

static void app_ble_update_conn_param_timer_handler(void const *param)
{
    app_start_custom_function_in_bt_thread(0, 0, 
        (uint32_t)app_ble_check_conn_param_status_peridically);

}

void app_init_ble_update_conn_param_state_machine(void)
{
    app_ble_update_conn_param_timer = osTimerCreate(
        osTimer(APP_BLE_UPDATE_CONN_PARAM_TIMER), 
        osTimerOnce, NULL);
}

void app_ble_stop_update_conn_param_op(void)
{
    app_ble_update_conn_param_times = 0;
    app_ble_update_conn_param_op = APP_BLE_UPDATE_CONN_PARAM_IDLE;
    osTimerStop(app_ble_update_conn_param_timer);
}

static void switch_to_low_speed_conn_interval_handler_in_bt_thread(void)
{
    if (APP_BLE_UPDATE_CONN_PARAM_TO_LOW_SPEED != app_ble_update_conn_param_op)
    {
        app_ble_update_conn_param_times = 0;
        app_ble_update_conn_param_op = APP_BLE_UPDATE_CONN_PARAM_TO_LOW_SPEED;
        
        osTimerStart(app_ble_update_conn_param_timer, APP_BLE_UPDATE_CONN_PARAM_CHECK_PERIOD_IN_MS);
    }
    else
    {
        if (app_ble_update_conn_param_times >= APP_BLE_UPDATE_CONN_PARAM_TRY_TIMES)
        {
            app_ble_stop_update_conn_param_op();
        }
        else
        {
            app_ble_update_conn_param_times++;
            struct gapc_conn_param conn_param;

            if (btapp_hfp_is_sco_active())
            {
                conn_param.intv_min = APP_BLE_LOW_SPEED_MIN_CONN_INTVEL_DURING_CALL; 
                conn_param.intv_max = APP_BLE_LOW_SPEED_MAX_CONN_INTVEL_DURING_CALL; 
            }
            else
            {
                conn_param.intv_min = APP_BLE_LOW_SPEED_MIN_CONN_INTVEL_NORMAL; 
                conn_param.intv_max = APP_BLE_LOW_SPEED_MAX_CONN_INTVEL_NORMAL; 
            }
            conn_param.latency	= 0;
            conn_param.time_out = 6000/10; 

            for (uint8_t index = 0;index < BLE_CONNECTION_MAX;index++)
            {
                if (app_env.context[index].isConnected)
                {
                    l2cap_update_param(index, &conn_param);
                }
            }

            osTimerStart(app_ble_update_conn_param_timer, APP_BLE_UPDATE_CONN_PARAM_CHECK_PERIOD_IN_MS);
        }
    }
}

void app_ble_update_conn_param_op_status(uint16_t updatedConnInterval)
{
    uint8_t isBleUpdateConnParamFinished = false;
    if (APP_BLE_UPDATE_CONN_PARAM_TO_LOW_SPEED == app_ble_update_conn_param_op)
    {
        if (btapp_hfp_is_sco_active())
        {
            if (updatedConnInterval >= APP_BLE_LOW_SPEED_MIN_CONN_INTVEL_DURING_CALL)
            {
                isBleUpdateConnParamFinished = true;
            }
        }
        else    if (updatedConnInterval >= APP_BLE_LOW_SPEED_MIN_CONN_INTVEL_NORMAL)
        {
            isBleUpdateConnParamFinished = true;
        }
    }
    else if (APP_BLE_UPDATE_CONN_PARAM_TO_HIGH_SPEED == app_ble_update_conn_param_op)
    {
        if (updatedConnInterval <= APP_BLE_HIGH_SPEED_MAX_CONN_INTVEL)
        {
            isBleUpdateConnParamFinished = true;
        }
    }
    else
    {
        if (btapp_hfp_is_sco_active())
        {            
            if (updatedConnInterval < APP_BLE_LOW_SPEED_MIN_CONN_INTVEL_DURING_CALL)
            {
                switch_to_low_speed_conn_interval_handler_in_bt_thread();
            }
        }
    }

    if (isBleUpdateConnParamFinished)
    {
        app_ble_stop_update_conn_param_op();
    }
}

void switch_to_low_speed_conn_interval(void)
{
    app_start_custom_function_in_bt_thread(0, 0, 
        (uint32_t)switch_to_low_speed_conn_interval_handler_in_bt_thread);
}

void switch_to_high_speed_conn_interval_handler_in_bt_thread(void)
{
    if (APP_BLE_UPDATE_CONN_PARAM_TO_HIGH_SPEED != app_ble_update_conn_param_op)
    {
        app_ble_update_conn_param_times = 0;
        app_ble_update_conn_param_op = APP_BLE_UPDATE_CONN_PARAM_TO_HIGH_SPEED;
        
        osTimerStart(app_ble_update_conn_param_timer, APP_BLE_UPDATE_CONN_PARAM_CHECK_PERIOD_IN_MS);
    }
    else
    {
        if (app_ble_update_conn_param_times >= APP_BLE_UPDATE_CONN_PARAM_TRY_TIMES)
        {
            app_ble_stop_update_conn_param_op();
        }
        else
        {
            app_ble_update_conn_param_times++;
            struct gapc_conn_param conn_param;

            conn_param.intv_min = APP_BLE_HIGH_SPEED_MIN_CONN_INTVEL; 
            conn_param.intv_max = APP_BLE_HIGH_SPEED_MAX_CONN_INTVEL; 
            conn_param.latency  = 0;
            conn_param.time_out = 6000/10; 

            for (uint8_t index = 0;index < BLE_CONNECTION_MAX;index++)
            {
                if (app_env.context[index].isConnected)
                {
                    l2cap_update_param(index, &conn_param);
                }
            }

            osTimerStart(app_ble_update_conn_param_timer, APP_BLE_UPDATE_CONN_PARAM_CHECK_PERIOD_IN_MS);
        }
    }
}

void switch_to_high_speed_conn_interval(void)
{
    app_start_custom_function_in_bt_thread(0, 0, 
        (uint32_t)switch_to_high_speed_conn_interval_handler_in_bt_thread);
}

static void app_ble_check_conn_param_status_peridically(void)
{
    if (APP_BLE_UPDATE_CONN_PARAM_TO_LOW_SPEED == app_ble_update_conn_param_op)
    {
        switch_to_low_speed_conn_interval_handler_in_bt_thread();
    }
    else if (APP_BLE_UPDATE_CONN_PARAM_TO_HIGH_SPEED == app_ble_update_conn_param_op)
    {
        switch_to_high_speed_conn_interval_handler_in_bt_thread();
    }
}
#endif

void appm_update_param(uint8_t conidx, struct gapc_conn_param *conn_param)
{
    BLE_APP_FUNC_ENTER();
    // Prepare the GAPC_PARAM_UPDATE_CMD message
    struct gapc_param_update_cmd *cmd = KE_MSG_ALLOC(GAPC_PARAM_UPDATE_CMD,
                                                     KE_BUILD_ID(TASK_GAPC, conidx), TASK_APP,
                                                     gapc_param_update_cmd);

    cmd->operation  = GAPC_UPDATE_PARAMS;
    cmd->intv_min   = conn_param->intv_min;
    cmd->intv_max   = conn_param->intv_max;
    cmd->latency    = conn_param->latency;
    cmd->time_out   = conn_param->time_out;

    // not used by a slave device
    cmd->ce_len_min = 0xFFFF;
    cmd->ce_len_max = 0xFFFF;

    // Send the message
    ke_msg_send(cmd);
    BLE_APP_FUNC_LEAVE();
}

void l2cap_update_param(uint8_t conidx, struct gapc_conn_param *conn_param)
{
    BLE_APP_FUNC_ENTER();
    
    struct l2cc_update_param_req *req = L2CC_SIG_PDU_ALLOC(conidx, L2C_CODE_CONN_PARAM_UPD_REQ,
                                                   KE_BUILD_ID(TASK_GAPC, conidx), l2cc_update_param_req);

    // generate packet identifier
    uint8_t pkt_id = co_rand_word() & 0xFF;
    if  (pkt_id == 0)
    {
        pkt_id = 1;
    }

    LOG_PRINT_BLE("update parame interval to %d", conn_param->intv_max);

    /* fill up the parameters */
    req->intv_max = conn_param->intv_max;
    req->intv_min = conn_param->intv_min;
    req->latency  = conn_param->latency;
    req->timeout  = conn_param->time_out;
    req->pkt_id   = pkt_id;

    l2cc_pdu_send(req);
    BLE_APP_FUNC_LEAVE();
}

uint8_t appm_get_dev_name(uint8_t* name)
{
    BLE_APP_FUNC_ENTER();
    // copy name to provided pointer
    memcpy(name, app_env.dev_name, app_env.dev_name_len);
    // return name length
    return app_env.dev_name_len;
    BLE_APP_FUNC_LEAVE();
}

void appm_exchange_mtu(uint8_t conidx)
{
    BLE_APP_FUNC_ENTER();
    struct gattc_exc_mtu_cmd *cmd = KE_MSG_ALLOC(GATTC_EXC_MTU_CMD,
                                                     KE_BUILD_ID(TASK_GATTC, conidx), TASK_APP,
                                                     gattc_exc_mtu_cmd);

    cmd->operation = GATTC_MTU_EXCH;
    cmd->seq_num= 0;
    
    ke_msg_send(cmd);
    BLE_APP_FUNC_LEAVE();
}

void appm_check_and_resolve_ble_address(uint8_t conidx)
{
    APP_BLE_CONN_CONTEXT_T* pContext = &(app_env.context[conidx]);

    // solved already, return
    if (pContext->isGotSolvedBdAddr)
    {
        LOG_PRINT_BLE("Already get solved bd addr.");
        return;
    }
    // not solved yet and the solving is in progress, return and wait
    else if (app_is_resolving_ble_bd_addr()) 
    {
        LOG_PRINT_BLE("Random bd addr solving on going.");
        return;
    }

    if (BLE_RANDOM_ADDR == pContext->peerAddrType)
    {
        memset(pContext->solvedBdAddr, 0, BD_ADDR_LEN);
        bool isSuccessful = appm_resolve_random_ble_addr_from_nv(conidx, pContext->bdAddr);
        if (isSuccessful)
        {   
            pContext->isBdAddrResolvingInProgress = true;
        }
        else
        {
            pContext->isGotSolvedBdAddr = true;
            pContext->isBdAddrResolvingInProgress = false;
        }
    }
    else
    {
        pContext->isGotSolvedBdAddr = true;
        pContext->isBdAddrResolvingInProgress = false;
        memcpy(pContext->solvedBdAddr, pContext->bdAddr, BD_ADDR_LEN);
    }

}

bool appm_resolve_random_ble_addr_from_nv(uint8_t conidx, uint8_t* randomAddr)
{
#ifdef _BLE_NVDS_
    struct gapm_resolv_addr_cmd *cmd = KE_MSG_ALLOC_DYN(GAPM_RESOLV_ADDR_CMD,
                                       KE_BUILD_ID(TASK_GAPM, conidx), TASK_APP,
                                       gapm_resolv_addr_cmd, 
                                       BLE_RECORD_NUM*GAP_KEY_LEN);
    uint8_t irkeyNum = nv_record_ble_fill_irk((uint8_t *)(cmd->irk));
    if (0 == irkeyNum)
    {
        LOG_PRINT_BLE("No history irk, cannot solve bd addr.");
        KE_MSG_FREE(cmd);
        return false;
    }

    LOG_PRINT_BLE("Start random bd addr solving.");
    
    cmd->operation = GAPM_RESOLV_ADDR;
    cmd->nb_key = irkeyNum;
    memcpy(cmd->addr.addr, randomAddr, GAP_BD_ADDR_LEN);
    ke_msg_send(cmd);   
    return true;
#else
    return false;
#endif    
    
}

void appm_resolve_random_ble_addr_with_sepcific_irk(uint8_t conidx, uint8_t* randomAddr, uint8_t* pIrk)
{
    struct gapm_resolv_addr_cmd *cmd = KE_MSG_ALLOC_DYN(GAPM_RESOLV_ADDR_CMD,
                                       KE_BUILD_ID(TASK_GAPM, conidx), TASK_APP,
                                       gapm_resolv_addr_cmd, 
                                       GAP_KEY_LEN);
    cmd->operation = GAPM_RESOLV_ADDR;
    cmd->nb_key = 1;
    memcpy(cmd->addr.addr, randomAddr, GAP_BD_ADDR_LEN);
    memcpy(cmd->irk, pIrk, GAP_KEY_LEN);
    ke_msg_send(cmd);   
}

void appm_random_ble_addr_solved(bool isSolvedSuccessfully, uint8_t* irkUsedForSolving)
{
    APP_BLE_CONN_CONTEXT_T* pContext;
    uint32_t conidx;
    for (conidx = 0;conidx < BLE_CONNECTION_MAX;conidx++)
    {
        pContext = &(app_env.context[conidx]);
        if (pContext->isBdAddrResolvingInProgress)
        {
            break;
        }
    }

    if (conidx < BLE_CONNECTION_MAX)
    {   
        pContext->isBdAddrResolvingInProgress = false;
        pContext->isGotSolvedBdAddr = true;
        
        LOG_PRINT_BLE("conidx %d isSolvedSuccessfully %d", conidx, isSolvedSuccessfully);
        if (isSolvedSuccessfully)
        {
#ifdef _BLE_NVDS_
            bool isSuccessful = nv_record_blerec_get_bd_addr_from_irk(app_env.context[conidx].solvedBdAddr, irkUsedForSolving);
            if (isSuccessful)
            {
                LOG_PRINT_BLE("Connected random address's original addr is:");
                LOG_PRINT_BLE_DUMP8("%02x ", app_env.context[conidx].solvedBdAddr, GAP_BD_ADDR_LEN);
            }
            else
#endif                
            {
                LOG_PRINT_BLE("Resolving of the connected BLE random addr failed.");
            }
        }
        else
        {
            LOG_PRINT_BLE("BLE random resolving failed.");
        }
    }

#if defined(CFG_VOICEPATH)
    ke_task_msg_retrieve(prf_get_task_from_id(TASK_ID_VOICEPATH));
#endif
    ke_task_msg_retrieve(TASK_GAPC);
    ke_task_msg_retrieve(TASK_APP);
    app_start_ble_tws_advertising(GAPM_ADV_UNDIRECT, BLE_TWS_ADV_SLOW_INTERVAL_IN_MS);
}

uint8_t app_ble_connection_count(void)
{
    return app_env.conn_cnt;
}

bool app_is_arrive_at_max_ble_connections(void)
{
    LOG_PRINT_BLE("ble connection count %d", app_env.conn_cnt);
    return (app_env.conn_cnt >= BLE_CONNECTION_MAX);
}

bool app_is_resolving_ble_bd_addr(void)
{
    for (uint32_t index = 0;index < BLE_CONNECTION_MAX;index++)
    {
        if (app_env.context[index].isBdAddrResolvingInProgress)
        {
            return true;
        }
    }

    return false;
}

__attribute__((weak)) void app_connected_evt_handler(const uint8_t* pPeerBdAddress)
{

}

__attribute__((weak)) void app_disconnected_evt_handler(void)
{

}

__attribute__((weak)) void app_advertising_stopped(uint8_t state)
{

}

__attribute__((weak)) void app_advertising_started(void)
{

}

__attribute__((weak)) void app_connecting_started(void)
{

}

__attribute__((weak)) void app_scanning_stopped(void)
{

}

__attribute__((weak)) void app_connecting_stopped(void)
{

}


__attribute__((weak)) void app_scanning_started(void)
{

}

__attribute__((weak)) void app_ble_system_ready(void)
{

}

__attribute__((weak)) void app_adv_reported_scanned(struct gapm_adv_report_ind* ptInd)
{

}

uint8_t* appm_get_current_ble_addr(void)
{
#if BLE_IS_USE_RPA
    return (uint8_t *)gapm_get_bdaddr();
#else
    return ble_addr;
#endif
}

#endif //(BLE_APP_PRESENT)

/// @} APP
