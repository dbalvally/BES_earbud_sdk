/**
 ****************************************************************************************
 *
 * @file ble_tws.c
 *
 * @date 24 May 2017
 *
 * @brief The application for the TWS handling
 *
 * Copyright (C) 2017
 *
 *
 ****************************************************************************************
 */
#if __BLE_TWS_ENABLED__

#include "string.h"
#include "cmsis_os.h"
#include "factory_section.h"
#include "hal_trace.h"
#include "hal_iomux.h"
#include "stdbool.h"
#include "app_thread.h"
#include "app_utils.h"
#include "apps.h"
#include "app.h"
#include "ble_tws.h"
#include "nvrecord.h"
#include "app_bt.h"
#include "app_bt_conn_mgr.h"
#include "app_tws.h"
#include "app_tws_role_switch.h"
#include "app_ble_cmd_handler.h"
#include "app_ble_custom_cmd.h"
#include "app_datapath_server.h"
#include "app_tws_if.h"
#include "hal_cmu.h"
#include "hal_bootmode.h"
#include "app_battery.h"
#include "hal_timer.h"
#include "hal_chipid.h"
#include "log_section.h"
#include "bt_drv_reg_op.h"
#include "nvrecord_ble.h"

extern bool no_mobile_flag;

bt_status_t btif_me_set_bt_address(uint8_t * btAddr);
bt_status_t btif_me_set_ble_bd_address(uint8_t* btAddr);
bt_status_t btif_me_ble_add_dev_to_whitelist(U8 addr_type, bt_bdaddr_t * addr);

#if BLE_TWS_USE_BLE_AS_INBOX_COMMUNICATION_PATH
/**< Periodic timer for switching between BLE advertising and scanning */
static void ble_adv_scan_switch_timer_cb(void const *n);
osTimerDef (BLE_ADV_SCAN_SWITCH_TIMER, ble_adv_scan_switch_timer_cb);
static osTimerId ble_adv_scan_switch_timer;

/**< lasting time of stopping the ble activity when tws is pairing */
static void ble_activity_idle_timer_cb(void const *n);
osTimerDef (BLE_ACTIVITY_IDLE_TIMER, ble_activity_idle_timer_cb);
static osTimerId ble_activity_idle_timer;

/**< if the ble interactive activity lasting time expires but still don't received the stop
    ble interactive activity cmd from the charger box, the device will stop the ble interactive activity */
static void ble_interactive_activity_supervisor_timer_cb(void const *n);
osTimerDef (BLE_INTERACTIVE_ACTIVITY_SUPERVISOR_TIMER, ble_interactive_activity_supervisor_timer_cb);
static osTimerId ble_interactive_activity_supervisor_timer;

#endif

static void check_before_repairing_timer_cb(void const *n);
osTimerDef (CHECK_BEFORE_REPAIRING_TIMER, check_before_repairing_timer_cb);
static osTimerId check_before_reparing_timer;

static void check_before_simple_repairing_timer_cb(void const *n);
osTimerDef (CHECK_BEFORE_SIMPLE_REPAIRING_TIMER, check_before_simple_repairing_timer_cb);
static osTimerId check_before_simple_reparing_timer;

/**< once timer for switching from high speed reconnection ble adv to low speed */
static void ble_adv_speed_switch_timer_cb(void const *n);
osTimerDef (BLE_ADV_SPEED_SWITCH_TIMER, ble_adv_speed_switch_timer_cb);
static osTimerId ble_adv_speed_switch_timer;

static void app_start_reconnecting_tws_slave_operation(void);
static void app_start_reconnecting_tws_master_operation(void);
static void app_command_sent_handler(void);
static void app_tws_parse_entering_ota_request(TWS_CHARGERBOX_ENTER_OTA_REQ_T* pReq);

static void app_fill_tws_adv_manu_data_common_handler(void);
static void app_fill_tws_adv_manu_data_in_charger_box(void);
static void app_tws_start_reset_pairing(void);
void app_bt_start_reset_pairing(void);


static void ble_tws_stop_all_activities_handler(void);
static void app_start_ble_tws_advertising_handler(uint8_t advType, uint16_t adv_interval_in_ms);
static void app_start_ble_tws_scanning_handler(uint16_t scanning_window_in_ms, uint16_t scanning_interval_in_ms);
static void app_start_ble_tws_connecting_handler(uint8_t* bleBdAddr);

static void app_restart_ble_tws_advertising(uint8_t advType, uint16_t adv_interval_in_ms);
static void app_tws_simple_repairing_request_handler(void);
static void app_tws_do_preparation_before_simple_reparing_inbox(void);

// the timer to assure that during the TWS BT reconnecting, there is no
// ble activity
static void app_tws_bt_reconnecting_timeout_timer_cb(void const *n);
osTimerDef (APP_TWS_BT_RECONNECTING_TIMEOUT_TIMER, app_tws_bt_reconnecting_timeout_timer_cb);
static osTimerId app_tws_bt_reconnecting_timeout_timer_id;
static uint8_t  isTwsBtReconnectingInProgress = false;

static void app_tws_ble_handshake_timeout_timer_cb(void const *n);
osTimerDef (APP_TWS_BLE_HANDSHAKE_TIMEOUT_TIMER, app_tws_ble_handshake_timeout_timer_cb);
static osTimerId app_tws_ble_handshake_timeout_timer_id;

TWS_BOX_ENV_T twsBoxEnv;

static bool isBleTwsInitialized = false;
static bool isBtInitialized = false;
static bool isTwsParingInProgress = false;

extern uint8_t bt_addr[BTIF_BD_ADDR_SIZE];

static BLE_TWS_CONFIG_T tBleTwsConfig = {TWS_LEFT_SIDE, TWS_RIGHT_SIDE, 0, TWS_BOX_RSSI_THRESHOLD, (void *)NULL};

#define PROTECTION_TO_AVOID_NON_ADV_ISSUE   0

#if PROTECTION_TO_AVOID_NON_ADV_ISSUE
#define BLE_TWS_INTERVAL_TO_FORCE_RESET_ACTIVITIES_IN_MS    5000
static uint32_t bleTwsLastTimeStampInMs = 0;
#endif

static TWS_BLE_OP_E pendingBLEOperation = TWS_BLE_OP_IDLE;

#if 1
#define SET_BLE_TWS_STATE(newState) do { \
                                    LOG_PRINT_BLE("ble state from %d to %d at line %d", twsBoxEnv.state, newState, __LINE__); \
                                    twsBoxEnv.state = (newState); \
                                } while (0);
#else
#define SET_BLE_TWS_STATE(newState) do { \
                                    twsBoxEnv.state = (newState); \
                                } while (0);
#endif

static void app_tws_disconnect_untrustable_ble_connection(void);
static void app_tws_generate_ble_initial_handshake_str(TWS_BLE_INITIAL_HANDSHAKE_T* ptHandshake);

void ble_tws_pending_operation_handler(void)
{
    TWS_BLE_OP_E op = pendingBLEOperation;
    pendingBLEOperation = TWS_BLE_OP_IDLE;
    switch (op)
    {
        case TWS_BLE_OP_START_ADV:
            app_start_ble_tws_advertising_handler(twsBoxEnv.advType, twsBoxEnv.advIntervalInMs);
            break;
        case TWS_BLE_OP_START_SCANNING:
            app_start_ble_tws_scanning_handler(twsBoxEnv.scanWindowInMs, twsBoxEnv.scanIntervalInMs);
            break;
        case TWS_BLE_OP_START_CONNECTING:
            app_start_ble_tws_connecting_handler(twsBoxEnv.twsScannedBleAddr);
            break;
        case TWS_BLE_OP_STOP_ADV:
        case TWS_BLE_OP_STOP_SCANNING:
        case TWS_BLE_OP_STOP_CONNECTING:
            ble_tws_stop_all_activities_handler();
            break;
        default:
            break;
    }
}

static void app_ble_tws_switch_activities(uint8_t stop)
{
    BLE_APP_DBG("TWS state: %d, stop: %d", twsBoxEnv.state, stop);
    switch (twsBoxEnv.state)
    {
        case TWS_BLE_STATE_STARTING_ADV:
            SET_BLE_TWS_STATE(TWS_BLE_STATE_ADVERTISING);
            break;
        case TWS_BLE_STATE_STARTING_SCANNING:
            SET_BLE_TWS_STATE(TWS_BLE_STATE_SCANNING);
            break;
        case TWS_BLE_STATE_STARTING_CONNECTING:
            SET_BLE_TWS_STATE(TWS_BLE_STATE_CONNECTING);
            break;
        case TWS_BLE_STATE_STOPPING_ADV:
        case TWS_BLE_STATE_STOPPING_SCANNING:
        case TWS_BLE_STATE_STOPPING_CONNECTING:
            SET_BLE_TWS_STATE(TWS_BLE_STATE_IDLE);
            break;
        case TWS_BLE_STATE_ADVERTISING:
            if (stop) {
                SET_BLE_TWS_STATE(TWS_BLE_STATE_IDLE);
                LOG_PRINT_BLE("Connected!!!");
            }
            break;
            
        default:
            break;
    }
    
    ble_tws_pending_operation_handler();
}

void app_tws_restart_operation(void)
{
    if (isTwsBtReconnectingInProgress)
    {
        return;
    }

    switch (twsBoxEnv.state)
    {
        case TWS_BLE_STATE_ADVERTISING:
            LOG_PRINT_BLE("restart adv");
            app_start_ble_tws_advertising_handler(twsBoxEnv.advType, twsBoxEnv.advIntervalInMs);
            break;
        case TWS_BLE_STATE_SCANNING:
            LOG_PRINT_BLE("restart scan");
            app_start_ble_tws_scanning_handler(twsBoxEnv.scanWindowInMs, twsBoxEnv.scanIntervalInMs);
            break;
        default:
            break;
    }
}

static void app_ble_advertising_starting_failed_handler(void)
{
    if (TWS_BLE_STATE_STARTING_ADV == twsBoxEnv.state)
    {
        twsBoxEnv.state = TWS_BLE_STATE_IDLE;
    }
}

/**
 * @brief Call back function when the advertising is stopped
 *
 */
void app_advertising_stopped(uint8_t state)
{
    //LOG_PRINT_BLE("ble adv is stopped. twsBoxEnv.state is %d", twsBoxEnv.state);
    app_start_custom_function_in_bt_thread(state,
            0, (uint32_t)app_ble_tws_switch_activities);
}

void app_advertising_starting_failed(void)
{
    app_start_custom_function_in_bt_thread(0,
            0, (uint32_t)app_ble_advertising_starting_failed_handler);
}

/**
 * @brief Call back function when the scanning is stopped
 *
 */
 //stop scan and then start adv
void app_scanning_stopped(void)
{
    //LOG_PRINT_BLE("Scanning is stopped. twsBoxEnv.state is %d", twsBoxEnv.state);
    app_start_custom_function_in_bt_thread(1,
        0, (uint32_t)app_ble_tws_switch_activities);
}

void app_connecting_started(void)
{
    app_start_custom_function_in_bt_thread(0,
            0, (uint32_t)app_ble_tws_switch_activities);
}

void app_connecting_stopped(void)
{
    app_start_custom_function_in_bt_thread(1,
            0, (uint32_t)app_ble_tws_switch_activities);
}

void app_advertising_started(void)
{
    app_start_custom_function_in_bt_thread(0,
            0, (uint32_t)app_ble_tws_switch_activities);
}

void app_scanning_started(void)
{
    app_start_custom_function_in_bt_thread(0,
            0, (uint32_t)app_ble_tws_switch_activities);
}

static void app_tws_handling_when_tws_reconnecting_timeout(void)
{
#ifdef __ENABLE_IN_BOX_STATE_HANDLE__
    if (app_is_in_charger_box())
    {
        if (!app_is_charger_box_closed())
        {
        #if BLE_TWS_USE_BLE_AS_INBOX_COMMUNICATION_PATH
            app_tws_start_normal_ble_activities_in_chargerbox();
        #endif
            conn_system_boot_up_handler();
        }
    }
    else
#endif
    {
        conn_system_boot_up_handler();
    }
}

static void app_tws_bt_reconnecting_timeout_timer_cb(void const *n)
{
    isTwsBtReconnectingInProgress = false;
    if (!app_tws_is_role_switching_on())
    {
        // check the charger box open state
        app_start_custom_function_in_bt_thread(0,
                        0, (uint32_t)app_tws_handling_when_tws_reconnecting_timeout);
    }
}

static void app_configure_pending_ble_tws_scanning(uint16_t scanning_window_in_ms, uint16_t scanning_interval_in_ms)
{
    twsBoxEnv.scanWindowInMs = scanning_window_in_ms;
    twsBoxEnv.scanIntervalInMs = scanning_interval_in_ms;
    pendingBLEOperation = TWS_BLE_OP_START_SCANNING;
}

static void app_start_ble_tws_scanning_handler(uint16_t scanning_window_in_ms, uint16_t scanning_interval_in_ms)
{
    LOG_PRINT_BLE("Start BLE scan with scan_win:%d ms scan_intv:%d state is %d", scanning_window_in_ms, scanning_interval_in_ms, twsBoxEnv.state);

    switch (twsBoxEnv.state)
    {
        case TWS_BLE_STATE_ADVERTISING:
            app_configure_pending_ble_tws_scanning(scanning_window_in_ms, scanning_interval_in_ms);
            SET_BLE_TWS_STATE(TWS_BLE_STATE_STOPPING_ADV);
            appm_stop_advertising();
            break;
        case TWS_BLE_STATE_SCANNING:
            app_configure_pending_ble_tws_scanning(scanning_window_in_ms, scanning_interval_in_ms);
            SET_BLE_TWS_STATE(TWS_BLE_STATE_STOPPING_SCANNING);
            appm_stop_scanning();
            break;
        case TWS_BLE_STATE_CONNECTING:
            app_configure_pending_ble_tws_scanning(scanning_window_in_ms, scanning_interval_in_ms);
            SET_BLE_TWS_STATE(TWS_BLE_STATE_STOPPING_CONNECTING);
            appm_stop_connecting();
            break;
        case TWS_BLE_STATE_STARTING_ADV:
        case TWS_BLE_STATE_STARTING_SCANNING:
        case TWS_BLE_STATE_STARTING_CONNECTING:
        case TWS_BLE_STATE_STOPPING_ADV:
        case TWS_BLE_STATE_STOPPING_SCANNING:
        case TWS_BLE_STATE_STOPPING_CONNECTING:
            app_configure_pending_ble_tws_scanning(scanning_window_in_ms, scanning_interval_in_ms);
            break;
        case TWS_BLE_STATE_IDLE:
            twsBoxEnv.scanWindowInMs = scanning_window_in_ms;
            twsBoxEnv.scanIntervalInMs = scanning_interval_in_ms;
            SET_BLE_TWS_STATE(TWS_BLE_STATE_STARTING_SCANNING);
#if defined(BLE_TWS_USE_BLE_AS_INBOX_COMMUNICATION_PATH) && defined(__ENABLE_IN_BOX_STATE_HANDLE__)
            if (!app_is_in_charger_box())
            {
                //LOG_PRINT_BLE("Start scanning ble device in whitelist");
                appm_start_scanning(twsBoxEnv.scanIntervalInMs, twsBoxEnv.scanWindowInMs, SCAN_ALLOW_ADV_WLST);
            }
            else
            {
                //LOG_PRINT_BLE("Start scanning all ble devices");
                appm_start_scanning(twsBoxEnv.scanIntervalInMs, twsBoxEnv.scanWindowInMs, SCAN_ALLOW_ADV_ALL);
            }
#else
            appm_start_scanning(twsBoxEnv.scanIntervalInMs, twsBoxEnv.scanWindowInMs, SCAN_ALLOW_ADV_WLST);
#endif  // #if BLE_TWS_USE_BLE_AS_INBOX_COMMUNICATION_PATH
            break;
        default:
            break;
    }
}

void app_start_ble_tws_scanning(uint16_t scanning_window_in_ms, uint16_t scanning_interval_in_ms)
{
    if (!isTwsBtReconnectingInProgress)
    {
        app_start_ble_tws_scanning_handler(scanning_window_in_ms, scanning_interval_in_ms);
    }
}

static void app_configure_pending_ble_tws_advertising(uint8_t advType, uint16_t adv_interval_in_ms)
{
    twsBoxEnv.advIntervalInMs = adv_interval_in_ms;
    twsBoxEnv.advType = advType;
    pendingBLEOperation = TWS_BLE_OP_START_ADV;
}

static void app_restart_ble_tws_advertising(uint8_t advType, uint16_t adv_interval_in_ms)
{
    app_configure_pending_ble_tws_advertising(advType, adv_interval_in_ms);
    SET_BLE_TWS_STATE(TWS_BLE_STATE_STOPPING_ADV);
    appm_stop_advertising();
}

extern btif_remote_device_t* mobileBtRemDev;

int app_scotxsilence_force_disable(void)
{
    uint16_t connHandle;

    if (!mobileBtRemDev){
        return -1;
    }
    connHandle = btif_me_get_remote_device_hci_handle(mobileBtRemDev);
    LOG_PRINT_BLE("%s hdl:%08x", __func__,connHandle);
    log_event_1(EVENT_MIC_SWITCHED,true);
    btif_me_set_sco_tx_silence(connHandle, false);
    return 0;
}

static void app_start_ble_tws_advertising_handler(uint8_t advType, uint16_t adv_interval_in_ms)
{
    if (!app_is_ble_adv_allowed())
    {
        return;
    }

    LOG_PRINT_BLE("Start BLE adv with interval %d ms advType:%d state is %d", adv_interval_in_ms, advType, twsBoxEnv.state);
    switch (twsBoxEnv.state)
    {
        case TWS_BLE_STATE_ADVERTISING:
            app_restart_ble_tws_advertising(advType, adv_interval_in_ms);
            break;
        case TWS_BLE_STATE_SCANNING:
            app_configure_pending_ble_tws_advertising(advType, adv_interval_in_ms);
            SET_BLE_TWS_STATE(TWS_BLE_STATE_STOPPING_SCANNING);
            appm_stop_scanning();
            break;
        case TWS_BLE_STATE_CONNECTING:
            app_configure_pending_ble_tws_advertising(advType, adv_interval_in_ms);
            SET_BLE_TWS_STATE(TWS_BLE_STATE_STOPPING_CONNECTING);
            appm_stop_connecting();
            break;
        case TWS_BLE_STATE_STARTING_ADV:
        case TWS_BLE_STATE_STARTING_SCANNING:
        case TWS_BLE_STATE_STARTING_CONNECTING:
        case TWS_BLE_STATE_STOPPING_ADV:
        case TWS_BLE_STATE_STOPPING_SCANNING:
        case TWS_BLE_STATE_STOPPING_CONNECTING:
            app_configure_pending_ble_tws_advertising(advType, adv_interval_in_ms);
            break;
        case TWS_BLE_STATE_IDLE:
        {
            twsBoxEnv.advType = advType;
            twsBoxEnv.advIntervalInMs = adv_interval_in_ms;
            twsBoxEnv.adv_manufacture_data_section.pairCode = twsBoxEnv.pairCode;

#if FPGA==0
            twsBoxEnv.adv_manufacture_data_section.batteryLevel = app_battery_current_level();
#endif

            twsBoxEnv.adv_manufacture_data_section.role = app_tws_get_mode();
            twsBoxEnv.adv_manufacture_data_section.isConnectedWithMobile = IS_CONNECTED_WITH_MOBILE();
            twsBoxEnv.adv_manufacture_data_section.isConnectedWithTws = IS_CONNECTED_WITH_TWS();

            SET_BLE_TWS_STATE(TWS_BLE_STATE_STARTING_ADV);

            uint8_t adv_channel_map = 7;
            #if IS_USE_SINGLE_CHANNEL_ADV   
            //if (TWS_INTERACTIVE_ADV == twsBoxEnv.advMode)
            {
                adv_channel_map = 1 << twsBoxEnv.currentAdvChannel;
                twsBoxEnv.currentAdvChannel++;
                if (3 == twsBoxEnv.currentAdvChannel)
                {
                    twsBoxEnv.currentAdvChannel = 0;
                }
            }
            #endif
            app_scotxsilence_force_disable();
            appm_start_advertising(twsBoxEnv.advType, adv_channel_map, twsBoxEnv.advIntervalInMs, twsBoxEnv.advIntervalInMs, \
                (uint8_t *)&twsBoxEnv.adv_manufacture_data_section,
                twsBoxEnv.adv_manufacture_data_section.dataLengthFollowing ? (twsBoxEnv.adv_manufacture_data_section.dataLengthFollowing+1) : 0);
            break;
        }
        default:
            break;
    }
}

void app_start_ble_tws_advertising(uint8_t advType, uint16_t adv_interval_in_ms)
{
    app_start_ble_tws_advertising_handler(advType, adv_interval_in_ms);
}

void app_start_ble_tws_advertising_in_bt_thread(uint8_t advType, uint16_t adv_interval_in_ms)
{
    app_start_custom_function_in_bt_thread(advType, adv_interval_in_ms, (uint32_t)app_start_ble_tws_advertising);
}

void app_start_connectable_ble_adv(uint16_t adv_interval_in_ms)
{
	app_start_custom_function_in_bt_thread(GAPM_ADV_UNDIRECT, adv_interval_in_ms, 
		(uint32_t)app_start_ble_tws_advertising);
}

static void ble_adv_speed_switch_timer_cb(void const *n)
{
    app_start_custom_function_in_bt_thread(GAPM_ADV_UNDIRECT,
                BLE_LOW_SPEED_RECONNECT_ADV_INTERVAL_IN_MS,
                (uint32_t)app_start_ble_tws_advertising);
}

void app_start_ble_tws_reconnect_adv(void)
{
    LOG_PRINT_BLE("TWS master start reconn ble adv.");

    log_event_1(EVENT_START_RECONNECT_BLE_ADV,appm_get_current_ble_addr());
    app_fill_tws_adv_manu_data_common_handler();

    app_start_custom_function_in_bt_thread(GAPM_ADV_UNDIRECT,
                    BLE_HIGH_SPEED_RECONNECT_ADV_INTERVAL_IN_MS, (uint32_t)app_start_ble_tws_advertising);

    osTimerStart(ble_adv_speed_switch_timer, BLE_HIGH_SPEED_RECONNECT_ADV_ENCURANCE_TIME_IN_MS);
}

#if BLE_TWS_USE_BLE_AS_INBOX_COMMUNICATION_PATH
void app_tws_start_pairing_in_chargerbox(void)
{
    app_tws_ui_set_reboot_to_pairing(true);
    twsBoxEnv.adv_manufacture_data_section.isConnectedWithMobile = 0;
    twsBoxEnv.adv_manufacture_data_section.isConnectedWithTws = 0;
    app_tws_start_interactive_ble_activities_in_chargerbox();
}
#endif

void app_start_ble_tws_out_of_box_adv(void)
{
    LOG_PRINT_BLE("TWS is out of box, start normal BLE adv.");
#if BLE_TWS_USE_BLE_AS_INBOX_COMMUNICATION_PATH
    osTimerStop(ble_adv_scan_switch_timer);
#endif

    osTimerStop(ble_adv_speed_switch_timer);

    app_fill_tws_adv_manu_data_common_handler();

    app_start_custom_function_in_bt_thread(GAPM_ADV_NON_CONN,
                    BLE_TWS_OUT_OF_BOX_ADV_INTERVAL_IN_MS, (uint32_t)app_start_ble_tws_advertising);
}

void app_start_ble_tws_reconnect_scanning(void)
{
    LOG_PRINT_BLE("Start ble scanning to supervise whether the peer device is connected with mobile, state is %d.",
        twsBoxEnv.state);

    LOG_PRINT_BLE("Peer BLE ADDR is:");
    LOG_PRINT_BLE_DUMP8("0x%02x ", tws_mode_ptr()->peerBleAddr.address, BTIF_BD_ADDR_SIZE);
    LOG_PRINT_BLE("Own BLE ADDR is:");
    LOG_PRINT_BLE_DUMP8("0x%02x ", appm_get_current_ble_addr(), BTIF_BD_ADDR_SIZE);

    app_start_custom_function_in_bt_thread(BLE_TWS_RECONNECT_HIGH_DUTYCYCLE_SCANNING_WINDOW_IN_MS,
        BLE_TWS_RECONNECT_HIGH_DUTYCYCLE_SCANNING_INTERVAL_IN_MS, (uint32_t)app_start_ble_tws_scanning);
}

static uint8_t isToRestoreBle = false;
static uint8_t tws_conidx = GAP_INVALID_CONIDX;

void app_tws_disconnected_evt_handler(uint8_t conidx)
{

    if (conidx == tws_conidx)
    {
        tws_conidx = GAP_INVALID_CONIDX;
        SET_BLE_TWS_STATE(TWS_BLE_STATE_IDLE);

        if (isToRestoreBle)
        {
            isToRestoreBle = false;
            if (app_tws_is_freeman_mode())
            {
#ifdef __ENABLE_IN_BOX_STATE_HANDLE__
                if (app_is_in_charger_box())
                {
                    app_tws_start_normal_ble_activities_in_chargerbox();
                }
#endif
            }
            else
            {
                if (!IS_CONNECTED_WITH_TWS())
                {
                    conn_ble_reconnection_machanism_handler(false);
                }
//support disable in box state handle
#ifdef __ENABLE_IN_BOX_STATE_HANDLE__
                else
                {
                    if (app_is_in_charger_box())
                    {
                        app_tws_start_normal_ble_activities_in_chargerbox();
                    }
                }
#endif
            }
        }

#if defined(BLE_TWS_USE_BLE_AS_INBOX_COMMUNICATION_PATH) && defined(__ENABLE_IN_BOX_STATE_HANDLE__)
        if (app_tws_ui_is_in_repairing())
        {
            app_tws_start_pairing_in_chargerbox();
            return;
        }
#endif

        LOG_PRINT_BLE("TWS BLE disconnection event is received! %d/%d", IS_CONNECTING_MASTER(), IS_CONNECTING_SLAVE());
        conn_start_tws_ble_connection_creation_supvervisor_timer(APP_TWS_CONNECTING_BLE_TIME_OUT_IN_MS);
        if (IS_CONNECTING_SLAVE()){
            conn_start_connecting_slave(CONNECTING_SLAVE_TIMEOUT_RECONNECT_IN_MS, true);
        }
    }
    else {
        app_start_ble_tws_advertising(GAPM_ADV_UNDIRECT, BLE_TWS_ADV_SLOW_INTERVAL_IN_MS);
    }
}

static void app_tws_ble_handshake_timeout_timer_cb(void const *n)
{
    app_start_custom_function_in_bt_thread(0, 0,
                        (uint32_t)app_tws_disconnect_untrustable_ble_connection);
}

static void app_tws_start_ble_handshake_supervising(void)
{
    osTimerStart(app_tws_ble_handshake_timeout_timer_id, TWS_BLE_HANDSHAKE_TIMEOUT_IN_MS);
}

void app_tws_stop_ble_handshake_supervising(void)
{
    osTimerStop(app_tws_ble_handshake_timeout_timer_id);
}

bool app_tws_is_connecting_ble(void)
{
    return (TWS_BLE_STATE_CONNECTING == twsBoxEnv.state);
}

static void app_connected_evt_handler_in_app_thread(void)
{   
    // for two cases the BLE connection won't be created
    // 1. The TWS pairing is in progress
    // 2. FreeMan
    // 3. TWS but the connected BLE addr doesn't match the peer device's
    LOG_PRINT_BLE("%s mode:%d state:%d", __func__, tws_mode_ptr()->mode, twsBoxEnv.state);
    LOG_PRINT_BLE("isTwsParingInProgress %d", isTwsParingInProgress);
    LOG_PRINT_BLE_DUMP8("%02x ", tws_mode_ptr()->peerBleAddr.address, BTIF_BD_ADDR_SIZE);

    // connection is created, come to the connected state from connecting state
    TWS_BLE_STATE_E formerState = twsBoxEnv.state;
    SET_BLE_TWS_STATE(TWS_BLE_STATE_CONNECTED);

    if (app_tws_is_master_mode() ||
        app_tws_is_freeman_mode()){
        bt_drv_reg_op_ble_llm_substate_hacker();
    }

    if (app_tws_ui_is_in_repairing())
    {
        conn_stop_tws_ble_connection_creation_supvervisor_timer();
        
        appm_disconnect(tws_conidx);
        LOG_PRINT_BLE("tws in repairing");
        return;
    }

    if (isTwsParingInProgress ||
        (app_tws_is_freeman_mode()))
    {
        LOG_PRINT_BLE("address error or paring in process");
        goto restoreBle;
    }

    if ((app_tws_is_slave_mode()) &&
        (TWS_BLE_STATE_CONNECTING != formerState)){
        LOG_PRINT_BLE("tws slave is not connecting");
        goto restoreBle;
    }

    if (TWS_BLE_STATE_CONNECTING == formerState)
    {
        LOG_PRINT_BLE("BLE connection for TWS reconnecting is created.");
        
        // device in ble scanning sends the handshake to the peer device
        conn_stop_tws_connection_creation_supvervisor_timer();

        LOG_PRINT_BLE("Send initial handshake to peer device.");
        TWS_BLE_INITIAL_HANDSHAKE_T expectedHandshake;
        app_tws_generate_ble_initial_handshake_str(&expectedHandshake);
        BLE_send_custom_command(OP_INITIAL_HANDSHAKE, (uint8_t *)&expectedHandshake, sizeof(expectedHandshake), false);
    }
    else
    {
        // device in ble adv waits for the correct handshake from the peer device
        app_tws_start_ble_handshake_supervising();
    }

    return;

restoreBle:
    isToRestoreBle = true;
    // the connected BD address is not the expected, disconnect and resume ble adv
    conn_stop_tws_ble_connection_creation_supvervisor_timer();
    appm_disconnect(tws_conidx);
}

void app_tws_connected_evt_handler(uint8_t conidx)
{
    if (GAP_INVALID_CONIDX == tws_conidx)
    {
        tws_conidx = conidx;
        app_datapath_server_connected_evt_handler(conidx);
        app_start_custom_function_in_bt_thread(0, 
                0, (uint32_t)app_connected_evt_handler_in_app_thread);
    }
}

static void app_start_ble_tws_connecting_handler(uint8_t* bleBdAddr)
{
    if ((TWS_BLE_STATE_CONNECTING != twsBoxEnv.state) && (TWS_BLE_STATE_STARTING_CONNECTING != twsBoxEnv.state))
    {
        memcpy(twsBoxEnv.twsScannedBleAddr, bleBdAddr, BTIF_BD_ADDR_SIZE);

        switch (twsBoxEnv.state)
        {
            case TWS_BLE_STATE_ADVERTISING:
                SET_BLE_TWS_STATE(TWS_BLE_STATE_STOPPING_ADV);
                pendingBLEOperation = TWS_BLE_OP_START_CONNECTING;
                appm_stop_advertising();
                break;
            case TWS_BLE_STATE_SCANNING:
                SET_BLE_TWS_STATE(TWS_BLE_STATE_STOPPING_SCANNING);
                pendingBLEOperation = TWS_BLE_OP_START_CONNECTING;
                appm_stop_scanning();
                break;
            case TWS_BLE_STATE_IDLE:
            {
                // start connecting
                SET_BLE_TWS_STATE(TWS_BLE_STATE_STARTING_CONNECTING);
                struct gap_bdaddr bdAddr;
                memcpy(bdAddr.addr.addr, bleBdAddr, BTIF_BD_ADDR_SIZE);
#if BLE_IS_USE_RPA
                bdAddr.addr_type = 1;
#else
                bdAddr.addr_type = 0;
#endif
                LOG_PRINT_BLE("Master paired with mobile dev is scanned, connect it via BLE.");
                appm_start_connecting(&bdAddr);
                break;
            }
            default:
                pendingBLEOperation = TWS_BLE_OP_START_CONNECTING;
                break;
        }

        conn_start_tws_ble_connection_creation_supvervisor_timer(APP_TWS_CONNECTING_BLE_TIME_OUT_IN_MS);
    }
}


static uint8_t bdAddrToConnect[BTIF_BD_ADDR_SIZE];
void app_start_ble_tws_connecting(void)
{
    app_start_ble_tws_connecting_handler(bdAddrToConnect);
}

static void app_fill_tws_adv_manu_data_role(void)
{
    switch (app_tws_get_mode())
    {
        case TWSMASTER:
            twsBoxEnv.adv_manufacture_data_section.role = TWS_MASTER;
            break;
        case TWSSLAVE:
            twsBoxEnv.adv_manufacture_data_section.role = TWS_SLAVE;
            break;
        default:
            twsBoxEnv.adv_manufacture_data_section.role = TWS_FREEMAN;
            break;
    }

    twsBoxEnv.adv_manufacture_data_section.earSide = tBleTwsConfig.earSide;
}

void app_fill_tws_adv_manu_data_common_handler(void)
{
    app_fill_tws_adv_manu_data_role();

    TWS_EARPHONE_BLE_ADV_INFO_T* pEarPhoneInfo = (TWS_EARPHONE_BLE_ADV_INFO_T *)(twsBoxEnv.adv_manufacture_data_section.data);

//support disable in box state handle
#ifdef __ENABLE_IN_BOX_STATE_HANDLE__
#if BLE_TWS_USE_BLE_AS_INBOX_COMMUNICATION_PATH
        pEarPhoneInfo->currentState = (app_is_in_charger_box() << TWS_EARPHONE_BOX_STATE_BIT_OFFSET) |
            (isTwsParingInProgress << TWS_EARPHONE_PAIRING_STATE_BIT_OFFSET) |
            (IS_CONNECTED_WITH_MOBILE() << TWS_EARPHONE_MOBILE_CONNECTION_STATE_BIT_OFFSET);
#else
        pEarPhoneInfo->currentState = (app_is_in_charger_box() << TWS_EARPHONE_BOX_STATE_BIT_OFFSET);
#endif
#endif

    pEarPhoneInfo->reqCounter = twsBoxEnv.operationRequestCounter;

    if (app_tws_is_master_mode())
    {
        memcpy(pEarPhoneInfo->btAddr, tws_mode_ptr()->masterAddr.address, BD_ADDR_LEN);
    }
    else if (app_tws_is_slave_mode())
    {
        memcpy(pEarPhoneInfo->btAddr, tws_mode_ptr()->slaveAddr.address, BD_ADDR_LEN);
    }
    else
    {
        memcpy(pEarPhoneInfo->btAddr, bt_addr, BD_ADDR_LEN);
    }
        twsBoxEnv.adv_manufacture_data_section.pairCode = twsBoxEnv.pairCode;
    twsBoxEnv.adv_manufacture_data_section.dataLengthFollowing =
        (uint32_t)&(((TWS_BLE_ADV_MANU_DATA_SECTION_T *)0)->data) + sizeof(TWS_EARPHONE_BLE_ADV_INFO_T) - 1;
}

void app_fill_tws_adv_manu_data_in_charger_box(void)
{
    app_fill_tws_adv_manu_data_role();

    uint8_t original_btAddr[BD_ADDR_LEN];
    factory_section_original_btaddr_get(original_btAddr);

    TWS_EARPHONE_BLE_ADV_INFO_T* pEarPhoneInfo = (TWS_EARPHONE_BLE_ADV_INFO_T *)(twsBoxEnv.adv_manufacture_data_section.data);
//support disable in box state handle
#ifdef __ENABLE_IN_BOX_STATE_HANDLE__
#if BLE_TWS_USE_BLE_AS_INBOX_COMMUNICATION_PATH
        pEarPhoneInfo->currentState = (app_is_in_charger_box() << TWS_EARPHONE_BOX_STATE_BIT_OFFSET) |
            (isTwsParingInProgress << TWS_EARPHONE_PAIRING_STATE_BIT_OFFSET) |
            (IS_CONNECTED_WITH_MOBILE() << TWS_EARPHONE_MOBILE_CONNECTION_STATE_BIT_OFFSET);
#else
        pEarPhoneInfo->currentState = (app_is_in_charger_box() << TWS_EARPHONE_BOX_STATE_BIT_OFFSET);
#endif
#endif

    pEarPhoneInfo->reqCounter = twsBoxEnv.operationRequestCounter;

    memcpy(pEarPhoneInfo->btAddr, original_btAddr, BD_ADDR_LEN);

    twsBoxEnv.adv_manufacture_data_section.isConnectedWithMobile = IS_CONNECTED_WITH_MOBILE();
    twsBoxEnv.adv_manufacture_data_section.isConnectedWithTws = IS_CONNECTED_WITH_TWS();
    twsBoxEnv.adv_manufacture_data_section.pairCode = twsBoxEnv.pairCode;

    twsBoxEnv.adv_manufacture_data_section.dataLengthFollowing =
        (uint32_t)&(((TWS_BLE_ADV_MANU_DATA_SECTION_T *)0)->data) + sizeof(TWS_EARPHONE_BLE_ADV_INFO_T) - 1;
}

static uint32_t ble_get_manufacture_data_ptr(uint8_t* advData, uint32_t dataLength, uint8_t* manufactureData)
{
    uint8_t followingDataLengthOfSection;
    uint8_t rawContentDataLengthOfSection;
    uint8_t flag;
    while (dataLength > 0)
    {
        followingDataLengthOfSection = *advData++;
        dataLength--;
        if (dataLength < followingDataLengthOfSection)
        {
            return 0;   // wrong adv data format
        }

        if (followingDataLengthOfSection > 0)
        {
            flag = *advData++;
            dataLength--;

            rawContentDataLengthOfSection = followingDataLengthOfSection - 1;
            if (BLE_ADV_MANU_FLAG == flag)
            {
                uint32_t lengthToCopy;
                if (dataLength < rawContentDataLengthOfSection)
                {
                    lengthToCopy = dataLength;
                }
                else
                {
                    lengthToCopy = rawContentDataLengthOfSection;
                }


                if ((lengthToCopy + 2) > sizeof(TWS_BLE_ADV_MANU_DATA_SECTION_T))
                {
                    return 0;
                }

                memcpy(manufactureData, advData - 2, lengthToCopy + 2);
                return lengthToCopy + 2;
            }
            else
            {
                advData += rawContentDataLengthOfSection;
                dataLength -= rawContentDataLengthOfSection;
            }
        }
    }

    return 0;
}

#if BLE_TWS_USE_BLE_AS_INBOX_COMMUNICATION_PATH
static void ble_activity_idle_timer_cb(void const *n)
{
//support disable in box state handle
#ifdef __ENABLE_IN_BOX_STATE_HANDLE__
    if (app_is_in_charger_box() && !app_is_charger_box_closed())
    {
        LOG_PRINT_BLE("Resume the interactive ble activities in the box.");
        // resume the ble activity
        app_tws_start_interactive_ble_activities_in_chargerbox();
    }
#endif
}
#endif


#ifdef __TWS_PAIR_DIRECTLY__
bool slaveDisconMobileFlag=FALSE;
bool slaveInReconMasterFlag=FALSE;
#endif

static void ble_adv_data_parse(uint8_t* bleBdAddr, int8_t rssi, unsigned char *adv_buf, unsigned char len)
{
    // use adv data to filer
    TWS_BLE_ADV_MANU_DATA_SECTION_T manu_data;
    LOG_PRINT_BLE("adv report:");
    LOG_PRINT_BLE_DUMP8("%02x ",adv_buf,len);
    if (ble_get_manufacture_data_ptr(adv_buf, len, (uint8_t *)&manu_data) > 0)
    {
        if (BEST_MAGICCODE == manu_data.magicCode)
        {
            //if (manu_data.pairCode != twsBoxEnv.pairCode)
                //return;

            LOG_PRINT_BLE("magic code is ok");
            switch (manu_data.role)
            {
                case TWS_MASTER:
                case TWS_SLAVE:
                {
                    if (app_tws_ui_is_in_repairing())
                    {
                        return;
                    }

                    uint8_t stateOfBle = manu_data.data[0];

#if BLE_TWS_USE_BLE_AS_INBOX_COMMUNICATION_PATH
                    TWS_EARPHONE_BLE_ADV_INFO_T* pEarphoneInfo = (TWS_EARPHONE_BLE_ADV_INFO_T *)manu_data.data;
                    if (!memcmp(pEarphoneInfo->btAddr, tws_mode_ptr()->masterAddr.address, BTIF_BD_ADDR_SIZE))
                    {
                        if (!IS_TWS_EARHPONE_ADV_STATE_IN_PAIRING(stateOfBle) &&
                            (!isTwsParingInProgress) &&
                            !IS_CONNECTED_WITH_TWS())
                        {
                            if (TWS_MASTER == manu_data.role)
                            {
                                // connect with the tws master via ble to start the tws re-connecting
                                memcpy(bdAddrToConnect, bleBdAddr, BTIF_BD_ADDR_SIZE);
                                app_start_custom_function_in_bt_thread(0, 0, (uint32_t)app_start_ble_tws_connecting);
                                LOG_PRINT_BLE("Start ble connecting to:");
                                LOG_PRINT_BLE_DUMP8("%02x ",bdAddrToConnect,BTIF_BD_ADDR_SIZE);
                            }
                        }
                    }
#else
                    TWS_EARPHONE_BLE_ADV_INFO_T* pEarphoneInfo = (TWS_EARPHONE_BLE_ADV_INFO_T *)manu_data.data;
                    if (!memcmp(pEarphoneInfo->btAddr, tws_mode_ptr()->masterAddr.address, BTIF_BD_ADDR_SIZE))
                    {
#ifdef __ALLOW_TWS_RECONNECT_WITHOUT_MOBILE_CONNECTED__
                        if ((!isTwsParingInProgress)  && (!IS_CONNECTED_WITH_TWS()))
                        {
                            // connect with the tws master via ble
                            if(TWSSLAVE == tws_mode_ptr()->mode)
                            {
                                slaveInReconMasterFlag=true;
                            }
                            memcpy(bdAddrToConnect, bleBdAddr, BD_ADDR_SIZE);
                            if((TWSSLAVE == tws_mode_ptr()->mode)&&(IS_CONNECTED_WITH_MOBILE()))
                            {
                                //diconnect mobilephone
                                TRACE("slave disconnect mobilephone  ");
                                slaveDisconMobileFlag=TRUE;
                                app_bt_disconnect_sco_link();
                                osDelay(200);
                                
                                app_bt_disconnect_all_mobile_devces();
                                osDelay(1000);
                            }
                            else
                            {
                                app_start_custom_function_in_bt_thread(0,
                                0, (uint32_t)app_start_ble_tws_connecting);

                            }
                            
 
                        }
#else
                        if (manu_data.isConnectedWithMobile && (!IS_CONNECTED_WITH_TWS()))
                        {
                            // connect with the tws master via ble
                            memcpy(bdAddrToConnect, bleBdAddr, BTIF_BD_ADDR_SIZE);
                            app_start_custom_function_in_bt_thread(0,
                                0, (uint32_t)app_start_ble_tws_connecting);
                        }
#endif
                    }
#endif  // #if BLE_TWS_USE_BLE_AS_INBOX_COMMUNICATION_PATH
                    break;
                }

#if defined(BLE_TWS_USE_BLE_AS_INBOX_COMMUNICATION_PATH) && defined(__ENABLE_IN_BOX_STATE_HANDLE__)
                case TWS_CHARGER_BOX:

                    // use rssi to filter
                    if(!app_is_in_charger_box())
                    {
                        LOG_PRINT_BLE("\nEarphone not in charger box.\n");
                        return;
                    }
                    if (rssi < tBleTwsConfig.rssiThreshold)
                    {
                        return;
                    }

                    // check the funcionality
                    switch ((TWS_BOX_FUNC_STATE_E)manu_data.data[0])
                    {
                        case TWS_BOX_SCANNING:
                            break;
                        case TWS_BOX_REQUESTING_TWS_PAIRING:
                        {
                            TWS_CHARGERBOX_TWS_PAIRING_REQ_T* pReq = (TWS_CHARGERBOX_TWS_PAIRING_REQ_T *)manu_data.data;
                            LOG_PRINT_BLE("Req's counter is %d, former counter is %d", pReq->reqCounter,
                                twsBoxEnv.operationRequestCounter);

                            uint8_t original_btAddr[BD_ADDR_LEN];
                            factory_section_original_btaddr_get(original_btAddr);

                            if (pReq->reqCounter == twsBoxEnv.operationRequestCounter)
                            {
                                break;
                            }

                            //if (manu_data.pairCode != twsBoxEnv.pairCode)
                            //{
                                //break;
                            //}

                            if (memcmp(pReq->btAddrMaster, original_btAddr, BTIF_BD_ADDR_SIZE) &&
                                memcmp(pReq->btAddrSlave, original_btAddr, BTIF_BD_ADDR_SIZE))
                            {
                                break;
                            }

                            LOG_PRINT_BLE("Get the valid TWS pairing request with reqCounter %d", pReq->reqCounter);

                            ble_tws_stop_all_activities();


                            twsBoxEnv.operationRequestCounter = pReq->reqCounter;
                            isTwsParingInProgress = true;
                            app_tws_start_pairing_inbox(TWS_DUAL_PAIRING, pReq->btAddrMaster, pReq->btAddrSlave);
                            break;
                        }
                        case TWS_BOX_REQUESTING_FREEMAN_PAIRING:
                        {
                            TWS_CHARGERBOX_FREEMAN_PAIRING_REQ_T* pReq = (TWS_CHARGERBOX_FREEMAN_PAIRING_REQ_T *)manu_data.data;
                            if (pReq->reqCounter == twsBoxEnv.operationRequestCounter)
                            {
                                break;
                            }

                            uint8_t original_btAddr[BD_ADDR_LEN];
                            factory_section_original_btaddr_get(original_btAddr);

                            if (memcmp(pReq->btAddrFreeman, original_btAddr, BTIF_BD_ADDR_SIZE))
                            {
                                break;
                            }

                            LOG_PRINT_BLE("Get the valid FreeMan pairing request with reqCounter %d", pReq->reqCounter);

                            ble_tws_stop_all_activities();

                            osTimerStart(ble_activity_idle_timer, BLE_ACTIVITY_IDLE_TIME_PERIOD_IN_MS);

                            twsBoxEnv.operationRequestCounter = pReq->reqCounter;
                            app_tws_start_pairing_inbox(TWS_FREEMAN_PAIRING, pReq->btAddrFreeman, NULL);
                            break;
                        }
                        case TWS_BOX_REQUESTING_ENTERING_OTA:
                        {
                            TWS_CHARGERBOX_ENTER_OTA_REQ_T* pReq = (TWS_CHARGERBOX_ENTER_OTA_REQ_T *)manu_data.data;
                            if (pReq->reqCounter == twsBoxEnv.operationRequestCounter)
                            {
                                break;
                            }
                            app_tws_parse_entering_ota_request((TWS_CHARGERBOX_ENTER_OTA_REQ_T *)manu_data.data);
                            break;
                        }
                        default:
                            break;
                    }
                    break;
#endif  // #if BLE_TWS_USE_BLE_AS_INBOX_COMMUNICATION_PATH
                default:
                    break;
            }
        }
    }
}

//received adv data
void app_adv_reported_scanned(struct gapm_adv_report_ind* ptInd)
{
    /*
    LOG_PRINT_BLE("Scanned RSSI %d BD addr:", (int8_t)ptInd->report.rssi);
    LOG_PRINT_BLE_DUMP8("0x%02x ", ptInd->report.adv_addr.address, BTIF_BD_ADDR_SIZE);
    LOG_PRINT_BLE("Scanned adv data:");
    LOG_PRINT_BLE_DUMP8("0x%02x ", ptInd->report.data, ptInd->report.data_len);
    */
    ptInd->report.rssi = bt_drv_rssi_correction(ptInd->report.rssi);

    ble_adv_data_parse(ptInd->report.adv_addr.addr, (int8_t)ptInd->report.rssi,
        ptInd->report.data, (unsigned char)ptInd->report.data_len);
}

#if BLE_TWS_USE_BLE_AS_INBOX_COMMUNICATION_PATH
static void ble_adv_scan_switch_handler(void)
{
#if PROTECTION_TO_AVOID_NON_ADV_ISSUE
    uint32_t intervalBetweenStateSwich, currentMs;
    currentMs = GET_CURRENT_MS();
    if (currentMs > bleTwsLastTimeStampInMs)
    {
        intervalBetweenStateSwich = currentMs - bleTwsLastTimeStampInMs;
    }
    else
    {
        intervalBetweenStateSwich = 0xFFFFFFFF - bleTwsLastTimeStampInMs + currentMs + 1;
    }

    if (intervalBetweenStateSwich > BLE_TWS_INTERVAL_TO_FORCE_RESET_ACTIVITIES_IN_MS)
    {
        // too long time not changing the ble activity, reset the system
        ASSERT(false, "");
    }
#endif

    //LOG_PRINT_BLE("Enter ble state switch at state %d", twsBoxEnv.state);
    if (TWS_BLE_STATE_ADVERTISING == twsBoxEnv.state)
    {
    #if PROTECTION_TO_AVOID_NON_ADV_ISSUE
        bleTwsLastTimeStampInMs = GET_CURRENT_MS();
    #endif
        SET_BLE_TWS_STATE(TWS_BLE_STATE_STOPPING_ADV);
        pendingBLEOperation = TWS_BLE_OP_START_SCANNING;
        appm_stop_advertising();
    }
    else if (TWS_BLE_STATE_SCANNING == twsBoxEnv.state)
    {
    #if PROTECTION_TO_AVOID_NON_ADV_ISSUE
        bleTwsLastTimeStampInMs = GET_CURRENT_MS();
    #endif
        SET_BLE_TWS_STATE(TWS_BLE_STATE_STOPPING_SCANNING);
        pendingBLEOperation = TWS_BLE_OP_START_ADV;
        appm_stop_scanning();
    }
}

/**
 * @brief Call back function of the scanning&advertising switch timer
 *
 * @param n Parameter imported during the timer creation osTimerCreate.
 *
 */
static void ble_adv_scan_switch_timer_cb(void const *n)
{
    app_start_custom_function_in_bt_thread(0,
                                            0, (uint32_t)ble_adv_scan_switch_handler);

}
#endif // BLE_TWS_USE_BLE_AS_INBOX_COMMUNICATION_PATH

bool ble_tws_is_initialized(void)
{
    return isBleTwsInitialized && isBtInitialized;
}

bool is_ble_stack_initialized(void)
{
    return isBleTwsInitialized;
}

#ifdef __TWS_USE_RANDOM_ADDRESS__
void gapm_set_address_type(uint8_t value);
#endif

void ble_tws_set_bt_ready_flag(void)
{
    isBtInitialized = true;
    if (ble_tws_is_initialized())
    {
    #if BLE_IS_USE_RPA
        btif_me_ble_add_dev_to_whitelist(1, &(tws_mode_ptr()->peerBleAddr));
    #else
        btif_me_ble_add_dev_to_whitelist(0, &(tws_mode_ptr()->peerBleAddr));
    #endif
        if (tBleTwsConfig.ble_tws_initialized_cb)
        {
            app_start_custom_function_in_bt_thread(0, 0,
                (uint32_t)tBleTwsConfig.ble_tws_initialized_cb);
        }
    }
}

void ble_tws_config(BLE_TWS_CONFIG_T* ptConfig)
{
    tBleTwsConfig = *ptConfig;
}

void ble_tws_config_init_done_callback(ble_tws_initialized_callback cb)
{
    tBleTwsConfig.ble_tws_initialized_cb = cb;
}

/**
 * @brief Initialize the BLE application for TWS
 *
 */
static void ble_tws_init(void)
{
    LOG_PRINT_BLE("BLE ready to start adv at %d ms", GET_CURRENT_MS());
    ble_tws_set_bt_ready_flag();

#if BLE_TWS_USE_BLE_AS_INBOX_COMMUNICATION_PATH
    // create and start a timer to switch between ble scanning and advertising
    ble_adv_scan_switch_timer =
        osTimerCreate(osTimer(BLE_ADV_SCAN_SWITCH_TIMER), osTimerPeriodic, NULL);

    ble_activity_idle_timer =
        osTimerCreate(osTimer(BLE_ACTIVITY_IDLE_TIMER), osTimerOnce, NULL);

    ble_interactive_activity_supervisor_timer =
        osTimerCreate(osTimer(BLE_INTERACTIVE_ACTIVITY_SUPERVISOR_TIMER), osTimerOnce, NULL);
#endif

    LOG_PRINT_BLE("line %d at %s", __LINE__, __FUNCTION__);

    check_before_reparing_timer =
        osTimerCreate(osTimer(CHECK_BEFORE_REPAIRING_TIMER), osTimerOnce, NULL);

    check_before_simple_reparing_timer =
        osTimerCreate(osTimer(CHECK_BEFORE_SIMPLE_REPAIRING_TIMER), osTimerOnce, NULL);

    ble_adv_speed_switch_timer =
        osTimerCreate(osTimer(BLE_ADV_SPEED_SWITCH_TIMER), osTimerOnce, NULL);

    LOG_PRINT_BLE("line %d at %s", __LINE__, __FUNCTION__);
    
    app_tws_bt_reconnecting_timeout_timer_id =
        osTimerCreate(osTimer(APP_TWS_BT_RECONNECTING_TIMEOUT_TIMER), osTimerOnce, NULL);

    app_tws_ble_handshake_timeout_timer_id = 
               osTimerCreate(osTimer(APP_TWS_BLE_HANDSHAKE_TIMEOUT_TIMER), osTimerOnce, NULL);

    twsBoxEnv.pairCode = 0xFF;
    twsBoxEnv.operationRequestCounter = 0xFF;
    SET_BLE_TWS_STATE(TWS_BLE_STATE_IDLE);
    pendingBLEOperation = TWS_BLE_OP_IDLE;
    twsBoxEnv.adv_manufacture_data_section.magicCode = BEST_MAGICCODE;
    twsBoxEnv.adv_manufacture_data_section.manufactureDataFlag = BLE_ADV_MANU_FLAG;

    twsBoxEnv.currentAdvChannel = 0;

    LOG_PRINT_BLE("line %d at %s", __LINE__, __FUNCTION__);

    isBleTwsInitialized = true;

    if (ble_tws_is_initialized())
    {
        LOG_PRINT_BLE("line %d at %s", __LINE__, __FUNCTION__);
#if BLE_IS_USE_RPA
        btif_me_ble_add_dev_to_whitelist(1, &(tws_mode_ptr()->peerBleAddr));
#else
        btif_me_ble_add_dev_to_whitelist(0, &(tws_mode_ptr()->peerBleAddr)); 
#endif
        if (tBleTwsConfig.ble_tws_initialized_cb)
        {
            app_start_custom_function_in_bt_thread(0, 0,
                (uint32_t)tBleTwsConfig.ble_tws_initialized_cb);
        }
    }
}

static void ble_tws_stop_all_activities_handler(void)
{
    switch (twsBoxEnv.state)
    {
        case TWS_BLE_STATE_ADVERTISING:
        {
            //LOG_PRINT_BLE("Stop BLE Adv!");
            //SET_BLE_TWS_STATE(TWS_BLE_STATE_STOPPING_ADV);
            //appm_stop_advertising();
            break;
        }
        case TWS_BLE_STATE_SCANNING:
        {
            //LOG_PRINT_BLE("Stop BLE scanning!");
            SET_BLE_TWS_STATE(TWS_BLE_STATE_STOPPING_SCANNING);
            appm_stop_scanning();
            break;
        }
        case TWS_BLE_STATE_CONNECTING:
        {
            //LOG_PRINT_BLE("Stop BLE scanning!");
            SET_BLE_TWS_STATE(TWS_BLE_STATE_STOPPING_CONNECTING);
            appm_stop_connecting();
            break;
        }
        case TWS_BLE_STATE_STARTING_ADV:
        {
            pendingBLEOperation = TWS_BLE_OP_STOP_ADV;
            break;
        }
        case TWS_BLE_STATE_STARTING_SCANNING:
        {
            pendingBLEOperation = TWS_BLE_OP_STOP_SCANNING;
            break;
        }
        case TWS_BLE_STATE_STARTING_CONNECTING:
            pendingBLEOperation = TWS_BLE_OP_STOP_CONNECTING;
            break;
        case TWS_BLE_STATE_STOPPING_ADV:
        case TWS_BLE_STATE_STOPPING_SCANNING:
        case TWS_BLE_STATE_STOPPING_CONNECTING:
            pendingBLEOperation = TWS_BLE_OP_IDLE;
            break;
        default:
            break;
    }
}

void ble_tws_stop_all_activities_in_app_thread(void)
{
    ble_tws_stop_all_activities_handler();
}

void ble_tws_clear_the_tws_pairing_state(void)
{
    isTwsParingInProgress = false;
}

void ble_tws_stop_all_activities(void)
{
    LOG_PRINT_BLE("%s", __func__);

    ble_tws_clear_the_tws_pairing_state();

#if BLE_TWS_USE_BLE_AS_INBOX_COMMUNICATION_PATH
    osTimerStop(ble_adv_scan_switch_timer);
    osTimerStop(ble_activity_idle_timer);
    osTimerStop(ble_interactive_activity_supervisor_timer);
#endif

    osTimerStop(ble_adv_speed_switch_timer);

    app_start_custom_function_in_bt_thread(0, 0, (uint32_t)ble_tws_stop_all_activities_in_app_thread);
}

#if BLE_TWS_USE_BLE_AS_INBOX_COMMUNICATION_PATH
static void ble_interactive_activity_supervisor_timer_cb(void const *n)
{
#ifdef __ENABLE_IN_BOX_STATE_HANDLE__
    if (app_is_in_charger_box())
    {
        ble_tws_clear_the_tws_pairing_state();
        // resume the normal mode ble activity
        app_tws_start_normal_ble_activities_in_chargerbox();
    }
    else
#endif
    {
        // stop ble activity
        ble_tws_stop_all_activities();
    }
}
#endif

#ifdef __TWS_PAIR_DIRECTLY__
void app_tws_start_normal_ble_activities_without_chargerbox(void)
{
    if(!app_is_ble_adv_allowed_without_chargerbox())
    {
        return;
    }
    TRACE("Start normal ble activities");
    app_fill_tws_adv_manu_data_in_charger_box();
    app_start_custom_function_in_bt_thread(GAPM_ADV_UNDIRECT,
                BLE_TWS_ADV_SLOW_INTERVAL_IN_MS, (uint32_t)app_start_ble_tws_advertising);
    twsBoxEnv.advMode = TWS_NORMAL_ADV;
}
#endif

//support disable in box state handle
#ifdef __ENABLE_IN_BOX_STATE_HANDLE__
void app_tws_start_normal_ble_activities_in_chargerbox(void)
{
    if (!app_is_ble_adv_allowed())
    {
        return;
    }

    LOG_PRINT_BLE("Start normal ble activities in chargerbox.");
#if BLE_TWS_USE_BLE_AS_INBOX_COMMUNICATION_PATH
    osTimerStop(ble_activity_idle_timer);
    osTimerStop(ble_interactive_activity_supervisor_timer);
    osTimerStop(ble_adv_scan_switch_timer);
#endif

    app_fill_tws_adv_manu_data_in_charger_box();

    app_start_custom_function_in_bt_thread(GAPM_ADV_UNDIRECT,
                BLE_TWS_ADV_SLOW_INTERVAL_IN_MS, (uint32_t)app_start_ble_tws_advertising);

#if PROTECTION_TO_AVOID_NON_ADV_ISSUE
    bleTwsLastTimeStampInMs = GET_CURRENT_MS();
#endif

    twsBoxEnv.advMode = TWS_NORMAL_ADV;
}
#endif

#if BLE_TWS_USE_BLE_AS_INBOX_COMMUNICATION_PATH
void app_tws_start_interactive_reconn_ble_activities(void)
{
    LOG_PRINT_BLE("Start interactive tws reconnecting ble activities in chargerbox.");

    app_fill_tws_adv_manu_data_in_charger_box();

    twsBoxEnv.scanWindowInMs = BLE_TWS_RECONNECT_LOW_DUTYCYCLE_SCANNING_WINDOW_IN_MS;
    twsBoxEnv.scanIntervalInMs = BLE_TWS_RECONNECT_LOW_DUTYCYCLE_SCANNING_INTERVAL_IN_MS;

    //twsBoxEnv.advMode = TWS_INTERACTIVE_ADV;
    //twsBoxEnv.currentAdvChannel = 0;

    app_start_custom_function_in_bt_thread(GAPM_ADV_UNDIRECT,
                BLE_LOW_SPEED_RECONNECT_ADV_INTERVAL_IN_MS, (uint32_t)app_start_ble_tws_advertising);

#if PROTECTION_TO_AVOID_NON_ADV_ISSUE
    bleTwsLastTimeStampInMs = GET_CURRENT_MS();
#endif
    osTimerStart(ble_adv_scan_switch_timer, BLE_RECONN_ADV_SCAN_SWITCH_PERIOD_IN_MS);
}

extern int chip_unique_get_word(void);
void app_tws_start_interactive_ble_activities_in_chargerbox(void)
{
    int rand_ms;

    LOG_PRINT_BLE("Start interactive ble activities in chargerbox.");
    osTimerStop(ble_activity_idle_timer);

    osTimerStart(ble_interactive_activity_supervisor_timer, BLE_INTERACTIVE_ACTIVITY_TIMEOUT_PERIO);

    app_fill_tws_adv_manu_data_in_charger_box();

    twsBoxEnv.scanWindowInMs = BLE_TWS_SCANNING_WINDOW_IN_MS;
    twsBoxEnv.scanIntervalInMs = BLE_TWS_SCANNING_INTERVAL_IN_MS;

    app_refresh_random_seed();
    rand_ms = rand()%BLE_TWS_FAST_ADV_INTERVAL_IN_MS;

    app_start_custom_function_in_bt_thread(GAPM_ADV_UNDIRECT,
                BLE_TWS_FAST_ADV_INTERVAL_IN_MS+rand_ms, (uint32_t)app_start_ble_tws_advertising);


#if PROTECTION_TO_AVOID_NON_ADV_ISSUE
    bleTwsLastTimeStampInMs = GET_CURRENT_MS();
#endif
    osTimerStart(ble_adv_scan_switch_timer, BLE_ADV_SCAN_SWITCH_PERIOD_IN_MS);
}
#endif

/**
 * @brief Initialize the BLE application for TWS when the BLE system is ready
 *
 */
extern void app_notify_stack_ready();
void app_ble_system_ready(void)
{
    LOG_PRINT_BLE("BLE is ready");
    ble_tws_init();
#if (BLE_OTA_APP_EN)||BLE_IS_USE_RPA
    app_start_ble_tws_advertising(GAPM_ADV_UNDIRECT, BLE_TWS_ADV_SLOW_INTERVAL_IN_MS);
#endif
#ifndef FPGA
    app_notify_stack_ready();
#endif
}

void app_tws_pending_pairing_req_handler(void)
{
    if (TWS_DUAL_PAIRING == twsBoxEnv.pendingPairingMode)
    {
        app_tws_pairing_request_handler(twsBoxEnv.pendingPairedBtAddr[0], twsBoxEnv.pendingPairedBtAddr[1]);
    }
    else
    {
        app_freeman_pairing_request_handler(twsBoxEnv.pendingPairedBtAddr[0]);
    }

#if !BLE_TWS_USE_BLE_AS_INBOX_COMMUNICATION_PATH
    app_tws_pairing_preparation_done();
#endif
}

bool app_tws_pairing_request_handler(uint8_t* masterBtAddr, uint8_t* slaveBtAddr)
{
    if (app_bt_conn_is_any_connection_on())
    {
        LOG_PRINT_BLE("%s, app_bt_conn_is_any_connection_on==true", __func__);
        return false;
    }

    uint8_t original_btAddr[BD_ADDR_LEN];
    factory_section_original_btaddr_get(original_btAddr);

    if (!memcmp(masterBtAddr, original_btAddr, BD_ADDR_LEN))
    {
        LOG_PRINT_BLE("Pairing request from charger box is received, start as the master.");
        twsBoxEnv.twsModeToStart = TWSMASTER;
    }
    else if (!memcmp(slaveBtAddr, original_btAddr, BD_ADDR_LEN))
    {
        LOG_PRINT_BLE("Pairing request from charger box is received, start as the slave.");
        twsBoxEnv.twsModeToStart = TWSSLAVE;
    }
    else
    {
        LOG_PRINT_BLE("no accept addr !");
         DUMP8("%02x ",original_btAddr,6);
         DUMP8("%02x ",slaveBtAddr,6);
         DUMP8("%02x ",masterBtAddr,6);
        return false;
    }

    memcpy(twsBoxEnv.twsBtBdAddr[0], masterBtAddr, BD_ADDR_LEN);
    memcpy(twsBoxEnv.twsBtBdAddr[1], slaveBtAddr, BD_ADDR_LEN);

    app_start_custom_function_in_bt_thread(0,
                        0, (uint32_t)app_tws_start_reset_pairing);

    return true;
}

bool app_freeman_pairing_request_handler(uint8_t* btAddr)
{
    if (app_bt_conn_is_any_connection_on())
    {
        LOG_PRINT_BLE("%s, app_bt_conn_is_any_connection_on==true", __func__);
        return false;
    }

    uint8_t original_btAddr[BD_ADDR_LEN];
    factory_section_original_btaddr_get(original_btAddr);

    if (!memcmp(btAddr, original_btAddr, BD_ADDR_LEN))
    {
        LOG_PRINT_BLE("Single pairing request from charger box is received, start as the freeman.");

    }
    else
    {
        return false;
    }

#if BLE_TWS_USE_BLE_AS_INBOX_COMMUNICATION_PATH
#ifdef __ENABLE_IN_BOX_STATE_HANDLE__
    if (app_is_in_charger_box())
    {
        // the slave can perceive the connection with master as the paring done
        app_tws_start_normal_ble_activities_in_chargerbox();
    }
#endif
#endif
#if FPGA==0
    app_tws_pairing_done();
#endif

    twsBoxEnv.twsModeToStart = TWSFREE;
    memcpy(twsBoxEnv.twsBtBdAddr[0], btAddr, BD_ADDR_LEN);

    app_start_custom_function_in_bt_thread(0,
                        0, (uint32_t)app_tws_start_reset_pairing);

    return true;
}

static void app_tws_parse_entering_ota_request(TWS_CHARGERBOX_ENTER_OTA_REQ_T* pReq)
{
    for (uint32_t index = 0;index < pReq->countOfDevices;index++)
    {
        if (!memcmp(bt_addr, pReq->btAddr[index], BTIF_BD_ADDR_SIZE))
        {
            LOG_PRINT_BLE("Receive the entering OTA mode request");
            app_tws_enter_ota_mode();

        }
    }
}

void app_tws_enter_ota_mode(void)
{
    LOG_PRINT_BLE("Enter OTA mode!");
    hal_sw_bootmode_set(HAL_SW_BOOTMODE_ENTER_HIDE_BOOT);
    hal_cmu_sys_reboot();
}

void app_bt_start_reset_pairing(void)
{
    app_tws_role_switch_reset_bt_info();
    set_conn_op_state_machine(CONN_OP_STATE_MACHINE_RE_PAIRING);
    //conn_remove_tws_history_paired_devices();

    // reset the mobile paired flag
    isMobileDevPaired = false;
    conn_stop_tws_connection_creation_supvervisor_timer();
    conn_stop_mobile_connection_creation_supvervisor_timer();
    conn_stop_mobile_connection_creation_idle_timer();
    ble_tws_stop_all_activities();
    set_conn_op_state(CONN_OP_STATE_IDLE);

    //update bt addr
    btif_me_set_bt_address(bt_addr);

    app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, BTIF_BAM_GENERAL_ACCESSIBLE,0,0);
    //app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, BTIF_BAM_LIMITED_ACCESSIBLE,0,0);
    app_start_10_second_timer(APP_PAIR_TIMER_ID);
    //set_conn_op_state(CONN_OP_STATE_WAITING_FOR_MOBILE_CONNECTION);

}


void app_tws_start_reset_pairing(void)
{
    app_tws_role_switch_reset_bt_info();

    set_conn_op_state_machine(CONN_OP_STATE_MACHINE_RE_PAIRING);

    conn_remove_tws_history_paired_devices();

    if (TWSFREE != twsBoxEnv.twsModeToStart)
    {
        memcpy(tws_mode_ptr()->masterAddr.address, twsBoxEnv.twsBtBdAddr[0], BTIF_BD_ADDR_SIZE);
        memcpy(tws_mode_ptr()->slaveAddr.address, twsBoxEnv.twsBtBdAddr[1], BTIF_BD_ADDR_SIZE);
        if (TWSMASTER == twsBoxEnv.twsModeToStart)
        {
            log_event_2(EVENT_PAIRING_STARTED, twsBoxEnv.twsModeToStart, twsBoxEnv.twsBtBdAddr[1]);
        }
        else
        {
            log_event_2(EVENT_PAIRING_STARTED, twsBoxEnv.twsModeToStart, twsBoxEnv.twsBtBdAddr[0]);
        }
    }
    else
    {
        log_event_2(EVENT_PAIRING_STARTED, twsBoxEnv.twsModeToStart, twsBoxEnv.twsBtBdAddr[0]);
    }
    
    // reset the mobile paired flag
    isMobileDevPaired = false;

    conn_stop_tws_connection_creation_supvervisor_timer();
    conn_stop_mobile_connection_creation_supvervisor_timer();
    conn_stop_mobile_connection_creation_idle_timer();

    set_conn_op_state(CONN_OP_STATE_IDLE);

    if (TWSMASTER == twsBoxEnv.twsModeToStart)
    {
        //app_bt_cancel_ongoing_connecting();
        memcpy(bt_addr, twsBoxEnv.twsBtBdAddr[0], BTIF_BD_ADDR_SIZE);
        btif_me_set_bt_address(bt_addr);
        nv_record_update_tws_mode(TWSMASTER);
        conn_start_connecting_slave(CONNECTING_SLAVE_TIMEOUT_RECONNECT_IN_CHARGER_BOX_MS,true);
    }
    else if (TWSSLAVE == twsBoxEnv.twsModeToStart)
    {
        memcpy(bt_addr, twsBoxEnv.twsBtBdAddr[1], BTIF_BD_ADDR_SIZE);
        btif_me_set_bt_address(bt_addr);
        nv_record_update_tws_mode(TWSSLAVE);
        conn_start_connectable_only_pairing(BT_SOURCE_CONN_MASTER);
#ifdef __TWS_PAIR_DIRECTLY__
        app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, BTIF_BAM_GENERAL_ACCESSIBLE,0,0);
#endif
    }
    else
    {
        memcpy(bt_addr, twsBoxEnv.twsBtBdAddr[0], BTIF_BD_ADDR_SIZE);
        btif_me_set_bt_address(bt_addr);
        app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, BTIF_BAM_GENERAL_ACCESSIBLE,0,0);
        set_conn_op_state(CONN_OP_STATE_WAITING_FOR_MOBILE_CONNECTION);
        nv_record_update_tws_mode(TWSFREE);
#ifndef FPGA        
        app_start_10_second_timer(APP_PAIR_TIMER_ID);
#endif
#if (_GFPS_)
        app_enter_fastpairing_mode();
#endif
    }
}

static void app_start_reconnecting_tws_slave_operation(void)
{
    if (IS_CONNECTED_WITH_TWS())
    {
        return;
    }

    if (IS_CONNECTING_SLAVE())
    {
        return;
    }

    // stop ble re-connecting supervisor timer
    conn_stop_tws_ble_connection_creation_supvervisor_timer();

    // stop on-going music playing
    app_tws_reconnect_stop_music();

    ble_tws_stop_all_activities();

    isTwsBtReconnectingInProgress = true;
    osTimerStart(app_tws_bt_reconnecting_timeout_timer_id, TWS_BT_RECONNECTING_TIMEOUT_IN_MS);

    set_conn_op_state_machine(CONN_OP_STATE_MACHINE_NORMAL);
    LOG_PRINT_BLE("Received the TWS slave's ble command to connect it.");
    set_conn_op_state(CONN_OP_STATE_CONNECTING_SLAVE);
}

static void app_start_reconnecting_tws_master_operation(void)
{
    // stop re-connecting supervisor timer
    app_stop_supervisor_timers();

    appm_disconnect(tws_conidx);
    ble_tws_stop_all_activities();
    set_conn_op_state_machine(CONN_OP_STATE_MACHINE_NORMAL);
    LOG_PRINT_BLE("Get response, start waiting for connection from TWS master");

    conn_start_connectable_only_pairing(BT_SOURCE_CONN_MASTER);

    isTwsBtReconnectingInProgress = true;
    osTimerStart(app_tws_bt_reconnecting_timeout_timer_id, TWS_BT_RECONNECTING_TIMEOUT_IN_MS);
}

static void app_tws_disconnect_untrustable_ble_connection(void)
{
    appm_disconnect(tws_conidx);
    app_bt_accessmode_set_scaninterval(0x80);
    conn_ble_reconnection_machanism_handler(false);
    isToRestoreBle = true;  
}

void BLE_to_connect_tws_slave_rsp_handler(BLE_CUSTOM_CMD_RET_STATUS_E retStatus, uint8_t* ptrParam, uint32_t paramLen)
{
    if (NO_ERROR == retStatus)
    {
        app_bt_accessmode_set_scaninterval(0x20);
        app_start_reconnecting_tws_master_operation();
    }
    else
    {
        if (TIMEOUT_WAITING_RESPONSE == retStatus)
        {
            LOG_PRINT_BLE("Timeout happens during waiting for the command of TWS.");
        }
        else
        {
            LOG_PRINT_BLE("Connection tws request is rejected with error code %d.", retStatus);
        }
        app_tws_disconnect_untrustable_ble_connection();
    }
}

void BLE_to_connect_tws_slave(uint32_t funcCode, uint8_t* ptrParam, uint32_t paramLen)
{
    if (!IS_CONNECTED_WITH_TWS())
    {
        app_start_reconnecting_tws_slave_operation();

        BLE_send_response_to_command(funcCode, NO_ERROR, NULL, 0, true);
    }
    else
    {
        BLE_send_response_to_command(funcCode, HANDLING_FAILED, NULL, 0, true);
    }
}

static void app_tws_generate_ble_initial_handshake_str(TWS_BLE_INITIAL_HANDSHAKE_T* ptHandshake)
{
    for (uint8_t index = 0;index < BTIF_BD_ADDR_SIZE;index++)
    {
        ptHandshake->ble_initial_handshake_str[index] =
            tws_mode_ptr()->masterAddr.address[index] +
            tws_mode_ptr()->slaveAddr.address[index];
    }   
}

void BLE_initial_handshake_rsp_handler(BLE_CUSTOM_CMD_RET_STATUS_E retStatus, uint8_t* ptrParam, uint32_t paramLen)
{
    if (NO_ERROR == retStatus)
    {
        conn_stop_tws_connection_creation_supvervisor_timer();
        LOG_PRINT_BLE("Send command to the peer device and let it connect me via BT.");
        BLE_send_custom_command(OP_LET_PEER_TWS_CONNECT_SLAVE, NULL, 0, false); 
    }
    else
    {
        if (TIMEOUT_WAITING_RESPONSE == retStatus)
        {
            LOG_PRINT_BLE("Timeout happens during waiting for the response of initial handshake.");
        }
        else
        {
            LOG_PRINT_BLE("Initial handshake is rejected with error code %d.", retStatus);
        }
        app_tws_disconnect_untrustable_ble_connection();
    }

}

// when the device in ble adv is connected, it will start a timer waiting for the initial handshake from the peer device,
// to assure that the peer device is TWS or mobile
void BLE_initial_handshake_handler(uint32_t funcCode, uint8_t* ptrParam, uint32_t paramLen)
{
    // case 2: connected with peer tws earbud
    app_tws_stop_ble_handshake_supervising();
    
    if (!IS_CONNECTED_WITH_TWS())
    {
        TWS_BLE_INITIAL_HANDSHAKE_T* ptHandshake = 
            (TWS_BLE_INITIAL_HANDSHAKE_T *)ptrParam;
        TWS_BLE_INITIAL_HANDSHAKE_T expectedHandshake;
        app_tws_generate_ble_initial_handshake_str(&expectedHandshake);
        if (memcmp((uint8_t *)ptHandshake, (uint8_t *)&expectedHandshake,
            sizeof(expectedHandshake)))
        {
            LOG_PRINT_BLE("The ble initial handshake is wrong.");
            
            BLE_send_response_to_command(funcCode, HANDLING_FAILED, NULL, 0, true);
        }
        else
        {
            TRACE("send no error response.");
            BLE_send_response_to_command(funcCode, NO_ERROR, NULL, 0, true);
        }
    }
    else
    {
        TRACE("send failure response.");
        BLE_send_response_to_command(funcCode, HANDLING_FAILED, NULL, 0, true);
    }
}
    
void app_ble_fill_debug_status_region(uint8_t debugStatus)
{
    twsBoxEnv.adv_manufacture_data_section.selfDefinedRegion = debugStatus;
}

static void check_before_repairing_timer_cb(void const *n)
{
    if (!app_conn_check_and_restore_op_state())
    {
        osTimerStart(check_before_reparing_timer,
            TWS_TIME_INTERVAL_WAITING_PENDING_OP_DONE_BEFORE_REPAIRING_IN_MS);
    }
    else
    {
        if (app_bt_conn_is_any_connection_on())
        {
            app_tws_do_preparation_before_paring();
        }
        else
        {
            if (TWS_DUAL_PAIRING == twsBoxEnv.pendingPairingMode)
            {
                app_tws_pairing_request_handler(twsBoxEnv.pendingPairedBtAddr[0],
                    twsBoxEnv.pendingPairedBtAddr[1]);
            }
#ifdef __TWS_PAIR_DIRECTLY__
            else if(TWS_SLAVE_ENTER_PAIRING== twsBoxEnv.pendingPairingMode)
            {
                app_tws_pairing_request_handler(NULL, twsBoxEnv.pendingPairedBtAddr[1]);
            }
#endif
            else
            {
                app_freeman_pairing_request_handler(twsBoxEnv.pendingPairedBtAddr[0]);
            }
        }
    }
}

static void check_before_simple_repairing_timer_cb(void const *n)
{
    if (!app_conn_check_and_restore_op_state())
    {
        osTimerStart(check_before_simple_reparing_timer,
            TWS_TIME_INTERVAL_WAITING_PENDING_OP_DONE_BEFORE_REPAIRING_IN_MS);
    }
    else
    {
        app_start_custom_function_in_bt_thread(0, 0,
            (uint32_t)app_tws_simple_repairing_request_handler);
    }
}

static void app_tws_prepare_for_simple_repairing_inbox(void)
{
    LOG_PRINT_BLE(" [ %s ] curr mode %d ", __func__,app_tws_get_mode());
    if ((app_tws_is_freeman_mode()) || (app_tws_is_master_mode()))
    {
        app_bt_disconnect_all_mobile_devces();
    }
}

static void app_tws_do_preparation_before_simple_reparing_inbox(void)
{
    set_conn_op_state_machine(CONN_OP_STATE_MACHINE_DISCONNECT_MOBILE_BEFORE_SIMPLE_REPAIRING);

    app_stop_supervisor_timers();

    app_start_custom_function_in_bt_thread(0, 0,
        (uint32_t)app_tws_prepare_for_simple_repairing_inbox);
}

void app_tws_start_reset_simple_pairing(void)
{
    // reset the mobile paired flag
    isMobileDevPaired = false;

    conn_stop_tws_connection_creation_supvervisor_timer();
    conn_stop_mobile_connection_creation_supvervisor_timer();
    conn_stop_mobile_connection_creation_idle_timer();

    set_conn_op_state(CONN_OP_STATE_IDLE);

    if (app_tws_is_master_mode())
    {
        set_conn_op_state_machine(CONN_OP_STATE_MACHINE_SIMPLE_REPAIRING);
        if (IS_CONNECTED_WITH_TWS())
        {
            // start connecting mobile device
            app_start_custom_function_in_bt_thread(0,
                0, (uint32_t)conn_start_connecting_mobile_device);
        }
        else
        {
            // connect the slave firstly
            conn_start_connecting_slave(CONNECTING_SLAVE_TIMEOUT_RECONNECT_IN_CHARGER_BOX_MS, true);
        }
    }
    else if (app_tws_is_slave_mode())
    {
        if (!IS_CONNECTED_WITH_TWS())
        {
            set_conn_op_state_machine(CONN_OP_STATE_MACHINE_SIMPLE_REPAIRING);
            conn_start_connectable_only_pairing(BT_SOURCE_CONN_MASTER);
        }
    }
    else
    {
        set_conn_op_state_machine(CONN_OP_STATE_MACHINE_SIMPLE_REPAIRING);
        app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, BTIF_BAM_GENERAL_ACCESSIBLE,0,0);
        set_conn_op_state(CONN_OP_STATE_WAITING_FOR_MOBILE_CONNECTION);
    }
#ifndef FPGA
    app_start_10_second_timer(APP_PAIR_TIMER_ID);
#endif
}

static void app_tws_simple_repairing_request_handler(void)
{
    // master:
    // 1. if connected with the mobile, disconnect it
    // 2. Assure it's connected with the slave firstly
    // 3. start public pairing

    // freeman:
    // 1. if connected with the mobile ,disconnect it
    // 2. start public pairing

    // slave:
    // 1. if connected with the master, do nothing
    // 2. if not connected with the master, enter connectable only mode

    if ((app_tws_is_master_mode()) ||
        (app_tws_is_freeman_mode()))
    {
        if (IS_CONNECTED_WITH_MOBILE())
        {
            app_tws_do_preparation_before_simple_reparing_inbox();
        }
        else
        {
            app_tws_start_reset_simple_pairing();
        }
    }
    else if (app_tws_is_slave_mode())
    {
        app_tws_start_reset_simple_pairing();
    }
}

void app_tws_start_simple_repairing_inbox(uint8_t pairCode)
{
    isInMobileBtSilentState = false;
    twsBoxEnv.pairCode = pairCode;
    twsBoxEnv.adv_manufacture_data_section.isConnectedWithMobile = 0;
    app_tws_start_normal_ble_activities_in_chargerbox();

    if (!app_conn_check_and_restore_op_state())
    {
        osTimerStart(check_before_simple_reparing_timer,
            TWS_TIME_INTERVAL_WAITING_PENDING_OP_DONE_BEFORE_REPAIRING_IN_MS);
        return;
    }

    app_tws_simple_repairing_request_handler();
}

void app_tws_start_pairing_inbox(uint32_t pairingMode, uint8_t* pMasterAddr, uint8_t* SlaveAddr)
{
#ifdef __TWS_PAIR_DIRECTLY__
    if ((TWS_DUAL_PAIRING == pairingMode)||(TWS_MASTER_CON_SLAVE==pairingMode))
#else
    if (TWS_DUAL_PAIRING == pairingMode)
#endif
    {
        if ((!app_conn_check_and_restore_op_state()) ||
            app_bt_conn_is_any_connection_on())
        {
            memcpy(twsBoxEnv.pendingPairedBtAddr[0], pMasterAddr, BTIF_BD_ADDR_SIZE);
            memcpy(twsBoxEnv.pendingPairedBtAddr[1], SlaveAddr, BTIF_BD_ADDR_SIZE);
            twsBoxEnv.pendingPairingMode = pairingMode;

            if (app_bt_conn_is_any_connection_on())
            {
                app_tws_do_preparation_before_paring();
            }
            else
            {
                osTimerStart(check_before_reparing_timer,
                    TWS_TIME_INTERVAL_WAITING_PENDING_OP_DONE_BEFORE_REPAIRING_IN_MS);
            }
        }
        else
        {
            app_tws_pairing_request_handler(pMasterAddr, SlaveAddr);
        }
    }
#ifdef __TWS_PAIR_DIRECTLY__
    else if(TWS_SLAVE_ENTER_PAIRING==pairingMode)
    {
        if ((!app_conn_check_and_restore_op_state()) ||
            app_bt_conn_is_any_connection_on())
        {
            memcpy(twsBoxEnv.pendingPairedBtAddr[0], pMasterAddr, BD_ADDR_SIZE);
            twsBoxEnv.pendingPairingMode = pairingMode;
            TRACE("!app_conn_check_and_restore_op_state or app_bt_conn_is_any_connection_on");

            if (app_bt_conn_is_any_connection_on())
            {
                app_tws_do_preparation_before_paring();
            }
            else
            {
                osTimerStart(check_before_reparing_timer,
                    TWS_TIME_INTERVAL_WAITING_PENDING_OP_DONE_BEFORE_REPAIRING_IN_MS);
            }
        }
        else
        {
            app_tws_pairing_request_handler(NULL,SlaveAddr);
        }

    }
#endif
    else
    {
        if ((!app_conn_check_and_restore_op_state()) ||
            app_bt_conn_is_any_connection_on())
        {
            memcpy(twsBoxEnv.pendingPairedBtAddr[0], pMasterAddr, BTIF_BD_ADDR_SIZE);
            twsBoxEnv.pendingPairingMode = pairingMode;

            if (app_bt_conn_is_any_connection_on())
            {
                app_tws_do_preparation_before_paring();
            }
            else
            {
                osTimerStart(check_before_reparing_timer,
                    TWS_TIME_INTERVAL_WAITING_PENDING_OP_DONE_BEFORE_REPAIRING_IN_MS);
            }
        }
        else
        {
            app_freeman_pairing_request_handler(pMasterAddr);
        }
    }
}


#ifdef __TWS_PAIR_DIRECTLY__

//bool app_bt_pairing_request_handler(uint8_t* masterBtAddr, uint8_t* slaveBtAddr)
bool app_bt_pairing_request_handler(void)
{
    if (app_bt_conn_is_any_connection_on())
    {
        TRACE("%s, app_bt_conn_is_any_connection_on==true", __func__);
        return false;
    }

    //  memcpy(twsBoxEnv.twsBtBdAddr[0], masterBtAddr, BD_ADDR_LEN);
    //  memcpy(twsBoxEnv.twsBtBdAddr[1], slaveBtAddr, BD_ADDR_LEN);
    app_start_custom_function_in_bt_thread(0,0, (uint32_t)app_bt_start_reset_pairing);

    return true;
}

void app_bt_start_pairing(uint32_t pairingMode, uint8_t* pMasterAddr, uint8_t* SlaveAddr)
{
    if ((!app_conn_check_and_restore_op_state()) ||
        app_bt_conn_is_any_connection_on())
    {
        memcpy(twsBoxEnv.pendingPairedBtAddr[0], pMasterAddr, BD_ADDR_SIZE);
        memcpy(twsBoxEnv.pendingPairedBtAddr[1], SlaveAddr, BD_ADDR_SIZE);
        twsBoxEnv.pendingPairingMode = pairingMode;

        if (app_bt_conn_is_any_connection_on())
        {
            app_tws_do_preparation_before_paring();
        }
        else
        {
            osTimerStart(check_before_reparing_timer,
                TWS_TIME_INTERVAL_WAITING_PENDING_OP_DONE_BEFORE_REPAIRING_IN_MS);
        }
    }
    else
    {
        app_bt_pairing_request_handler();
    }

}



bool app_is_ble_adv_allowed_without_chargerbox(void)
{
    // for all cases, return when the charger box is closed or the role switch is ongoing
    if (app_tws_is_role_switching_on()
        ||app_tws_is_bt_roleswitch_in_progress())
    {
        TRACE("not allowed ble adv !");
        return false;
    }
    else
    {
        if (app_tws_is_slave_mode())
        {
        	TRACE("not allowed ble adv !!");
            return false;
        }
        else
        {            
            return true;
        }
    }
}
#endif

bool app_is_ble_adv_allowed(void)
{
    // for all cases, return when the charger box is closed or the role switch is ongoing
//support disable in box state handle
#ifdef __ENABLE_IN_BOX_STATE_HANDLE__
    if (app_is_charger_box_closed() ||
        app_tws_is_role_switching_on())
#else
    if (app_tws_is_role_switching_on())
#endif
    {
        return false;
    }

    if (btapp_hfp_is_sco_active())
    {
        return false;
    }

    if (app_tws_is_bt_roleswitch_in_progress())
    {
        return false;
    }
//support disable in box state handle
#ifdef __ENABLE_IN_BOX_STATE_HANDLE__
    if (app_is_in_charger_box())
    {
        return true;
    }
    else
#endif
    {
        if (app_tws_is_slave_mode())
        {
            return false;
        }
        else
        {
            return true;
        }
    }
}

void app_ble_update_adv_connect_with_mobile(uint8_t isConnectedWithMobile)
{
    if(isConnectedWithMobile != twsBoxEnv.adv_manufacture_data_section.isConnectedWithMobile)
    {
        twsBoxEnv.adv_manufacture_data_section.isConnectedWithMobile = isConnectedWithMobile;
        twsBoxEnv.adv_manufacture_data_section.isConnectedWithTws = IS_CONNECTED_WITH_TWS();
        app_start_custom_function_in_bt_thread(GAPM_ADV_UNDIRECT,
            BLE_TWS_ADV_SLOW_INTERVAL_IN_MS, (uint32_t)app_start_ble_tws_advertising);
    }

}
TWS_BOX_ENV_T* app_tws_get_env(void)
{
    return &twsBoxEnv;
}

bool app_ble_tws_is_connected_with_peer_earbud(const uint8_t* connectedBleAddr)
{
    if (memcmp(connectedBleAddr, tws_mode_ptr()->peerBleAddr.address, BD_ADDR_SIZE))
    {
        return false;
    }
    else
    {
        return true;
    }
}


#endif // #if __BLE_TWS_ENABLED__

