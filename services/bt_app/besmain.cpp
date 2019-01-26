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
//#include "mbed.h"
#include <stdio.h>
#include "cmsis_os.h"
#include "factory_section.h"
#include "hal_uart.h"
#include "hal_timer.h"
#include "audioflinger.h"
#include "lockcqueue.h"
#include "hal_trace.h"
#include "hal_cmu.h"
#include "hal_chipid.h"
#include "analog.h"
#include "apps.h"
#include "app_status_ind.h"
#include "app_utils.h"
#include "app_bt_stream.h"
#include "tgt_hardware.h"
#include "besbt_cfg.h"
#include "app_bt_func.h"
#include "os_api.h"
#include "hci_api.h"
#include "bt_if.h"
extern "C" {
#include "app_tws_cmd_handler.h"
#ifdef __TWS__
#include "app_tws.h"
#include "app_tws_if.h"
#endif
#ifdef IAG_BLE_INCLUDE
#include "besble.h"
#endif
void BESHCI_Open(void);
void BESHCI_Poll(void);
void BESHCI_SCO_Data_Start(void);
void BESHCI_SCO_Data_Stop(void);
void BESHCI_LockBuffer(void);
void BESHCI_UNLockBuffer(void);
}
#include "rtos.h"
#include "besbt.h"

#include "cqueue.h"
#include "btapp.h"
#include "app_bt.h"
#include "app_bt_conn_mgr.h"
#include "app_spp.h"

#if VOICE_DATAPATH
extern "C" void app_voicepath_bt_init(void);
#endif

struct besbt_cfg_t besbt_cfg = {
#ifdef __BTIF_SNIFF__
    .sniff = true,
#else
    .sniff = false,
#endif
#ifdef __BT_ONE_BRING_TWO__
    .one_bring_two = true,
#else
    .one_bring_two = false,
#endif
};

btif_sbc_decoder_t  sbc_decoder;

osMessageQDef(evm_queue, 128, message_t);
osMessageQId  evm_queue_id;

/* besbt thread */
#ifdef IAG_BLE_INCLUDE
#define BESBT_STACK_SIZE 1024*5
#else
#ifdef __TWS__
#define BESBT_STACK_SIZE (1024*4)
#else
#define BESBT_STACK_SIZE (1024*2+512)
#endif
#endif

static osThreadId besbt_tid;
osThreadDef(BesbtThread, osPriorityAboveNormal, BESBT_STACK_SIZE);

static BESBT_HOOK_HANDLER bt_hook_handler[BESBT_HOOK_USER_QTY] = {0};

int Besbt_hook_handler_set(enum BESBT_HOOK_USER_T user, BESBT_HOOK_HANDLER handler)
{
    bt_hook_handler[user]= handler;
    return 0;
}

static void Besbt_hook_proc(void)
{
    uint8_t i;

    for (i = 0; i < BESBT_HOOK_USER_QTY; i++) {
        if (bt_hook_handler[i]) {
            bt_hook_handler[i]();
        }
    }
}

extern "C" void hci_simulate_reconnect_state_machine_handler(void);
extern "C" void tws_received_acl_data_handler(void);
extern "C" void tws_transmitted_acl_data_handler(void);
void app_tws_pending_role_switch_op_handling(void);

#define BESTHR_MAILBOX_MAX (30)
osMailQDef (besthr_mailbox, BESTHR_MAILBOX_MAX, BESTHR_MSG_BLOCK);
osMailQId bes_mailbox;


int besthr_mailbox_put(BESTHR_MSG_BLOCK* msg_src)
{
    osStatus status;

    BESTHR_MSG_BLOCK *msg_p = NULL;

    msg_p = (BESTHR_MSG_BLOCK*)osMailAlloc(bes_mailbox, 0);
    if(!msg_p) {
        TRACE("%s fail, evt:%d,arg=%d %d,%x \n",__func__,msg_src->msg_id,msg_src->param0,msg_src->param1,msg_src->ptr);
        return -1;
    }

    ASSERT(msg_p, "osMailAlloc error");

    msg_p->msg_id = msg_src->msg_id;
    msg_p->param0 = msg_src->param0;
    msg_p->param1 = msg_src->param1;
    msg_p->ptr= msg_src->ptr;

    status = osMailPut(bes_mailbox, msg_p);

    return (int)status;
}

int besthr_mailbox_free(BESTHR_MSG_BLOCK* msg_p)
{
    osStatus status;

    status = osMailFree(bes_mailbox, msg_p);

    return (int)status;
}

int besthr_mailbox_get(BESTHR_MSG_BLOCK** msg_p)
{
    osEvent evt;
    evt = osMailGet(bes_mailbox, 0);
    if (evt.status == osEventMail) {
        *msg_p = (BESTHR_MSG_BLOCK *)evt.value.p;
        return 0;
    }
    return -1;
}

static int Besthr_Mailbox_Init(void)
{
    bes_mailbox = osMailCreate(osMailQ(besthr_mailbox), NULL);
    if (bes_mailbox == NULL)  {
        TRACE("Failed to Create tws_ctl_mailbox\n");
        return -1;
    }
    return 0;
}


static int besthr_msg_process(BESTHR_MSG_BLOCK* msg_p)
{
    int ret=0;
    switch (msg_p->msg_id) {
		case APP_BT_REQ_BT_THREAD_CALL:
			if (msg_p->ptr){
				((APP_BT_REQ_CUSTOMER_CALl_CB_T)(msg_p->ptr))((void *)msg_p->param0, (void *)msg_p->param1);
			}
			break;
         default:
            ret = -1;
            break;
    }

    return ret;
}

extern void a2dp_init(void);
extern void app_hfp_init(void);

unsigned char *randaddrgen_get_bt_addr(void)
{
    return bt_addr;
}

unsigned char *randaddrgen_get_ble_addr(void)
{
    return ble_addr;
}

const char *randaddrgen_get_btd_localname(void)
{
    return BT_LOCAL_NAME;
}

#ifdef __TWS_CALL_DUAL_CHANNEL__
void bt_sco_check_pcm(void);
#endif
void app_tws_free_src_stream(void);

void pair_handler_func(enum pair_event evt, int err_code);
#ifdef BTIF_SECURITY
void auth_handler_func();
#endif

static void __set_local_dev_name(void)
{
    factory_section_data_t *factory_data = factory_section_data_ptr_get();

    if (factory_data) {
#ifdef __TWS_PAIR_DIRECTLY__
        BT_LOCAL_NAME = (const char*)factory_data->device_name;
#endif
        bt_set_local_dev_name((const unsigned char*)(factory_data->device_name),
                                strlen((char *)(factory_data->device_name)) + 1);
    } else {
        bt_set_local_dev_name((const unsigned char*)BT_LOCAL_NAME,
                                strlen(BT_LOCAL_NAME) + 1);
    }
}

static void __set_bt_sco_num(void)
{
    uint8_t sco_num;

#ifdef CHIP_BEST1000
#if !defined(HFP_1_6_ENABLE)
    if (hal_get_chip_metal_id() >= HAL_CHIP_METAL_ID_2) {
        sco_num = 2;
    } else
#endif
    {
        sco_num = 1;
    }
#else
    sco_num = 2;
#endif
    bt_set_sco_number(sco_num);
}

extern "C" void bes_ble_init(void);
extern "C" void bes_ble_schedule(void);

int besmain(void)
{
    BESHCI_Open();
    __set_bt_sco_num();
#ifdef IAG_BLE_INCLUDE
    bes_ble_init();
#endif
    /* bes stack init */
    bt_stack_initilize();

    conn_mgr_init();
    btif_avrcp_init();
    a2dp_init();
    app_hfp_init();

    bt_pairing_init(pair_handler_func);
    bt_authing_init(auth_handler_func);

    a2dp_codec_init();
#ifdef __TWS__
    static tws_env_cfg_t tws_env_cfg;
    app_tws_env_init(&tws_env_cfg);
    init_tws(&tws_env_cfg);
    tws_init_cmd_handler();
#endif
    btif_hci_init();

    __set_local_dev_name();
    bt_stack_config();

#if VOICE_DATAPATH
	app_voicepath_bt_init();
#endif

    ///init bt key
    bt_key_init();

    osapi_notify_evm();
#ifdef __TWS__
	notify_tws_btenv_ready();
#endif

#if defined(__BQB_FUNCTION_CODE__)
    app_tws_free_src_stream();
#endif

    app_spp_init();
    while (1) {
#ifdef __WATCHER_DOG_RESET__
        app_wdt_ping();
        app_wdt_thread_check_ping(APP_WDT_THREAD_CHECK_USER_BT);
#endif
        app_sysfreq_req(APP_SYSFREQ_USER_APP_1, APP_SYSFREQ_32K);
        osMessageGet(evm_queue_id, osWaitForever);
        app_sysfreq_req(APP_SYSFREQ_USER_APP_1, APP_SYSFREQ_52M);
    //    BESHCI_LockBuffer();
#ifdef __LOCK_AUDIO_THREAD__
        bool stream_a2dp_sbc_isrun = app_bt_stream_isrun(APP_BT_STREAM_A2DP_SBC);
        if (stream_a2dp_sbc_isrun) {
            af_lock_thread();
        }
#endif
        bt_process_stack_events();
        Besbt_hook_proc();

        bt_key_handle();

#ifdef IAG_BLE_INCLUDE
            bes_ble_schedule();
#endif

#ifdef __LOCK_AUDIO_THREAD__
        if (stream_a2dp_sbc_isrun) {
            af_unlock_thread();
        }
#endif

        BESHCI_Poll();
#ifdef __TWS__
        app_tws_check_pcm_signal();
#ifdef __TWS_CALL_DUAL_CHANNEL__
        bt_sco_check_pcm();
#endif

#endif
         {
            BESTHR_MSG_BLOCK *msg_p = NULL;
            besthr_mailbox_get(&msg_p);
            if (msg_p) {
                besthr_msg_process(msg_p);
                osapi_notify_evm();
                besthr_mailbox_free(msg_p);
            }
         }

        hci_simulate_reconnect_state_machine_handler();

        app_tws_pending_role_switch_op_handling();

        tws_received_acl_data_handler();

        tws_transmitted_acl_data_handler();

        app_check_pending_stop_sniff_op();

    }

    return 0;
}

void BesbtThread(void const *argument)
{
    besmain();
}

osThreadId besBtThreadId(void)
{
    return besbt_tid;
}

void BesbtInit(void)
{
    app_bt_mail_init();
    evm_queue_id = osMessageCreate(osMessageQ(evm_queue), NULL);
    Besthr_Mailbox_Init();
#ifdef __WATCHER_DOG_RESET__
    app_wdt_thread_check_enable(APP_WDT_THREAD_CHECK_USER_BT, 15);
#endif
    /* bt */
    besbt_tid = osThreadCreate(osThread(BesbtThread), NULL);
    TRACE("BesbtThread: %x\n", besbt_tid);
}



void bt_thread_set_priority(osPriority priority)
{
    osThreadSetPriority(besbt_tid, priority);
}

