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
#include "stdio.h"
#include "cmsis_os.h"
#include "factory_section.h"
#include "list.h"
#include "string.h"

#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_bootmode.h"

#include "audioflinger.h"
#include "apps.h"
#include "app_thread.h"
#include "app_key.h"
#include "app_pwl.h"
#include "app_audio.h"
#include "app_overlay.h"
#include "app_battery.h"
#include "app_utils.h"
#include "app_status_ind.h"
#ifdef __FACTORY_MODE_SUPPORT__
#include "app_factory.h"
#include "app_factory_bt.h"
#endif
#include "bt_drv_interface.h"
#include "besbt.h"
#include "nvrecord.h"
#include "nvrecord_env.h"


#include "me_api.h"
#include "a2dp_api.h"
#include "os_api.h"
#include "btapp.h"
#include "app_bt.h"


#if defined(HALFDUPLEXUART)
#include "halfduplexuart.h"
#endif
#if GSOUND_OTA_ENABLED
#include "gsound_ota.h"
#endif
#ifdef BES_OTA_ENABLED
#include "ota_handler.h"
#endif
#ifdef MEDIA_PLAYER_SUPPORT
#include "resources.h"
#include "app_media_player.h"
#endif
#include "app_bt_media_manager.h"
#include "hal_sleep.h"
#if VOICE_DATAPATH
#include "app_voicepath.h"
#endif

#if defined(__FORCE_OTABOOT_UPDATE__)
#include "apps_ota_checker.h"
#endif

#if defined(BTUSB_AUDIO_MODE)
extern "C"  bool app_usbaudio_mode_on(void) ;
#endif
#ifdef ANC_APP
#include "app_anc.h"
#endif
#include "app_ble_cmd_handler.h"
#include "app_ble_custom_cmd.h"
#include "rwapp_config.h"
#ifdef __TWS__
#include "app_tws.h"
#endif
#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)
void app_switch_player_role_key(APP_KEY_STATUS *status, void *param);
int rb_ctl_init(void);
void player_role_thread_init(void);
#endif

#include "app_bt_conn_mgr.h"
#include "app_tws_if.h"
#include "ble_tws.h"
#include "app_tws_ui.h"
#include "app_tws_role_switch.h"
#include "analog.h"
#include "log_section.h"
#include "app_hfp.h"
#include "app_spp.h"

extern "C"
{
    void hal_trace_crashdump_print(void);

}

#define APP_BATTERY_LEVEL_LOWPOWERTHRESHOLD (1)

static uint8_t  app_system_status = APP_SYSTEM_STATUS_INVALID;

uint8_t app_system_status_set(int flag)
{
    uint8_t old_status = app_system_status;
    app_system_status = flag;
    return old_status;
}

uint8_t app_system_status_get(void)
{
    return app_system_status;
}

#ifdef MEDIA_PLAYER_RBCODEC
extern int rb_ctl_init();
extern bool rb_ctl_is_init_done(void);
extern void app_rbplay_audio_reset_pause_status(void);
#endif

#ifdef __COWIN_V2_BOARD_ANC_SINGLE_MODE__
extern bool app_pwr_key_monitor_get_val(void);
static bool anc_single_mode_on = false;
extern "C" bool anc_single_mode_is_on(void)
{
    return anc_single_mode_on;
}
#endif

#ifdef __ANC_STICK_SWITCH_USE_GPIO__
extern bool app_anc_switch_get_val(void);
#endif

#ifndef APP_TEST_MODE
static uint8_t app_status_indication_init(void)
{
    struct APP_PWL_CFG_T cfg;
    memset(&cfg, 0, sizeof(struct APP_PWL_CFG_T));
    app_pwl_open();
    app_pwl_setup(APP_PWL_ID_0, &cfg);
    app_pwl_setup(APP_PWL_ID_1, &cfg);
    return 0;
}
#endif

void app_refresh_random_seed(void)
{
	uint32_t generatedSeed = hal_sys_timer_get();
	for (uint8_t index = 0; index < sizeof(bt_addr); index++)
	{
		generatedSeed ^= (((uint32_t)(bt_addr[index])) << (hal_sys_timer_get()&0xF));
	}
	srand(generatedSeed);
}
#if defined(__BTIF_EARPHONE__) && defined(__BTIF_AUTOPOWEROFF__)

void PairingTransferToConnectable(void);

typedef void (*APP_10_SECOND_TIMER_CB_T)(void);

void app_pair_timerout(void);
void app_poweroff_timerout(void);
void CloseEarphone(void);

typedef struct
{
    uint8_t timer_id;
    uint8_t timer_en;
    uint8_t timer_count;
    uint8_t timer_period;
    APP_10_SECOND_TIMER_CB_T cb;
}APP_10_SECOND_TIMER_STRUCT;



APP_10_SECOND_TIMER_STRUCT app_pair_timer =
{
    .timer_id =APP_PAIR_TIMER_ID,
    .timer_en = 0,
    .timer_count = 0,
    .timer_period = 6,
    .cb =    PairingTransferToConnectable
};

extern void app_reconnect_timeout_handle(void);

APP_10_SECOND_TIMER_STRUCT app_reconnect_timer =
{
    .timer_id =APP_RECONNECT_TIMER_ID,
    .timer_en = 0,
    .timer_count = 0,
    .timer_period = 120,
    .cb =    app_reconnect_timeout_handle
};
APP_10_SECOND_TIMER_STRUCT app_poweroff_timer=
{
    .timer_id =APP_POWEROFF_TIMER_ID,
    .timer_en = 0,
    .timer_count = 0,
    .timer_period = 180,
    .cb =    CloseEarphone
};

APP_10_SECOND_TIMER_STRUCT  *app_10_second_array[APP_10_SECOND_TIMER_TOTAL_NUM] =
{
    &app_pair_timer,
    &app_poweroff_timer,
    &app_reconnect_timer,
};



void app_stop_10_second_timer(uint8_t timer_id)
{
    APP_10_SECOND_TIMER_STRUCT *timer = app_10_second_array[timer_id];
    timer->timer_en = 0;
    timer->timer_count = 0;
}

void app_start_10_second_timer(uint8_t timer_id)
{
    APP_10_SECOND_TIMER_STRUCT *timer = app_10_second_array[timer_id];

    timer->timer_en = 1;
    timer->timer_count = 0;
}

void app_10_second_timerout_handle(uint8_t timer_id)
{
    APP_10_SECOND_TIMER_STRUCT *timer = app_10_second_array[timer_id];

    timer->timer_en = 0;
    timer->cb();
}




void app_10_second_timer_check(void)
{
    uint8_t i;
    for(i=0;i<sizeof(app_10_second_array)/sizeof(app_10_second_array[0]);i++)
    {
        if(app_10_second_array[i]->timer_en)
        {
            app_10_second_array[i]->timer_count++;
            if(app_10_second_array[i]->timer_count >= app_10_second_array[i]->timer_period)
            {
                app_10_second_timerout_handle(i);
            }
        }

    }
}
#endif

void intersys_sleep_checker(void);
extern int app_bt_stream_local_volume_get(void);
extern int voicebtpcm_pcm_msbc_sync_offset_get_local(uint16_t *msbc_sync_offset);


static void app_peridic_ble_activity_check(void)
{
#ifdef __ENABLE_IN_BOX_STATE_HANDLE__
    if (!app_is_charger_box_closed())
#endif
    {
        if (app_tws_is_master_mode() &&
            IS_CONNECTED_WITH_MOBILE() &&
            !IS_CONNECTED_WITH_TWS() &&
            TWS_BLE_STATE_IDLE != app_tws_get_env()->state){

            app_start_custom_function_in_bt_thread(0,
                        0, (uint32_t)app_tws_restart_operation);
        }
    }
}

static void app_env_checker_handler(void const *param)
{
    APP_MESSAGE_BLOCK msg;
    msg.mod_id = APP_MODUAL_ENVCHECKER;
    app_mailbox_put(&msg);
}

static int app_env_checker_process(APP_MESSAGE_BODY *msg_body)
{
    {
        a2dp_stream_t* sink_stream = app_tws_get_sink_stream();
        a2dp_stream_t* source_stream = app_tws_get_tws_source_stream();
        struct btdevice_volume *dev_vol_p = app_bt_stream_volume_get_ptr();
        int local_vol = app_bt_stream_local_volume_get();
        int msbc_sync_valid;
        uint16_t msbc_sync_offset;

        btif_remote_device_t *sink_remDev = NULL;
        btif_remote_device_t *source_remDev = NULL;
        struct AF_STREAM_CONFIG_T *stream_cfg = NULL;
        af_stream_get_cfg(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK, &stream_cfg, false);

        msbc_sync_valid = voicebtpcm_pcm_msbc_sync_offset_get_local(&msbc_sync_offset);

        sink_remDev =  btif_a2dp_get_remote_device(sink_stream);
        source_remDev = btif_a2dp_get_remote_device(source_stream);
#ifdef __ENABLE_IN_BOX_STATE_HANDLE__
        LOG_PRINT_ENV("inbox %d boxOpened %d connMobile:%x connTws:%x mode:%d roleproc:%d gainadj:%d ble:%d",
                app_is_in_charger_box(), !app_is_charger_box_closed(),
                CURRENT_MOBILE_DEV_CONN_STATE(), CURRENT_TWS_CONN_STATE(),
                app_tws_get_mode(), app_tws_is_role_switching_on(),
                btdrv_rf_rx_gain_adjust_getstatus(),
                app_tws_get_env()->state);
#else
        LOG_PRINT_ENV("connMobile:%x connTws:%x mode:%d roleproc:%d gainadj:%d ble:%d",
                CURRENT_MOBILE_DEV_CONN_STATE(), CURRENT_TWS_CONN_STATE(),
                app_tws_get_mode(), app_tws_is_role_switching_on(),
                btdrv_rf_rx_gain_adjust_getstatus(),
                app_tws_get_env()->state);
#endif

        if (sink_stream){
            LOG_PRINT_ENV("SINK STATE=%x", btif_a2dp_get_stream_state(sink_stream));
        }

        LOG_PRINT_ENV("sink:%08x %02x/%02x/%02x source:%08x %02x/%02x/%02x",
                                            sink_remDev,
                                            btif_me_get_remote_device_state(sink_remDev),
                                            btif_me_get_current_role(sink_remDev),
                                            btif_me_get_current_mode(sink_remDev),
                                            source_remDev,
                                            btif_me_get_remote_device_state(source_remDev),
                                            btif_me_get_current_role(source_remDev),
                                            btif_me_get_current_mode(source_remDev));

        LOG_PRINT_ENV("env: usefreq:%d/%d volume loc: %d, a2dp:%d hfp:%d real:%d msbc_offset:%d",
                                                                         hal_sysfreq_get(),
                                                                         hal_cmu_sys_get_freq(),
                                                                         local_vol,
                                                                         dev_vol_p?dev_vol_p->a2dp_vol:0xff,
                                                                         dev_vol_p?dev_vol_p->hfp_vol:0xff,
                                                                         stream_cfg?stream_cfg->vol:0xff,
                                                                         !msbc_sync_valid?msbc_sync_offset:0xff);
#if 0
                LOG_PRINT_ENV("MAX SLOT=%x", BT_DRIVER_GET_U32_REG_VAL(0xc0003df8));
                LOG_PRINT_ENV("MAX len=%x", BT_DRIVER_GET_U32_REG_VAL(0xc0003e04));
                if (ble_tws_is_initialized())
                {
                    TRACE("LMP STATUS 0x%x", *(uint32_t *)0xc0006484);
                    ASSERT(0 == *(uint32_t *)0xc0006484, "Capture the LMP worn-out case!!!");
                }
                TRACE("AFH0:");
                DUMP8("%02x ",(uint8_t *)0xc0004067,10);
                TRACE("\n");
                TRACE("AFH1:");
                DUMP8("%02x ",(uint8_t *)0xc0004071,10);
                TRACE("\n");
                TRACE("AFH2:");
                DUMP8("%02x ",(uint8_t *)0xc000407b,10);
                TRACE("\n");
                TRACE("BITOFF1=%x,CLKOFF1=%x",*(uint16_t *)0xD0210228,*(uint32_t *)0xD0210220);
                TRACE("BITOFF0=%x,CLKOFF0=%x",*(uint16_t *)0xD02101c8,*(uint32_t *)0xD02101c0);

                TRACE_LVL(HAL_TRACE_LEVEL_ENV, "diggain1=%x", BT_DRIVER_GET_U32_REG_VAL(0x403000f8));
                TRACE_LVL(HAL_TRACE_LEVEL_ENV, "diggain2=%x", BT_DRIVER_GET_U32_REG_VAL(0x403000fc));

                TRACE_LVL(HAL_TRACE_LEVEL_ENV, "hdr=%x", BT_DRIVER_GET_U32_REG_VAL(0xd02101b4));
                TRACE_LVL(HAL_TRACE_LEVEL_ENV, "hdr1=%x", BT_DRIVER_GET_U32_REG_VAL(0xd02101b4+96));

                
                uint16_t val;                
                uint32_t *buf_ptr;
                uint8_t buf_count=0;
                hal_analogif_reg_read(0x64, &val);
                TRACE_LVL(HAL_TRACE_LEVEL_ENV, "anagain=%x", val);

                TRACE_LVL(HAL_TRACE_LEVEL_ENV, "env: usefreq:%d/%d volume loc: %d, a2dp:%d hfp:%d real:%d",
                                                                                 hal_sysfreq_get(),
                                                                                 hal_cmu_sys_get_freq(),
                                                                                 local_vol,
                                                                                 dev_vol_p?dev_vol_p->a2dp_vol:0xff,
                                                                                 dev_vol_p?dev_vol_p->hfp_vol:0xff,
                                                                                 stream_cfg?stream_cfg->vol:0xff);

                TRACE_LVL(HAL_TRACE_LEVEL_ENV, "tx flow=%x",*(uint32_t *)0xc0003f9c);
                TRACE_LVL(HAL_TRACE_LEVEL_ENV, "remote_tx flow=%x",*(uint32_t *)0xc0003fa2);


                TRACE_LVL(HAL_TRACE_LEVEL_ENV, "tx buff = %x %x",*(uint16_t *)0xc0005d48,hci_current_left_tx_packets_left());
                buf_ptr = (uint32_t *)0xc0003af0;
                buf_count = 0;
                while(*buf_ptr)
                {
                    buf_count++;
                    buf_ptr = (uint32_t *)(*buf_ptr);
                }

                TRACE_LVL(HAL_TRACE_LEVEL_ENV, "rxbuff = %x %x",buf_count,hci_current_left_rx_packets_left());


                buf_ptr = (uint32_t *)0xc0003af4;
                buf_count = 0;
                while(*buf_ptr)
                {
                    buf_count++;
                    buf_ptr = (uint32_t *)(*buf_ptr);
                }
                TRACE_LVL(HAL_TRACE_LEVEL_ENV, "air rxbuff = %x",buf_count);
#endif
    }

    app_start_custom_function_in_bt_thread(0,
                0, (uint32_t)app_peridic_ble_activity_check);
    intersys_sleep_checker();
    osapi_notify_evm();
    return 0;
}

#define APP_ENV_CHECKER_INTERVAL_MS (7000)
osTimerDef (APP_ENV_CHECKER, app_env_checker_handler);
static osTimerId app_env_checker_timer = NULL;

int app_status_battery_report(uint8_t level)
{
    intersys_sleep_checker();
#if DEBUG_LOG_ENABLED||USER_STATISTIC_DATA_LOG_ENABLED
    log_update_time_stamp();
#endif

#ifdef __COWIN_V2_BOARD_ANC_SINGLE_MODE__
    if( anc_single_mode_on )//anc power on,anc only mode
        return 0;
#endif
#if defined(__BTIF_EARPHONE__)
  //  app_bt_state_checker();
    app_10_second_timer_check();
#endif
    if (level<=APP_BATTERY_LEVEL_LOWPOWERTHRESHOLD){
        //add something
    }
#if defined(SUPPORT_BATTERY_REPORT) || defined(SUPPORT_HF_INDICATORS)
    if(!app_tws_is_role_switching_on())
    {
        app_hfp_battery_report(level);
    }
#else
    LOG_PRINT_APPS("[%s] Can not enable HF_CUSTOM_FEATURE_BATTERY_REPORT",__func__);
#endif
    return 0;
}

#ifdef MEDIA_PLAYER_SUPPORT

void app_status_set_num(const char* p)
{
    media_Set_IncomingNumber(p);
}

#if defined(HALFDUPLEXUART)
void usr_set_gpio(enum HAL_GPIO_PIN_T io_pin, bool set_flag)
{
    struct HAL_IOMUX_PIN_FUNCTION_MAP pinmux_usr_set_gpio[1] = {
        {(HAL_IOMUX_PIN_T)io_pin, HAL_IOMUX_FUNC_AS_GPIO, HAL_IOMUX_PIN_VOLTAGE_VIO, HAL_IOMUX_PIN_PULLUP_ENALBE},
    };

    hal_iomux_init(pinmux_usr_set_gpio, ARRAY_SIZE(pinmux_usr_set_gpio));
    hal_gpio_pin_set_dir(io_pin, HAL_GPIO_DIR_IN, 1);
    if(set_flag)
        hal_gpio_pin_set(io_pin);
    else
        hal_gpio_pin_clr(io_pin);
}
#endif

extern "C" int app_voice_report(APP_STATUS_INDICATION_T status,uint8_t device_id)
{
#if defined(BTUSB_AUDIO_MODE)
    if(app_usbaudio_mode_on()) return 0;
#endif
    LOG_PRINT_APPS("%s %d",__func__, status);
    AUD_ID_ENUM id = MAX_RECORD_NUM;
#ifdef __COWIN_V2_BOARD_ANC_SINGLE_MODE__
    if(anc_single_mode_on)
        return 0;
#endif
    switch (status) {
        case APP_STATUS_INDICATION_POWERON:
            id = AUD_ID_POWER_ON;
            break;
        case APP_STATUS_INDICATION_POWEROFF:
            id = AUD_ID_POWER_OFF;
            break;
        case APP_STATUS_INDICATION_CONNECTED:
            id = AUD_ID_BT_CONNECTED;
            break;
        case APP_STATUS_INDICATION_DISCONNECTED:
            id = AUD_ID_BT_DIS_CONNECT;
            break;
        case APP_STATUS_INDICATION_CALLNUMBER:
            id = AUD_ID_BT_CALL_INCOMING_NUMBER;
            break;
        case APP_STATUS_INDICATION_CHARGENEED:
            id = AUD_ID_BT_CHARGE_PLEASE;
            break;
        case APP_STATUS_INDICATION_FULLCHARGE:
            id = AUD_ID_BT_CHARGE_FINISH;
            break;
        case APP_STATUS_INDICATION_PAIRSUCCEED:
            id = AUD_ID_BT_PAIRING_SUC;
            break;
        case APP_STATUS_INDICATION_PAIRFAIL:
            id = AUD_ID_BT_PAIRING_FAIL;
            break;

        case APP_STATUS_INDICATION_HANGUPCALL:
            id = AUD_ID_BT_CALL_HUNG_UP;
            break;

        case APP_STATUS_INDICATION_REFUSECALL:
            id = AUD_ID_BT_CALL_REFUSE;
            break;

        case APP_STATUS_INDICATION_ANSWERCALL:
            id = AUD_ID_BT_CALL_ANSWER;
            break;

        case APP_STATUS_INDICATION_CLEARSUCCEED:
            id = AUD_ID_BT_CLEAR_SUCCESS;
            break;

        case APP_STATUS_INDICATION_CLEARFAIL:
            id = AUD_ID_BT_CLEAR_FAIL;
            break;
        case APP_STATUS_INDICATION_INCOMINGCALL:
            id = AUD_ID_BT_CALL_INCOMING_CALL;
            break;
        case APP_STATUS_INDICATION_BOTHSCAN:
            id = AUD_ID_BT_PAIR_ENABLE;
            break;
        case APP_STATUS_INDICATION_WARNING:
            id = AUD_ID_BT_WARNING;
            break;
#ifdef __TWS__
        case APP_STATUS_INDICATION_TWS_ISMASTER:
            id = AUD_ID_BT_TWS_ISMASTER;
            break;
        case APP_STATUS_INDICATION_TWS_ISSLAVE:
            id = AUD_ID_BT_TWS_ISSLAVE;
            break;
        case APP_STATUS_INDICATION_TWS_SEARCH:
            id = AUD_ID_BT_TWS_SEARCH;
            break;
        case APP_STATUS_INDICATION_TWS_STOPSEARCH:
            id = AUD_ID_BT_TWS_STOPSEARCH;
            break;
        case APP_STATUS_INDICATION_TWS_DISCOVERABLE:
            id = AUD_ID_BT_TWS_DISCOVERABLE;
            break;
        case APP_STATUS_INDICATION_TWS_LEFTCHNL:
            id = AUD_ID_BT_TWS_LEFTCHNL;
            break;
        case APP_STATUS_INDICATION_TWS_RIGHTCHNL:
            id = AUD_ID_BT_TWS_RIGHTCHNL;
            break;
#endif



        default:
            break;
    }

    if ((app_system_status_get() == APP_SYSTEM_STATUS_POWEROFF_PROC)&&
        (status == APP_STATUS_INDICATION_BOTHSCAN    ||
         status == APP_STATUS_INDICATION_DISCONNECTED||
         status == APP_STATUS_INDICATION_CHARGENEED  ||
         status == APP_STATUS_INDICATION_CONNECTING  ||
         status == APP_STATUS_INDICATION_CONNECTED)){
        //block ring tong
    }else{
        media_PlayAudio(id, device_id);
    }
    return 0;
}
#endif


#define POWERON_PRESSMAXTIME_THRESHOLD_MS  (5000)

enum APP_POWERON_CASE_T {
    APP_POWERON_CASE_NORMAL = 0,
    APP_POWERON_CASE_DITHERING,
    APP_POWERON_CASE_REBOOT,
    APP_POWERON_CASE_ALARM,
    APP_POWERON_CASE_CALIB,
    APP_POWERON_CASE_BOTHSCAN,
    APP_POWERON_CASE_CHARGING,
    APP_POWERON_CASE_FACTORY,
    APP_POWERON_CASE_TEST,
    APP_POWERON_CASE_INVALID,

    APP_POWERON_CASE_NUM
};


static osThreadId apps_init_tid = NULL;

static enum APP_POWERON_CASE_T pwron_case = APP_POWERON_CASE_INVALID;

static void app_poweron_normal(APP_KEY_STATUS *status, void *param)
{
    LOG_PRINT_APPS("%s %d,%d",__func__, status->code, status->event);
    pwron_case = APP_POWERON_CASE_NORMAL;

    osSignalSet(apps_init_tid, 0x2);
}

static void app_poweron_scan(APP_KEY_STATUS *status, void *param)
{
    LOG_PRINT_APPS("%s %d,%d",__func__, status->code, status->event);
    pwron_case = APP_POWERON_CASE_BOTHSCAN;

    osSignalSet(apps_init_tid, 0x2);
}

#ifdef __ENGINEER_MODE_SUPPORT__
static void app_poweron_factorymode(APP_KEY_STATUS *status, void *param)
{
    LOG_PRINT_APPS("%s %d,%d",__func__, status->code, status->event);
    hal_sw_bootmode_clear(HAL_SW_BOOTMODE_REBOOT);
    app_factorymode_enter();
}
#endif

static void app_poweron_finished(APP_KEY_STATUS *status, void *param)
{
    LOG_PRINT_APPS("%s %d,%d",__func__, status->code, status->event);
    osSignalSet(apps_init_tid, 0x2);
}

void app_poweron_wait_finished(void)
{
    osSignalWait(0x2, osWaitForever);
}

#if  defined(__POWERKEY_CTRL_ONOFF_ONLY__)
void app_bt_key_shutdown(APP_KEY_STATUS *status, void *param);
const  APP_KEY_HANDLE  pwron_key_handle_cfg[] = {
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_UP},           "power on: shutdown"     , app_bt_key_shutdown, NULL},
};
#elif defined(__ENGINEER_MODE_SUPPORT__)
const  APP_KEY_HANDLE  pwron_key_handle_cfg[] = {
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_INITUP},           "power on: normal"     , app_poweron_normal, NULL},
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_INITLONGPRESS},    "power on: both scan"  , app_poweron_scan  , NULL},
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_INITLONGLONGPRESS},"power on: factory mode", app_poweron_factorymode  , NULL},
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_INITFINISHED},     "power on: finished"   , app_poweron_finished  , NULL},
};
#else
const  APP_KEY_HANDLE  pwron_key_handle_cfg[] = {
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_INITUP},           "power on: normal"     , app_poweron_normal, NULL},
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_INITLONGPRESS},    "power on: both scan"  , app_poweron_scan  , NULL},
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_INITFINISHED},     "power on: finished"   , app_poweron_finished  , NULL},
};
#endif

#ifndef APP_TEST_MODE
static void app_poweron_key_init(void)
{
    uint8_t i = 0;
    LOG_PRINT_APPS("%s",__func__);

    for (i=0; i<(sizeof(pwron_key_handle_cfg)/sizeof(APP_KEY_HANDLE)); i++){
        app_key_handle_registration(&pwron_key_handle_cfg[i]);
    }
}

static uint8_t app_poweron_wait_case(void)
{
    uint32_t stime, etime;

#ifdef __POWERKEY_CTRL_ONOFF_ONLY__
    pwron_case = APP_POWERON_CASE_NORMAL;
#else
    pwron_case = APP_POWERON_CASE_INVALID;

    stime = hal_sys_timer_get();
    osSignalWait(0x2, POWERON_PRESSMAXTIME_THRESHOLD_MS);
    etime = hal_sys_timer_get();
    LOG_PRINT_APPS("powon raw case:%d time:%d", pwron_case, TICKS_TO_MS(etime - stime));
#endif
    return pwron_case;

}
#endif

extern "C" int system_shutdown(void);
int app_shutdown(void)
{
    system_shutdown();
    return 0;
}

int system_reset(void);
int app_reset(void)
{
    system_reset();
    return 0;
}

extern "C" void app_notify_stack_ready()
{
    TRACE("app_notify_stack_ready");
    osSignalSet(apps_init_tid, 0x3);
}
static void app_wait_stack_ready(void)
{
    uint32_t stime, etime;
    stime = hal_sys_timer_get();
    osSignalWait(0x3, 1000);
    etime = hal_sys_timer_get();
    TRACE("app_wait_stack_ready: wait:%d ms", TICKS_TO_MS(etime - stime));
}

static void app_postponed_reset_timer_handler(void const *param);
osTimerDef(APP_POSTPONED_RESET_TIMER, app_postponed_reset_timer_handler);
static osTimerId app_postponed_reset_timer = NULL;
#define APP_RESET_PONTPONED_TIME_IN_MS	2000
static void app_postponed_reset_timer_handler(void const *param)
{
	hal_sw_bootmode_set(HAL_SW_BOOTMODE_ENTER_HIDE_BOOT);
	system_reset();
}

void app_start_postponed_reset(void)
{
	if (NULL == app_postponed_reset_timer)
	{
		osTimerCreate(osTimer(APP_POSTPONED_RESET_TIMER), osTimerOnce, NULL);	
	}

	osTimerStart(app_postponed_reset_timer, APP_RESET_PONTPONED_TIME_IN_MS);
}


void app_bt_key_shutdown(APP_KEY_STATUS *status, void *param)
{
    LOG_PRINT_APPS("%s %d,%d",__func__, status->code, status->event);
#ifdef __POWERKEY_CTRL_ONOFF_ONLY__
    hal_sw_bootmode_clear(HAL_SW_BOOTMODE_REBOOT);
    app_reset();
#else
    app_shutdown();
#endif
}

void app_bt_key_enter_testmode(APP_KEY_STATUS *status, void *param)
{
    LOG_PRINT_APPS("%s\n",__FUNCTION__);

    if(app_status_indication_get() == APP_STATUS_INDICATION_BOTHSCAN){
#ifdef __FACTORY_MODE_SUPPORT__
        app_factorymode_bt_signalingtest(status, param);
#endif
    }
}

void app_bt_key_enter_nosignal_mode(APP_KEY_STATUS *status, void *param)
{
    LOG_PRINT_APPS("%s\n",__FUNCTION__);
    if(app_status_indication_get() == APP_STATUS_INDICATION_BOTHSCAN){
#ifdef __FACTORY_MODE_SUPPORT__
        app_factorymode_bt_nosignalingtest(status, param);
#endif
    }
}


void app_bt_singleclick(APP_KEY_STATUS *status, void *param)
{
    LOG_PRINT_APPS("%s %d,%d",__func__, status->code, status->event);
}

void app_bt_doubleclick(APP_KEY_STATUS *status, void *param)
{
    LOG_PRINT_APPS("%s %d,%d",__func__, status->code, status->event);
}

void app_power_off(APP_KEY_STATUS *status, void *param)
{
    LOG_PRINT_APPS("app_power_off\n");
}

#ifdef _BT_TALK_THROU_SWITCH_
void app_talk_thru_key(APP_KEY_STATUS *status, void *param)
{
//
}
#endif

extern "C" void OS_NotifyEvm(void);

#define PRESS_KEY_TO_ENTER_OTA_INTERVEL    (15000)          // press key 15s enter to ota
#define PRESS_KEY_TO_ENTER_OTA_REPEAT_CNT    ((PRESS_KEY_TO_ENTER_OTA_INTERVEL - 2000) / 500)
void app_otaMode_enter(APP_KEY_STATUS *status, void *param)
{
    LOG_PRINT_APPS("%s",__func__);
    bt_drv_rf_reset();
    hal_sw_bootmode_set(HAL_SW_BOOTMODE_ENTER_HIDE_BOOT);
#ifdef __KMATE106__
    app_status_indication_set(APP_STATUS_INDICATION_OTA);
    app_voice_report(APP_STATUS_INDICATION_WARNING, 0);
    osDelay(1200);
#endif
    hal_cmu_sys_reboot();
}

void app_ota_key_handler(APP_KEY_STATUS *status, void *param)
{
    static uint32_t time = hal_sys_timer_get();
    static uint16_t cnt = 0;

    LOG_PRINT_APPS("%s %d,%d",__func__, status->code, status->event);

    if (TICKS_TO_MS(hal_sys_timer_get() - time) > 600) // 600 = (repeat key intervel)500 + (margin)100
        cnt = 0;
    else
        cnt++;

    if (cnt == PRESS_KEY_TO_ENTER_OTA_REPEAT_CNT) {
        app_otaMode_enter(NULL, NULL);
    }

    time = hal_sys_timer_get();
}

#if (_GFPS_)
extern "C" void app_enter_fastpairing_mode(void);
#endif

void app_tws_simulate_pairing(void);
void app_tws_freeman_simulate_pairing(void);
void app_tws_req_set_lbrt(uint8_t en, uint8_t initial_req, uint32_t ticks);
uint8_t iic_mask=0;
extern "C" void pmu_sleep_en(unsigned char sleep_en);
void app_bt_key_simulation(APP_KEY_STATUS *status, void *param)
{
    LOG_PRINT_APPS("Get key event %d", status->event);
    switch(status->event)
    {
        case APP_KEY_EVENT_CLICK:
#ifdef __BT_ONE_BRING_TWO__
            if (MEC(activeCons)< 2) {
                app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, BAM_GENERAL_ACCESSIBLE,0,0);
            }
#else
#ifdef LBRT         
            app_tws_req_set_lbrt(0,1,bt_syn_get_curr_ticks(app_tws_get_tws_conhdl()));
#else
            app_start_custom_function_in_bt_thread(RESTORE_THE_ORIGINAL_PLAYING_STATE,
                    0, (uint32_t)app_tws_kickoff_switching_role);
#endif
#endif
            //bt_sco_player_restart_requeset(1);
            //app_tws_charger_box_opened();
            break;
        case APP_KEY_EVENT_DOUBLECLICK:
#if 0
            if(iic_mask ==0)
            {
                iic_mask=1;
                hal_iomux_set_analog_i2c();

            }
            else if(iic_mask == 1)
            {
                pmu_sleep_en(0);
                iic_mask = 2;
            }
            else
            {
                iic_mask=0;
                hal_iomux_set_uart0();
                pmu_sleep_en(1);
            }
            return;
#endif

#ifdef LBRT
            app_tws_req_set_lbrt(1,1,bt_syn_get_curr_ticks(app_tws_get_tws_conhdl()));
#else
            app_tws_freeman_simulate_pairing();
#endif
            break;
        case APP_KEY_EVENT_TRIPLECLICK:
            clear_dump_log();
            break;
        case APP_KEY_EVENT_LONGPRESS:
            app_tws_simulate_pairing();
            break;
        default:
            break;
    }
}

void app_bt_key_switch_role(APP_KEY_STATUS *status, void *param)
{
#ifdef IAG_BLE_INCLUDE
    LOG_PRINT_APPS("Get key event %d", status->event);
    if (APP_KEY_CODE_FN1 == status->code)
    {
        switch(status->event)
        {
#ifdef __ENABLE_IN_BOX_STATE_HANDLE__
            case APP_KEY_EVENT_CLICK:
                LOG_PRINT_APPS("open the chargerbox.");
                app_tws_charger_box_opened();
				break;
            case APP_KEY_EVENT_DOUBLECLICK:
				LOG_PRINT_APPS("Put out of the chargerbox.");
				app_tws_out_of_charger_box();
				break;
#endif
            case APP_KEY_EVENT_TRIPLECLICK:
                break;
            default:
                break;
        }
    }
    else if (APP_KEY_CODE_FN2 == status->code)
    {
        switch(status->event)
        {
            case APP_KEY_EVENT_CLICK:
#ifdef __ENABLE_IN_BOX_STATE_HANDLE__
                LOG_PRINT_APPS("Put into the chargerbox.");
                app_tws_put_in_charger_box();
#endif
                break;
            case APP_KEY_EVENT_DOUBLECLICK:
            {
#ifdef __ENABLE_WEAR_STATUS_DETECT__
                static uint8_t isWeared = false;
                app_tws_update_wear_status(!isWeared);
                isWeared = !isWeared;
#endif
                break;
            }
            default:
                break;
        }
    }
#endif
}

void app_bt_key(APP_KEY_STATUS *status, void *param)
{
    LOG_PRINT_APPS("%s %d,%d",__func__, status->code, status->event);
    switch(status->event)
    {
        case APP_KEY_EVENT_CLICK:
            LOG_PRINT_APPS("first blood!");
            //hal_sw_bootmode_set(HAL_SW_BOOTMODE_FLASH_BOOT);
            //hal_cmu_sys_reboot();
            break;
        case APP_KEY_EVENT_DOUBLECLICK:
            LOG_PRINT_APPS("double kill");
            break;
        case APP_KEY_EVENT_TRIPLECLICK:
            LOG_PRINT_APPS("triple kill");
            break;
        case APP_KEY_EVENT_ULTRACLICK:
            LOG_PRINT_APPS("ultra kill");
            break;
        case APP_KEY_EVENT_RAMPAGECLICK:
            LOG_PRINT_APPS("rampage kill!you are crazy!");
            break;
    }
#if defined( __FACTORY_MODE_SUPPORT__) && !defined(__TWS__)
    if (app_status_indication_get() == APP_STATUS_INDICATION_BOTHSCAN && (status->event == APP_KEY_EVENT_DOUBLECLICK)){
        app_factorymode_languageswitch_proc();
    }else
#endif
    {
#ifdef __COWIN_V2_BOARD_ANC_SINGLE_MODE__
    if(!anc_single_mode_on)
#endif
        bt_key_send(status->code, status->event);
    }
}

#ifdef __POWERKEY_CTRL_ONOFF_ONLY__
#if defined(__APP_KEY_FN_STYLE_A__)
const APP_KEY_HANDLE  app_key_handle_cfg[] = {
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_UP},"bt function key",app_bt_key_shutdown, NULL},
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_LONGPRESS},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_UP},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_DOUBLECLICK},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN2,APP_KEY_EVENT_UP},"bt volume up key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN2,APP_KEY_EVENT_LONGPRESS},"bt play backward key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN3,APP_KEY_EVENT_UP},"bt volume down key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN3,APP_KEY_EVENT_LONGPRESS},"bt play forward key",app_bt_key, NULL},
#ifdef SUPPORT_SIRI
    {{APP_KEY_CODE_NONE ,APP_KEY_EVENT_NONE},"none function key",app_bt_key, NULL},
#endif

};
#else //#elif defined(__APP_KEY_FN_STYLE_B__)
const APP_KEY_HANDLE  app_key_handle_cfg[] = {
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_UP},"bt function key",app_bt_key_shutdown, NULL},
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_LONGPRESS},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_UP},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_DOUBLECLICK},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN2,APP_KEY_EVENT_REPEAT},"bt volume up key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN2,APP_KEY_EVENT_UP},"bt play backward key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN3,APP_KEY_EVENT_REPEAT},"bt volume down key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN3,APP_KEY_EVENT_UP},"bt play forward key",app_bt_key, NULL},
#ifdef SUPPORT_SIRI
    {{APP_KEY_CODE_NONE ,APP_KEY_EVENT_NONE},"none function key",app_bt_key, NULL},
#endif

};
#endif
#else
#if defined(__APP_KEY_FN_STYLE_A__)
const APP_KEY_HANDLE  app_key_handle_cfg[] = {
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_LONGLONGPRESS},"bt function key",app_bt_key_shutdown, NULL},
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_CLICK},"bt function key",app_bt_key_switch_role, NULL},
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_DOUBLECLICK},"bt function key",app_bt_key_switch_role, NULL},
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_TRIPLECLICK},"bt function key",app_bt_key_switch_role, NULL},
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_LONGPRESS},"bt function key",app_bt_key_switch_role, NULL},

    {{APP_KEY_CODE_FN2,APP_KEY_EVENT_CLICK},"bt function key",app_bt_key_switch_role, NULL},
    {{APP_KEY_CODE_FN2,APP_KEY_EVENT_DOUBLECLICK},"bt function key",app_bt_key_switch_role, NULL},
    {{APP_KEY_CODE_FN2,APP_KEY_EVENT_LONGPRESS},"bt function key",app_bt_key_switch_role, NULL},

    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_DOUBLECLICK},"bt function key",app_bt_key_simulation, NULL},


    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_ULTRACLICK},"bt function key",app_bt_key_simulation, NULL},
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_TRIPLECLICK},"bt function key",app_bt_key_simulation, NULL},
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_LONGPRESS},"bt function key",app_bt_key_simulation, NULL},
#ifdef SUPPORT_SIRI
    {{APP_KEY_CODE_NONE ,APP_KEY_EVENT_NONE},"none function key",app_bt_key, NULL},
#endif
#ifdef __BT_ANC_KEY__
     {{APP_KEY_CODE_PWR,APP_KEY_EVENT_CLICK},"bt anc key",app_anc_key, NULL},
#else
     {{APP_KEY_CODE_PWR,APP_KEY_EVENT_CLICK},"bt function key",app_bt_key_simulation, NULL},
#endif
#if (VOICE_DATAPATH)
    {{APP_KEY_CODE_GOOGLE, APP_KEY_EVENT_DOWN}, "google assistant key", app_voicepath_key, NULL},
    {{APP_KEY_CODE_GOOGLE, APP_KEY_EVENT_UP}, "google assistant key", app_voicepath_key, NULL},
    {{APP_KEY_CODE_GOOGLE, APP_KEY_EVENT_LONGPRESS}, "google assistant key", app_voicepath_key, NULL},
    {{APP_KEY_CODE_GOOGLE, APP_KEY_EVENT_CLICK}, "google assistant key", app_voicepath_key, NULL},
    {{APP_KEY_CODE_GOOGLE, APP_KEY_EVENT_DOUBLECLICK}, "google assistant key", app_voicepath_key, NULL},
#endif
};
#else //#elif defined(__APP_KEY_FN_STYLE_B__)
const APP_KEY_HANDLE  app_key_handle_cfg[] = {
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_LONGLONGPRESS},"bt function key",app_bt_key_shutdown, NULL},
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_LONGPRESS},"bt function key",app_bt_key, NULL},
#ifdef __BT_ANC_KEY__
     {{APP_KEY_CODE_PWR,APP_KEY_EVENT_CLICK},"bt anc key",app_anc_key, NULL},
#else
     {{APP_KEY_CODE_PWR,APP_KEY_EVENT_CLICK},"bt function key",app_bt_key, NULL},
#endif    
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_DOUBLECLICK},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_TRIPLECLICK},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_ULTRACLICK},"bt function key",app_bt_key_enter_nosignal_mode, NULL},
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_RAMPAGECLICK},"bt function key",app_bt_key_enter_testmode, NULL},
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_LONGPRESS},"bt enter freeman key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_REPEAT},"bt volume up key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_UP},"bt play backward key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN2,APP_KEY_EVENT_REPEAT},"bt volume down key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN2,APP_KEY_EVENT_UP},"bt play forward key",app_bt_key, NULL},
#ifdef SUPPORT_SIRI
    {{APP_KEY_CODE_NONE ,APP_KEY_EVENT_NONE},"none function key",app_bt_key, NULL},
#endif
#if (VOICE_DATAPATH)
    {{APP_KEY_CODE_GOOGLE, APP_KEY_EVENT_DOWN}, "google assistant key", app_voicepath_key, NULL},
    {{APP_KEY_CODE_GOOGLE, APP_KEY_EVENT_UP}, "google assistant key", app_voicepath_key, NULL},
    {{APP_KEY_CODE_GOOGLE, APP_KEY_EVENT_LONGPRESS}, "google assistant key", app_voicepath_key, NULL},
    {{APP_KEY_CODE_GOOGLE, APP_KEY_EVENT_CLICK}, "google assistant key", app_voicepath_key, NULL},
    {{APP_KEY_CODE_GOOGLE, APP_KEY_EVENT_DOUBLECLICK}, "google assistant key", app_voicepath_key, NULL},
#endif

};
#endif
#endif

void app_key_init(void)
{
    uint8_t i = 0;
    LOG_PRINT_APPS("%s",__func__);
    for (i=0; i<(sizeof(app_key_handle_cfg)/sizeof(APP_KEY_HANDLE)); i++){
        app_key_handle_registration(&app_key_handle_cfg[i]);
    }
}
void app_key_init_on_charging(void)
{
    uint8_t i = 0;
    const APP_KEY_HANDLE  key_cfg[] = {
        {{APP_KEY_CODE_PWR,APP_KEY_EVENT_REPEAT},"ota function key",app_ota_key_handler, NULL},
    };

    LOG_PRINT_APPS("%s",__func__);
    for (i=0; i<(sizeof(key_cfg)/sizeof(APP_KEY_HANDLE)); i++){
        app_key_handle_registration(&key_cfg[i]);
    }
}

#ifdef __BTIF_AUTOPOWEROFF__


#ifdef __TWS__
extern  tws_dev_t  tws;
#endif


void CloseEarphone(void)
{
    int activeCons;
    osapi_lock_stack();
    activeCons =  btif_me_get_activeCons();
    osapi_unlock_stack();

#ifdef ANC_APP
    if(app_anc_work_status()) {
        app_poweroff_timer.timer_en = 1;
        app_poweroff_timer.timer_period = 30;
        return;
    }
#endif
#ifdef __TWS__
    if(activeCons == 0 || ((tws.tws_source.connected == false) && \
        (app_tws_get_conn_state() == TWS_MASTER_CONN_SLAVE)))
#else
    if(activeCons == 0)
#endif
    {
      LOG_PRINT_APPS("!!!CloseEarphone\n");
      app_shutdown();
    }
}
#endif /* __AUTOPOWEROFF__ */


void a2dp_suspend_music_force(void);
extern uint8_t app_tws_auto_poweroff;
uint8_t app_poweroff_flag=0;
int app_deinit(int deinit_case)
{
    int nRet = 0;
    LOG_PRINT_APPS("%s case:%d",__func__, deinit_case);

#if defined(BTUSB_AUDIO_MODE)
    if(app_usbaudio_mode_on()) return 0;
#endif
    if (!deinit_case){
    app_poweroff_flag = 1;
    app_status_indication_filter_set(APP_STATUS_INDICATION_BOTHSCAN);
    app_status_indication_set(APP_STATUS_INDICATION_POWEROFF);

    a2dp_suspend_music_force();
    osDelay(200);
#ifndef FPGA
    nv_record_flash_flush();
#endif

    dump_whole_logs_in_normal_case();

    if(  btif_me_get_activeCons())
    {
        conn_bt_disconnect_all();
        osDelay(2000);
    }
#ifdef __TWS__
    if(app_tws_auto_poweroff == 0){
        app_tws_disconnect_slave();
    }
#endif

    osDelay(2000);
#ifdef MEDIA_PLAYER_SUPPORT
    app_voice_report(APP_STATUS_INDICATION_POWEROFF, 0);
#endif
    osDelay(2000);
    af_close();
    app_poweroff_flag = 0;
    }

    return nRet;
}

#ifdef APP_TEST_MODE
extern void app_test_init(void);
int app_init(void)
{
    int nRet = 0;
    uint8_t pwron_case = APP_POWERON_CASE_INVALID;
    LOG_PRINT_APPS("%s",__func__);
    app_poweroff_flag = 0;

    apps_init_tid = osThreadGetId();
    app_sysfreq_req(APP_SYSFREQ_USER_APP_0, APP_SYSFREQ_52M);
    list_init();
    af_open();
    app_os_init();
    app_pwl_open();
    app_audio_open();
    app_audio_manager_open();
    app_overlay_open();
    if (app_key_open(true))
    {
        nRet = -1;
        goto exit;
    }

    app_test_init();
exit:
    app_sysfreq_req(APP_SYSFREQ_USER_APP_0, APP_SYSFREQ_32K);
    return nRet;
}
#else
#define NVRAM_ENV_FACTORY_TESTER_STATUS_TEST_PASS (0xffffaa55)
int app_bt_connect2tester_init(void)
{
    btif_device_record_t rec;
    bt_bdaddr_t  tester_addr;
    uint8_t i;
    bool find_tester = false;
    struct nvrecord_env_t *nvrecord_env;
    btdevice_profile *btdevice_plf_p;
    factory_section_data_t *factory_section_data = factory_section_data_ptr_get();

    nv_record_env_get(&nvrecord_env);

    if (nvrecord_env->factory_tester_status.status != NVRAM_ENV_FACTORY_TESTER_STATUS_DEFAULT)
        return 0;

    if (factory_section_data){
        memcpy(tester_addr.address, factory_section_data->bt_address, BTIF_BD_ADDR_SIZE);
        nv_record_open(section_usrdata_ddbrecord);
        for (i = 0; nv_record_enum_dev_records(i, &rec) == BT_STS_SUCCESS; i++) {
            if (!memcmp(rec.bdAddr.address, tester_addr.address, BTIF_BD_ADDR_SIZE)){
                find_tester = true;
            }
        }
        if(i==0 && !find_tester){
            memset(&rec, 0, sizeof(btif_device_record_t));
            memcpy(rec.bdAddr.address, tester_addr.address, BTIF_BD_ADDR_SIZE);
            nv_record_add(section_usrdata_ddbrecord, &rec);
            btdevice_plf_p = (btdevice_profile *)app_bt_profile_active_store_ptr_get(rec.bdAddr.address);
            btdevice_plf_p->hfp_act = true;
            btdevice_plf_p->a2dp_act = true;
        }
        if (find_tester && i>2){
            nv_record_ddbrec_delete(&tester_addr);
            nvrecord_env->factory_tester_status.status = NVRAM_ENV_FACTORY_TESTER_STATUS_TEST_PASS;
            nv_record_env_set(nvrecord_env);
        }
    }

    return 0;
}

int app_nvrecord_rebuild(void)
{
    struct nvrecord_env_t *nvrecord_env;
    nv_record_env_get(&nvrecord_env);

    nv_record_sector_clear();
    nv_record_env_init();
    nvrecord_env->factory_tester_status.status = NVRAM_ENV_FACTORY_TESTER_STATUS_TEST_PASS;
    nv_record_env_set(nvrecord_env);
    nv_record_flash_flush();

    return 0;
}

#ifdef __TWS__
extern void app_tws_start_reconnct(struct tws_mode_t *tws_mode);
extern uint16_t app_tws_delay_count;
#endif

void bt_change_to_jlink(APP_KEY_STATUS *status, void *param);
void bt_enable_tports(void);

#if defined(BTUSB_AUDIO_MODE)
#include "app_audio.h"
#include "usb_audio_frm_defs.h"
#include "usb_audio_app.h"

static bool app_usbaudio_mode = false;

extern "C" void btusbaudio_entry(void);
void app_usbaudio_entry(void)
{
    app_usbaudio_mode = true ;

    btusbaudio_entry();
}

bool app_usbaudio_mode_on(void)
{
    return app_usbaudio_mode;
}

void app_usb_key(APP_KEY_STATUS *status, void *param)
{
    LOG_PRINT_APPS("%s %d,%d",__func__, status->code, status->event);

}

const APP_KEY_HANDLE  app_usb_handle_cfg[] = {
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_UP},"USB HID FN1 UP key",app_usb_key, NULL},
    {{APP_KEY_CODE_FN2,APP_KEY_EVENT_UP},"USB HID FN2 UP key",app_usb_key, NULL},
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_UP},"USB HID PWR UP key",app_usb_key, NULL},
};

void app_usb_key_init(void)
{
    uint8_t i = 0;
    LOG_PRINT_APPS("%s",__func__);
    for (i=0; i<(sizeof(app_usb_handle_cfg)/sizeof(APP_KEY_HANDLE)); i++){
        app_key_handle_registration(&app_usb_handle_cfg[i]);
    }
}

#endif
extern void app_tws_start_pairing_in_chargerbox();
void app_ble_tws_initialized_callback_handler(void)
{
    // TODO: add the handlings executed when the BLE tws initialized is done

#if BLE_TWS_USE_BLE_AS_INBOX_COMMUNICATION_PATH
    if (app_tws_ui_is_in_repairing())
    {
        app_tws_start_pairing_in_chargerbox();
    }
#endif

}

void app_tws_config_bt_address(void)
{
    if (app_tws_is_master_mode()){
        memcpy(bt_addr, tws_mode_ptr()->masterAddr.address, 6);
    }else if (app_tws_is_slave_mode()){
        memcpy(bt_addr, tws_mode_ptr()->slaveAddr.address, 6);
    }else if (app_tws_is_freeman_mode()){
        //do noting
    }else{
        ASSERT(0, "INVALID TWS MODE:%d", app_tws_get_mode());
    }
    LOG_PRINT_APPS("%s tws_mode:%d ",__func__, app_tws_get_mode());
    LOG_PRINT_APPS_DUMP8("%02x ", bt_addr, 6);
}
void app_tws_config_bt_mode_after_btroleswitch(void)
{
#if 0
    uint8_t twsMode=app_tws_get_mode();
    if((twsMode!=app_tws_get_originalmode())&&(twsMode!=TWSFREE))
    {
        TRACE("recovery tws originalmode !");
        twsMode=app_tws_get_originalmode();
        nv_record_update_tws_mode(twsMode);        
    }
#else
    uint8_t twsMode=app_tws_get_mode();
    if(twsMode !=TWSFREE)
    {
        if(!memcmp(bt_addr,tws_mode_ptr()->masterAddr.address,6))
        {
            nv_record_update_tws_mode(TWSMASTER);
        }
        if(!memcmp(bt_addr,tws_mode_ptr()->slaveAddr.address,6))
        {
            nv_record_update_tws_mode(TWSSLAVE);
        }
    }
#endif
}
static void app_config_ble_tws(void)
{
    BLE_TWS_CONFIG_T tBleTwsConfig;
#ifdef __LEFT_SIDE__
    tBleTwsConfig.earSide = TWS_LEFT_SIDE;
#else
    tBleTwsConfig.earSide = TWS_RIGHT_SIDE;
#endif
    tBleTwsConfig.earSideAsMasterByDefault = TWS_RIGHT_SIDE;
    tBleTwsConfig.rssiThreshold = TWS_BOX_RSSI_THRESHOLD;
    tBleTwsConfig.ble_tws_initialized_cb = app_ble_tws_initialized_callback_handler;

    ble_tws_config(&tBleTwsConfig);
}

#if OS_HAS_CPU_STAT
extern "C" void rtx_show_all_threads_usage(void);
#define CPU_USAGE_TIMER_TMO_VALUE (_CPU_STATISTICS_PEROID_/3)
static void cpu_usage_timer_handler(void const *param);
osTimerDef(cpu_usage_timer, cpu_usage_timer_handler);
static osTimerId cpu_usage_timer_id = NULL;
static void cpu_usage_timer_handler(void const *param)
{
    rtx_show_all_threads_usage();
}
#endif
#ifdef __TWS_PAIR_DIRECTLY__
extern "C" void app_bt_start_pairing(uint32_t pairingMode, uint8_t * pMasterAddr, uint8_t * SlaveAddr);
#endif

int app_init(void)
{
    int nRet = 0;
    struct nvrecord_env_t *nvrecord_env;
    bool need_check_key = true;
    bool isInCharging = false;
    uint8_t pwron_case = APP_POWERON_CASE_INVALID;
    LOG_PRINT_APPS("app_init\n");
#ifdef __WATCHER_DOG_RESET__
    app_wdt_open(15);
#endif
#ifdef ANC_APP
    app_anc_ios_init();
#endif
    app_sysfreq_req(APP_SYSFREQ_USER_APP_0, APP_SYSFREQ_104M);
#if defined(MCU_HIGH_PERFORMANCE_MODE)
    LOG_PRINT_APPS("sys freq calc : %d\n", hal_sys_timer_calc_cpu_freq(100, 0));
#endif
    apps_init_tid = osThreadGetId();
    list_init();
    app_os_init();
    app_status_indication_init();

#if OS_HAS_CPU_STAT
        cpu_usage_timer_id = osTimerCreate(osTimer(cpu_usage_timer), osTimerPeriodic, NULL);
        if (cpu_usage_timer_id != NULL) {
            osTimerStart(cpu_usage_timer_id, CPU_USAGE_TIMER_TMO_VALUE);
        }
#endif

    app_config_ble_tws();


    if (hal_sw_bootmode_get() & HAL_SW_BOOTMODE_REBOOT){
        hal_sw_bootmode_clear(HAL_SW_BOOTMODE_REBOOT);
        pwron_case = APP_POWERON_CASE_REBOOT;
        need_check_key = false;
        LOG_PRINT_APPS("REBOOT!!!");
    }

    if (hal_sw_bootmode_get() & HAL_SW_BOOTMODE_TEST_MODE){
        hal_sw_bootmode_clear(HAL_SW_BOOTMODE_TEST_MODE);
        pwron_case = APP_POWERON_CASE_TEST;
        need_check_key = false;
        LOG_PRINT_APPS("TESTER!!!");
    }

    nRet = app_battery_open();
    LOG_PRINT_APPS("%d",nRet);
#if defined(__CHARGING_POWERON_AS_NORMAL__)
    nRet = 0;
    need_check_key = false;
    pwron_case = APP_POWERON_CASE_NORMAL;
#else
    if (nRet < 0){
        nRet = -1;
        goto exit;
    }else if (nRet > 0 &&
        pwron_case != APP_POWERON_CASE_TEST){
        pwron_case = APP_POWERON_CASE_CHARGING;
        app_status_indication_set(APP_STATUS_INDICATION_CHARGING);
        LOG_PRINT_APPS("CHARGING!!!");
        app_battery_start();
#if !defined(BTUSB_AUDIO_MODE)
        btdrv_start_bt();
        btdrv_hciopen();
        btdrv_sleep_config(1);
        btdrv_hcioff();
#endif
        app_key_open(false);
        app_key_init_on_charging();
        nRet = 0;
        goto exit;
    }else{
        nRet = 0;
    }
#endif
    if (app_key_open(need_check_key)){
        nRet = -1;
        goto exit;
    }

    hal_sw_bootmode_set(HAL_SW_BOOTMODE_REBOOT);
#if defined(_AUTO_TEST_)
    AUTO_TEST_SEND("Power on.");
#endif

#if defined(__FORCE_OTABOOT_UPDATE__)
    apps_ota_checker();
#endif

    app_bt_init();
#ifdef __TWS_RECONNECT_USE_BLE__
    app_tws_ble_reconnect_init();
    app_tws_delay_count=400;
#endif

    af_open();
    app_audio_open();
    app_audio_manager_open();
    app_overlay_open();

    nv_record_env_init();
    factory_section_open();
#ifdef __TWS_PAIR_DIRECTLY__
    app_tws_config_bt_mode_after_btroleswitch();
#endif
    app_tws_config_bt_address();
//    app_bt_connect2tester_init();
    nv_record_env_get(&nvrecord_env);

#if defined(HALFDUPLEXUART)
	usr_halfduplexuart_open(115200);
#endif
#ifdef ANC_APP
	app_anc_open_module();
#endif
#ifdef MEDIA_PLAYER_SUPPORT
    app_play_audio_set_lang(nvrecord_env->media_language.language);
#endif
    app_bt_stream_volume_ptr_update(NULL);
    btdrv_start_bt();
    if (pwron_case != APP_POWERON_CASE_TEST)
    {
        BesbtInit();
        app_wait_stack_ready();
        bt_drv_extra_config_after_init();
    }
    app_sysfreq_req(APP_SYSFREQ_USER_APP_0, APP_SYSFREQ_52M);
    TRACE("\n\n\nbt_stack_init_done:%d\n\n\n", pwron_case);

    if (pwron_case == APP_POWERON_CASE_REBOOT){
        app_start_custom_function_in_bt_thread((uint32_t)1,
                    0, (uint32_t) btif_me_write_bt_inquiry_scan_type);
        app_start_custom_function_in_bt_thread((uint32_t)1,
                    0, (uint32_t) btif_me_write_bt_page_scan_type);
        app_tws_ui_init();
        ble_tws_set_bt_ready_flag();        
#if VOICE_DATAPATH
        app_voicepath_init();
#endif    
#if GSOUND_OTA_ENABLED&&VOICE_DATAPATH
        GSoundOtaHandlerInit();
#endif
#ifdef BES_OTA_ENABLED
        ota_handler_init();
#endif
        app_bt_accessmode_set(BTIF_BAM_NOT_ACCESSIBLE);
        app_key_init();
        app_battery_start();
        app_start_auto_power_off_supervising();
        if (!isInCharging)
        {
            conn_system_boot_up_handler();
        }

#if defined(IAG_BLE_INCLUDE) && defined(BTIF_BLE_APP_DATAPATH_SERVER)
        BLE_custom_command_init();
#endif
    }
#ifdef __ENGINEER_MODE_SUPPORT__
    else if(pwron_case == APP_POWERON_CASE_TEST){
        app_factorymode_set(true);
        app_status_indication_set(APP_STATUS_INDICATION_POWERON);
#ifdef MEDIA_PLAYER_SUPPORT
        app_voice_report(APP_STATUS_INDICATION_POWERON, 0);
#endif
#ifdef __WATCHER_DOG_RESET__
        app_wdt_close();
#endif
        LOG_PRINT_APPS("!!!!!ENGINEER_MODE!!!!!\n");
        nRet = 0;
        app_nvrecord_rebuild();
        if (hal_sw_bootmode_get() & HAL_SW_BOOTMODE_TEST_SIGNALINGMODE){
            hal_sw_bootmode_clear(HAL_SW_BOOTMODE_TEST_MASK);
            app_factorymode_bt_signalingtest(NULL, NULL);
        }
        if (hal_sw_bootmode_get() & HAL_SW_BOOTMODE_TEST_BRIDGEMODE){
            hal_sw_bootmode_clear(HAL_SW_BOOTMODE_TEST_MASK);
            app_factorymode_bt_bridgemode(NULL, NULL);
        }
        app_factorymode_key_init();
    }
#endif
    else{
        app_status_indication_set(APP_STATUS_INDICATION_POWERON);
#ifdef MEDIA_PLAYER_SUPPORT
        app_voice_report(APP_STATUS_INDICATION_POWERON, 0);
#endif
#if defined(IAG_BLE_INCLUDE) && defined(BTIF_BLE_APP_DATAPATH_SERVER)
        BLE_custom_command_init();
#endif
#if !defined(__CHARGING_POWERON_AS_NORMAL__)
        if (pwron_case != APP_POWERON_CASE_CHARGING)
        {
            app_poweron_key_init();
            pwron_case = app_poweron_wait_case();
        }
#endif
        {
            if (pwron_case != APP_POWERON_CASE_INVALID && pwron_case != APP_POWERON_CASE_DITHERING){
                LOG_PRINT_APPS("hello world i'm bes1000 hahaha case:%d\n", pwron_case);
                nRet = 0;
#ifndef __POWERKEY_CTRL_ONOFF_ONLY__
                app_status_indication_set(APP_STATUS_INDICATION_INITIAL);
#endif
                app_start_custom_function_in_bt_thread((uint32_t)1,
                            0, (uint32_t)btif_me_write_bt_inquiry_scan_type);
                app_start_custom_function_in_bt_thread((uint32_t)1,
                            0, (uint32_t)btif_me_write_bt_page_scan_type);
                app_start_custom_function_in_bt_thread((uint32_t)1,
                            0, (uint32_t)btif_me_write_bt_sleep_enable);

                app_tws_ui_init();
                ble_tws_set_bt_ready_flag();
#if VOICE_DATAPATH
                app_voicepath_init();
#endif
#if GSOUND_OTA_ENABLED&&VOICE_DATAPATH
                GSoundOtaHandlerInit();
#endif
#ifdef BES_OTA_ENABLED
                ota_handler_init();
#endif
                switch (pwron_case) {
                    case APP_POWERON_CASE_CALIB:
                        break;
#ifdef __TWS_PAIR_DIRECTLY__
                    case APP_POWERON_CASE_BOTHSCAN:
                        TRACE("APP_POWERON_CASE_BOTHSCAN !!!");
                        app_bt_start_pairing(0,NULL,NULL);
                        app_start_10_second_timer(APP_PAIR_TIMER_ID);
                        break;
#endif
                    case APP_POWERON_CASE_NORMAL:
#if defined( __BTIF_EARPHONE__ ) && !defined(__EARPHONE_STAY_BOTH_SCAN__)
                        app_bt_accessmode_set(BAM_NOT_ACCESSIBLE);
#endif
                    case APP_POWERON_CASE_REBOOT:
                    case APP_POWERON_CASE_ALARM:
                    default:
                        app_status_indication_set(APP_STATUS_INDICATION_PAGESCAN);
#if defined(__CHARGING_POWERON_AS_NORMAL__)
                        app_tws_simulate_pairing();
                        //app_tws_freeman_simulate_pairing();
#else
#ifdef __FORCE_BOX_OPEN_FOR_POWER_ON_RECONNECT__
                        //debug bt reconnet
                        app_tws_reconnect_after_sys_startup();
#endif  //__FORCE_BOX_OPEN_FOR_POWER_ON_RECONNECT__
#endif
                        break;
                }
             if (pwron_case != APP_POWERON_CASE_CHARGING)
             {
#if !defined(__POWERKEY_CTRL_ONOFF_ONLY__) && !defined(__CHARGING_POWERON_AS_NORMAL__)
                app_poweron_wait_finished();
#endif
            }
                app_key_init();
                app_battery_start();
                app_start_auto_power_off_supervising();
                app_env_checker_timer = osTimerCreate (osTimer(APP_ENV_CHECKER), osTimerPeriodic, NULL);
                osTimerStart(app_env_checker_timer, APP_ENV_CHECKER_INTERVAL_MS);
                app_set_threadhandle(APP_MODUAL_ENVCHECKER, app_env_checker_process);

#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)
                player_role_thread_init();
                rb_ctl_init();
#endif
            }else{
                af_close();
                app_key_close();
                nRet = -1;
            }
        }
    }
exit:
    //bt_change_to_jlink(NULL,NULL);
    //bt_enable_tports();

#ifdef ANC_APP
    app_anc_set_init_done();
#endif
#if defined(BTUSB_AUDIO_MODE)
    if(pwron_case == APP_POWERON_CASE_CHARGING) {

#ifdef __WATCHER_DOG_RESET__
        app_wdt_close();
#endif
        af_open();
        app_key_handle_clear();
        app_usb_key_init();

        app_usbaudio_entry();

    }
#endif

    app_sysfreq_req(APP_SYSFREQ_USER_APP_0, APP_SYSFREQ_32K);
    return nRet;
}
#endif

extern bool app_bt_has_connectivitys(void);
void app_auto_power_off_timer(bool turnon)
{
//and no active connection
    if(turnon&&!app_bt_has_connectivitys())
        app_start_10_second_timer(APP_POWEROFF_TIMER_ID);
    else
        app_stop_10_second_timer(APP_POWEROFF_TIMER_ID);
}
void app_start_auto_power_off_supervising(void)
{
}
void app_stop_auto_power_off_supervising(void)
{
}
