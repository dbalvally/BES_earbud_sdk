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
#ifndef __APP_BT_FUNC_H__
#define __APP_BT_FUNC_H__

#include "cmsis_os.h"
#include "hal_trace.h"
#include "me_api.h"
#include "a2dp_api.h"
#include "hfp_api.h"
#ifdef __cplusplus
extern "C" {
#endif


typedef enum _bt_fn_req {
        Bt_Generic_req                              = 0,
        Me_switch_sco_req                           = 1,
        ME_SwitchRole_req                           = 2,
        ME_SetConnectionRole_req                    = 3,
        MeDisconnectLink_req                        = 4,
        ME_StopSniff_req                            = 5,
        ME_SetAccessibleMode_req                    = 6,
        Me_SetLinkPolicy_req                        = 7,
        ME_SetConnectionQosInfo_req                 = 8,
        CMGR_SetSniffTimer_req                      = 9,
        CMGR_SetSniffInofToAllHandlerByRemDev_req   = 10,
        A2DP_OpenStream_req                         = 11,
        A2DP_CloseStream_req                        = 12,
        A2DP_SetMasterRole_req                      = 13,
        HF_CreateServiceLink_req                    = 14,
        HF_DisconnectServiceLink_req                = 15,
        HF_CreateAudioLink_req                      = 16,
        HF_DisconnectAudioLink_req                  = 17,
        HF_EnableSniffMode_req                      = 18,
        HF_SetMasterRole_req                        = 19,
#if defined (__HSP_ENABLE__)
        HS_CreateServiceLink_req                    = 20,
        HS_CreateAudioLink_req                      = 21,
        HS_DisconnectAudioLink_req                  = 22,
        HS_EnableSniffMode_req                      = 23,
        HS_DisconnectServiceLink_req                = 24,
#endif
        HF_Report_Gain                              = 25,
        A2DP_Reort_Gain                             = 26,
        Me_Set_LBRT_req                             = 27,
}bt_fn_req;

typedef void (*APP_BT_GENERIC_REQ_CB_T)(void *, void *);

typedef union _bt_fn_param {
    struct {
        uint32_t    handler;
        uint32_t    param0;
        uint32_t    param1;
    } Generic_req_param;

    // bt_status_t Me_switch_sco(uint16_t  scohandle)
    struct {
        uint16_t  scohandle;
    } Me_switch_sco_param;

    // bt_status_t ME_SwitchRole(BtRemoteDevice *remDev)
    struct {
        btif_remote_device_t* remDev;
    } ME_SwitchRole_param;

    //BtConnectionRole ME_SetConnectionRole(BtConnectionRole role)
    struct {
        btif_connection_role_t role;
    } BtConnectionRole_param;

    // void MeDisconnectLink(BtRemoteDevice* remDev)
    struct {
        btif_remote_device_t* remDev;
    } MeDisconnectLink_param;
    
    //bt_status_t ME_StopSniff(BtRemoteDevice *remDev)
    struct {
        btif_remote_device_t* remDev;
    } ME_StopSniff_param;

    //bt_status_t ME_SetAccessibleMode(BtAccessibleMode mode, const BtAccessModeInfo *info)
    struct {
        btif_accessible_mode_t mode;
        btif_access_mode_info_t info;
    } ME_SetAccessibleMode_param;

    //bt_status_t Me_SetLinkPolicy(BtRemoteDevice *remDev, BtLinkPolicy policy)
    struct {
        btif_remote_device_t *remDev;
        btif_link_policy_t policy;
    } Me_SetLinkPolicy_param;
    /*bt_status_t ME_SetConnectionQosInfo(BtRemoteDevice *remDev,
                                              BtQosInfo *qosInfo)
    */
    struct {
        uint16_t  conhandle;
        uint8_t enable;
    } ME_Set_LBRT_param;
    /*bt_status_t Me_Set_LBRT_Enable(U16 connHandle,U8 enable)  */

    struct {
        btif_remote_device_t *remDev;
        btif_qos_info_t qosInfo;
    } ME_SetConnectionQosInfo_param;

    /*bt_status_t CMGR_SetSniffTimer(CmgrHandler *Handler,
                                BtSniffInfo* SniffInfo,
                                TimeT Time)
       */
    struct {
        btif_cmgr_handler_t *Handler;
        btif_sniff_info_t SniffInfo;
        TimeT Time;
    } CMGR_SetSniffTimer_param;

    /*bt_status_t CMGR_SetSniffInofToAllHandlerByRemDev(BtSniffInfo* SniffInfo,
                                                                BtRemoteDevice *RemDev)
    */
    struct {
        btif_sniff_info_t SniffInfo;
        btif_remote_device_t *RemDev;
    } CMGR_SetSniffInofToAllHandlerByRemDev_param;

    //bt_status_t A2DP_OpenStream(A2dpStream *Stream, BT_BD_ADDR *Addr)
    struct {
        a2dp_stream_t *Stream;
        bt_bdaddr_t *Addr;
    } A2DP_OpenStream_param;

    //bt_status_t A2DP_CloseStream(A2dpStream *Stream);
    struct {
        a2dp_stream_t *Stream;
    } A2DP_CloseStream_param;

    //bt_status_t A2DP_SetMasterRole(A2dpStream *Stream, BOOL Flag);    
    struct {
        a2dp_stream_t *Stream;
        BOOL Flag;
    } A2DP_SetMasterRole_param;

    //bt_status_t HF_CreateServiceLink(hf_chan_handle_t Chan, BT_BD_ADDR *Addr)
    struct {
        hf_chan_handle_t Chan;
        bt_bdaddr_t*Addr;
    } HF_CreateServiceLink_param;

    //bt_status_t HF_DisconnectServiceLink(hf_chan_handle_t Chan)
    struct {
        hf_chan_handle_t Chan;
    } HF_DisconnectServiceLink_param;

    //bt_status_t HF_CreateAudioLink(hf_chan_handle_t Chan)
    struct {
        hf_chan_handle_t Chan;
    } HF_CreateAudioLink_param;

    //bt_status_t HF_DisconnectAudioLink(hf_chan_handle_t Chan)
    struct {
        hf_chan_handle_t Chan;
    } HF_DisconnectAudioLink_param;

    //bt_status_t HF_EnableSniffMode(hf_chan_handle_t Chan, BOOL Enable)
    struct {
        hf_chan_handle_t Chan;
        BOOL Enable;
    } HF_EnableSniffMode_param;

    //bt_status_t HF_SetMasterRole(hf_chan_handle_t Chan, BOOL Flag);
    struct {
        hf_chan_handle_t Chan;
        BOOL Flag;
    } HF_SetMasterRole_param;

#if defined (__HSP_ENABLE__)
    //bt_status_t HS_CreateServiceLink(HsChannel *Chan, BT_BD_ADDR *Addr)
    struct {
        HsChannel *Chan;
        bt_bdaddr_t *Addr;
    } HS_CreateServiceLink_param;

    //bt_status_t HS_CreateAudioLink(HsChannel *Chan)
    struct {
        HsChannel *Chan;
    } HS_CreateAudioLink_param;

    //bt_status_t HS_DisconnectAudioLink(HsChannel *Chan)
    struct {
        HsChannel *Chan;
    } HS_DisconnectAudioLink_param;

    //bt_status_t HS_DisconnectServiceLink(HsChannel *Chan)
    struct {
        HsChannel *Chan;
    } HS_DisconnectServiceLink_param;

    //bt_status_t HS_EnableSniffMode(HsChannel *Chan, BOOL Enable)
    struct {
        HsChannel *Chan;
        BOOL Enable;
    } HS_EnableSniffMode_param;


#endif

    struct {
        uint16_t cmdid;
        uint32_t param;
        uint8_t *dataptr;
        uint32_t datalen;
    } SPP_WRITE_param;
} bt_fn_param;

typedef struct {
    uint32_t src_thread;
    uint32_t request_id;
    bt_fn_param param;
} APP_BT_MAIL;

int app_bt_mail_init(void);

int app_bt_generic_req(uint32_t param0, uint32_t param1, uint32_t handler);

int app_bt_Me_switch_sco(uint16_t  scohandle);

int app_bt_ME_SwitchRole(btif_remote_device_t* remDev);

int app_bt_ME_SetConnectionRole(btif_connection_role_t role);

int app_bt_MeDisconnectLink(btif_remote_device_t* remDev);

int app_bt_ME_StopSniff(btif_remote_device_t *remDev);

int app_bt_ME_SetAccessibleMode(btif_accessible_mode_t mode, const btif_access_mode_info_t *info);

int app_bt_Me_SetLinkPolicy(btif_remote_device_t *remDev, btif_link_policy_t policy);

int app_bt_ME_SetConnectionQosInfo(btif_remote_device_t *remDev,
                                              btif_qos_info_t *qosInfo);

int app_bt_CMGR_SetSniffTimer(btif_cmgr_handler_t *Handler, 
                                        btif_sniff_info_t* SniffInfo, 
                                        TimeT Time);

#define app_bt_ME_BLOCK_RF(conhandle, enable) app_bt_ME_Set_LBRT_req(conhandle, enable);
int app_bt_ME_Set_LBRT_req(uint16_t conhandle, uint8_t enable);

int app_bt_CMGR_SetSniffInofToAllHandlerByRemDev(btif_sniff_info_t* SniffInfo, 
                                                                 btif_remote_device_t *RemDev);

int app_bt_A2DP_OpenStream(a2dp_stream_t *Stream, bt_bdaddr_t *Addr);

int app_bt_A2DP_CloseStream(a2dp_stream_t *Stream);

int app_bt_A2DP_SetMasterRole(a2dp_stream_t *Stream, BOOL Flag);

int app_bt_HF_CreateServiceLink(hf_chan_handle_t Chan, bt_bdaddr_t *Addr);

int app_bt_HF_DisconnectServiceLink(hf_chan_handle_t Chan);

int app_bt_HF_CreateAudioLink(hf_chan_handle_t Chan);

int app_bt_HF_DisconnectAudioLink(hf_chan_handle_t Chan);

int app_bt_HF_EnableSniffMode(hf_chan_handle_t Chan, BOOL Enable);

int app_bt_HF_SetMasterRole(hf_chan_handle_t Chan, BOOL Flag);

#if defined (__HSP_ENABLE__)
int app_bt_HS_CreateServiceLink(HsChannel *Chan, bt_bdaddr_t* Addr);

int app_bt_HS_CreateAudioLink(HsChannel *Chan);

int app_bt_HS_DisconnectAudioLink(HsChannel *Chan);

int app_bt_HS_DisconnectServiceLink(HsChannel *Chan);

int app_bt_HS_EnableSniffMode(HsChannel *Chan, BOOL Enable);

#endif

int app_bt_HF_Report_Gain(void);
int app_bt_A2dp_Report_Gain(void);

void app_bt_accessible_manager_process(const btif_event_t *Event);
void app_bt_role_manager_process(const btif_event_t *Event, a2dp_stream_t *Stream);
void app_bt_sniff_manager_process(const btif_event_t *Event);
void app_bt_golbal_handle_hook(const btif_event_t *Event);

#ifdef __cplusplus
}
#endif
#endif /* __APP_BT_FUNC_H__ */

