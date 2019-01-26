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
#include "factory_section.h"
#include "hal_trace.h"
#include "hal_sleep.h"
#include "bt_drv_interface.h"
#include "intersyshci.h"
#include "apps.h"
#include "app_factory.h"
#include "app_factory_bt.h"
#include "app_battery.h"
#include "app_utils.h"
#include "pmu.h"
#include "tgt_hardware.h"
#include "string.h"

#include "conmgr_api.h"
#include "me_api.h"
#include "hal_chipid.h"

#define APP_FACT_CPU_WAKE_LOCK              HAL_CPU_WAKE_LOCK_USER_3

#ifdef __FACTORY_MODE_SUPPORT__
static U8 inquiry_buff[] = {0x01, 0x72, 0x77, 0xb0, 0x18, 0x57, 0x60, 0x01, 0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00};


static btif_cmgr_handler_t *app_factorymode_cmgrHandler;

static void app_factorymode_bt_inquiry_buff_update(void)
{
    factory_section_data_t *factory_section_data = factory_section_data_ptr_get();

    if(factory_section_data)
    {
        memcpy((void *)&inquiry_buff[1], factory_section_data->bt_address,BTIF_BD_ADDR_SIZE);
        LOG_PRINT_APPS_DUMP8("0x%02x ", &inquiry_buff[2], BTIF_BD_ADDR_SIZE);
    }
}

static void app_factorymode_CmgrCallback(btif_cmgr_handler_t *cHandler,
                              cmgr_event_t    Event,
                              bt_status_t     Status)
{
    APP_FACTORY_TRACE("%s cHandler:%x Event:%d status:%d", __func__, cHandler, Event, Status);
    if (Event == BTIF_CMEVENT_DATA_LINK_CON_CNF){
        if (Status == BT_STS_SUCCESS){
            APP_FACTORY_TRACE("connect ok");
            app_factorymode_result_set(true);
            btif_cmgr_remove_data_link(cHandler);

        }else{
            APP_FACTORY_TRACE("connect failed");
            app_factorymode_result_set(false);
        }
    }

    if (Event == BTIF_CMEVENT_DATA_LINK_DIS){
        if (Status == BT_STS_SUCCESS){
            APP_FACTORY_TRACE("disconnect ok");
        }else{
            APP_FACTORY_TRACE("disconnect failed");
        }
    }
}

static void app_factorymode_bt_InquiryResult_add(void)
{
    U8 len = 15;
    bool rssi = false, extended  = false;
    U8* parm = (U8*)inquiry_buff;


    /* Found one or more devices. Report to clients */
    APP_FACTORY_TRACE("%s len:%d rssi:%d extended:%d", __func__, len, rssi, extended);
    LOG_PRINT_APPS_DUMP8("0x%02x ", parm, len);
    btif_me_inquiry_result_setup(parm, rssi, extended);
}

void app_factorymode_bt_create_connect(void)
{
    bt_status_t status;
    bt_bdaddr_t *bdAddr = (bt_bdaddr_t *)(inquiry_buff+1);

    status = btif_cmgr_create_data_link(app_factorymode_cmgrHandler, bdAddr);
    APP_FACTORY_TRACE("%s:%d", __func__, status);
}

void app_factorymode_bt_init_connect(void)
{
    app_factorymode_cmgrHandler = btif_cmgr_handler_create();

    btif_cmgr_register_handler(app_factorymode_cmgrHandler,
                                        app_factorymode_CmgrCallback);
    app_factorymode_bt_inquiry_buff_update();
    app_factorymode_bt_InquiryResult_add();

    //    bt_status_t status = ME_Inquiry(BT_IAC_GIAC, 8, 0);
    //    APP_FACTORY_TRACE("connect_test status:%d", status);
}

void app_factorymode_bt_signalingtest(APP_KEY_STATUS *status, void *param)
{
    factory_section_data_t *factory_section_data = factory_section_data_ptr_get();
    APP_FACTORY_TRACE("%s",__func__);
#ifdef __WATCHER_DOG_RESET__
    app_wdt_close();
#endif
    hal_cpu_wake_lock(APP_FACT_CPU_WAKE_LOCK);
    app_stop_10_second_timer(APP_PAIR_TIMER_ID);
    app_stop_10_second_timer(APP_POWEROFF_TIMER_ID);
    app_status_indication_set(APP_STATUS_INDICATION_TESTMODE);
    pmu_sleep_en(0);
    BESHCI_Close();
    btdrv_hciopen();
#ifdef BT_50_FUNCTION
#else	
    btdrv_sleep_config(0);
#endif
//    btdrv_hcioff();
    osDelay(2000);
//    btdrv_testmode_start();
//    btdrv_sleep_config(0);
#ifdef BT_50_FUNCTION

#else
    btdrv_ins_patch_test_init();
#endif
    btdrv_feature_default();
//    btdrv_test_mode_addr_set();

    if (factory_section_data){
        btdrv_write_localinfo((char *)factory_section_data->device_name, strlen((char *)(factory_section_data->device_name)) + 1, factory_section_data->bt_address);
    }

    btdrv_enable_dut();
}


void app_factorymode_bt_txtest(APP_KEY_STATUS *status, void *param)
{
    factory_section_data_t *factory_section_data = factory_section_data_ptr_get();
    APP_FACTORY_TRACE("%s",__func__);
#ifdef __WATCHER_DOG_RESET__
    app_wdt_close();
#endif
    hal_cpu_wake_lock(APP_FACT_CPU_WAKE_LOCK);
    app_stop_10_second_timer(APP_PAIR_TIMER_ID);
    app_stop_10_second_timer(APP_POWEROFF_TIMER_ID);
    app_status_indication_set(APP_STATUS_INDICATION_TESTMODE);
    pmu_sleep_en(0);
    BESHCI_Close();
    btdrv_hciopen();
#ifdef BT_50_FUNCTION
#else	
    btdrv_sleep_config(0);
#endif
//    btdrv_hcioff();
    osDelay(2000);
//    btdrv_testmode_start();
//    btdrv_sleep_config(0);
#ifdef BT_50_FUNCTION

#else
    btdrv_ins_patch_test_init();
#endif
    btdrv_feature_default();
//    btdrv_test_mode_addr_set();
    if (factory_section_data){
        btdrv_write_localinfo((char *)factory_section_data->device_name, strlen((char *)(factory_section_data->device_name)) + 1, factory_section_data->bt_address);
    }

  //  btdrv_enable_dut();
  btdrv_enable_nonsig_tx(0);

}


int app_battery_stop(void);

void app_factorymode_bt_bridgemode(APP_KEY_STATUS *status, void *param)
{
    factory_section_data_t *factory_section_data = factory_section_data_ptr_get();
    APP_FACTORY_TRACE("%s",__func__);
#ifdef __WATCHER_DOG_RESET__
    app_wdt_close();
#endif
    hal_cpu_wake_lock(APP_FACT_CPU_WAKE_LOCK);
    app_stop_10_second_timer(APP_PAIR_TIMER_ID);
    app_stop_10_second_timer(APP_POWEROFF_TIMER_ID);
    app_status_indication_set(APP_STATUS_INDICATION_TESTMODE1);
    app_battery_stop();
#ifdef BT_50_FUNCTION
#else
    pmu_sleep_en(0);
#endif
    BESHCI_Close();
    btdrv_hciopen();
#ifdef BT_50_FUNCTION

#else    
    btdrv_sleep_config(0);
    btdrv_feature_default();
    if (factory_section_data){
        btdrv_write_localinfo((char *)factory_section_data->device_name, strlen((char *)(factory_section_data->device_name)) + 1, factory_section_data->bt_address);
    }
#ifdef CHIP_BEST2300    
#ifndef __HW_AGC__
    if(hal_get_chip_metal_id() > HAL_CHIP_METAL_ID_1 && hal_get_chip_metal_id() <= HAL_CHIP_METAL_ID_4)
    {
        *(volatile uint8_t *)0xc0004137 = 2;
    }
#endif
#endif
#endif//    btdrv_hcioff();
    osDelay(2000);
//    btdrv_testmode_start();
 //   btdrv_sleep_config(0);
 #ifdef BT_50_FUNCTION

#else
    btdrv_ins_patch_test_init();
#endif
#ifdef CHIP_BEST2300    
    *(volatile uint32_t *)0xd03503a0 |= 1;
#endif

    btdrv_hcioff();
    btdrv_uart_bridge_loop();
}

void app_factorymode_bt_nosignalingtest(APP_KEY_STATUS *status, void *param)
{
    factory_section_data_t *factory_section_data = factory_section_data_ptr_get();
    APP_FACTORY_TRACE("%s",__func__);
#ifdef __WATCHER_DOG_RESET__
    app_wdt_close();
#endif
    hal_cpu_wake_lock(APP_FACT_CPU_WAKE_LOCK);
    app_stop_10_second_timer(APP_PAIR_TIMER_ID);
    app_stop_10_second_timer(APP_POWEROFF_TIMER_ID);
    app_status_indication_set(APP_STATUS_INDICATION_TESTMODE1);
    app_battery_stop();
    pmu_sleep_en(0);
    BESHCI_Close();
    btdrv_hciopen();
    btdrv_sleep_config(0);
//    btdrv_hcioff();
    osDelay(2000);
//    btdrv_testmode_start();
 //   btdrv_sleep_config(0);
#ifdef BT_50_FUNCTION

#else 
    btdrv_ins_patch_test_init();
#endif
    btdrv_feature_default();

    if (factory_section_data){
        btdrv_write_localinfo((char *)factory_section_data->device_name, strlen((char *)(factory_section_data->device_name)) + 1, factory_section_data->bt_address);
    }
    bt_drv_check_calib();
    btdrv_hcioff();
}

int app_factorymode_bt_xtalcalib_proc(void)
{
    uint32_t capval = 0x80;
    int nRet;

    APP_FACTORY_TRACE("%s",__func__);
    hal_cpu_wake_lock(APP_FACT_CPU_WAKE_LOCK);
    APP_FACTORY_TRACE("calib default, capval:%d", capval);
    
    btdrv_hciopen();
#ifdef BT_50_FUNCTION

#else 
    btdrv_ins_patch_test_init();
#endif
    btdrv_hcioff();
    capval = 0x80;
    bt_drv_calib_open();
//    bt_drv_calib_rxonly_porc();
    nRet = bt_drv_calib_result_porc(&capval);
    bt_drv_calib_close();
    TRACE("!!!!!!!!!!!!!!!!!!!!!!!!!!!calib ret:%d, capval:%d", nRet, capval);
    if (!nRet){
        factory_section_xtal_fcap_set((unsigned int)capval);
    }
    return nRet;
}

void app_factorymode_bt_xtalcalib(APP_KEY_STATUS *status, void *param)
{
    APP_FACTORY_TRACE("%s",__func__);
    app_factorymode_bt_xtalcalib_proc();
}

#endif

