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
#include "string.h"
#include "hal_trace.h"
#include "bluetooth.h"
#include "besbt.h"
#include "app_bt_func.h"
#include "hfp_api.h"
#include "app_hfp.h"

extern "C" void OS_NotifyEvm(void);
//extern "C" void btapp_hfp_report_speak_gain(void);
extern "C" void btapp_a2dp_report_speak_gain(void);

#define APP_BT_MAILBOX_MAX (15)
osMailQDef (app_bt_mailbox, APP_BT_MAILBOX_MAX, APP_BT_MAIL);
static osMailQId app_bt_mailbox = NULL;

void btapp_a2dp_report_speak_gain(void);

inline int app_bt_mail_process(APP_BT_MAIL* mail_p)
{
    bt_status_t status = BT_STS_LAST_CODE;
    if (mail_p->request_id != CMGR_SetSniffTimer_req){
        TRACE("%s src_thread:0x%08x request_id:%d enter",__func__, mail_p->src_thread, mail_p->request_id);
    }
    switch (mail_p->request_id) {
        case Bt_Generic_req:
            if (mail_p->param.Generic_req_param.handler){
                ((APP_BT_GENERIC_REQ_CB_T)(mail_p->param.Generic_req_param.handler))(
                    (void *)mail_p->param.Generic_req_param.param0,
                    (void *)mail_p->param.Generic_req_param.param1);
            }
            break;
        case Me_switch_sco_req:
            status = btif_me_switch_sco(mail_p->param.Me_switch_sco_param.scohandle);
            break;
        case ME_SwitchRole_req:
            status = btif_me_switch_role(mail_p->param.ME_SwitchRole_param.remDev);
            break;
        case ME_SetConnectionRole_req:
            btif_me_set_connection_role(mail_p->param.BtConnectionRole_param.role);
            status =  BT_STS_SUCCESS;
            break;
        case MeDisconnectLink_req:
            btif_me_disconnect_link((btif_handler*) btif_me_get_bt_handler(),mail_p->param.MeDisconnectLink_param.remDev);
            status =  BT_STS_SUCCESS;
            break;
        case ME_StopSniff_req:
            status = btif_me_stop_sniff(mail_p->param.ME_StopSniff_param.remDev);
            break;
        case ME_SetAccessibleMode_req:
            status = btif_me_set_accessible_mode(mail_p->param.ME_SetAccessibleMode_param.mode,
                                          &mail_p->param.ME_SetAccessibleMode_param.info);
            break;
        case Me_SetLinkPolicy_req:
            status = btif_me_set_link_policy(mail_p->param.Me_SetLinkPolicy_param.remDev,
                                      mail_p->param.Me_SetLinkPolicy_param.policy);
            break;
        case ME_SetConnectionQosInfo_req:
            status = btif_me_set_connection_qos_info(mail_p->param.ME_SetConnectionQosInfo_param.remDev,
                                             &mail_p->param.ME_SetConnectionQosInfo_param.qosInfo);
            break;
        case Me_Set_LBRT_req:
#ifdef CHIP_BEST2300
            status =   btif_me_set_lbrt_enable(mail_p->param.ME_Set_LBRT_param.conhandle,
                                mail_p->param.ME_Set_LBRT_param.enable);
#else
            status =  BT_STS_SUCCESS;
#endif
            break;
        case CMGR_SetSniffTimer_req:
            if (mail_p->param.CMGR_SetSniffTimer_param.SniffInfo.maxInterval == 0){
                status = btif_cmgr_set_sniff_timer (mail_p->param.CMGR_SetSniffTimer_param.Handler,
                                            NULL,
                                            mail_p->param.CMGR_SetSniffTimer_param.Time);
            }else{
                status = btif_cmgr_set_sniff_timer(mail_p->param.CMGR_SetSniffTimer_param.Handler,
                                            &mail_p->param.CMGR_SetSniffTimer_param.SniffInfo,
                                            mail_p->param.CMGR_SetSniffTimer_param.Time);
            }
            break;
        case CMGR_SetSniffInofToAllHandlerByRemDev_req:
            status =   btif_cmgr_set_sniff_info_to_all_handler_by_remDev(&mail_p->param.CMGR_SetSniffInofToAllHandlerByRemDev_param.SniffInfo,
                                                            mail_p->param.CMGR_SetSniffInofToAllHandlerByRemDev_param.RemDev);
            break;
        case A2DP_OpenStream_req:
            status = btif_a2dp_open_stream(mail_p->param.A2DP_OpenStream_param.Stream,
                                     mail_p->param.A2DP_OpenStream_param.Addr);
            break;
        case A2DP_CloseStream_req:
            status = btif_a2dp_close_stream(mail_p->param.A2DP_CloseStream_param.Stream);
            break;
        case A2DP_SetMasterRole_req:
            status = btif_a2dp_set_master_role(mail_p->param.A2DP_SetMasterRole_param.Stream,
                                        mail_p->param.A2DP_SetMasterRole_param.Flag);
            break;
        case HF_CreateServiceLink_req:
            status = btif_hf_create_service_link(mail_p->param.HF_CreateServiceLink_param.Chan,
                                          mail_p->param.HF_CreateServiceLink_param.Addr);
            break;
        case HF_DisconnectServiceLink_req:
            status = btif_hf_disconnect_service_link(mail_p->param.HF_DisconnectServiceLink_param.Chan);
            break;
        case HF_CreateAudioLink_req:
            status = btif_hf_create_audio_link(mail_p->param.HF_CreateAudioLink_param.Chan);
            break;
        case HF_DisconnectAudioLink_req:
            status = btif_hf_disc_audio_link(mail_p->param.HF_DisconnectAudioLink_param.Chan);
            break;
        case HF_EnableSniffMode_req:
            status = btif_hf_enable_sniff_mode(mail_p->param.HF_EnableSniffMode_param.Chan,
                                        mail_p->param.HF_EnableSniffMode_param.Enable);
            break;
        case HF_SetMasterRole_req:
            status = btif_hf_set_master_role(mail_p->param.HF_SetMasterRole_param.Chan,
                                      mail_p->param.HF_SetMasterRole_param.Flag);
            break;
#if defined (__HSP_ENABLE__)
        case HS_CreateServiceLink_req:
            status = HS_CreateServiceLink(mail_p->param.HS_CreateServiceLink_param.Chan,
                                          mail_p->param.HS_CreateServiceLink_param.Addr);
            break;
        case HS_CreateAudioLink_req:
            status = HS_CreateAudioLink(mail_p->param.HS_CreateAudioLink_param.Chan);
            break;
        case HS_DisconnectAudioLink_req:
            status = HS_DisconnectAudioLink(mail_p->param.HS_DisconnectAudioLink_param.Chan);
            break;
        case HS_DisconnectServiceLink_req:
            status = HS_DisconnectServiceLink(mail_p->param.HS_DisconnectServiceLink_param.Chan);
            break;
        case HS_EnableSniffMode_req:
            status = HS_EnableSniffMode(mail_p->param.HS_EnableSniffMode_param.Chan,
                                        mail_p->param.HS_EnableSniffMode_param.Enable);
            break;
#endif
        case HF_Report_Gain:
            btapp_hfp_report_speak_gain();
            break;
        case A2DP_Reort_Gain:
            btapp_a2dp_report_speak_gain();
            break;
    }

    if (mail_p->request_id != CMGR_SetSniffTimer_req){
        TRACE("%s request_id:%d :status:%d exit",__func__, mail_p->request_id, status);
    }
    return 0;
}

static inline int app_bt_mail_alloc(APP_BT_MAIL** mail)
{
    *mail = (APP_BT_MAIL*)osMailAlloc(app_bt_mailbox, 0);
    ASSERT(*mail, "app_bt_mail_alloc error");
    return 0;
}

static inline int app_bt_mail_send(APP_BT_MAIL* mail)
{
    osStatus status;

    ASSERT(mail, "osMailAlloc NULL");
    status = osMailPut(app_bt_mailbox, mail);
    ASSERT(osOK == status, "osMailAlloc Put failed");

    OS_NotifyEvm();

    return (int)status;
}

static inline int app_bt_mail_free(APP_BT_MAIL* mail_p)
{
    osStatus status;

    status = osMailFree(app_bt_mailbox, mail_p);
    ASSERT(osOK == status, "osMailAlloc Put failed");

    return (int)status;
}

static inline int app_bt_mail_get(APP_BT_MAIL** mail_p)
{
    osEvent evt;
    evt = osMailGet(app_bt_mailbox, 0);
    if (evt.status == osEventMail) {
        *mail_p = (APP_BT_MAIL *)evt.value.p;
        return 0;
    }
    return -1;
}

static void app_bt_mail_poll(void)
{
    APP_BT_MAIL *mail_p = NULL;
    if (!app_bt_mail_get(&mail_p)){
        app_bt_mail_process(mail_p);
        app_bt_mail_free(mail_p);
    }
}

int app_bt_mail_init(void)
{
    app_bt_mailbox = osMailCreate(osMailQ(app_bt_mailbox), NULL);
    if (app_bt_mailbox == NULL)  {
        TRACE("Failed to Create app_mailbox\n");
        return -1;
    }
    Besbt_hook_handler_set(BESBT_HOOK_USER_1, app_bt_mail_poll);

    return 0;
}

int app_bt_generic_req(uint32_t param0, uint32_t param1, uint32_t handler)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = Bt_Generic_req;
    mail->param.Generic_req_param.handler = handler;
    mail->param.Generic_req_param.param0 = param0;
    mail->param.Generic_req_param.param1 = param1;
    app_bt_mail_send(mail);
    return 0;
}

int app_bt_Me_switch_sco(uint16_t  scohandle)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = Me_switch_sco_req;
    mail->param.Me_switch_sco_param.scohandle = scohandle;
    app_bt_mail_send(mail);
    return 0;
}

int app_bt_ME_SwitchRole(btif_remote_device_t* remDev)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = ME_SwitchRole_req;
    mail->param.ME_SwitchRole_param.remDev = remDev;
    app_bt_mail_send(mail);
    return 0;
}

int app_bt_ME_SetConnectionRole(btif_connection_role_t role)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = ME_SetConnectionRole_req;
    mail->param.BtConnectionRole_param.role = role;
    app_bt_mail_send(mail);
    return 0;
}

int app_bt_MeDisconnectLink(btif_remote_device_t* remDev)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = MeDisconnectLink_req;
    mail->param.MeDisconnectLink_param.remDev = remDev;
    app_bt_mail_send(mail);
    return 0;
}

int app_bt_ME_StopSniff(btif_remote_device_t *remDev)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = ME_StopSniff_req;
    mail->param.ME_StopSniff_param.remDev = remDev;
    app_bt_mail_send(mail);
    return 0;
}

int app_bt_ME_SetAccessibleMode(btif_accessible_mode_t mode, const btif_access_mode_info_t *info)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = ME_SetAccessibleMode_req;
    mail->param.ME_SetAccessibleMode_param.mode = mode;
    memcpy(&mail->param.ME_SetAccessibleMode_param.info, info, sizeof( btif_access_mode_info_t));
    app_bt_mail_send(mail);
    return 0;
}

int app_bt_Me_SetLinkPolicy(btif_remote_device_t *remDev, btif_link_policy_t policy)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = Me_SetLinkPolicy_req;
    mail->param.Me_SetLinkPolicy_param.remDev = remDev;
    mail->param.Me_SetLinkPolicy_param.policy = policy;
    app_bt_mail_send(mail);
    return 0;
}

int app_bt_ME_SetConnectionQosInfo(btif_remote_device_t *remDev,
                                              btif_qos_info_t *qosInfo)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = ME_SetConnectionQosInfo_req;
    mail->param.ME_SetConnectionQosInfo_param.remDev = remDev;
    memcpy(&mail->param.ME_SetConnectionQosInfo_param.qosInfo, qosInfo, sizeof( btif_qos_info_t));
    app_bt_mail_send(mail);
    return 0;
}

int app_bt_ME_Set_LBRT_req(uint16_t conhandle, uint8_t enable)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = Me_Set_LBRT_req;
    mail->param.ME_Set_LBRT_param.conhandle = conhandle;
    mail->param.ME_Set_LBRT_param.enable = enable;
    app_bt_mail_send(mail);
    return 0;
}

int app_bt_CMGR_SetSniffTimer(   btif_cmgr_handler_t *Handler,
                                                btif_sniff_info_t* SniffInfo,
                                                TimeT Time)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = CMGR_SetSniffTimer_req;
    mail->param.CMGR_SetSniffTimer_param.Handler = Handler;
    if (SniffInfo){
        memcpy(&mail->param.CMGR_SetSniffTimer_param.SniffInfo, SniffInfo, sizeof(btif_sniff_info_t));
    }else{
        memset(&mail->param.CMGR_SetSniffTimer_param.SniffInfo, 0, sizeof(btif_sniff_info_t));
    }
    mail->param.CMGR_SetSniffTimer_param.Time = Time;
    app_bt_mail_send(mail);
    return 0;
}

int app_bt_CMGR_SetSniffInofToAllHandlerByRemDev(btif_sniff_info_t* SniffInfo,
                                                                 btif_remote_device_t *RemDev)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = CMGR_SetSniffInofToAllHandlerByRemDev_req;
    memcpy(&mail->param.CMGR_SetSniffInofToAllHandlerByRemDev_param.SniffInfo, SniffInfo, sizeof(btif_sniff_info_t));
    mail->param.CMGR_SetSniffInofToAllHandlerByRemDev_param.RemDev = RemDev;
    app_bt_mail_send(mail);
    return 0;
}

int app_bt_A2DP_OpenStream(a2dp_stream_t *Stream, bt_bdaddr_t *Addr)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = A2DP_OpenStream_req;
    mail->param.A2DP_OpenStream_param.Stream = Stream;
    mail->param.A2DP_OpenStream_param.Addr = Addr;
    app_bt_mail_send(mail);
    return 0;
}

int app_bt_A2DP_CloseStream(a2dp_stream_t *Stream)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = A2DP_CloseStream_req;
    mail->param.A2DP_CloseStream_param.Stream = Stream;
    app_bt_mail_send(mail);
    return 0;
}

int app_bt_A2DP_SetMasterRole(a2dp_stream_t *Stream, BOOL Flag)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = A2DP_SetMasterRole_req;
    mail->param.A2DP_SetMasterRole_param.Stream = Stream;
    mail->param.A2DP_SetMasterRole_param.Flag = Flag;
    app_bt_mail_send(mail);
    return 0;
}

int app_bt_HF_CreateServiceLink(hf_chan_handle_t Chan, bt_bdaddr_t *Addr)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = HF_CreateServiceLink_req;
    mail->param.HF_CreateServiceLink_param.Chan = Chan;
    mail->param.HF_CreateServiceLink_param.Addr = Addr;
    app_bt_mail_send(mail);
    return 0;
}

int app_bt_HF_DisconnectServiceLink(hf_chan_handle_t Chan)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = HF_DisconnectServiceLink_req;
    mail->param.HF_DisconnectServiceLink_param.Chan = Chan;
    app_bt_mail_send(mail);
    return 0;
}

int app_bt_HF_CreateAudioLink(hf_chan_handle_t Chan)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = HF_CreateAudioLink_req;
    mail->param.HF_CreateAudioLink_param.Chan = Chan;
    app_bt_mail_send(mail);
    return 0;
}

int app_bt_HF_DisconnectAudioLink(hf_chan_handle_t Chan)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = HF_DisconnectAudioLink_req;
    mail->param.HF_DisconnectAudioLink_param.Chan = Chan;
    app_bt_mail_send(mail);
    return 0;
}

int app_bt_HF_EnableSniffMode(hf_chan_handle_t Chan, BOOL Enable)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = HF_EnableSniffMode_req;
    mail->param.HF_EnableSniffMode_param.Chan = Chan;
    mail->param.HF_EnableSniffMode_param.Enable = Enable;
    app_bt_mail_send(mail);
    return 0;
}

int app_bt_HF_SetMasterRole(hf_chan_handle_t Chan, BOOL Flag)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = HF_SetMasterRole_req;
    mail->param.HF_SetMasterRole_param.Chan = Chan;
    mail->param.HF_SetMasterRole_param.Flag = Flag;
    app_bt_mail_send(mail);
    return 0;
}

#if defined (__HSP_ENABLE__)
int app_bt_HS_CreateServiceLink(HsChannel *Chan, bt_bdaddr_t *Addr)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = HS_CreateServiceLink_req;
    mail->param.HS_CreateServiceLink_param.Chan = Chan;
    mail->param.HS_CreateServiceLink_param.Addr = Addr;
    app_bt_mail_send(mail);
    return 0;
}

int app_bt_HS_CreateAudioLink(HsChannel *Chan)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = HS_CreateAudioLink_req;
    mail->param.HS_CreateAudioLink_param.Chan = Chan;
    app_bt_mail_send(mail);
    return 0;
}


int app_bt_HS_DisconnectAudioLink(HsChannel *Chan)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = HS_DisconnectAudioLink_req;
    mail->param.HS_DisconnectAudioLink_param.Chan = Chan;
    app_bt_mail_send(mail);
    return 0;
}

int app_bt_HS_DisconnectServiceLink(HsChannel *Chan)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = HS_DisconnectServiceLink_req;
    mail->param.HS_DisconnectServiceLink_param.Chan = Chan;
    app_bt_mail_send(mail);
    return 0;
}


int app_bt_HS_EnableSniffMode(HsChannel *Chan, BOOL Enable)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = HS_EnableSniffMode_req;
    mail->param.HS_EnableSniffMode_param.Chan = Chan;
    mail->param.HS_EnableSniffMode_param.Enable = Enable;
    app_bt_mail_send(mail);
    return 0;
}




#endif



int app_bt_HF_Report_Gain(void)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = HF_Report_Gain;
    app_bt_mail_send(mail);
    return 0;
}


int app_bt_A2dp_Report_Gain(void)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = A2DP_Reort_Gain;
    app_bt_mail_send(mail);
    return 0;
}
