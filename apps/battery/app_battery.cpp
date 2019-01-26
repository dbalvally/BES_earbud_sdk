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
#include "cmsis_os.h"
#include "tgt_hardware.h"
#include "pmu.h"
#include "hal_timer.h"
#include "hal_gpadc.h"
#include "hal_trace.h"
#include "hal_gpio.h"
#include "hal_iomux.h"
#include "hal_chipid.h"
#include "app_thread.h"
#include "app_battery.h"
#include "apps.h"
#include "app_status_ind.h"
#include "log_section.h"

#if defined(BTUSB_AUDIO_MODE)
extern "C" bool app_usbaudio_mode_on(void);
#endif
#define APP_BATTERY_TRACE(s,...)
// LOG_PRINT_APPS(s, ##__VA_ARGS__)

#ifndef APP_BATTERY_MIN_MV
#define APP_BATTERY_MIN_MV (3200)
#endif

#ifndef APP_BATTERY_MAX_MV
#define APP_BATTERY_MAX_MV (4200)
#endif

#ifndef APP_BATTERY_PD_MV
#define APP_BATTERY_PD_MV   (3100)
#endif

#ifndef APP_BATTERY_CHARGE_TIMEOUT_MIN
#define APP_BATTERY_CHARGE_TIMEOUT_MIN (90)
#endif

#ifndef APP_BATTERY_CHARGE_OFFSET_MV
#define APP_BATTERY_CHARGE_OFFSET_MV (20)
#endif

#define APP_BATTERY_CHARGING_PLUGOUT_DEDOUNCE_CNT (APP_BATTERY_CHARGING_PERIODIC_MS<500?3:1)

#define APP_BATTERY_CHARGING_EXTPIN_MEASURE_CNT (APP_BATTERY_CHARGING_PERIODIC_MS<2*1000?2*1000/APP_BATTERY_CHARGING_PERIODIC_MS:1)
#define APP_BATTERY_CHARGING_EXTPIN_DEDOUNCE_CNT (6)

#define APP_BATTERY_CHARGING_OVERVOLT_MEASURE_CNT (APP_BATTERY_CHARGING_PERIODIC_MS<2*1000?2*1000/APP_BATTERY_CHARGING_PERIODIC_MS:1)
#define APP_BATTERY_CHARGING_OVERVOLT_DEDOUNCE_CNT (3)

#define APP_BATTERY_CHARGING_SLOPE_MEASURE_CNT (APP_BATTERY_CHARGING_PERIODIC_MS<20*1000?20*1000/APP_BATTERY_CHARGING_PERIODIC_MS:1)
#define APP_BATTERY_CHARGING_SLOPE_TABLE_COUNT (6)


#define APP_BATTERY_REPORT_INTERVAL (5)

#define APP_BATTERY_MV_BASE ((APP_BATTERY_MAX_MV-APP_BATTERY_PD_MV)/(APP_BATTERY_LEVEL_NUM))

#define APP_BATTERY_STABLE_COUNT (5)
#define APP_BATTERY_MEASURE_PERIODIC_FAST_MS (200)
#define APP_BATTERY_MEASURE_PERIODIC_NORMAL_MS (10000)
#define APP_BATTERY_CHARGING_PERIODIC_MS (200)

enum APP_BATTERY_MEASURE_PERIODIC_T {
    APP_BATTERY_MEASURE_PERIODIC_FAST = 0,
    APP_BATTERY_MEASURE_PERIODIC_NORMAL,

    APP_BATTERY_MEASURE_PERIODIC_QTY,
};

enum APP_BATTERY_CHARGER_T {
    APP_BATTERY_CHARGER_PLUGOUT = 0,
    APP_BATTERY_CHARGER_PLUGIN,

    APP_BATTERY_CHARGER_QTY,
};

struct APP_BATTERY_MEASURE_CHARGER_STATUS_T {
    HAL_GPADC_MV_T prevolt;
    int32_t slope_1000[APP_BATTERY_CHARGING_SLOPE_TABLE_COUNT];
    int slope_1000_index;
    int cnt;
};

typedef void (*APP_BATTERY_EVENT_CB_T)(enum APP_BATTERY_STATUS_T, APP_BATTERY_MV_T volt);
struct APP_BATTERY_MEASURE_T {
    uint32_t start_tim;
    enum APP_BATTERY_STATUS_T status;
    uint8_t currlevel;
    APP_BATTERY_MV_T currvolt;
    APP_BATTERY_MV_T lowvolt;
    APP_BATTERY_MV_T highvolt;
    APP_BATTERY_MV_T pdvolt;
    uint32_t chargetimeout;
    enum APP_BATTERY_MEASURE_PERIODIC_T periodic;
    HAL_GPADC_MV_T voltage[APP_BATTERY_STABLE_COUNT];
    uint16_t index;
    struct APP_BATTERY_MEASURE_CHARGER_STATUS_T charger_status;
    APP_BATTERY_EVENT_CB_T cb;
    APP_BATTERY_CB_T user_cb;
};



static void app_battery_timehandler(void const *param);
osTimerDef (APP_BATTERY, app_battery_timehandler);
static osTimerId app_batter_timer = NULL;
static struct APP_BATTERY_MEASURE_T app_battery_measure;

static int app_battery_charger_handle_process(void);

void app_battery_irqhandler(uint16_t irq_val, HAL_GPADC_MV_T volt)
{
    uint8_t i;
    uint32_t meanBattVolt = 0;
    HAL_GPADC_MV_T vbat = volt;
    APP_BATTERY_TRACE("%s %d",__func__, vbat);
    if (vbat == HAL_GPADC_BAD_VALUE){
        app_battery_measure.cb(APP_BATTERY_STATUS_INVALID, vbat);
        return;
    }

#if defined(BTUSB_AUDIO_MODE)
    if(app_usbaudio_mode_on()) return ;
#endif
    app_battery_measure.voltage[app_battery_measure.index++%APP_BATTERY_STABLE_COUNT] = vbat<<2;

    if (app_battery_measure.index > APP_BATTERY_STABLE_COUNT){
        for (i=0;i<APP_BATTERY_STABLE_COUNT;i++){
            meanBattVolt += app_battery_measure.voltage[i];
        }
        meanBattVolt /= APP_BATTERY_STABLE_COUNT;
        if (app_battery_measure.cb){
            if (meanBattVolt>app_battery_measure.highvolt){
                app_battery_measure.cb(APP_BATTERY_STATUS_OVERVOLT, meanBattVolt);
            }else if((meanBattVolt>app_battery_measure.pdvolt) && (meanBattVolt<app_battery_measure.lowvolt)){
                app_battery_measure.cb(APP_BATTERY_STATUS_UNDERVOLT, meanBattVolt);
            }else if(meanBattVolt<app_battery_measure.pdvolt){
                app_battery_measure.cb(APP_BATTERY_STATUS_PDVOLT, meanBattVolt);
            }else{
                app_battery_measure.cb(APP_BATTERY_STATUS_NORMAL, meanBattVolt);
            }
        }
    }else{
        int8_t level = 0;
        meanBattVolt = vbat<<2;
        level = (meanBattVolt-APP_BATTERY_PD_MV)/APP_BATTERY_MV_BASE;

        if (level<APP_BATTERY_LEVEL_MIN)
            level = APP_BATTERY_LEVEL_MIN;
        if (level>APP_BATTERY_LEVEL_MAX)
            level = APP_BATTERY_LEVEL_MAX;

        app_battery_measure.currvolt = meanBattVolt;
        app_battery_measure.currlevel = level;
    }
}

static void app_battery_timehandler(void const *param)
{
    hal_gpadc_open(HAL_GPADC_CHAN_BATTERY, HAL_GPADC_ATP_ONESHOT, app_battery_irqhandler);
}

static void app_battery_event_process(enum APP_BATTERY_STATUS_T status, APP_BATTERY_MV_T volt)
{
    uint32_t app_battevt;
    APP_MESSAGE_BLOCK msg;

    APP_BATTERY_TRACE("%s %d,%d",__func__, status, volt);

    msg.mod_id = APP_MODUAL_BATTERY;
    APP_BATTERY_SET_MESSAGE(app_battevt, status, volt);
    msg.msg_body.message_id = app_battevt;
    msg.msg_body.message_ptr = (uint32_t)NULL;
    app_mailbox_put(&msg);
    
#if defined __TWS_GET_SLAVE_MOBILE_RSSI__
#ifdef CHIP_BEST2300
    if(hal_get_chip_metal_id() >= HAL_CHIP_METAL_ID_5)
    {
         app_tws_get_tws_slave_mobile_rssi();
    }
#endif 
#endif
}

#if defined(__TWS_PAIR_DIRECTLY__) && defined(__TWS_ROLE_SWITCH__) && defined(__ENABLE_ROLE_SWITCH_FOR_BATTERY_BALANCE__)
extern void app_tws_cmd_req_battery_level(void);
#endif

static enum APP_BATTERY_CHARGER_T app_battery_charger_forcegetstatus(void);
static int app_battery_handle_process(APP_MESSAGE_BODY *msg_body)
{
    int8_t level = 0;
    uint8_t status;
    APP_BATTERY_MV_T volt;
#ifdef CHIP_BEST1000
    static uint8_t charging_plugout_cnt = 0;
#endif

    app_refresh_random_seed();
    
    if ((app_battery_measure.status == APP_BATTERY_STATUS_CHARGING)){
        app_battery_measure.periodic = APP_BATTERY_MEASURE_PERIODIC_NORMAL;
        osTimerStop(app_batter_timer);
        osTimerStart(app_batter_timer, APP_BATTERY_CHARGING_PERIODIC_MS);
    }else if (app_battery_measure.periodic == APP_BATTERY_MEASURE_PERIODIC_FAST){
        app_battery_measure.periodic = APP_BATTERY_MEASURE_PERIODIC_NORMAL;
        osTimerStop(app_batter_timer);
        osTimerStart(app_batter_timer, APP_BATTERY_MEASURE_PERIODIC_NORMAL_MS);
    }
    APP_BATTERY_GET_STATUS(msg_body->message_id, status);
    APP_BATTERY_GET_VOLT(msg_body->message_id, volt);

    LOG_PRINT_APPS("%s %d,%d",__func__, status, volt);

#if defined(__TWS_PAIR_DIRECTLY__) && defined(__TWS_ROLE_SWITCH__) && defined(__ENABLE_ROLE_SWITCH_FOR_BATTERY_BALANCE__)
    //app_tws_cmd_req_battery_level();
#endif
    
    if (app_battery_measure.status == APP_BATTERY_STATUS_NORMAL){
        switch (status) {
            case APP_BATTERY_STATUS_UNDERVOLT:
                LOG_PRINT_APPS("UNDERVOLT:%d", volt);
                app_status_indication_set(APP_STATUS_INDICATION_CHARGENEED);
                log_event(EVENT_LOW_BATTERY_LEVEL_WARNING);
#ifdef MEDIA_PLAYER_SUPPORT
                app_voice_report(APP_STATUS_INDICATION_CHARGENEED, 0);
#endif
            case APP_BATTERY_STATUS_NORMAL:
            case APP_BATTERY_STATUS_OVERVOLT:
                app_battery_measure.currvolt = volt;
                level = (volt-APP_BATTERY_PD_MV)/APP_BATTERY_MV_BASE;

                if (level<APP_BATTERY_LEVEL_MIN)
                    level = APP_BATTERY_LEVEL_MIN;
                if (level>APP_BATTERY_LEVEL_MAX)
                {
                    log_event(EVENT_BATTERY_FULL);
                    level = APP_BATTERY_LEVEL_MAX;
                }

                if (level != app_battery_measure.currlevel)
                {
                    log_event_1(EVENT_BATTERY_LEVEL_CHANGED, level);
                }
                app_battery_measure.currlevel = level;

                app_status_battery_report(app_battery_measure.currlevel);
                break;
            case APP_BATTERY_STATUS_PDVOLT:
                LOG_PRINT_APPS("[IO] PDVOLT-->POWEROFF:%d", volt);
                log_event(EVENT_LOW_BATTERY_POWER_OFF);
                osTimerStop(app_batter_timer);
                app_shutdown();
                break;
            case APP_BATTERY_STATUS_CHARGING:
                LOG_PRINT_APPS("[IO] CHARGING-->APP_BATTERY_CHARGER :%d", volt);
#if !defined(__CHARGING_POWERON_AS_NORMAL__)
                if (volt == APP_BATTERY_CHARGER_PLUGIN){
                    LOG_PRINT_APPS("[IO] CHARGING-->POWEROFF:%d", volt);
                    osTimerStop(app_batter_timer);
                    app_reset();
                }
#endif
                break;
            case APP_BATTERY_STATUS_INVALID:
            default:
                break;
        }
    }else if (app_battery_measure.status == APP_BATTERY_STATUS_CHARGING){
        enum APP_BATTERY_CHARGER_T status_charger = app_battery_charger_forcegetstatus();
        if (status_charger == APP_BATTERY_CHARGER_PLUGOUT){
#if !defined(__CHARGING_POWERON_AS_NORMAL__)
#ifdef CHIP_BEST1000
            if (hal_get_chip_metal_id() >= HAL_CHIP_METAL_ID_2)
#endif
            {
                LOG_PRINT_APPS("[IO] CHARGING-->RESET");
                osTimerStop(app_batter_timer);
                app_shutdown();
                goto exit;
            }
#ifdef CHIP_BEST1000
            else if (++charging_plugout_cnt>=APP_BATTERY_CHARGING_PLUGOUT_DEDOUNCE_CNT){
                LOG_PRINT_APPS("[IO] CHARGING-->RESET");
                osTimerStop(app_batter_timer);
                app_shutdown();
                goto exit;
            }
#endif
#endif
        }
#ifdef CHIP_BEST1000
        else{
            charging_plugout_cnt = 0;
        }
#endif
        switch (status) {
            case APP_BATTERY_STATUS_OVERVOLT:
            case APP_BATTERY_STATUS_NORMAL:
            case APP_BATTERY_STATUS_UNDERVOLT:
                app_battery_measure.currvolt = volt;
                break;
            case APP_BATTERY_STATUS_CHARGING:               
                LOG_PRINT_APPS("[IO] CHARGING:%d", volt);
#if !defined(__CHARGING_POWERON_AS_NORMAL__)
                if (volt == APP_BATTERY_CHARGER_PLUGOUT){
                    LOG_PRINT_APPS("[IO] CHARGING-->RESET");
                    osTimerStop(app_batter_timer);
                    app_shutdown();
                    goto exit;
                }
#endif
                break;
            case APP_BATTERY_STATUS_INVALID:
            default:
                break;
        }

        if (app_battery_charger_handle_process()<=0){
            if (app_status_indication_get() != APP_STATUS_INDICATION_FULLCHARGE){                    
                LOG_PRINT_APPS("[IO] FULL_CHARGING:%d", app_battery_measure.currvolt);
                app_status_indication_set(APP_STATUS_INDICATION_FULLCHARGE);
#ifdef MEDIA_PLAYER_SUPPORT
                app_voice_report(APP_STATUS_INDICATION_FULLCHARGE, 0);
#endif
            }
        }
    }

exit:
    if(NULL != app_battery_measure.user_cb) {
      app_battery_measure.user_cb(app_battery_measure.currvolt,
          app_battery_measure.currlevel,
          app_battery_measure.status);
    }
    return 0;
}

int app_battery_register(APP_BATTERY_CB_T user_cb) {
  if(NULL == app_battery_measure.user_cb) {
    app_battery_measure.user_cb = user_cb;
    return 0;
  }
  return 1;
}

int app_battery_get_info(APP_BATTERY_MV_T *currvolt, uint8_t *currlevel, enum APP_BATTERY_STATUS_T *status)
{
    if (currvolt)
    {
        *currvolt = app_battery_measure.currvolt;
    }

    if (currlevel)
    {
        *currlevel = app_battery_measure.currlevel;
    }

    if (status)
    {
        *status = app_battery_measure.status;
    }

    return 0;
}

int app_battery_open(void)
{
    int nRet = 0;

    APP_BATTERY_TRACE("%s batt range:%d~%d",__func__, APP_BATTERY_MIN_MV, APP_BATTERY_MAX_MV);

    if (app_batter_timer == NULL)
        app_batter_timer = osTimerCreate (osTimer(APP_BATTERY), osTimerPeriodic, NULL);

    app_battery_measure.status = APP_BATTERY_STATUS_NORMAL;
    app_battery_measure.currlevel = APP_BATTERY_LEVEL_MAX;
    app_battery_measure.currvolt = APP_BATTERY_MAX_MV;
    app_battery_measure.lowvolt = APP_BATTERY_MIN_MV;
    app_battery_measure.highvolt = APP_BATTERY_MAX_MV;
    app_battery_measure.pdvolt = APP_BATTERY_PD_MV;
    app_battery_measure.chargetimeout = APP_BATTERY_CHARGE_TIMEOUT_MIN;

    app_battery_measure.periodic = APP_BATTERY_MEASURE_PERIODIC_QTY;
    app_battery_measure.cb = app_battery_event_process;
    app_battery_measure.user_cb = NULL;

    app_battery_measure.charger_status.prevolt = 0;
    app_battery_measure.charger_status.slope_1000_index = 0;
    app_battery_measure.charger_status.cnt = 0;

    app_set_threadhandle(APP_MODUAL_BATTERY, app_battery_handle_process);

    if (app_battery_ext_charger_detecter_cfg.pin != HAL_IOMUX_PIN_NUM){
        hal_iomux_init((struct HAL_IOMUX_PIN_FUNCTION_MAP *)&app_battery_ext_charger_detecter_cfg, 1);
        hal_gpio_pin_set_dir((enum HAL_GPIO_PIN_T)app_battery_ext_charger_detecter_cfg.pin, HAL_GPIO_DIR_IN, 1);
    }

    if (app_battery_charger_indication_open() == APP_BATTERY_CHARGER_PLUGIN){
        app_battery_measure.status = APP_BATTERY_STATUS_CHARGING;
        pmu_charger_plugin_config();
        if (app_battery_ext_charger_indicator_cfg.pin != HAL_IOMUX_PIN_NUM){
            hal_gpio_pin_set_dir((enum HAL_GPIO_PIN_T)app_battery_ext_charger_indicator_cfg.pin, HAL_GPIO_DIR_IN, 1);
        }
        nRet = 1;
    }else{
        if (app_battery_ext_charger_indicator_cfg.pin != HAL_IOMUX_PIN_NUM){
            hal_gpio_pin_set_dir((enum HAL_GPIO_PIN_T)app_battery_ext_charger_indicator_cfg.pin, HAL_GPIO_DIR_OUT, 0);
            }
        nRet = 0;
    }

    hal_gpadc_open(HAL_GPADC_CHAN_BATTERY, HAL_GPADC_ATP_ONESHOT, app_battery_irqhandler);
    osDelay(10);

    if (nRet == 0 && app_battery_measure.currvolt <= app_battery_measure.pdvolt){
        nRet = -1;
    }
    LOG_PRINT_APPS("[IO] %s battery currvolt:%d nRet:%d",__func__, app_battery_measure.currvolt, nRet);
    return nRet;
}

int app_battery_start(void)
{
    APP_BATTERY_TRACE("%s %d",__func__, APP_BATTERY_MEASURE_PERIODIC_FAST_MS);

    app_battery_measure.start_tim = hal_sys_timer_get();
    app_battery_measure.periodic = APP_BATTERY_MEASURE_PERIODIC_FAST;
    osTimerStart(app_batter_timer, APP_BATTERY_MEASURE_PERIODIC_FAST_MS);

    return 0;
}

int app_battery_stop(void)
{
    osTimerStop(app_batter_timer);

    return 0;
}

int app_battery_close(void)
{
    hal_gpadc_close(HAL_GPADC_CHAN_BATTERY);

    return 0;
}


static int32_t app_battery_charger_slope_calc(int32_t t1, int32_t v1, int32_t t2, int32_t v2)
{
    int32_t slope_1000;
    slope_1000 = (v2-v1)*1000/(t2-t1);
    return slope_1000;
}

static int app_battery_charger_handle_process(void)
{
    int nRet = 1;
    int8_t i=0,cnt=0;
    uint32_t slope_1000 = 0;
    uint32_t charging_min;
    static uint8_t overvolt_full_charge_cnt = 0;
    static uint8_t ext_pin_full_charge_cnt = 0;

    charging_min = hal_sys_timer_get() - app_battery_measure.start_tim;
    charging_min = TICKS_TO_MS(charging_min)/1000/60;
    if (charging_min >= app_battery_measure.chargetimeout){
        LOG_PRINT_APPS("[IO] TIMEROUT-->FULL_CHARGING");     
        nRet = -1;
        goto exit;
    }

    if ((app_battery_measure.charger_status.cnt++%APP_BATTERY_CHARGING_OVERVOLT_MEASURE_CNT) == 0){
        if (app_battery_measure.currvolt>=(app_battery_measure.highvolt+APP_BATTERY_CHARGE_OFFSET_MV)){
            overvolt_full_charge_cnt++;
        }else{
            overvolt_full_charge_cnt = 0;
        }
        if (overvolt_full_charge_cnt>=APP_BATTERY_CHARGING_OVERVOLT_DEDOUNCE_CNT){          
            LOG_PRINT_APPS("[IO] OVERVOLT-->FULL_CHARGING");     
            nRet = -1;
            goto exit;
        }
    }

    if ((app_battery_measure.charger_status.cnt++%APP_BATTERY_CHARGING_EXTPIN_MEASURE_CNT) == 0){
        if (app_battery_ext_charger_detecter_cfg.pin != HAL_IOMUX_PIN_NUM){
            if (hal_gpio_pin_get_val((enum HAL_GPIO_PIN_T)app_battery_ext_charger_detecter_cfg.pin)){
                ext_pin_full_charge_cnt++;
            }else{
                ext_pin_full_charge_cnt = 0;
            }
            if (ext_pin_full_charge_cnt>=APP_BATTERY_CHARGING_EXTPIN_DEDOUNCE_CNT){         
                LOG_PRINT_APPS("[IO] EXT PIN-->FULL_CHARGING");
                nRet = -1;
                goto exit;
            }
        }
    }

    if ((app_battery_measure.charger_status.cnt++%APP_BATTERY_CHARGING_SLOPE_MEASURE_CNT) == 0){
        if (!app_battery_measure.charger_status.prevolt){
            app_battery_measure.charger_status.slope_1000[app_battery_measure.charger_status.slope_1000_index%APP_BATTERY_CHARGING_SLOPE_TABLE_COUNT] = slope_1000;
            app_battery_measure.charger_status.prevolt = app_battery_measure.currvolt;
            for (i=0;i<APP_BATTERY_CHARGING_SLOPE_TABLE_COUNT;i++){
                app_battery_measure.charger_status.slope_1000[i]=100;
            }
        }else{
            slope_1000 = app_battery_charger_slope_calc(0, app_battery_measure.charger_status.prevolt,
                                                        APP_BATTERY_CHARGING_PERIODIC_MS*APP_BATTERY_CHARGING_SLOPE_MEASURE_CNT/1000, app_battery_measure.currvolt);
            app_battery_measure.charger_status.slope_1000[app_battery_measure.charger_status.slope_1000_index%APP_BATTERY_CHARGING_SLOPE_TABLE_COUNT] = slope_1000;
            app_battery_measure.charger_status.prevolt = app_battery_measure.currvolt;
            for (i=0;i<APP_BATTERY_CHARGING_SLOPE_TABLE_COUNT;i++){
                if (app_battery_measure.charger_status.slope_1000[i]>0)
                    cnt++;
                else
                    cnt--;
                LOG_PRINT_APPS("slope_1000[%d]=%d cnt:%d", i,app_battery_measure.charger_status.slope_1000[i], cnt);
            }
            LOG_PRINT_APPS("app_battery_charger_slope_proc slope*1000=%d cnt:%d nRet:%d", slope_1000, cnt, nRet);           
            if (cnt>1){
                nRet = 1;
            }/*else (3>=cnt && cnt>=-3){
                nRet = 0;
            }*/else{
                if (app_battery_measure.currvolt>=(app_battery_measure.highvolt-APP_BATTERY_CHARGE_OFFSET_MV)){
                    LOG_PRINT_APPS("[IO] SLOPE-->FULL_CHARGING");
                    nRet = -1;
                }
            }
        }
        app_battery_measure.charger_status.slope_1000_index++;
    }
exit:
    return nRet;
}

static enum APP_BATTERY_CHARGER_T app_battery_charger_forcegetstatus(void)
{
    enum APP_BATTERY_CHARGER_T status = APP_BATTERY_CHARGER_QTY;
    enum PMU_CHARGER_STATUS_T charger;

    charger = pmu_charger_get_status();

    if (charger == PMU_CHARGER_PLUGIN) {
        status = APP_BATTERY_CHARGER_PLUGIN;
        LOG_PRINT_APPS("force APP_BATTERY_CHARGER_PLUGIN");
    } else{
        status = APP_BATTERY_CHARGER_PLUGOUT;
        LOG_PRINT_APPS("force APP_BATTERY_CHARGER_PLUGOUT");
    }

    return status;
}

static void app_battery_charger_handler(enum PMU_CHARGER_STATUS_T status)
{
    LOG_PRINT_APPS("%s: status=%d", __func__, status);

    if (status != PMU_CHARGER_UNKNOWN)
    {
        app_battery_event_process(APP_BATTERY_STATUS_CHARGING,
            (status == PMU_CHARGER_PLUGIN) ? APP_BATTERY_CHARGER_PLUGIN : APP_BATTERY_CHARGER_PLUGOUT);
    }
}

int app_battery_charger_indication_open(void)
{
    enum APP_BATTERY_CHARGER_T status = APP_BATTERY_CHARGER_QTY;
    uint8_t cnt = 0;

    APP_BATTERY_TRACE("%s",__func__);

    pmu_charger_init();

    do{
        status = app_battery_charger_forcegetstatus();
        if (status == APP_BATTERY_CHARGER_PLUGIN)
            break;
        osDelay(20);
    }while(cnt++<10);

    if (app_battery_ext_charger_detecter_cfg.pin != HAL_IOMUX_PIN_NUM){
        if (!hal_gpio_pin_get_val((enum HAL_GPIO_PIN_T)app_battery_ext_charger_detecter_cfg.pin)){
            status = APP_BATTERY_CHARGER_PLUGIN;
        }
    }

    if (status == APP_BATTERY_CHARGER_PLUGOUT)
    {
        pmu_charger_set_irq_handler(app_battery_charger_handler);
    }

    return status;
}

int8_t app_battery_current_level(void)
{
    return app_battery_measure.currlevel;
}

typedef uint16_t NTP_VOLTAGE_MV_T;
typedef uint16_t NTP_TEMPERATURE_C_T;

#define NTC_CAPTURE_STABLE_COUNT (5)
#define NTC_CAPTURE_TEMPERATURE_STEP (4)
#define NTC_CAPTURE_TEMPERATURE_REF (15)
#define NTC_CAPTURE_VOLTAGE_REF (1100)

typedef void (*NTC_CAPTURE_MEASURE_CB_T)(NTP_TEMPERATURE_C_T);

struct NTC_CAPTURE_MEASURE_T
{
    NTP_TEMPERATURE_C_T temperature;
    NTP_VOLTAGE_MV_T currvolt;
    NTP_VOLTAGE_MV_T voltage[NTC_CAPTURE_STABLE_COUNT];
    uint16_t index;
    NTC_CAPTURE_MEASURE_CB_T cb;
};

static struct NTC_CAPTURE_MEASURE_T ntc_capture_measure;

void ntc_capture_irqhandler(uint16_t irq_val, HAL_GPADC_MV_T volt)
{
    uint32_t meanVolt = 0;
    LOG_PRINT_APPS("%s %d irq:0x%04x",__func__, volt, irq_val);

    if (volt == HAL_GPADC_BAD_VALUE){
        return;
    }

    ntc_capture_measure.voltage[ntc_capture_measure.index++%NTC_CAPTURE_STABLE_COUNT] = volt;

    if (ntc_capture_measure.index > NTC_CAPTURE_STABLE_COUNT)
    {
        for (uint8_t i=0; i<NTC_CAPTURE_STABLE_COUNT; i++)
        {
            meanVolt += ntc_capture_measure.voltage[i];
        }
        meanVolt /= NTC_CAPTURE_STABLE_COUNT;
        ntc_capture_measure.currvolt = meanVolt;
    }else if (!ntc_capture_measure.currvolt){
        ntc_capture_measure.currvolt = volt;
    }
    ntc_capture_measure.temperature = ((int32_t)ntc_capture_measure.currvolt - NTC_CAPTURE_VOLTAGE_REF)/NTC_CAPTURE_TEMPERATURE_STEP + NTC_CAPTURE_TEMPERATURE_REF;
    pmu_ntc_capture_disable();
    LOG_PRINT_APPS("%s ad:%d temperature:%d",__func__, ntc_capture_measure.currvolt, ntc_capture_measure.temperature);
}

int ntc_capture_open(void)
{

    ntc_capture_measure.currvolt = 0;
    ntc_capture_measure.index = 0;
    ntc_capture_measure.temperature = 0;
    ntc_capture_measure.cb = NULL;

    pmu_ntc_capture_enable();
    hal_gpadc_open(HAL_GPADC_CHAN_0, HAL_GPADC_ATP_ONESHOT, ntc_capture_irqhandler);
    return 0;
}

int ntc_capture_start(void)
{
    pmu_ntc_capture_enable();
    hal_gpadc_open(HAL_GPADC_CHAN_0, HAL_GPADC_ATP_ONESHOT, ntc_capture_irqhandler);
    return 0;
}

