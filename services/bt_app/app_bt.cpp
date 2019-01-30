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
#include "hal_trace.h"
#include "hal_aud.h"
#include "apps.h"
#include "app_thread.h"
#include "app_status_ind.h"

#include "nvrecord.h"
#include "hal_timer.h"
#include "hal_chipid.h"


#include "besbt_cfg.h"
#include "me_api.h"
#include "a2dp_api.h"
#include "hci_api.h"
#include "l2cap_api.h"
#include "os_api.h"

#include "hfp_api.h"
#include "bt_xtal_sync.h"
#include "bt_drv.h"
#include "bt_drv_reg_op.h"

#include "btapp.h"
#include "app_bt.h"
#include "app_bt_func.h"
#include "bt_drv_interface.h"
#include "tgt_hardware.h"
#include "nvrecord_env.h"

#ifdef __TWS__
#include "app_tws.h"
#include "app_tws_if.h"
#include "app_bt_media_manager.h"
#endif
#include "ble_tws.h"

#include "app_bt_conn_mgr.h"
#include "app_hfp.h"
#include "app_tws_role_switch.h"
#include "log_section.h"
#if VOICE_DATAPATH
#include "app_voicepath.h"
#endif


#ifdef BES_OTA_TWS
#include "ota_control.h"
#endif

#ifdef __TWS__
extern  tws_dev_t  tws;
#endif

#ifdef __AMA_VOICE__
#include "app_ama_handle.h"
#endif

#ifdef __3RETX_SNIFF__
#define CMGR_ESCO_SNIFF_INTERVAL   60
#define CMGR_ESCO_SNIFF_TIMER      3000
#endif


extern struct BT_DEVICE_T  app_bt_device;

#ifdef __BT_ONE_BRING_TWO__
btif_device_record_t record2_copy;
uint8_t record2_avalible;
#endif
static btif_accessible_mode_t gBT_DEFAULT_ACCESS_MODE = BTIF_BAM_NOT_ACCESSIBLE;
static uint8_t bt_access_mode_set_pending = 0;

typedef struct
{
    btif_remote_device_t * remDev;
    btif_link_policy_t    policy;
} BT_SET_LINKPOLICY_REQ_T;
#define PENDING_SET_LINKPOLICY_REQ_BUF_CNT  5
static BT_SET_LINKPOLICY_REQ_T pending_set_linkpolicy_req[PENDING_SET_LINKPOLICY_REQ_BUF_CNT];

static uint8_t pending_set_linkpolicy_in_cursor = 0;
static uint8_t pending_set_linkpolicy_out_cursor = 0;

static void app_bt_print_pending_set_linkpolicy_req(void)
{
    LOG_PRINT_BT_LINK_MGR("Pending set link policy requests:");
    uint8_t index = pending_set_linkpolicy_out_cursor;
    while (index != pending_set_linkpolicy_in_cursor)
    {
        LOG_PRINT_BT_LINK_MGR("index %d RemDev 0x%x LinkPolicy %d", index,
            pending_set_linkpolicy_req[index].remDev,
            pending_set_linkpolicy_req[index].policy);
        index++;
        if (PENDING_SET_LINKPOLICY_REQ_BUF_CNT == index)
        {
            index = 0;
        }
    }
}

static void app_bt_push_pending_set_linkpolicy(btif_remote_device_t *remDev, btif_link_policy_t policy)
{
    // go through the existing pending list to see if the remDev is already in
    uint8_t index = pending_set_linkpolicy_out_cursor;
    while (index != pending_set_linkpolicy_in_cursor)
    {
        if (remDev == pending_set_linkpolicy_req[index].remDev)
        {
            pending_set_linkpolicy_req[index].policy = policy;
            return;
        }
        index++;
        if (PENDING_SET_LINKPOLICY_REQ_BUF_CNT == index)
        {
            index = 0;
        }
    }

    pending_set_linkpolicy_req[pending_set_linkpolicy_in_cursor].remDev = remDev;
    pending_set_linkpolicy_req[pending_set_linkpolicy_in_cursor].policy = policy;
    pending_set_linkpolicy_in_cursor++;
    if (PENDING_SET_LINKPOLICY_REQ_BUF_CNT == pending_set_linkpolicy_in_cursor)
    {
        pending_set_linkpolicy_in_cursor = 0;
    }

    app_bt_print_pending_set_linkpolicy_req();
}

static BT_SET_LINKPOLICY_REQ_T* app_bt_pop_pending_set_linkpolicy(void)
{
    if (pending_set_linkpolicy_out_cursor == pending_set_linkpolicy_in_cursor)
    {
        return NULL;
    }

    BT_SET_LINKPOLICY_REQ_T* ptReq = &pending_set_linkpolicy_req[pending_set_linkpolicy_out_cursor];
    pending_set_linkpolicy_out_cursor++;
    if (PENDING_SET_LINKPOLICY_REQ_BUF_CNT == pending_set_linkpolicy_out_cursor)
    {
        pending_set_linkpolicy_out_cursor = 0;
    }

    app_bt_print_pending_set_linkpolicy_req();
    return ptReq;
}



void app_bt_set_linkpolicy(btif_remote_device_t *remDev, btif_link_policy_t policy)
{
    bt_status_t ret =  btif_me_set_link_policy(remDev, policy);//  Me_SetLinkPolicy(remDev, policy);

    LOG_PRINT_BT_LINK_MGR("%s policy %d returns %d", __FUNCTION__, policy, ret);

    osapi_lock_stack();
    if (BT_STS_IN_PROGRESS == ret)
    {
        app_bt_push_pending_set_linkpolicy(remDev, policy);
    }
    osapi_unlock_stack();
}

void app_bt_accessmode_set(btif_accessible_mode_t mode)
{
    const btif_access_mode_info_t info = { BTIF_BT_DEFAULT_INQ_SCAN_INTERVAL,
                                    BTIF_BT_DEFAULT_INQ_SCAN_WINDOW,
                                    BTIF_BT_DEFAULT_PAGE_SCAN_INTERVAL,
                                    BTIF_BT_DEFAULT_PAGE_SCAN_WINDOW };
    bt_status_t status;
    osapi_lock_stack();
    gBT_DEFAULT_ACCESS_MODE = mode;

    status =  btif_me_set_accessible_mode(mode, &info);


    LOG_PRINT_BT_LINK_MGR("app_bt_accessmode_set status=0x%x",status);

    if(status == BT_STS_IN_PROGRESS)
        bt_access_mode_set_pending = 1;
    else
        bt_access_mode_set_pending = 0;
    osapi_unlock_stack();
}

void app_bt_accessmode_get(btif_accessible_mode_t *mode)
{
    osapi_lock_stack();
    *mode = gBT_DEFAULT_ACCESS_MODE;
    osapi_unlock_stack();
}

void app_bt_accessmode_set_scaninterval(uint32_t scan_intv)
{
    LOG_PRINT_BT_LINK_MGR("app_bt_accessmode_set_scaninterval = %x",scan_intv);

    return;
    osapi_lock_stack();
#ifdef CHIP_BEST2000
    if(hal_get_chip_metal_id()>=2){
        bt_drv_reg_op_scan_intv_set(scan_intv);
    }
#endif
    osapi_unlock_stack();
}

#ifdef __TWS__
extern  tws_dev_t  tws;
#endif
void PairingTransferToConnectable(void)
{
    int activeCons;
    osapi_lock_stack();
    activeCons = btif_me_get_activeCons();
    osapi_unlock_stack();

#ifdef __TWS__
    if(activeCons == 0 || ((activeCons==1) && (tws.mobile_sink.connected == false) && \
        (app_tws_get_conn_state() == TWS_MASTER_CONN_SLAVE)))
#else
    if(activeCons == 0)
#endif
    {
        log_event(EVENT_PAIRING_TIMEOUT);
        LOG_PRINT_BT_LINK_MGR("!!!Pairing timeout Non-discoverable or connectable\n");
        //app_bt_accessmode_set(BAM_NOT_ACCESSIBLE);
        app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, BTIF_BAM_NOT_ACCESSIBLE,0,0);

        #if defined(TEST_OVER_THE_AIR_ENANBLED)
        if (TWSFREE == twsBoxEnv.twsModeToStart) {
            // stop adv to close the anc tool connecting
            ble_tws_stop_all_activities();
        }
        #endif
    }
}

void app_reconnect_timeout_handle(void)
{
    int activeCons;
    osapi_lock_stack();
    activeCons = btif_me_get_activeCons();
    osapi_unlock_stack();

#ifdef __TWS__
    if(activeCons == 0 || ((activeCons==1) && (tws.mobile_sink.connected == false) && \
        (app_tws_get_conn_state() == TWS_MASTER_CONN_SLAVE)))
#else
    if(activeCons == 0)
#endif
    {
        LOG_PRINT_BT_CONN_MGR("!!!Reconnect timeout Non-discoverable or connectable\n");
        app_bt_accessmode_set(BTIF_BAM_NOT_ACCESSIBLE);
    }

    isInMobileBtSilentState = true;
}

#define APP_BT_SWITCHROLE_LIMIT (3)

btif_handler  app_bt_handler;
#ifdef __TWS__
uint8_t app_tws_auto_poweroff=0;
#endif

static void app_bt_global_handle(const btif_event_t *Event);

void app_tws_reconnect_stop_music(void)
{
    if (app_bt_device.a2dp_streamming[0] == 1) { // playing music now.
         LOG_PRINT_BT_CONN_MGR("%s bleReconnectPending = 1", __func__);
         tws.bleReconnectPending = 1;
         tws_player_stop(BT_STREAM_SBC);
    }
}

#if defined(__TWS__)
#define COUNT_OF_PENDING_REMOTE_DEV_TO_EXIT_SNIFF_MODE  8
static btif_remote_device_t* pendingRemoteDevToExitSniffMode[COUNT_OF_PENDING_REMOTE_DEV_TO_EXIT_SNIFF_MODE];
static uint8_t  maskOfRemoteDevPendingForExitingSniffMode = 0;
void app_check_pending_stop_sniff_op(void)
{
    if (maskOfRemoteDevPendingForExitingSniffMode > 0)
    {
        for (uint8_t index = 0;index < COUNT_OF_PENDING_REMOTE_DEV_TO_EXIT_SNIFF_MODE;index++)
        {
            if (maskOfRemoteDevPendingForExitingSniffMode & (1 << index))
            {
                btif_remote_device_t* remDev = pendingRemoteDevToExitSniffMode[index];
                if (! btif_me_is_op_in_progress(remDev) )
                {
                    if ( btif_me_get_remote_device_state(remDev) == BTIF_BDS_CONNECTED){
                        if ( btif_me_get_current_mode(remDev) == BTIF_BLM_SNIFF_MODE){
                            LOG_PRINT_BT_LINK_MGR("!!! stop sniff currmode:%d\n", btif_me_get_current_mode(remDev));
                            bt_status_t ret = btif_me_stop_sniff(remDev);
                            LOG_PRINT_BT_LINK_MGR("Return status %d", ret);
                            if (BT_STS_IN_PROGRESS != ret)
                            {
                                maskOfRemoteDevPendingForExitingSniffMode &= (~(1<<index));
                                break;
                            }
                        }
                    }
                }
            }
        }

        if (maskOfRemoteDevPendingForExitingSniffMode > 0)
        {
            osapi_notify_evm();
        }
    }
}

void app_add_pending_stop_sniff_op(btif_remote_device_t* remDev)
{
    for (uint8_t index = 0;index < COUNT_OF_PENDING_REMOTE_DEV_TO_EXIT_SNIFF_MODE;index++)
    {
        if (maskOfRemoteDevPendingForExitingSniffMode & (1 << index))
        {
            if (pendingRemoteDevToExitSniffMode[index] == remDev)
            {
                return;
            }
        }
    }

    for (uint8_t index = 0;index < COUNT_OF_PENDING_REMOTE_DEV_TO_EXIT_SNIFF_MODE;index++)
    {
        if (0 == (maskOfRemoteDevPendingForExitingSniffMode & (1 << index)))
        {
            pendingRemoteDevToExitSniffMode[index] = remDev;
            maskOfRemoteDevPendingForExitingSniffMode |= (1 << index);
        }
    }
}

void app_tws_stop_sniff(void)
{
    a2dp_stream_t *tws_source;
    btif_remote_device_t *remDev;

    tws_source = app_tws_get_tws_source_stream();
    remDev = btif_a2dp_get_remote_device(tws_source);
    if ( btif_me_get_remote_device_state(remDev) == BTIF_BDS_CONNECTED){
        if (btif_me_get_current_mode(remDev) == BTIF_BLM_SNIFF_MODE){
            LOG_PRINT_BT_LINK_MGR("!!! stop tws sniff currmode:%d\n", btif_me_get_current_mode(remDev));
            bt_status_t ret =   btif_me_stop_sniff(remDev);
            LOG_PRINT_BT_LINK_MGR("Return status %d", ret);
            if (BT_STS_IN_PROGRESS == ret)
            {
                app_add_pending_stop_sniff_op(remDev);
            }
        }
    }
}
#endif


static void switchrole_delay_timer_cb(void const *n)
{
    btif_remote_device_t *remDev = (btif_remote_device_t *)(*(uint32_t *)n);

    if (remDev){
        bt_drv_reg_op_encryptchange_errcode_reset( btif_me_get_remote_device_hci_handle(remDev));
        app_start_custom_function_in_bt_thread((uint32_t)remDev, 0, (uint32_t)btif_me_switch_role);
    }
}

uint32_t mobile_device_switchrole_delay_timer_argument;
osTimerId mobile_device_switchrole_delay_timer_id = NULL;
osTimerDef (MOBILE_DEVICE_SWITCHROLE_DELAY_TIMER, switchrole_delay_timer_cb);

static void mobile_device_switchrole_delay_timer_init(void)
{
    mobile_device_switchrole_delay_timer_id =
        osTimerCreate(osTimer(MOBILE_DEVICE_SWITCHROLE_DELAY_TIMER), osTimerOnce, &mobile_device_switchrole_delay_timer_argument);
}

static void mobile_device_switchrole_start(uint32_t ms, btif_remote_device_t *remDev)
{
    mobile_device_switchrole_delay_timer_argument = (uint32_t)remDev;
    osTimerStop(mobile_device_switchrole_delay_timer_id);
    osTimerStart(mobile_device_switchrole_delay_timer_id, ms);
}

static void mobile_device_switchrole_cancel(void)
{
    mobile_device_switchrole_delay_timer_argument = 0;
    osTimerStop(mobile_device_switchrole_delay_timer_id);
}

uint32_t tws_switchrole_delay_timer_argument;
osTimerId tws_switchrole_delay_timer_id = NULL;
osTimerDef (TWS_SWITCHROLE_DELAY_TIMER, switchrole_delay_timer_cb);

static void tws_switchrole_delay_timer_init(void)
{
    tws_switchrole_delay_timer_id =
        osTimerCreate(osTimer(TWS_SWITCHROLE_DELAY_TIMER), osTimerOnce, &tws_switchrole_delay_timer_argument);
}

static void tws_switchrole_start(uint32_t ms, btif_remote_device_t *remDev)
{
    tws_switchrole_delay_timer_argument = (uint32_t)remDev;
    osTimerStop(tws_switchrole_delay_timer_id);
    osTimerStart(tws_switchrole_delay_timer_id, ms);
}

static void tws_switchrole_cancel(void)
{
    tws_switchrole_delay_timer_argument = 0;
    osTimerStop(tws_switchrole_delay_timer_id);
}

static btif_remote_device_t *app_earphone_remDev = NULL;
static uint8_t mobile_device_switchrole_cnt = 0;
static uint8_t tws_switchrole_cnt = 0;

static void app_bt_earphone_role_process(const btif_event_t *Event)
{
    //on phone connecting

    uint8_t event_type = btif_me_get_callback_event_type(Event);
    switch (event_type) {
        case BTIF_BTEVENT_ROLE_CHANGE:
            LOG_PRINT_BT_LINK_MGR("%s BTEVENT_ROLE_CHANGE errCode:0x%x role:%d,newrole=%x",__func__,
                btif_me_get_callback_event_err_code( Event), btif_me_get_callback_event_rem_dev_role(Event), btif_me_get_callback_event_role_change_new_role(Event));
            switch (btif_me_get_callback_event_role_change_new_role(Event)) {
                case BTIF_BCR_MASTER:
                    if (++mobile_device_switchrole_cnt<=APP_BT_SWITCHROLE_LIMIT){
                        mobile_device_switchrole_start(300,   btif_me_get_callback_event_rem_dev(Event));
                    }
                    break;
                case BTIF_BCR_SLAVE:
                    mobile_device_switchrole_cancel();
                    app_bt_set_linkpolicy( btif_me_get_callback_event_rem_dev(Event), BTIF_BLP_SNIFF_MODE);
                    break;
                case BTIF_BCR_ANY:
                    break;
                case BTIF_BCR_UNKNOWN:
                    break;
                default:
                    break;
            }
            LOG_PRINT_BT_LINK_MGR("Exit BTEVENT_ROLE_CHANGE handling.");
            break;
        default:
           break;
        }
}



void app_bt_disconnect_sco_link(void)
{
    hf_chan_handle_t hf_channel_tmp = NULL;
    hf_channel_tmp =app_bt_device.hf_channel[app_bt_device.curr_hf_channel_id];
    if(app_bt_is_hfp_audio_on())
    {
        LOG_PRINT_BT_CONN_MGR("Disconnect Sco !!!");
        btif_hf_disc_audio_link(hf_channel_tmp);
    }
    else
    {
        LOG_PRINT_BT_CONN_MGR("Stop Sco !!!");
        app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_STOP,BT_STREAM_VOICE,BT_DEVICE_ID_1,MAX_RECORD_NUM,0,0);
    }
}

void bt_global_mobile_disconnected_ind_common_handler(const btif_event_t *Event)
{
    LOG_PRINT_BT_LINK_MGR("Link with mobile is disconnected, BD address is:");
    LOG_PRINT_BT_CONN_MGR_DUMP8("0x%02x ",  btif_me_get_callback_event_rem_dev_bd_addr(Event)->address, BTIF_BD_ADDR_SIZE);

    mobile_device_switchrole_cnt = 0;
    mobile_device_switchrole_cancel();
#ifdef  __ALLOW_TWS_RECONNECT_WITHOUT_MOBILE_CONNECTED__
    //MobileDevPaired = false;
    //slaveDisconMobileFlag=false;
    if(TRUE==slaveDisconMobileFlag)
    {
        app_start_custom_function_in_bt_thread(0,
        0, (uint32_t)app_start_ble_tws_connecting);
    }
#endif
    app_earphone_remDev = NULL;

    // update the state of connection
    CLEAR_CONN_STATE_LINK(mobileDevConnectionState);
}

void app_bt_get_mobile_device_active_profiles(uint8_t* bdAddr)
{
    CLEAR_MOBILE_DEV_ACTIVE_PROFILE();

    btdevice_profile *btdevice_plf_p = (btdevice_profile *)app_bt_profile_active_store_ptr_get(bdAddr);
    if (btdevice_plf_p->a2dp_act)
    {
        SET_MOBILE_DEV_ACTIVE_PROFILE(BT_PROFILE_A2DP);
    }

    if (btdevice_plf_p->hfp_act)
    {
        SET_MOBILE_DEV_ACTIVE_PROFILE(BT_PROFILE_HFP);
    }

    if (btdevice_plf_p->hsp_act)
    {
        SET_MOBILE_DEV_ACTIVE_PROFILE(BT_PROFILE_HSP);
    }
}

#define REMOTE_DISCONNECT_REQ_QUEUE_LEN     5
static btif_remote_device_t* remDevToDisconnect[REMOTE_DISCONNECT_REQ_QUEUE_LEN];
static uint8_t  remoteDisconnectReqQueueInCursor = 0;
static uint8_t  remoteDisconnectReqQueueOutCursor = 0;
static uint8_t  pendingRemoteDisconnectReqCount = 0;

#define REMDEV_DISCONNECT_DELAY_IN_MS   200
osTimerId   remDev_disconnect_delay_timer_id;
static void remDev_disconnect_delay_timer_cb(void const *n);
osTimerDef (REMDEV_DISCONNECT_DELAY_TIMER, remDev_disconnect_delay_timer_cb);

static void app_bt_pop_remDev_disconnect_req(btif_remote_device_t** remDev);
static bool app_bt_push_remDev_disconnect_req(btif_remote_device_t* remDev);

static void remDev_disconnect_delay_timer_cb(void const *n)
{
    btif_remote_device_t* remDevToDisconnect;
    app_bt_pop_remDev_disconnect_req(&remDevToDisconnect);
    if (NULL != remDevToDisconnect)
    {
        app_start_custom_function_in_bt_thread((uint32_t)remDevToDisconnect,
                    0, (uint32_t)app_bt_disconnect_link);
    }
}

static bool app_bt_push_remDev_disconnect_req(btif_remote_device_t* remDev)
{
    uint8_t currentCursor = remoteDisconnectReqQueueInCursor;
    for (uint8_t index = 0;index < pendingRemoteDisconnectReqCount;index++)
    {
        if (remDevToDisconnect[currentCursor] == remDev)
        {
            return true;
        }
        currentCursor++;
        if (REMOTE_DISCONNECT_REQ_QUEUE_LEN == currentCursor)
        {
            currentCursor = 0;
        }
    }

    remDevToDisconnect[currentCursor] = remDev;
    remoteDisconnectReqQueueInCursor = currentCursor;
    pendingRemoteDisconnectReqCount++;

    LOG_PRINT_BT_CONN_MGR("Push remDev 0x%x into the cursor %d of the queue, now %d disconn reqs are pending.",
        remDev, remoteDisconnectReqQueueInCursor, pendingRemoteDisconnectReqCount);

    ASSERT(pendingRemoteDisconnectReqCount < REMOTE_DISCONNECT_REQ_QUEUE_LEN, "The remote disconnect request queue is full!!!");
    return false;
}

static void app_bt_pop_remDev_disconnect_req(btif_remote_device_t** remDev)
{
    if (pendingRemoteDisconnectReqCount > 0)
    {
        *remDev = remDevToDisconnect[remoteDisconnectReqQueueOutCursor];
        remoteDisconnectReqQueueOutCursor++;
        if (REMOTE_DISCONNECT_REQ_QUEUE_LEN == remoteDisconnectReqQueueOutCursor)
        {
            remoteDisconnectReqQueueOutCursor = 0;
        }
        pendingRemoteDisconnectReqCount--;
    }
    else
    {
        *remDev = NULL;
    }

    LOG_PRINT_BT_CONN_MGR("Pop remDev 0x%x from cursor %d of the queue, now %d disconn reqs are pending.",
        *remDev, remoteDisconnectReqQueueOutCursor, pendingRemoteDisconnectReqCount);
}

extern btif_remote_device_t* mobileBtRemDev;

void app_bt_disconnect_link(btif_remote_device_t* remDev)
{
    BT_FUNCTION_ROLE_E functionModeToDisconnect;
    if ((app_tws_is_master_mode()) ||
        (app_tws_is_freeman_mode()))
    {
        if (mobileBtRemDev == remDev)
        {
            functionModeToDisconnect = BT_FUNCTION_ROLE_MOBILE;
        }
        else
        {
            functionModeToDisconnect = BT_FUNCTION_ROLE_SLAVE;
        }
    }
    else
    {
        functionModeToDisconnect= BT_FUNCTION_ROLE_MASTER;
    }

    log_event_2(EVENT_DISCONNECT_BT_CONNECTION, functionModeToDisconnect, 3& btif_me_get_remote_device_hci_handle(remDev));

    bt_status_t ret ;
    LOG_PRINT_BT_CONN_MGR("Disconnect stat %d, optype %d:",   btif_me_get_remote_device_state(remDev), btif_me_get_remote_device_op_optype(remDev));

    ret =  btif_me_force_disconnect_link_with_reason(0, remDev, BTIF_BEC_USER_TERMINATED, TRUE);
    LOG_PRINT_BT_CONN_MGR("Disconnect ret %d ,stat %d, optype %d:", ret,   btif_me_get_remote_device_state(remDev),btif_me_get_remote_device_op_optype(remDev));
    LOG_PRINT_BT_CONN_MGR_DUMP8("0x%02x ", btif_me_get_remote_device_bdaddr(remDev)->address, BTIF_BD_ADDR_SIZE);

    if (BT_STS_IN_PROGRESS == ret)
    {
        // it means connection operation is in progress, so we push this disconnection request into the queue
        // and do it in the delay timer handler
        bool isAlreadyIn = app_bt_push_remDev_disconnect_req(remDev);
        if (!isAlreadyIn)
        {
            osTimerStart(remDev_disconnect_delay_timer_id, REMDEV_DISCONNECT_DELAY_IN_MS);
        }
    }
    else
    {
        ASSERT(BT_STS_SUCCESS == ret, "Disconnection try failed with ret %d!!!", ret);
    }
}

void app_bt_write_linksupervtimeout(btif_remote_device_t* remDev)
{
   // ME_WriteLinkSupervTimeout(remDev->hciHandle & 0xff, 0x2000);
    btif_me_write_link_super_vtimeout(btif_me_get_remote_device_hci_handle(remDev) & 0xff, 0x2000);
}

void bt_global_mobile_connected_ind_common_handler(const btif_event_t *Event)
{
    if (app_tws_is_master_mode())
    {
        if (!memcmp((void *)btif_me_get_callback_event_rem_dev_bd_addr(Event), (void *)tws_mode_ptr()->slaveAddr.address, BTIF_BD_ADDR_SIZE) ||
            !memcmp((void *)btif_me_get_callback_event_rem_dev_bd_addr(Event), (void *)tws_mode_ptr()->masterAddr.address, BTIF_BD_ADDR_SIZE))
        {
            LOG_PRINT_BT_CONN_MGR("Master is waiting for connection from mobile, but it's connected with TWS by corner case! Disconnect.");

            app_start_custom_function_in_bt_thread((uint32_t)btif_me_get_callback_event_rem_dev(Event),
                        0, (uint32_t)app_bt_disconnect_link);
            app_tws_role_switch_remove_bt_info_via_bd_addr(btif_me_get_callback_event_rem_dev_bd_addr(Event)->address);
            return;
        }
    }
#if FPGA==0
    app_stop_auto_power_off_supervising();
#endif
    LOG_PRINT_BT_CONN_MGR("Link with mobile is created, BD address is:");
    LOG_PRINT_BT_CONN_MGR_DUMP8("0x%02x ", btif_me_get_callback_event_rem_dev_bd_addr(Event), BTIF_BD_ADDR_SIZE);

    // save the bd address of the mobile device
    memcpy(mobileDevBdAddr.address, (void *)btif_me_get_callback_event_rem_dev_bd_addr(Event), BTIF_BD_ADDR_SIZE);

    mobileBtRemDev = btif_me_get_callback_event_rem_dev(Event);

    isMobileDevPaired = true;

    // update the state of connection
    SET_CONN_STATE_LINK(mobileDevConnectionState);

    app_earphone_remDev = btif_me_get_callback_event_rem_dev(Event);

    LOG_PRINT_BT_CONN_MGR("errCode %d role %d",   btif_me_get_callback_event_err_code( Event), btif_me_get_callback_event_rem_dev_role(Event));
    if (is_simulating_reconnecting_in_progress())
    {
        return;
    }

    if(btif_me_get_callback_event_err_code( Event) == 0) {
        switch (btif_me_get_callback_event_rem_dev_role(Event)) {
            case BTIF_BCR_MASTER:
                LOG_PRINT_BT_LINK_MGR("mobile_device_switchrole_cnt %d", mobile_device_switchrole_cnt);
                app_bt_set_linkpolicy(btif_me_get_callback_event_rem_dev(Event),BTIF_BLP_MASTER_SLAVE_SWITCH|BTIF_BLP_SNIFF_MODE);
                if (++mobile_device_switchrole_cnt<=APP_BT_SWITCHROLE_LIMIT)
                {
                    LOG_PRINT_BT_LINK_MGR("Switch role to slave");
                    btif_me_switch_role(btif_me_get_callback_event_rem_dev(Event));
                }
                break;
            case BTIF_BCR_SLAVE:
            case BTIF_BCR_PSLAVE:
                app_bt_set_linkpolicy(btif_me_get_callback_event_rem_dev(Event),BTIF_BLP_SNIFF_MODE);
                break;
            case BTIF_BCR_ANY:
            case BTIF_BCR_UNKNOWN:
            default:
                break;
        }
    }
}

void bt_global_tws_connected_ind_common_handler(const btif_event_t *Event)
{
#ifdef __ENABLE_IN_BOX_STATE_HANDLE__
    if (!app_is_in_charger_box())
#endif
    {
        ble_tws_stop_all_activities();
    }

    // connected with slave
    if (IS_CONNECTING_SLAVE())
    {
        // check the connected bd address, if not pending connected slave, disconnect
        if (memcmp((void *)btif_me_get_callback_event_rem_dev_bd_addr(Event), (void *)tws_mode_ptr()->slaveAddr.address, BTIF_BD_ADDR_SIZE))
        {
            LOG_PRINT_BT_CONN_MGR("master can only connect with slave!disconnect.");
            app_start_custom_function_in_bt_thread((uint32_t)btif_me_get_callback_event_rem_dev(Event),
                        0, (uint32_t)app_bt_disconnect_link);
            app_tws_role_switch_remove_bt_info_via_bd_addr(btif_me_get_callback_event_rem_dev_bd_addr(Event)->address);
            return;
        }

        slaveBtRemDev = btif_me_get_callback_event_rem_dev(Event);

        tws_switchrole_cnt = 0;
        tws_switchrole_cancel();

        LOG_PRINT_BT_CONN_MGR("Link with TWS slave is created, BD address is:");
        LOG_PRINT_BT_CONN_MGR_DUMP8("0x%02x ",     btif_me_get_callback_event_rem_dev_bd_addr(Event)->address, BTIF_BD_ADDR_SIZE);

        // save the tws mode and the peer bd address
        app_tws_store_info_to_nv(TWSMASTER, btif_me_get_callback_event_rem_dev_bd_addr(Event)->address);
        if (!is_simulating_reconnecting_in_progress())
        {
            app_start_custom_function_in_bt_thread((uint32_t)btif_me_get_callback_event_rem_dev(Event),
                            0, (uint32_t)app_bt_write_linksupervtimeout);
        }
    }
    // connected with master
    else
    {
        // check the connected bd address, if not pending connected master, disconnect
#ifdef __TWS_PAIR_DIRECTLY__
        //for tws master search slave ui
        memcpy(tws_mode_ptr()->masterAddr.address,btif_me_get_callback_event_rem_dev_bd_addr(Event)->address, BTIF_BD_ADDR_SIZE);
#endif
        if (memcmp((void *)btif_me_get_callback_event_rem_dev_bd_addr(Event)->address, (void *)tws_mode_ptr()->masterAddr.address, BTIF_BD_ADDR_SIZE))
        {
            LOG_PRINT_BT_CONN_MGR("slave can only connect with master!disconnect.");
            app_start_custom_function_in_bt_thread((uint32_t)btif_me_get_callback_event_rem_dev(Event),
                        0, (uint32_t)app_bt_disconnect_link);
            app_tws_role_switch_remove_bt_info_via_bd_addr(btif_me_get_callback_event_rem_dev_bd_addr(Event)->address);
            return;
        }

        masterBtRemDev = btif_me_get_callback_event_rem_dev(Event);

        if (is_simulating_reconnecting_in_progress())
        {
            //masterBtRemDev->role = BTIF_BCR_MASTER;
        btif_me_set_remote_device_role(masterBtRemDev,BTIF_BCR_MASTER);
        }
        else
        {
#ifdef  CHIP_BEST2300
            //bt_drv_reg_op_reset_sniffer_env();
            //if(bt_drv_reg_op_ld_sniffer_env_monitored_dev_state_get())
            //bool ret=false;
            //ret=bt_drv_reg_op_ld_sniffer_env_monitored_dev_state_get();
            // TRACE("##ret:%d",ret);
#ifdef __TWS_PAIR_DIRECTLY__       
            TRACE_HIGHPRIO("#bt_drv_reg_op_ld_sniffer_env_monitored_dev_state_set");
            bt_drv_reg_op_ld_sniffer_env_monitored_dev_state_set(0);                
#endif
            btif_me_set_sniffer_env(1, SNIFFER_ROLE, (uint8_t *)btif_me_get_remote_device_bdaddr(masterBtRemDev)->address, tws_mode_ptr()->slaveAddr.address);
#elif defined(CHIP_BEST2000)
            btif_me_set_sniffer_env(1, SNIFFER_ROLE, tws_mode_ptr()->slaveAddr.address, tws_mode_ptr()->slaveAddr.address);
#endif
        }

#if FPGA==0
        app_stop_auto_power_off_supervising();
#endif

        LOG_PRINT_BT_CONN_MGR("Link with TWS master is created, BD address is:");
        LOG_PRINT_BT_CONN_MGR_DUMP8("0x%02x ", btif_me_get_callback_event_rem_dev_bd_addr(Event)->address, BTIF_BD_ADDR_SIZE);
        app_bt_accessmode_set_scaninterval(0x80);

        // update the TWS mode and the BD addresses of master and slave
        app_tws_store_info_to_nv(TWSSLAVE, tws_mode_ptr()->masterAddr.address);
    }

    // update the state of connection
    SET_CONN_STATE_LINK(twsConnectionState);
}

void bt_global_tws_disconnected_ind_common_handler(const btif_event_t *Event)
{
    if ((!app_tws_is_freeman_mode()) &&
        ((!memcmp(btif_me_get_callback_event_disconnect_rem_dev_bd_addr(Event)->address, tws_mode_ptr()->slaveAddr.address, BTIF_BD_ADDR_SIZE)) ||
        (!memcmp(btif_me_get_callback_event_disconnect_rem_dev_bd_addr(Event)->address, tws_mode_ptr()->masterAddr.address, BTIF_BD_ADDR_SIZE))))
    {
        if (app_tws_is_master_mode())
        {
            LOG_PRINT_BT_CONN_MGR("Link with TWS slave is disconnected, BD address is:");
        }
        else
        {
            LOG_PRINT_BT_CONN_MGR("Link with TWS master is disconnected, BD address is:");
        }
        LOG_PRINT_BT_CONN_MGR_DUMP8("0x%02x ", btif_me_get_callback_event_rem_dev_bd_addr(Event)->address, BTIF_BD_ADDR_SIZE);

#ifdef BES_OTA_TWS
	ota_check_and_reboot_to_use_new_image();
#endif

#ifdef __TWS_PAIR_DIRECTLY__
        if((BTIF_BEC_USER_TERMINATED == btif_me_get_callback_event_disconnect_rem_dev_disc_reason(Event))
            &&(CONN_OP_STATE_MACHINE_ROLE_SWITCH != CURRENT_CONN_OP_STATE_MACHINE()))
        {
            TRACE("shut down !!!");
            app_shutdown();
        }
#endif

        if (slaveBtRemDev){
            slaveBtRemDev  = NULL;
        }

        if (masterBtRemDev){
            masterBtRemDev = NULL;
        }

        tws_switchrole_cnt = 0;
        tws_switchrole_cancel();


#ifdef __ENABLE_IN_BOX_STATE_HANDLE__
        app_tws_event_exchange_supervisor_timer_cancel();
#endif
        // update the state of connection
        CLEAR_CONN_STATE_LINK(twsConnectionState);
    }
}

void bt_global_connected_handler(const btif_event_t *Event)
{
    if (BTIF_BEC_NO_ERROR == btif_me_get_callback_event_err_code( Event)) // connection has been successfully created
    {
        LOG_PRINT_BT_CONN_MGR("Enter %s current op state %d", __FUNCTION__, conn_op_state);
#if  defined(_AUTO_TEST_)
        AUTO_TEST_SEND("Connect ok.");
#endif

#ifndef __BT_ONE_BRING_TWO__
        // Disconnect if meeting following conditions:
        // 1. not during the pairing
        // 2. the connected device is not in nv
        if ((CONN_OP_STATE_MACHINE_RE_PAIRING != CURRENT_CONN_OP_STATE_MACHINE()) &&
            (CONN_OP_STATE_MACHINE_SIMPLE_REPAIRING != CURRENT_CONN_OP_STATE_MACHINE()))
        {
            btif_device_record_t record;
            if (BT_STS_SUCCESS != btif_sec_find_device_record(btif_me_get_callback_event_rem_dev_bd_addr(Event),
                &record))
            {
                LOG_PRINT_BT_CONN_MGR("The device is connected by a un-saved device when the pairing is not on-going!");
                LOG_PRINT_BT_CONN_MGR("Disconnect it!");
                app_start_custom_function_in_bt_thread((uint32_t)btif_me_get_callback_event_rem_dev(Event),
                    0, (uint32_t)app_bt_disconnect_link);
                app_tws_role_switch_remove_bt_info_via_bd_addr(btif_me_get_callback_event_rem_dev_bd_addr(Event)->address);
                return;
            }
        }
#endif

        uint8_t isConnectedWithTws = false;
        uint8_t isConnectedWithMobile = false;

#ifdef __TWS_PAIR_DIRECTLY__
        //compare remote addr with local addr,if the same,it is tws peer dev
        char * rem_device_address = (char *) btif_me_get_callback_event_rem_dev_bd_addr(Event);
        if(!memcmp(&rem_device_address[3],&bt_addr[3], BD_ADDR_LEN-3))
        {
            TRACE("connecting tws !!");
            isConnectedWithTws=true;
            //memcpy(app_tws_get_env_pointer()->peer_addr.addr, int, size_t);
            memcpy(tws.peer_addr.address, rem_device_address, BD_ADDR_LEN);
        }
        else
        {
#ifdef __ALLOW_TWS_RECONNECT_WITHOUT_MOBILE_CONNECTED__

            if(app_tws_is_slave_mode()&&IS_WAITING_FOR_MASTER_CONNECTION())
            {
                //for twsslave con phone ui
                LOG_PRINT_BT_CONN_MGR("twsslave waiting for master con ,refuse mobile con!");
                LOG_PRINT_BT_CONN_MGR("Disconnect it!");
                app_start_custom_function_in_bt_thread((uint32_t)btif_me_get_callback_event_rem_dev(Event),
                    0, (uint32_t)app_bt_disconnect_link);
                 app_tws_role_switch_remove_bt_info_via_bd_addr(btif_me_get_callback_event_rem_dev_bd_addr(Event)->address);
                return ;
            }   
#endif
            TRACE("connecting mobilephone !!");
            isConnectedWithMobile=true;
        }
#else

        btif_device_record_t record;
        bt_status_t ret = btif_sec_find_device_record(btif_me_get_callback_event_rem_dev_bd_addr(Event), &record);
        if (BT_STS_SUCCESS == ret)
        {
            if ((!memcmp(btif_me_get_callback_event_rem_dev_bd_addr(Event)->address, tws_mode_ptr()->slaveAddr.address, BD_ADDR_LEN)) ||
                (!memcmp(btif_me_get_callback_event_rem_dev_bd_addr(Event)->address, tws_mode_ptr()->masterAddr.address, BD_ADDR_LEN)))
            {
                isConnectedWithTws = true;
            }
            else
            {
                isConnectedWithMobile = true;
            }

            LOG_PRINT_BT_CONN_MGR("isConnectedWithTws %d isConnectedWithMobile %d", isConnectedWithTws,
                isConnectedWithMobile);
        }
#endif

        if (isConnectedWithMobile){
            if (!(IS_WAITING_FOR_MOBILE_CONNECTION() || IS_CONNECTING_MOBILE())){
                LOG_PRINT_BT_CONN_MGR("%s warning isMoble but not in connectMoble state ", __FUNCTION__);
#ifdef __TWS_PAIR_DIRECTLY__
#if 0
                //for twsslave con phone ui
                if(app_tws_is_slave_mode())
                {
                    LOG_PRINT_BT_CONN_MGR("fix slave connect phone con state machine !");
                    set_conn_op_state(CONN_OP_STATE_WAITING_FOR_MOBILE_CONNECTION);
                }
#endif
#endif
            }
        }

        if (isConnectedWithTws){
            if (!(IS_WAITING_FOR_MASTER_CONNECTION() || IS_CONNECTING_MASTER() || IS_CONNECTING_SLAVE())){
                LOG_PRINT_BT_CONN_MGR("%s warning isTws but not in connectTws state ", __FUNCTION__);
            }
            app_tws_config_master_wsco_by_connect_state(false, NONE_MODE);
        }

        // connected with mobile device
        if (IS_WAITING_FOR_MOBILE_CONNECTION() || IS_CONNECTING_MOBILE() || isConnectedWithMobile)
        {
            conn_start_connecting_mobile_supervising();

            bt_global_mobile_connected_ind_common_handler(Event);

            simulated_reconnecting_post_handler_after_connection_is_created();
            app_tws_check_max_slot_setting();
            mobile_device_connection_state_updated_handler(BTIF_BEC_NO_ERROR);

            log_event_3(EVENT_BT_CONNECTION_CREATED, BT_FUNCTION_ROLE_MOBILE,
                3&( btif_me_get_remote_device_hci_handle( btif_me_get_callback_event_rem_dev(Event))),  btif_me_get_callback_event_rem_dev_bd_addr(Event)->address);
        }

        if (IS_WAITING_FOR_MASTER_CONNECTION() || IS_CONNECTING_MASTER() || IS_CONNECTING_SLAVE()
            || isConnectedWithTws)
        {
            bt_global_tws_connected_ind_common_handler(Event);

            simulated_reconnecting_post_handler_after_connection_is_created();
            btif_L2CAP_BuildConnectionToTwsReservedChannel(btif_me_get_callback_event_rem_dev(Event));
            tws_connection_state_updated_handler(BTIF_BEC_NO_ERROR);

            if (IS_WAITING_FOR_MASTER_CONNECTION() || IS_CONNECTING_MASTER())
            {
                log_event_3(EVENT_BT_CONNECTION_CREATED, BT_FUNCTION_ROLE_MASTER,
                    3&(btif_me_get_remote_device_hci_handle(btif_me_get_callback_event_rem_dev(Event))), btif_me_get_callback_event_rem_dev_bd_addr(Event)->address);
            }
            else
            {
                log_event_3(EVENT_BT_CONNECTION_CREATED, BT_FUNCTION_ROLE_SLAVE,
                    3&(btif_me_get_remote_device_hci_handle(btif_me_get_callback_event_rem_dev(Event))), btif_me_get_callback_event_rem_dev_bd_addr(Event)->address);
            }
        }
    }else if (BTIF_BEC_PAGE_TIMEOUT == btif_me_get_callback_event_err_code( Event)){
        bt_drv_reg_op_set_swagc_mode(0);
        if (IS_CONNECTING_SLAVE() &&
            is_tws_connection_creation_supervisor_need_retry()){
            btif_device_record_t record;
            bt_status_t ret = btif_sec_find_device_record(btif_me_get_callback_event_rem_dev_bd_addr(Event), &record);
            if (BT_STS_SUCCESS == ret)
            {
                if (!memcmp(btif_me_get_callback_event_rem_dev_bd_addr(Event)->address, tws_mode_ptr()->slaveAddr.address, BD_ADDR_LEN))
                {
                    LOG_PRINT_BT_CONN_MGR("%s in CONNECTING_SLAVE process try it again", __FUNCTION__);
                    app_start_custom_function_in_bt_thread(CONNECTING_SLAVE_TIMEOUT_RECONNECT_IN_MS, false, (uint32_t)conn_start_connecting_slave);
                }
            }
        }
    }
}

btif_remote_device_t* authenticate_failure_remote_dev = NULL;
bool disconn_voice_flag = false;



void bt_global_disconnected_ind_handler(const btif_event_t *Event)
{
    LOG_PRINT_BT_CONN_MGR("BTEVENT_LINK_DISCONNECT eType=%d,reason=0x%02x",btif_me_get_callback_event_type(Event), btif_me_get_callback_event_disconnect_rem_dev_disc_reason_saved(Event));
    LOG_PRINT_BT_CONN_MGR("Disconnected remote BD addr:");
    LOG_PRINT_BT_CONN_MGR_DUMP8("0x%02x ", btif_me_get_callback_event_rem_dev_bd_addr(Event)->address, BTIF_BD_ADDR_SIZE);
    if (!((BTIF_BEC_USER_TERMINATED == btif_me_get_callback_event_disconnect_rem_dev_disc_reason_saved(Event)) ||
                    (BTIF_BEC_LOCAL_TERMINATED == btif_me_get_callback_event_disconnect_rem_dev_disc_reason(Event))))  {
        LOG_PRINT_BT_CONN_MGR("%s disconnect reason :%d", __func__, btif_me_get_callback_event_disconnect_rem_dev_disc_reason_saved(Event));
        disconn_voice_flag = true;
    }

    BT_FUNCTION_ROLE_E disconnectedBtRole = BT_FUNCTION_ROLE_MOBILE;

    // disconnected with mobile device?
    if (IS_CONN_STATE_LINK_ON(mobileDevConnectionState) &&
        (!memcmp(  btif_me_get_callback_event_disconnect_rem_dev_bd_addr(Event)->address, mobileDevBdAddr.address, BTIF_BD_ADDR_SIZE)))
    {
     btdrv_seq_bak_mode(1,  btif_me_get_remote_device_hci_handle(btif_me_get_callback_event_rem_dev(Event))-0x80);

        disconnectedBtRole = BT_FUNCTION_ROLE_MOBILE;
        bt_global_mobile_disconnected_ind_common_handler(Event);
        mobile_device_connection_state_updated_handler(btif_me_get_callback_event_disconnect_rem_dev_disc_reason_saved(Event));
    }
    // disconnected with tws?
    else if (IS_CONN_STATE_LINK_ON(twsConnectionState))
    {
        if (app_tws_is_master_mode())
        {
            disconnectedBtRole = BT_FUNCTION_ROLE_SLAVE;
        }
        else
        {
            disconnectedBtRole = BT_FUNCTION_ROLE_MASTER;
        }

        bt_global_tws_disconnected_ind_common_handler(Event);
        btif_l2cap_close_tws_reserved_channel();
        tws_connection_state_updated_handler(btif_me_get_callback_event_disconnect_rem_dev_disc_reason_saved(Event));
    }

    log_event_3(EVENT_BT_CONNECTION_DOWN, disconnectedBtRole, 3& btif_me_get_remote_device_hci_handle( btif_me_get_callback_event_disconnect_rem_dev(Event)),
        btif_me_get_callback_event_disconnect_rem_dev_disc_reason_saved(Event));

#ifdef BT_XTAL_SYNC
    bt_term_xtal_sync_default();
#endif
    if ( btif_me_get_callback_event_disconnect_rem_dev_disc_reason_saved(Event) == BTIF_BEC_AUTHENTICATE_FAILURE){
       btif_sec_delete_device_record(btif_me_get_callback_event_rem_dev_bd_addr(Event));
    }

    if (authenticate_failure_remote_dev == btif_me_get_callback_event_rem_dev(Event)){
        authenticate_failure_remote_dev = NULL;
        btif_sec_delete_device_record(btif_me_get_callback_event_rem_dev_bd_addr(Event));
    }
}

bool app_bt_is_tws_role_switch_timeout(void)
{
    return tws_switchrole_cnt >= APP_BT_SWITCHROLE_LIMIT;
}

extern uint8_t esco_retx_nb;
extern uint8_t slave_sco_active;
uint8_t app_sco_sniff_conflict_check_flag = 0;
extern "C" void app_notify_stack_ready();
static void app_bt_global_handle(const btif_event_t *Event)
{

    uint8_t etype = btif_me_get_callback_event_type(Event);
    btif_remote_device_t *remDev;

    switch (etype) {
        case BTIF_BTEVENT_HCI_INITIALIZED:
#ifndef IAG_BLE_INCLUDE
            app_notify_stack_ready();
#endif
            break;
        case BTIF_BTEVENT_LINK_DISCONNECT:
            TRACE("BTEVENT_LINK_DISCONNECT");
            bt_global_disconnected_ind_handler(Event);
            return;
        case BTIF_BTEVENT_LINK_CONNECT_CNF:
            TRACE("BTEVENT_LINK_CONNECT_CNF");
            if (BTIF_BEC_NO_ERROR != btif_me_get_callback_event_err_code(Event)){
                if (  btif_me_get_pendCons()){
                    LOG_PRINT_BT_CONN_MGR("!!!!!!!!!!WARNING Event->errCode:%d pendCons:%d", btif_me_get_callback_event_err_code(Event),  btif_me_get_pendCons());
                }
            }
        case BTIF_BTEVENT_LINK_CONNECT_IND:
            TRACE("BTEVENT_LINK_CONNECT_IND");
            bt_global_connected_handler(Event);
            break;
        case BTIF_BTEVENT_ACCESSIBLE_CHANGE:
            LOG_PRINT_BT_LINK_MGR("ACCESSIBLE_CHANGE evt:%d errCode:0x%0x aMode=0x%0x",etype, btif_me_get_callback_event_err_code(Event), btif_me_get_callback_event_a_mode(Event));
            if (btif_me_get_callback_event_err_code(Event)){
                bt_drv_reg_op_set_accessible_mode(btif_me_get_callback_event_a_mode(Event));
            }
            if(bt_access_mode_set_pending ==1){
                LOG_PRINT_BT_LINK_MGR("BEM_ACCESSIBLE_CHANGE PENDING");
                bt_access_mode_set_pending = 0;
                app_bt_accessmode_set(gBT_DEFAULT_ACCESS_MODE);
            }
            break;
        case BTIF_BTEVENT_MAX_SLOT_CHANGED:
            LOG_PRINT_BT_LINK_MGR("Max slot of conn handle 0x%x changes to %d", btif_me_get_callback_event_max_slot_changed_connHandle(Event),
                btif_me_get_callback_event_max_slot_changed_max_slot(Event));
            break;
        case BTIF_BTEVENT_SNIFFER_CONTROL_DONE:
            LOG_PRINT_BT_LINK_MGR("The sniffer control operation is done.");
            tws_slave_update_sniffer_in_sco_state();
            break;
        case BTIF_BTEVENT_LINK_POLICY_CHANGED:
            LOG_PRINT_BT_LINK_MGR("BTEVENT_LINK_POLICY_CHANGED");
            {
                BT_SET_LINKPOLICY_REQ_T* pReq = app_bt_pop_pending_set_linkpolicy();
                if (NULL != pReq)
                {
                    app_bt_set_linkpolicy(pReq->remDev, pReq->policy);
                }
                break;
            }
        case BTIF_BTEVENT_AUTHENTICATED:
            LOG_PRINT_BT_LINK_MGR("GLOBAL HANDER AUTH error=%x", btif_me_get_callback_event_err_code(Event));
            if(btif_me_get_callback_event_err_code(Event) == 5 && btif_me_get_callback_event_rem_dev(Event))
            {
                LOG_PRINT_BT_LINK_MGR("Authentication failed with error %d",btif_me_get_callback_event_err_code(Event));
                authenticate_failure_remote_dev =  btif_me_get_callback_event_rem_dev(Event);
                //TRACE("AUTH ERROR SO DELETE THE DEVICE RECORD addr=");
                //DUMP8("%02x ",Event->p.remDev->bdAddr.addr,6);
                //nv_record_ddbrec_delete(&Event->p.remDev->bdAddr);
            }
            break;
        default:
            break;
    }

    if (is_simulating_reconnecting_in_progress())
    {
        return;
    }
    if (BTIF_BTEVENT_MODE_CHANGE == etype){
        LOG_PRINT_BT_LINK_MGR("MODE_CHANGE evt:%d errCode:0x%0x curMode=0x%0x, interval=%d ",etype,  btif_me_get_callback_event_err_code(Event),
            btif_me_get_callback_event_mode_change_curMode(Event), btif_me_get_callback_event_mode_change_interval(Event));
        if ((app_tws_is_master_mode()) &&
            (!IS_CONNECTED_WITH_TWS()) &&
            (IS_CONNECTED_WITH_MOBILE()))
        {
            LOG_PRINT_BT_LINK_MGR("BTEVENT_MODE_CHANGE-->conn_ble_reconnection_machanism_handler");
            conn_ble_reconnection_machanism_handler(false);
        }

#ifdef __3RETX_SNIFF__
        if((btif_me_get_callback_event_mode_change_interval(Event)==CMGR_ESCO_SNIFF_INTERVAL))
        {
                 if((btif_me_get_callback_event_mode_change_curMode(Event) ==BTIF_BLM_SNIFF_MODE) &&  (btif_me_get_callback_event_err_code(Event) == BTIF_BEC_NO_ERROR))
            {
                app_sco_sniff_conflict_check_flag = 1;
                bt_drv_reg_op_retx_att_nb_manager(RETX_NEGO);
                LOG_PRINT_BT_LINK_MGR("enter sniff resume retxnb\n");
            }
            else if( btif_me_get_callback_event_mode_change_curMode(Event) ==BTIF_BLM_ACTIVE_MODE)
            {
                // TWS device event
                btif_cmgr_handler_t *cmgrHandler;
                btif_sniff_info_t   sniffInfo;
                a2dp_stream_t *tws_source;

                if ((app_tws_get_conn_state() == TWS_MASTER_CONN_SLAVE) && btapp_hfp_call_is_active())
                {
                    tws_source = app_tws_get_tws_source_stream();
                    remDev = btif_a2dp_get_remote_device(tws_source);
                    if (btif_me_get_remote_device_state(remDev) == BTIF_BDS_CONNECTED){
                        /* Start the sniff timer */
                        sniffInfo.minInterval = CMGR_ESCO_SNIFF_INTERVAL;
                        sniffInfo.maxInterval = CMGR_ESCO_SNIFF_INTERVAL;
                        sniffInfo.attempt = CMGR_SNIFF_ATTEMPT-2;
                        sniffInfo.timeout = CMGR_SNIFF_TIMEOUT;
                        cmgrHandler = btif_cmgr_get_first_handler(remDev);
                        if (cmgrHandler){
                            app_bt_set_linkpolicy(remDev, BTIF_BLP_SNIFF_MODE);
                            btif_cmgr_set_sniff_exit_policy(cmgrHandler,0);
                            btif_cmgr_set_sniff_timer(cmgrHandler, &sniffInfo, CMGR_ESCO_SNIFF_TIMER);

                            bt_drv_reg_op_esco_acl_sniff_delay_cal( btif_me_get_remote_device_hci_handle(remDev),true);
                        }
                    }
                }
                //BLM_ACTIVE_MODE decrease 3 RETX to 2 RETX
                if((esco_retx_nb == 3) && (btif_me_get_callback_event_err_code(Event) == BTIF_BEC_NO_ERROR))
                {
                    if (((app_tws_get_conn_state() == TWS_MASTER_CONN_SLAVE) && btapp_hfp_call_is_active())||
                        ((app_tws_get_conn_state() == TWS_SLAVE_CONN_MASTER) && slave_sco_active))
                    {
                        LOG_PRINT_BT_LINK_MGR("APP BT sniff_sco_conflict esco_retx_nb =%d",esco_retx_nb);
                        bt_drv_reg_op_retx_att_nb_manager(RETX_NB_2);
                    }
                }
            }
         }
#endif
    }

    // following event happens when the mobile device or tws is connected,
    // so firstly check the event source

    if (BTIF_BTEVENT_ROLE_CHANGE == etype)
    {
        if (IS_CONN_STATE_LINK_ON(mobileDevConnectionState) &&
        (!memcmp( btif_me_get_callback_event_disconnect_rem_dev_bd_addr(Event)->address, mobileDevBdAddr.address, BTIF_BD_ADDR_SIZE)))
        {
            // mobile device event
            app_bt_earphone_role_process(Event);
            #ifdef __3M_PACK__
            btdrv_current_packet_type_hacker(btif_me_get_remote_device_hci_handle( btif_me_get_callback_event_disconnect_rem_dev(Event)),BT_TX_3M);
            #else
            btdrv_current_packet_type_hacker(btif_me_get_remote_device_hci_handle( btif_me_get_callback_event_disconnect_rem_dev(Event)),BT_TX_2M);
            #endif
        }
        else if (IS_CONN_STATE_LINK_ON(twsConnectionState) &&
            (btif_me_get_callback_event_disconnect_rem_dev(Event) == slaveBtRemDev))
        {
            if (BTIF_BCR_SLAVE == btif_me_get_callback_event_role_change_new_role(Event))
            {
                if (tws_switchrole_cnt++ < APP_BT_SWITCHROLE_LIMIT)
                {
                    LOG_PRINT_BT_LINK_MGR("Switch the role of the peer TWS slave failed, %d try!", tws_switchrole_cnt);
                    tws_switchrole_start(300, btif_me_get_callback_event_disconnect_rem_dev(Event));
                }
            }
            else if (BTIF_BCR_MASTER == btif_me_get_callback_event_role_change_new_role(Event))
            {
                tws_switchrole_cancel();
            }
        }
    }
    else
    {
        // TWS device event
    btif_cmgr_handler_t *cmgrHandler;
    btif_sniff_info_t   sniffInfo;

    switch (etype) {
#ifdef __BT_SNIFF__
        case BTIF_BTEVENT_ACL_DATA_NOT_ACTIVE:
            remDev = btif_me_get_callback_event_rem_dev(Event);
            LOG_PRINT_BT_LINK_MGR("BTEVENT_ACL_DATA_NOT_ACTIVE state:%d mode:%d", btif_me_get_remote_device_state(remDev), btif_me_get_remote_device_mode(remDev));
            LOG_PRINT_BT_LINK_MGR("addr:");
            LOG_PRINT_BT_CONN_MGR_DUMP8("%02x ", btif_me_get_remote_device_bdaddr(remDev)->address, 6);
            a2dp_stream_t *tws_source;
            tws_source = app_tws_get_tws_source_stream();
            if ((remDev ==   btif_a2dp_get_remote_device(tws_source)) &&
                (btif_me_get_remote_device_state(remDev) == BTIF_BDS_CONNECTED) &&
                (btif_me_get_current_mode(remDev) == BTIF_BLM_SNIFF_MODE))
            {
                bt_status_t ret =btif_me_stop_sniff(remDev);
                LOG_PRINT_BT_LINK_MGR("Return status %d", ret);
                if (BT_STS_IN_PROGRESS == ret)
                {
                    app_add_pending_stop_sniff_op(remDev);
                }
            }
            break;
#endif
        case BTIF_BTEVENT_SCO_CONNECT_IND:
        case BTIF_BTEVENT_SCO_CONNECT_CNF:
        btif_remote_device_t *remDev;
            app_bt_set_linkpolicy(btif_me_get_callback_event_sco_connect_rem_dev( Event), BTIF_BLP_DISABLE_ALL);
#ifdef __TWS__

            if (besbt_cfg.sniff){
                if (app_earphone_remDev == btif_me_get_callback_event_rem_dev(Event)){
                    cmgrHandler = btif_cmgr_get_first_handler(btif_me_get_callback_event_rem_dev(Event));
                     if (cmgrHandler)
                    {
                        btif_cmgr_clear_sniff_timer(cmgrHandler);
                        btif_cmgr_disable_sniff_timer(cmgrHandler);
                    }
                }
                #ifdef __3RETX_SNIFF__
                if (app_tws_get_conn_state() == TWS_MASTER_CONN_SLAVE){
                    tws_source = app_tws_get_tws_source_stream();
                    remDev = btif_a2dp_get_remote_device(tws_source);
                    if (btif_me_get_remote_device_state(remDev) == BTIF_BDS_CONNECTED){
                        /*If a long sniff(760slot) timer exists,exit it first*/
                         if(btif_me_get_current_mode(remDev) == BTIF_BLM_SNIFF_MODE)
                        {
                            bt_status_t ret = btif_me_stop_sniff(remDev);
                            if (BT_STS_IN_PROGRESS == ret)
                            {
                                app_add_pending_stop_sniff_op(remDev);
                            }
                         }
                        /* Start the sniff timer */
                        sniffInfo.minInterval = CMGR_ESCO_SNIFF_INTERVAL;
                        sniffInfo.maxInterval = CMGR_ESCO_SNIFF_INTERVAL;
                        sniffInfo.attempt = CMGR_SNIFF_ATTEMPT-2;
                        sniffInfo.timeout = CMGR_SNIFF_TIMEOUT;
                        cmgrHandler = btif_cmgr_get_first_handler(remDev);
                        if (cmgrHandler){
                            app_bt_set_linkpolicy(remDev, BTIF_BLP_SNIFF_MODE);
                            btif_cmgr_set_sniff_exit_policy(cmgrHandler,0);
                            btif_cmgr_set_sniff_timer(cmgrHandler, &sniffInfo, CMGR_ESCO_SNIFF_TIMER);
                            bt_drv_reg_op_esco_acl_sniff_delay_cal(btif_me_get_remote_device_hci_handle(remDev),true);
                        }
                    }
                }
                #else
                if (app_tws_get_conn_state() == TWS_MASTER_CONN_SLAVE){

                    app_tws_stop_sniff();

                    a2dp_stream_t* tws_source = app_tws_get_tws_source_stream();
                    remDev = btif_a2dp_get_remote_device(tws_source);
                    if (remDev && btif_me_get_remote_device_state(remDev) == BTIF_BDS_CONNECTED){
                        cmgrHandler = btif_cmgr_get_first_handler(remDev);
                        if (cmgrHandler){
                            btif_cmgr_clear_sniff_timer(cmgrHandler);
                            btif_cmgr_disable_sniff_timer(cmgrHandler);
                        }
                    }
                }
                #endif
            }
#endif
            break;
        case BTIF_BTEVENT_SCO_DISCONNECT:
#ifdef __BT_ONE_BRING_TWO__
            app_bt_set_linkpolicy(btif_me_get_callback_event_sco_connect_rem_dev(Event),  BTIF_BLP_SNIFF_MODE);
#else
            app_bt_set_linkpolicy(btif_me_get_callback_event_sco_connect_rem_dev(Event),  BTIF_BLP_MASTER_SLAVE_SWITCH|BTIF_BLP_SNIFF_MODE);
#endif
#ifdef __TWS__
            if (besbt_cfg.sniff){
                if (app_earphone_remDev ==   btif_me_get_callback_event_rem_dev(Event)){
                    /* Start the sniff timer */
                    sniffInfo.minInterval = BTIF_CMGR_SNIFF_MIN_INTERVAL;
                    sniffInfo.maxInterval = BTIF_CMGR_SNIFF_MAX_INTERVAL;
                    sniffInfo.attempt = BTIF_CMGR_SNIFF_ATTEMPT;
                    sniffInfo.timeout = BTIF_CMGR_SNIFF_TIMEOUT;
                    cmgrHandler = btif_cmgr_get_first_handler(btif_me_get_callback_event_rem_dev(Event));
                    if (cmgrHandler){
                        btif_cmgr_set_sniff_timer(cmgrHandler, &sniffInfo, BTIF_CMGR_SNIFF_TIMER);
                    }
                }

                if (app_tws_get_conn_state() == TWS_MASTER_CONN_SLAVE){
                    a2dp_stream_t* tws_source = app_tws_get_tws_source_stream();
                    remDev = btif_a2dp_get_remote_device(tws_source);
                    if (btif_me_get_remote_device_state(remDev) == BTIF_BDS_CONNECTED){
                        #ifdef __3RETX_SNIFF__
                        /*If a short sniff(144slot) timer exists,exit it first*/
                        if(btif_me_get_current_mode(remDev) == BTIF_BLM_SNIFF_MODE)
                        {
                            bt_status_t  ret = btif_me_stop_sniff(remDev);
                            if (BT_STS_IN_PROGRESS == ret)
                            {
                                app_add_pending_stop_sniff_op(remDev);
                            }
                        }
                        bt_drv_reg_op_esco_acl_sniff_delay_cal( btif_me_get_remote_device_hci_handle(remDev),false);
                        #endif
                        /* Start the sniff timer */
                        sniffInfo.minInterval = BTIF_CMGR_SNIFF_MIN_INTERVAL-40;
                        sniffInfo.maxInterval = BTIF_CMGR_SNIFF_MAX_INTERVAL-40;
                        sniffInfo.attempt = BTIF_CMGR_SNIFF_ATTEMPT;
                        sniffInfo.timeout = BTIF_CMGR_SNIFF_TIMEOUT;
                        cmgrHandler = btif_cmgr_get_first_handler(remDev);
                        if (cmgrHandler){
                            #ifdef __3RETX_SNIFF__
                            btif_cmgr_set_sniff_exit_policy(cmgrHandler,BTIF_CMGR_SNIFF_EXIT_ON_AUDIO);
                            #endif
                            btif_cmgr_set_sniff_timer(cmgrHandler, &sniffInfo, BTIF_CMGR_SNIFF_TIMER);
                        }
                    }
                }
            }
#endif
            break;
        case BTIF_BTEVENT_ACL_DATA_ACTIVE:
#ifdef __TWS__
            if (besbt_cfg.sniff){
                if (app_earphone_remDev == btif_me_get_callback_event_rem_dev(Event)){
                    cmgrHandler = btif_cmgr_get_first_handler  (btif_me_get_callback_event_rem_dev(Event));
                    if (app_bt_device.hf_audio_state[BT_DEVICE_ID_1] == BTIF_HF_AUDIO_CON){
                        if (cmgrHandler){
                            btif_cmgr_clear_sniff_timer(cmgrHandler);
                            btif_cmgr_disable_sniff_timer(cmgrHandler);
                        }
                    }else{
                        /* Start the sniff timer */
                        sniffInfo.minInterval = BTIF_CMGR_SNIFF_MIN_INTERVAL;
                        sniffInfo.maxInterval = BTIF_CMGR_SNIFF_MAX_INTERVAL;
                        sniffInfo.attempt = BTIF_CMGR_SNIFF_ATTEMPT;
                        sniffInfo.timeout = BTIF_CMGR_SNIFF_TIMEOUT;
                        if (cmgrHandler){
                            btif_cmgr_set_sniff_timer(cmgrHandler, &sniffInfo, BTIF_CMGR_SNIFF_TIMER);
                        }
                    }
                }

                if (app_tws_get_conn_state() == TWS_MASTER_CONN_SLAVE){
                    a2dp_stream_t* tws_source = app_tws_get_tws_source_stream();
                    remDev = btif_a2dp_get_remote_device(tws_source);
                    if (btif_me_get_remote_device_state(remDev) == BTIF_BDS_CONNECTED){
                        cmgrHandler = btif_cmgr_get_first_handler(remDev);
                        switch (app_bt_device.hf_audio_state[BT_DEVICE_ID_1]) {
                            case BTIF_HF_AUDIO_CON:
                                #ifdef __3RETX_SNIFF__

                                #else
                                if (cmgrHandler){
                                    btif_cmgr_clear_sniff_timer(cmgrHandler);
                                    btif_cmgr_disable_sniff_timer(cmgrHandler);
                                }
                                #endif
                                break;
                            case BTIF_HF_AUDIO_DISCON:
                                /* Start the sniff timer */
                                sniffInfo.minInterval = BTIF_CMGR_SNIFF_MIN_INTERVAL-40;
                                sniffInfo.maxInterval = BTIF_CMGR_SNIFF_MAX_INTERVAL-40;
                                sniffInfo.attempt = BTIF_CMGR_SNIFF_ATTEMPT;
                                sniffInfo.timeout = BTIF_CMGR_SNIFF_TIMEOUT;
                                if (cmgrHandler){
                                    btif_cmgr_set_sniff_timer(cmgrHandler, &sniffInfo, BTIF_CMGR_SNIFF_TIMER);
                                }
                                break;
                            default:
                                break;
                        }
                    }
                }
            }
#else
            if (besbt_cfg.sniff){
                /* Start the sniff timer */
                btif_sniff_info_t   sniffInfo;
                btif_cmgr_handler_t    *cmgrHandler;
                sniffInfo.minInterval = BTIF_CMGR_SNIFF_MIN_INTERVAL;
                sniffInfo.maxInterval = BTIF_CMGR_SNIFF_MAX_INTERVAL;
                sniffInfo.attempt = BTIF_CMGR_SNIFF_ATTEMPT;
                sniffInfo.timeout = BTIF_CMGR_SNIFF_TIMEOUT;
                cmgrHandler = btif_cmgr_get_first_handler(btif_me_get_callback_event_rem_dev(Event));
                if (cmgrHandler)
                        btif_cmgr_set_sniff_timer(cmgrHandler, &sniffInfo, BTIF_CMGR_SNIFF_TIMER);
            }
#endif
            break;
        }
    }
}

void app_bt_snifftimeout_handler(evm_timer_t * timer, BOOL *skipInternalHandler)
{
    btif_cmgr_handler_t *handler = (btif_cmgr_handler_t *)timer->context;
    if ((app_bt_device.hf_audio_state[BT_DEVICE_ID_1] == BTIF_HF_AUDIO_CON)||
        (app_bt_device.a2dp_streamming[BT_DEVICE_ID_1])){
        #ifdef __3RETX_SNIFF__
        if(app_bt_device.a2dp_streamming[BT_DEVICE_ID_1] )
        {
            LOG_PRINT_BT_LINK_MGR("%s restart sniff timer", __func__);
            *skipInternalHandler = TRUE;
            btif_evm_start_timer(timer,  btif_cmgr_get_cmgrhandler_sniff_timeout(handler));
        }
        #else
        LOG_PRINT_BT_LINK_MGR("%s restart sniff timer", __func__);
        *skipInternalHandler = TRUE;
        btif_evm_start_timer(timer, btif_cmgr_get_cmgrhandler_sniff_timeout(handler));
        #endif
    }else{
        LOG_PRINT_BT_LINK_MGR("%s use internal sniff timer", __func__);
        *skipInternalHandler = FALSE;
    }
}



void app_bt_golbal_handle_init(void)
{
    btif_event_mask_t mask = BTIF_BEM_NO_EVENTS;

    //ME_InitHandler(&app_bt_handler);
    btif_me_init_handler(&app_bt_handler);
    app_bt_handler.callback = app_bt_global_handle;
    btif_me_register_global_handler(&app_bt_handler);

    mask |= BTIF_BEM_ROLE_CHANGE | BTIF_BEM_SCO_CONNECT_CNF | BTIF_BEM_SCO_DISCONNECT | BTIF_BEM_SCO_CONNECT_IND | BTIF_BEM_ACCESSIBLE_CHANGE | BTIF_BEM_AUTHENTICATED;
#ifdef __EARPHONE_STAY_BOTH_SCAN__
    mask |= BTIF_BEM_LINK_DISCONNECT;
#endif

    btif_me_set_connection_role(BTIF_BCR_ANY);

#ifdef __TWS__
    mask |= BTIF_BEM_LINK_CONNECT_IND  |BTIF_BEM_LINK_CONNECT_CNF | BTIF_BEM_LINK_DISCONNECT | BTIF_BEM_MODE_CHANGE;
#endif
    mask |= BTIF_BEM_MAX_SLOT_CHANGED;
    mask |= BTIF_BEM_SNIFFER_CONTROL_DONE;
    mask |= BTIF_BEM_LINK_POLICY_CHANGED;

    btif_me_set_event_mask(&app_bt_handler, mask);

    btif_me_set_connection_role(BTIF_BCR_ANY);
    for (uint8_t i=0; i<BT_DEVICE_NUM; i++){
        a2dp_stream_t* stream = app_bt_get_mobile_stream(i);
        btif_a2dp_set_master_role(stream, FALSE);
        //HF_SetMasterRole(&app_bt_device.hf_channel[i], FALSE);
    }
#ifdef __TWS__
    //CMGR_SetSniffTimeoutHandlerExt(app_bt_snifftimeout_handler);
    btif_cmgr_set_sniff_timeout_handler_ext(app_bt_snifftimeout_handler);
#endif
}

void app_bt_send_request(uint32_t message_id, uint32_t param0, uint32_t param1, uint32_t ptr)
{
    if ((uint32_t)APP_BT_REQ_BT_THREAD_CALL != message_id)
    {
        APP_MESSAGE_BLOCK msg;
        msg.mod_id = APP_MODUAL_BT;
        msg.msg_body.message_id = message_id;
        msg.msg_body.message_Param0 = param0;
        msg.msg_body.message_Param1 = param1;
        msg.msg_body.message_ptr = ptr;
        app_mailbox_put(&msg);

    }
    else
    {
        BESTHR_MSG_BLOCK msg;
        msg.msg_id = message_id;
        msg.param0 = param0;
        msg.param1 = param1;
        msg.ptr = ptr;
        besthr_mailbox_put(&msg);

        osapi_notify_evm();
    }
}

static int app_bt_handle_process(APP_MESSAGE_BODY *msg_body)
{
    switch (msg_body->message_id) {
        case APP_BT_REQ_ACCESS_MODE_SET:
            if (msg_body->message_Param0 == BTIF_DEFAULT_ACCESS_MODE_PAIR){
                #if FPGA == 0
                app_status_indication_set(APP_STATUS_INDICATION_BOTHSCAN);
                app_voice_report(APP_STATUS_INDICATION_BOTHSCAN, 0);
                #endif
                app_bt_accessmode_set(BTIF_DEFAULT_ACCESS_MODE_PAIR);
            }else if (msg_body->message_Param0 == BTIF_BAM_CONNECTABLE_ONLY){
                app_bt_accessmode_set(BTIF_BAM_CONNECTABLE_ONLY);
            }else if (msg_body->message_Param0 == BTIF_BAM_NOT_ACCESSIBLE){
                app_bt_accessmode_set(BTIF_BAM_NOT_ACCESSIBLE);
            }else if (msg_body->message_Param0 == BTIF_BAM_LIMITED_ACCESSIBLE){
                app_bt_accessmode_set(BTIF_BAM_LIMITED_ACCESSIBLE);
            }
            
            break;
        case APP_BT_REQ_CUSTOMER_CALL:
            if (msg_body->message_ptr){
                ((APP_BT_REQ_CUSTOMER_CALl_CB_T)(msg_body->message_ptr))((void *)msg_body->message_Param0, (void *)msg_body->message_Param1);
            }
            break;
        default:
            break;
    }

    return 0;
}

void *app_bt_profile_active_store_ptr_get(uint8_t *bdAddr)
{
    static btdevice_profile device_profile = {false, false, false,0};
    btdevice_profile *ptr;
    nvrec_btdevicerecord *record = NULL;

#if  FPGA==0
    if (!nv_record_btdevicerecord_find((bt_bdaddr_t *)bdAddr,&record)){
        ptr = &(record->device_plf);
        LOG_PRINT_BT_CONN_MGR_DUMP8("0x%02x ", bdAddr, BTIF_BD_ADDR_SIZE);
        LOG_PRINT_BT_CONN_MGR("%s hfp_act:%d hsp_act:%d a2dp_act:0x%x,codec_type=0x%x", __func__, ptr->hfp_act, ptr->hsp_act, ptr->a2dp_act,ptr->a2dp_codectype);
    }else
#endif
    {
        ptr = &device_profile;
        LOG_PRINT_BT_CONN_MGR("%s default", __func__);
    }
    return (void *)ptr;
}

enum bt_profile_reconnect_mode{
    bt_profile_reconnect_null,
    bt_profile_reconnect_openreconnecting,
    bt_profile_reconnect_reconnecting,
};

enum bt_profile_connect_status{
    bt_profile_connect_status_unknown,
    bt_profile_connect_status_success,
    bt_profile_connect_status_failure,
};

struct app_bt_profile_manager{
    bool has_connected;
    enum bt_profile_connect_status hfp_connect;
    enum bt_profile_connect_status hsp_connect;
    enum bt_profile_connect_status a2dp_connect;
    bt_bdaddr_t rmt_addr;
    bt_profile_reconnect_mode reconnect_mode;
    a2dp_stream_t *stream;
    //HfChannel *chan;
    hf_chan_handle_t chan;
    uint16_t reconnect_cnt;
    osTimerId connect_timer;
   btif_cmgr_handler_t* pCmgrHandler;
    void (* connect_timer_cb)(void const *);
};

//reconnect = (INTERVAL+PAGETO)*CNT = 3000ms*5000ms*15 = 120s
#define APP_BT_PROFILE_RECONNECT_RETRY_INTERVAL_MS (3000)
#define APP_BT_PROFILE_RECONNECT_RETRY_LIMIT_CNT (15)
#define APP_BT_PROFILE_CONNECT_RETRY_MS (10000)

static struct app_bt_profile_manager bt_profile_manager[BT_DEVICE_NUM];
static void app_bt_profile_reconnect_timehandler(void const *param);

osTimerDef (BT_PROFILE_CONNECT_TIMER0, app_bt_profile_reconnect_timehandler);                      // define timers
#ifdef __BT_ONE_BRING_TWO__
osTimerDef (BT_PROFILE_CONNECT_TIMER1, app_bt_profile_reconnect_timehandler);
#endif

#ifdef __AUTO_CONNECT_OTHER_PROFILE__
static void app_bt_profile_connect_hf_retry_timehandler(void const *param)
{
    struct app_bt_profile_manager *bt_profile_manager_p = (struct app_bt_profile_manager *)param;
    //HF_CreateServiceLink(bt_profile_manager_p->chan, &bt_profile_manager_p->rmt_addr);
    btif_hf_create_service_link(bt_profile_manager_p->chan, &bt_profile_manager_p->rmt_addr);
}

static void app_bt_profile_connect_a2dp_retry_timehandler(void const *param)
{
    log_event_1(EVENT_START_CONNECTING_A2DP, BT_FUNCTION_ROLE_MOBILE);
    struct app_bt_profile_manager *bt_profile_manager_p = (struct app_bt_profile_manager *)param;
    btif_a2dp_open_stream(bt_profile_manager_p->stream, &bt_profile_manager_p->rmt_addr);
}
#endif

static void app_bt_profile_reconnect_timehandler(void const *param)
{
    struct app_bt_profile_manager *bt_profile_manager_p = (struct app_bt_profile_manager *)param;
    btdevice_profile *btdevice_plf_p = (btdevice_profile *)app_bt_profile_active_store_ptr_get(bt_profile_manager_p->rmt_addr.address);

    if (bt_profile_manager_p->connect_timer_cb) {
        bt_profile_manager_p->connect_timer_cb(param);
        bt_profile_manager_p->connect_timer_cb = NULL;
        return;
    }

    if (btdevice_plf_p->hfp_act){
        LOG_PRINT_BT_CONN_MGR("try connect hf");

        bt_status_t retStatus = btif_hf_create_service_link(bt_profile_manager_p->chan, &bt_profile_manager_p->rmt_addr);

        if ((retStatus == BT_STS_SUCCESS) || (retStatus == BT_STS_PENDING)) {
            bt_profile_manager_p->pCmgrHandler = btif_hf_get_chan_manager_handler(bt_profile_manager_p->chan);
        }
    } else if(btdevice_plf_p->a2dp_act) {
        LOG_PRINT_BT_CONN_MGR("try connect a2dp");
        log_event_1(EVENT_START_CONNECTING_A2DP, BT_FUNCTION_ROLE_MOBILE);
        bt_status_t retStatus = btif_a2dp_open_stream(bt_profile_manager_p->stream, &bt_profile_manager_p->rmt_addr);
        if ((retStatus == BT_STS_SUCCESS) || (retStatus == BT_STS_PENDING)) {
            bt_profile_manager_p->pCmgrHandler =  btif_a2dp_get_stream_devic_cmgrHandler(bt_profile_manager_p->stream);
        }
    }
}

void app_bt_profile_connect_manager_open(void)
{
    uint8_t i=0;

    for (i = 0; i < BT_DEVICE_NUM; i++){
        bt_profile_manager[i].has_connected = false;
        bt_profile_manager[i].hfp_connect = bt_profile_connect_status_unknown;
        bt_profile_manager[i].hsp_connect = bt_profile_connect_status_unknown;
        bt_profile_manager[i].a2dp_connect = bt_profile_connect_status_unknown;
        memset(bt_profile_manager[i].rmt_addr.address, 0, BTIF_BD_ADDR_SIZE);
        bt_profile_manager[i].reconnect_mode = bt_profile_reconnect_null;
        bt_profile_manager[i].stream = NULL;
        bt_profile_manager[i].chan = NULL;
        bt_profile_manager[i].reconnect_cnt = 0;
        bt_profile_manager[i].connect_timer_cb = NULL;
    }

    bt_profile_manager[BT_DEVICE_ID_1].connect_timer = osTimerCreate (osTimer(BT_PROFILE_CONNECT_TIMER0), osTimerOnce, &bt_profile_manager[BT_DEVICE_ID_1]);
#ifdef __BT_ONE_BRING_TWO__
    bt_profile_manager[BT_DEVICE_ID_2].connect_timer = osTimerCreate (osTimer(BT_PROFILE_CONNECT_TIMER1), osTimerOnce, &bt_profile_manager[BT_DEVICE_ID_2]);
#endif
}

bt_status_t app_bt_execute_connecting_mobile_device(uint8_t* pBdAddr)
{
    bt_bdaddr_t *rmt_addr = &bt_profile_manager[BT_DEVICE_ID_1].rmt_addr;
    bt_status_t retStatus = BT_STS_FAILED;
    log_event_2(EVENT_START_BT_CONNECTING, BT_FUNCTION_ROLE_MOBILE, pBdAddr);

    memcpy(bt_profile_manager[BT_DEVICE_ID_1].rmt_addr.address, pBdAddr, BTIF_BD_ADDR_SIZE);
    //btdevice_profile *btdevice_plf_p = (btdevice_profile *)app_bt_profile_active_store_ptr_get(bt_profile_manager[BT_DEVICE_ID_1].rmt_addr.address);
    btdevice_profile *btdevice_plf_p = (btdevice_profile *)app_bt_profile_active_store_ptr_get(rmt_addr->address);
    bt_profile_manager[BT_DEVICE_ID_1].chan = app_bt_device.hf_channel[BT_DEVICE_ID_1];

#if defined(A2DP_LHDC_ON)
    if(btdevice_plf_p->a2dp_codectype == BTIF_AVDTP_CODEC_TYPE_LHDC){
        bt_profile_manager[BT_DEVICE_ID_1].stream = app_bt_device.a2dp_lhdc_stream->a2dp_stream;
    }else
#endif
#if defined(A2DP_AAC_ON)
    if(btdevice_plf_p->a2dp_codectype == BTIF_AVDTP_CODEC_TYPE_MPEG2_4_AAC){
        bt_profile_manager[BT_DEVICE_ID_1].stream = app_bt_device.a2dp_aac_stream->a2dp_stream;
    }else
#endif
    {
        bt_profile_manager[BT_DEVICE_ID_1].stream = app_bt_device.a2dp_stream[BT_DEVICE_ID_1]->a2dp_stream;
    }


    if(btdevice_plf_p->a2dp_act && !IS_CONN_STATE_A2DP_ON(mobileDevConnectionState))
    {
        a2dp_stream_t * stream = bt_profile_manager[BT_DEVICE_ID_1].stream;

        log_event_1(EVENT_START_CONNECTING_A2DP, BT_FUNCTION_ROLE_MOBILE);
        retStatus = btif_a2dp_open_stream(stream, rmt_addr);
        if ((retStatus == BT_STS_SUCCESS) || (retStatus == BT_STS_PENDING))
        {
            bt_profile_manager[BT_DEVICE_ID_1].pCmgrHandler = btif_a2dp_get_stream_devic_cmgrHandler(stream);
        }
        LOG_PRINT_BT_CONN_MGR("Connecting mobile a2dp ret %d", retStatus);
    }
    else if (btdevice_plf_p->hfp_act && !IS_CONN_STATE_HFP_ON(mobileDevConnectionState))
    {
        hf_chan_handle_t chan = bt_profile_manager[BT_DEVICE_ID_1].chan;

        bt_status_t status = btif_hf_create_service_link(chan, rmt_addr);
        if ((status == BT_STS_SUCCESS) || (status == BT_STS_PENDING))
        {
            bt_profile_manager[BT_DEVICE_ID_1].pCmgrHandler =
                    btif_hf_get_chan_manager_handler(chan);
        }
        LOG_PRINT_BT_CONN_MGR("Connecting mobile HFP ret %d", retStatus);
    }

    return retStatus;
}

void app_bt_dump_env(void)
{
    uint8_t i;
    nvrec_btdevicerecord *record = NULL;

    a2dp_stream_t* sink_stream = app_tws_get_sink_stream();
    a2dp_stream_t* source_stream = app_tws_get_tws_source_stream();
    btif_remote_device_t *sink_remDev = NULL;
    btif_remote_device_t *source_remDev = NULL;

    sink_remDev = btif_a2dp_get_remote_device(sink_stream);
    source_remDev = btif_a2dp_get_remote_device(source_stream);

    TRACE_HIGHPRIO_IMM("");
    TRACE_HIGHPRIO_IMM("");
    TRACE_HIGHPRIO("Tws Env:");

    if (!ble_tws_is_initialized()){
        TRACE_HIGHPRIO("ble tws has not initialized");
        return;
    }

    do {
        if (!sink_remDev){
            TRACE_HIGHPRIO("sink:null");
            break;
        }else{
#ifndef FPGA        
            if (nv_record_btdevicerecord_find(btif_me_get_remote_device_bdaddr( sink_remDev),&record)){
                TRACE_HIGHPRIO("sink record:null");
                break;
            }
#endif
            TRACE_HIGHPRIO("sink info");

            TRACE_HIGHPRIO_NOCRLF("addr");
            for (i=0; i<6; i++){
                TRACE_HIGHPRIO_NOCRLF(":%02x", record->record.bdAddr.address[i]);
            }
            TRACE_HIGHPRIO_IMM("");

            TRACE_HIGHPRIO_NOCRLF("linkkey");
            for (i=0; i<16; i++){
                TRACE_HIGHPRIO_NOCRLF(":%02x", record->record.linkKey[i]);
            }
            TRACE_HIGHPRIO_IMM("");

            TRACE_HIGHPRIO("State:%02x Role:%02x Mode:%02x",
                                                btif_me_get_remote_device_state(sink_remDev),
                                                btif_me_get_current_role(sink_remDev),
                                                btif_me_get_current_mode(sink_remDev));
        }
    }while(0);

    do {
        if (!source_remDev){
            TRACE_HIGHPRIO("source:null");
            break;
        }else{
#ifndef FPGA
            if (nv_record_btdevicerecord_find(btif_me_get_remote_device_bdaddr(source_remDev),&record)){
                TRACE_HIGHPRIO("source record:null");
                break;
            }
#endif
            TRACE_HIGHPRIO("source info");

            TRACE_HIGHPRIO_NOCRLF("addr");
            for (i=0; i<6; i++){
                TRACE_HIGHPRIO_NOCRLF(":%02x", record->record.bdAddr.address[i]);
            }
            TRACE_HIGHPRIO_IMM("");

            TRACE_HIGHPRIO_NOCRLF("linkkey");
            for (i=0; i<16; i++){
                TRACE_HIGHPRIO_NOCRLF(":%02x", record->record.linkKey[i]);
            }
            TRACE_HIGHPRIO_IMM("");

            TRACE_HIGHPRIO("State:%02x Role:%02x Mode:%02x",
                                                btif_me_get_remote_device_state(source_remDev),
                                                btif_me_get_current_role(source_remDev),
                                                btif_me_get_current_mode(source_remDev));
        }
    }while(0);

    TRACE_HIGHPRIO("tws_mode_ptr_mode:%08x", app_tws_get_mode());
    TRACE_HIGHPRIO("current_conn_op_state_machine:%08x", CURRENT_CONN_OP_STATE_MACHINE());
    TRACE_HIGHPRIO("mobileDevConnectionState:%08x", mobileDevConnectionState);
    TRACE_HIGHPRIO("twsConnectionState:%08x", twsConnectionState);
    TRACE_HIGHPRIO("conn_op_state:%08x", conn_op_state);
    TRACE_HIGHPRIO("app_tws_is_role_switching_on:%08x", app_tws_is_role_switching_on());
    TRACE_HIGHPRIO("currentSimulateReconnectionState:%08x", currentSimulateReconnectionState);
    bt_drv_reg_op_crash_dump();
    TRACE_HIGHPRIO_IMM("");
    TRACE_HIGHPRIO_IMM("");
    TRACE_HIGHPRIO_IMM("");
    TRACE_HIGHPRIO_IMM("");
}

#define APP_TWS_RECONNECT_HFP_DELAY_IN_MS     1000
osTimerId   tws_reconnect_hfp_delay_timer_id;
static void tws_reconnect_hfp_delay_timer_cb(void const *n);
osTimerDef (APP_TWS_RECONNECT_HFP_DELAY_TIMER, tws_reconnect_hfp_delay_timer_cb);

static void tws_reconnect_hfp_delay_timer_cb(void const *n)
{
    if (app_tws_is_reconnecting_hfp_in_progress() && mobileBtRemDev)
    {
        LOG_PRINT_BT_CONN_MGR("Reconnect the hfp.");
        app_start_custom_function_in_bt_thread(BT_DEVICE_ID_1,
            (uint32_t)&mobileDevBdAddr, (uint32_t)app_bt_connect_hfp);
    }
}

void app_bt_start_tws_reconnect_hfp_timer(void)
{
    osTimerStart(tws_reconnect_hfp_delay_timer_id, APP_TWS_RECONNECT_HFP_DELAY_IN_MS);
}

void app_bt_stop_tws_reconnect_hfp_timer(void)
{
    osTimerStop(tws_reconnect_hfp_delay_timer_id);
}



#ifdef __BT_LL_MONITOR_EN__


const void  *monitor_str[27] =
{
    "TX DM1",
    "TX DH1",
    "TX DM3",
    "TX DH3",
    "TX DM5",
    "TX DH5",
    "TX 2DH1",
    "TX 3DH1",
    "TX 2DH3",
    "TX 3DH3",
    "TX 2DH5",
    "TX 3DH5",

    "RX DM1",
    "RX DH1",
    "RX DM3",
    "RX DH3",
    "RX DM5",
    "RX DH5",
    "RX 2DH1",
    "RX 3DH1",
    "RX 2DH3",
    "RX 3DH3",
    "RX 2DH5",
    "RX 3DH5",
    "hec error",
    "crc error",
    "fec error",

};

void app_bt_ll_monitor(const unsigned char *buf, unsigned int buf_len)
{
    LOG_PRINT_BT_LINK_MGR("LL MONITOR LEN=%x handle=%x,statues=%x",buf_len,(uint16_t)(buf[1]|buf[2]<<8),buf[3]);
    for(uint8_t i=0;i<27;i++)
    {
        uint32_t temp =buf[4+i*4] | (buf[5+i*4]<<8) \
                    |(buf[6+i*4]<<16) | (buf[7+i*4]<<24);
        if(temp != 0)
        {
            LOG_PRINT_BT_LINK_MGR("%s %d\n",monitor_str[i],temp);
        }
    }
}


extern "C" void register_hci_ll_monitor_evt_code_callback(bt_hci_dbg_ll_monitor_callback_func func);
#endif
void app_bt_controller_crash_dump(const unsigned char *buf, unsigned int buf_len)
{
    //check  [!!ASSERT]  ASCII code
    if(buf[1]==0x21&&buf[2]==0x21&&buf[3]==0x41&&buf[4]==0x53)
    {
        bt_drv_reg_op_crash_dump();
    }
}

BOOL app_bt_is_reconnect_mobile_dev(void *ptr)
{
    btif_remote_device_t* remDev = (btif_remote_device_t*)ptr;
    LOG_PRINT_BT_CONN_MGR("app_bt_is_reconnect_mobile_dev IS_CONNECTING_MOBILE %d",IS_CONNECTING_MOBILE());
    if((!memcmp(btif_me_get_remote_device_bdaddr(remDev)->address,mobileDevBdAddr.address,6))&&
        IS_CONNECTING_MOBILE())
    {
        return true;
    }
    else
    {
        return false;
    }
}

void app_bt_init(void)
{
    hal_trace_crash_dump_register(app_bt_dump_env);

    remDev_disconnect_delay_timer_id =
        osTimerCreate(osTimer(REMDEV_DISCONNECT_DELAY_TIMER), osTimerOnce, NULL);

    tws_reconnect_hfp_delay_timer_id =
        osTimerCreate(osTimer(APP_TWS_RECONNECT_HFP_DELAY_TIMER), osTimerOnce, NULL);

    mobile_device_switchrole_delay_timer_init();
    tws_switchrole_delay_timer_init();

    app_set_threadhandle(APP_MODUAL_BT, app_bt_handle_process);

//only can be used by 2300 version c or later
#ifdef __BT_LL_MONITOR_EN__
#ifdef CHIP_BEST2300
    if(hal_get_chip_metal_id() >= HAL_CHIP_METAL_ID_1)
    {
        btif_register_hci_ll_monitor_evt_code_callback(app_bt_ll_monitor);
    }
#endif
#endif
    btif_register_hci_dbg_trace_warning_evt_code_callback(app_bt_controller_crash_dump);
    btif_me_sec_set_io_cap_rsp_reject_ext(app_bt_is_reconnect_mobile_dev);
}

#ifdef BTIF_HID_DEVICE
void hid_exit_shutter_mode(void);
#endif

void app_bt_disconnect_all_mobile_devces(void)
{
    if ((0 != mobileBtRemDev) && !IS_CONNECTED_WITH_MOBILE())
    {
        ASSERT(0, "mobile is disconnected while link down callback is not called.");
    }

    if ((0 == mobileBtRemDev) && IS_CONNECTED_WITH_MOBILE())
    {
        ASSERT(0, "mobile is connected while link up callback is not called.");
    }

#ifdef __BT_ONE_BRING_TWO__
    LOG_PRINT_BT_CONN_MGR("Disconnect one device");
    hf_chan_handle_t chan_2 = app_bt_device.hf_channel[BT_DEVICE_ID_2];
    a2dp_stream_t *stream_2 = app_bt_device.a2dp_stream[BT_DEVICE_ID_2].a2dp_stream;

    if(btif_get_hf_chan_state(chan_2) == BTIF_HF_STATE_OPEN) {
        if(app_bt_device.hf_channel[BT_DEVICE_ID_2].cmgrHandler.remDev)
        {
            app_bt_disconnect_link(app_bt_device.hf_channel[BT_DEVICE_ID_2].cmgrHandler.remDev);
        }
    }
    else if( btif_a2dp_get_stream_state(stream_2) == BTIF_AVDTP_STRM_STATE_STREAMING ||
        btif_a2dp_get_stream_state(stream_2) == BTIF_AVDTP_STRM_STATE_OPEN) {
        if (btif_a2dp_get_stream_devic_cmgrHandler_remdev(stream_2)) {
            btif_remote_device_t *rem_dev =
                (btif_remote_device_t*)btif_a2dp_get_stream_devic_cmgrHandler_remdev(stream_2);
            app_bt_disconnect_link(rem_dev);
        }
#else
    LOG_PRINT_BT_CONN_MGR("Disconnect mobile 0x%x", mobileBtRemDev);
    if (NULL != mobileBtRemDev)
    {
#if BTIF_HID_DEVICE == BTIF_ENABLED
        hid_exit_shutter_mode();
#endif

        app_bt_disconnect_link(mobileBtRemDev);
    }
#endif
}

BOOL app_bt_bypass_hcireceiveddata_cb(void)
{
    list_entry_t        *tmpRxBuffList;
    btif_hci_buffer_t        *hciRxBuffer;
    btif_hci_buffer_t        *next;
    BOOL              handled = FALSE;
    BOOL              nRet =  TRUE;

    tmpRxBuffList =btif_hci_get_rx_buffer_list();;

    osapi_stop_hardware();
    if (!is_list_empty(tmpRxBuffList)) {
        iterate_list_safe(btif_hci_get_rx_buffer_list(), hciRxBuffer, next, btif_hci_buffer_t *) {
            if (hciRxBuffer->flags & BTIF_HCI_BUFTYPE_EVENT) {
                handled = TRUE;
                remove_entry_list(&hciRxBuffer->node);
                break;
            }
        }
    }
    if (handled){
        //        TRACE("InsertHeadList hciRxBuffer(event)");
        insert_head_list(tmpRxBuffList, &(hciRxBuffer->node));
        osapi_notify_evm();
        nRet =  FALSE;
    }

    osapi_resume_hardware();
#if defined(__TWS__)
    if (app_tws_get_a2dpbuff_available() > 1024){
        //        TRACE("!!!Resume HciBypassProcessReceivedData");
        btif_HciBypassProcessReceivedDataExt(NULL);
        nRet =  FALSE;
    }
#endif
    return nRet;
}

bt_bdaddr_t* app_bt_get_peer_remote_device_bd_addr(uint8_t deviceId)
{
    return &(bt_profile_manager[deviceId].rmt_addr);
}

void app_bt_set_peer_remote_device_bd_addr(uint8_t deviceId, bt_bdaddr_t* pBtAddr)
{
    bt_profile_manager[deviceId].rmt_addr = *pBtAddr;
}

void app_bt_open_mobile_a2dp_stream(uint32_t deviceId)
{
    log_event_1(EVENT_START_CONNECTING_A2DP, BT_FUNCTION_ROLE_MOBILE);
    a2dp_stream_t * stream = bt_profile_manager[deviceId].stream;

    bt_status_t retStatus = btif_a2dp_open_stream(stream, &bt_profile_manager[deviceId].rmt_addr);
    if ((retStatus == BT_STS_SUCCESS) || (retStatus == BT_STS_PENDING)) {
        bt_profile_manager[deviceId].pCmgrHandler = btif_a2dp_get_stream_devic_cmgrHandler(stream);
    }
}

void app_bt_open_mobile_hfp_stream(uint32_t deviceId)
{
    hf_chan_handle_t chan = bt_profile_manager[BT_DEVICE_ID_1].chan;
    bt_status_t retStatus;

    retStatus = btif_hf_create_service_link(chan, &bt_profile_manager[deviceId].rmt_addr);
    if ((retStatus == BT_STS_SUCCESS) || (retStatus == BT_STS_PENDING)) {
        bt_profile_manager[deviceId].pCmgrHandler =
                btif_hf_get_chan_manager_handler(chan);
    }
}

void app_bt_fill_mobile_a2dp_stream(uint32_t deviceId, a2dp_stream_t *stream)
{
    bt_profile_manager[deviceId].stream = stream;
}

void app_bt_fill_mobile_hfp_channel(uint32_t deviceId, hf_chan_handle_t chan)
{
    bt_profile_manager[deviceId].chan = chan;
}

void app_bt_cancel_connecting(uint32_t deviceId)
{
    btif_remote_device_t *rem_dev;
    btif_cmgr_handler_t * cmgr_handler;

    cmgr_handler = bt_profile_manager[deviceId].pCmgrHandler;
    rem_dev = btif_cmgr_get_cmgrhandler_remdev(cmgr_handler);
    LOG_PRINT_BT_CONN_MGR(" [ %s ] ",__func__);
    LOG_PRINT_BT_CONN_MGR_DUMP8(" %02x ",  btif_me_get_remote_device_bdaddr(rem_dev)->address, 6);
    if (bt_profile_manager[deviceId].pCmgrHandler && rem_dev) {
        btif_handler *btif_handler;

        LOG_PRINT_BT_CONN_MGR("remDev state is %d",  btif_me_get_remote_device_state(rem_dev));
        btif_handler = btif_cmgr_get_cmgrhandler_remdev_bthandle(cmgr_handler);
        bt_status_t ret = btif_me_cancel_create_link(btif_handler, rem_dev);
        LOG_PRINT_BT_CONN_MGR("ret %d", ret);
    }
}

void app_bt_connect_hfp(uint32_t deviceId, bt_bdaddr_t *addr)
{
    hf_chan_handle_t chan = bt_profile_manager[BT_DEVICE_ID_1].chan;
    bt_status_t retStatus = btif_hf_create_service_link(chan, addr);
    uint16_t hci_handle;

    LOG_PRINT_BT_CONN_MGR("Connect hfp returns %d", retStatus);
    if ((retStatus == BT_STS_SUCCESS) || (retStatus == BT_STS_PENDING)) {
        bt_profile_manager[deviceId].pCmgrHandler =
                            btif_hf_get_chan_manager_handler(chan);

        hci_handle = btif_cmgr_get_cmgrhandler_remdev_hci_handle(
                            bt_profile_manager[deviceId].pCmgrHandler);
        log_event_1(EVENT_START_CONNECTING_HFP, hci_handle & 3);
    }
}

void app_bt_cleanup_mobile_cmgrhandler(void)
{
    bt_profile_manager[BT_DEVICE_ID_1].pCmgrHandler = NULL;
    //HF_ReleaseHfChannel(bt_profile_manager[BT_DEVICE_ID_1].chan);
}

void app_bt_connect_avrcp(uint32_t deviceId, bt_bdaddr_t *addr)
{
    btif_avrcp_chnl_handle_t chan_handle;
    uint16_t hci_handle;

    chan_handle = app_bt_device.avrcp_channel[deviceId]->avrcp_channel_handle;
    hci_handle  = btif_me_get_remote_device_hci_handle(chan_handle);
    log_event_1(EVENT_START_CONNECTING_AVRCP, hci_handle & 3);
    btif_avrcp_connect(app_bt_device.avrcp_channel[deviceId], (bt_bdaddr_t*) addr);
}



//extern "C" void HciSendCompletePacketCommandRightNow(uint16_t handle,uint8_t num);

void app_bt_hcireceived_data_clear(uint16_t conhdl)
{
    list_entry_t        *tmpRxBuffList;
    btif_hci_buffer_t        *hciRxBuffer;
    btif_hci_buffer_t        *next;
    uint16_t   buff_handle;
    // tmpRxBuffList = &HCC(rxBuffList);
    tmpRxBuffList =  btif_hci_get_rx_buffer_list();
    osapi_stop_hardware();

    if (is_list_empty(tmpRxBuffList)) {
        osapi_resume_hardware();
        return;
    }

    iterate_list_safe( btif_hci_get_rx_buffer_list(), hciRxBuffer, next, btif_hci_buffer_t *) {
        if (hciRxBuffer->flags & BTIF_HCI_BUFTYPE_ACL_DATA) {
            buff_handle = le_to_host16(hciRxBuffer->buffer);
            buff_handle &= 0x0FFF;
                 LOG_PRINT_BT_LINK_MGR("hciRxBuffer BUFF HANDLE=%x",buff_handle);
            if(buff_handle == conhdl) {
                remove_entry_list(&hciRxBuffer->node);
                    LOG_PRINT_BT_LINK_MGR("free hciRxBuffer(event)");
                // RXBUFF_Free(hciRxBuffer);
                btif_hci_rxbuffer_free(hciRxBuffer);
                btif_hci_send_complete_packet_command_right_now(conhdl,1);

            }
        }
    }
    osapi_resume_hardware();
}

a2dp_stream_t * app_bt_get_mobile_a2dp_stream(uint32_t deviceId)
{
    return bt_profile_manager[deviceId].stream;
}

bool app_bt_is_hfp_audio_on(void)
{
    return (BTIF_HF_AUDIO_CON == app_bt_device.hf_audio_state[BT_DEVICE_ID_1]);
}

bool app_bt_is_source_avrcp_connected(void)
{
    int avrcp_state;
    btif_avrcp_chnl_handle_t chan_handle;

    chan_handle = app_bt_device.avrcp_channel[BT_DEVICE_ID_1]->avrcp_channel_handle;
    avrcp_state = btif_avrcp_get_channel_avrcp_state(chan_handle);
    LOG_PRINT_BT_CONN_MGR("%s state is %d", __FUNCTION__, avrcp_state);
    return (BTIF_AVRCP_STATE_DISCONNECTED != avrcp_state);
}

bool app_bt_is_avrcp_operation_available(void)
{
    int avrcp_int_state;
    btif_avrcp_chnl_handle_t chan_handle;

    chan_handle = app_bt_device.ptrAvrcpChannel[BT_DEVICE_ID_1]->avrcp_channel_handle;
    avrcp_int_state = btif_avrcp_get_channel_panel_int_state(chan_handle);

    LOG_PRINT_AVRCP("AVRCP panel state is %d", avrcp_int_state);
    return (BTIF_AVRCP_PANEL_STATE_C_IDLE == avrcp_int_state) ||
                    (BTIF_AVRCP_PANEL_STATE_C_SKIP == avrcp_int_state);
}

bt_status_t app_bt_disconnect_source_avrcp(void)
{
    int avrcp_int_state;
    btif_avrcp_chnl_handle_t chan_handle;

    chan_handle = app_bt_device.ptrAvrcpChannel[BT_DEVICE_ID_1]->avrcp_channel_handle;

    avrcp_int_state = btif_avrcp_get_channel_panel_int_state(chan_handle);
    if ((BTIF_AVRCP_PANEL_STATE_C_IDLE == avrcp_int_state) ||
                (BTIF_AVRCP_PANEL_STATE_C_SKIP == avrcp_int_state)) {
        log_event_1(EVENT_DISCONNECT_AVRCP,
            ( btif_avrcp_get_cmgrhandler_remDev_hciHandle(app_bt_device.ptrAvrcpChannel[BT_DEVICE_ID_1]))&3);

        return btif_avrcp_disconnect(chan_handle);
    }
    else
    {
        TRACE("State of the avrcp is %d, pending for the on-going op is completed!",
           // app_bt_device.ptrAvrcpChannel[BT_DEVICE_ID_1]->panel.Int.state);
        btif_avrcp_get_channel_panel_int_state(chan_handle));
        return BT_STS_FAILED;
    }
}

bool app_is_custom_func_in_app_thread_ready(void)
{
    return app_is_module_registered(APP_MODUAL_BT);
}

void app_start_custom_function_in_app_thread(uint32_t param0, uint32_t param1, uint32_t ptr)
{
    app_bt_send_request(APP_BT_REQ_CUSTOMER_CALL, param0, param1, (uint32_t)ptr);
}

void app_start_custom_function_in_bt_thread(uint32_t param0, uint32_t param1, uint32_t ptr)
{
    app_bt_send_request(APP_BT_REQ_BT_THREAD_CALL, param0, param1, (uint32_t)ptr);
}

a2dp_stream_t *app_bt_get_mobile_stream(uint8_t deviceId)
{
    return bt_profile_manager[deviceId].stream;
}

void app_connect_a2dp_mobile(uint8_t *pBdAddr)
{
    memcpy(bt_profile_manager[BT_DEVICE_ID_1].rmt_addr.address, pBdAddr, BTIF_BD_ADDR_SIZE);
    btdevice_profile *btdevice_plf_p = (btdevice_profile *)app_bt_profile_active_store_ptr_get(bt_profile_manager[BT_DEVICE_ID_1].rmt_addr.address);
    bt_profile_manager[BT_DEVICE_ID_1].chan = app_bt_device.hf_channel[BT_DEVICE_ID_1];
    //TRACE("start connect mobile codec type=%x %x",btdevice_plf_p->a2dp_codectype, &app_bt_device.a2dp_stream[BT_DEVICE_ID_1]);

#if defined(A2DP_LHDC_ON)
    if(btdevice_plf_p->a2dp_codectype == BTIF_AVDTP_CODEC_TYPE_LHDC){
        bt_profile_manager[BT_DEVICE_ID_1].stream = &app_bt_device.a2dp_lhdc_stream;
    }else
#endif
#if defined(A2DP_AAC_ON)
    if(btdevice_plf_p->a2dp_codectype == BTIF_AVDTP_CODEC_TYPE_MPEG2_4_AAC){
        bt_profile_manager[BT_DEVICE_ID_1].stream = app_bt_device.a2dp_aac_stream->a2dp_stream;
    }else{
        bt_profile_manager[BT_DEVICE_ID_1].stream = app_bt_device.a2dp_stream[BT_DEVICE_ID_1]->a2dp_stream;
    }
#else
    {
        bt_profile_manager[BT_DEVICE_ID_1].stream = app_bt_device.a2dp_stream[BT_DEVICE_ID_1]->a2dp_stream;
    }
#endif
    bt_profile_manager[BT_DEVICE_ID_1].pCmgrHandler = btif_a2dp_get_stream_devic_cmgrHandler(bt_profile_manager[BT_DEVICE_ID_1].stream);
}

extern "C" bool app_sco_sniff_check(void)
{
    bool nret = false;
    a2dp_stream_t *tws_source;
    btif_remote_device_t *remDev;

    if (app_tws_get_conn_state() != TWS_MASTER_CONN_SLAVE)
        return nret;

    tws_source = app_tws_get_tws_source_stream();
    remDev = btif_a2dp_get_remote_device(tws_source);
    if (btif_me_get_remote_device_state(remDev) == BTIF_BDS_CONNECTED){
        switch (app_bt_device.hf_audio_state[BT_DEVICE_ID_1]) {
        case BTIF_HF_AUDIO_CON:
            nret =true;
            break;
        default:
            nret=false;
            break;
        }
    }

    return nret;
}

void app_acl_sniff_sco_conflict_process(void)
{
    a2dp_stream_t *tws_source;
    btif_remote_device_t *remDev;
    uint16_t hci_handle;

    if (app_tws_get_conn_state() != TWS_MASTER_CONN_SLAVE)
        return;

    tws_source = app_tws_get_tws_source_stream();
    remDev = btif_a2dp_get_remote_device(tws_source);
    hci_handle = btif_me_get_remote_device_hci_handle(remDev);

    if (bt_drv_reg_op_check_esco_acl_sniff_conflict(hci_handle))
        return;

    if (btif_me_get_remote_device_state(remDev) != BTIF_BDS_CONNECTED)
        return;

    switch (app_bt_device.hf_audio_state[BT_DEVICE_ID_1]) {
    case BTIF_HF_AUDIO_CON:
        /*If a short sniff(60slot) timer conflict with esco,exit it first*/
        if( btif_me_get_current_mode(remDev) == BTIF_BLM_SNIFF_MODE) {
            TRACE("%s",__func__);
            bt_status_t ret = btif_me_stop_sniff(remDev);
            if (BT_STS_IN_PROGRESS == ret) {
                app_add_pending_stop_sniff_op(remDev);
            }
            bt_drv_reg_op_esco_acl_sniff_delay_cal(hci_handle, true);
        }
        break;
    default:
        break;
    }
}

void app_acl_sniff_sco_conflict_check(void)
{
    //master role check acl and esco conflict when sniff mode
    if(app_tws_mode_is_master() && ( btif_me_get_current_mode(slaveBtRemDev) == BTIF_BLM_SNIFF_MODE)) {
        if(bt_drv_reg_op_check_esco_acl_sniff_conflict(app_tws_get_tws_conhdl()) &&
                        app_sco_sniff_conflict_check_flag) {
            //send message to BT thread to exit sniff
            app_start_custom_function_in_bt_thread(0, 0, (uint32_t)app_acl_sniff_sco_conflict_process);
            app_sco_sniff_conflict_check_flag = 0;
        }
    }
}

void fast_pair_enter_pairing_mode_handler(void)
{
    app_bt_accessmode_set(BTIF_BAM_CONNECTABLE_ONLY);
}

uint8_t app_bt_get_num_of_connected_dev(void)
{
    uint8_t num_of_connected_dev = 0;
    uint8_t deviceId;
    for (deviceId = 0; deviceId < BT_DEVICE_NUM; deviceId++)
    {
        if (bt_profile_manager[deviceId].has_connected)
        {
            num_of_connected_dev++;
        }
    }

    return num_of_connected_dev;
}

bool app_bt_is_device_connected(uint8_t deviceId)
{
    if (deviceId < BT_DEVICE_NUM) {
        return bt_profile_manager[deviceId].has_connected;
    } else {
        // Indicate no connection is user passes invalid deviceId
        return false;
    }
}

bool app_bt_get_device_bdaddr(uint8_t deviceId, uint8_t* btAddr)
{
    if (app_bt_is_device_connected(deviceId))
    {
        bt_bdaddr_t *addr = btif_me_get_remote_device_bdaddr(btif_me_enumerate_remdev(deviceId));

        memcpy(btAddr, addr->address, BTIF_BD_ADDR_SIZE);
        return true;
    }
    else
    {
        return false;
    }
}


void hfp_service_connected_set(bool on)
{
    TRACE("%s %d", __func__, on);
    if(on)
        bt_profile_manager[BT_DEVICE_ID_1].hfp_connect = bt_profile_connect_status_success;
    else
        bt_profile_manager[BT_DEVICE_ID_1].hfp_connect = bt_profile_connect_status_unknown;
}

bool app_is_hfp_service_connected(void)
{
    return (bt_profile_manager[BT_DEVICE_ID_1].hfp_connect == bt_profile_connect_status_success);
}

void a2dp_service_connected_set(bool on)
{
    TRACE("%s %d", __func__, on);
    if(on)
        bt_profile_manager[BT_DEVICE_ID_1].a2dp_connect = bt_profile_connect_status_success;
    else
        bt_profile_manager[BT_DEVICE_ID_1].a2dp_connect = bt_profile_connect_status_unknown;
}

bool a2dp_service_is_connected(void)
{
	return (bt_profile_manager[BT_DEVICE_ID_1].a2dp_connect == bt_profile_connect_status_success);
}

bool app_is_link_connected()
{
	if(btif_me_get_activeCons() == 0)
		return false;

	return true;
}

