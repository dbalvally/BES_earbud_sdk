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
#include "hal_uart.h"
#include "hal_timer.h"
#include "audioflinger.h"
#include "lockcqueue.h"
#include "hal_trace.h"
#include "hal_cmu.h"
#include "analog.h"
#include "bt_drv.h"
#include "app_audio.h"
#include "bt_drv_interface.h"
#include "app_bt_stream.h"
#include "nvrecord.h"
#include "nvrecord_env.h"
#if defined(A2DP_LHDC_ON)
#include "lhdcUtil_ext.h"
#endif

#include "bt_drv_reg_op.h"
#include "rtos.h"
#include "besbt.h"

#include "cqueue.h"
#include "btapp.h"
#include "app_bt.h"
#include "apps.h"
#include "resources.h"
#include "app_bt_media_manager.h"
#include "tgt_hardware.h"

#include "a2dp_api.h"
#include "hci_api.h"

#ifdef __TWS__
#include "tws_api.h"
#include "app_bt.h"
#include "app_tws_if.h"
#endif
#if defined(A2DP_AAC_ON)
#include "btapp.h"
#endif

#include "app_bt_conn_mgr.h"
#include "app_tws_role_switch.h"
#include "app_tws_ui.h"
#include "app_hfp.h"
#include "log_section.h"
#if VOICE_DATAPATH
#include "app_voicepath.h"
#endif

extern bool disconn_voice_flag;

#if FPGA==0
extern uint8_t app_poweroff_flag;
#endif
int a2dp_volume_set(U8 vol);


#define A2DP_DEBUG

#ifdef A2DP_DEBUG
#define A2DP_TRACE(str,...) TRACE(str, ##__VA_ARGS__)
#else
#define A2DP_TRACE(str,...)
#endif


extern "C" void switch_to_high_speed_conn_interval(void);
extern "C" void switch_to_low_speed_conn_interval(void);


static struct BT_DEVICE_ID_DIFF stream_id_flag;

static void app_AVRCP_sendCustomCmdRsp(uint8_t device_id, btif_avrcp_channel_t *chnl, uint8_t isAccept, uint8_t transId);
static void app_AVRCP_CustomCmd_Received(uint8_t* ptrData, uint32_t len);

void get_value1_pos(U8 mask,U8 *start_pos, U8 *end_pos)
{
    U8 num = 0;

    for(U8 i=0;i<8;i++){
        if((0x01<<i) & mask){
            *start_pos = i;//start_pos,end_pos stands for the start and end position of value 1 in mask
            break;
        }
    }
    for(U8 i=0;i<8;i++){
        if((0x01<<i) & mask)
            num++;//number of value1 in mask
    }
    *end_pos = *start_pos + num - 1;
}
U8 get_valid_bit(U8 elements, U8 mask)
{
    U8 start_pos,end_pos;

    get_value1_pos(mask,&start_pos,&end_pos);
    //    TRACE("!!!start_pos:%d,end_pos:%d\n",start_pos,end_pos);
    for(U8 i = start_pos; i <= end_pos; i++){
        if((0x01<<i) & elements){
            elements = ((0x01<<i) | (elements & (~mask)));
            break;
        }
    }
    return elements;
}


struct BT_DEVICE_T  app_bt_device;

#if defined(A2DP_LHDC_ON)
uint8_t bits_depth;
uint8_t bt_sbc_player_get_bitsDepth(void){
    if (app_bt_device.sample_bit[stream_id_flag.id] != bits_depth) {
        /* code */
        bits_depth = app_bt_device.sample_bit[stream_id_flag.id];
    }
    return bits_depth;
}
#endif


void app_avrcp_connect_timeout_handler(void const *param)
{
    TRACE("app_avrcp_connect_timeout_handler a2dp state=%d,avrcp state=%d is slave:%d ",app_bt_device.a2dp_state[0],
        btif_get_avrcp_state(app_bt_device.avrcp_channel[0]),is_slave_tws_mode());
    if(app_bt_device.a2dp_state[0] == 1 &&
       btif_get_avrcp_state(app_bt_device.avrcp_channel[0]) != BTIF_AVRCP_STATE_CONNECTED &&
       !is_slave_tws_mode())
    {
        TRACE("%s %d has remote device:%d",__func__,__LINE__, btif_a2dp_stream_has_remote_device(app_tws_get_mobile_sink_stream()) );
        if(btif_a2dp_stream_has_remote_device(app_tws_get_mobile_sink_stream()))
        {
            A2DP_TRACE("start connect avrcp");
            btif_avrcp_connect(app_bt_device.avrcp_channel[0], btif_a2dp_stream_get_remote_bd_addr(app_tws_get_mobile_sink_stream()));
        }
    }
}

osTimerDef (APP_AVRCP_CONNECT, app_avrcp_connect_timeout_handler);
osTimerId POSSIBLY_UNUSED app_avrcp_connect_timer = NULL;

#ifdef BTIF_AVRCP_ADVANCED_CONTROLLER

//user defined a2dp stream codec capabilities:
const char sbc_codec_cap_user[BTIF_A2DP_SBC_OCTET_NUMBER ] = {
    A2D_SBC_IE_SAMP_FREQ_48 | A2D_SBC_IE_SAMP_FREQ_44 | A2D_SBC_IE_CH_MD_STEREO | A2D_SBC_IE_CH_MD_JOINT,
    A2D_SBC_IE_BLOCKS_16 | A2D_SBC_IE_BLOCKS_12 | A2D_SBC_IE_SUBBAND_8 | A2D_SBC_IE_ALLOC_MD_L,
    A2D_SBC_IE_MIN_BITPOOL,
    BTA_AV_CO_SBC_MAX_BITPOOL
};

const char aac_codec_cap_user[BTIF_A2DP_AAC_OCTET_NUMBER ] ={
    BTIF_A2DP_AAC_OCTET0_MPEG2_AAC_LC,
    BTIF_A2DP_AAC_OCTET1_SAMPLING_FREQUENCY_44100,
    BTIF_A2DP_AAC_OCTET2_CHANNELS_1 | BTIF_A2DP_AAC_OCTET2_CHANNELS_2 | BTIF_A2DP_AAC_OCTET2_SAMPLING_FREQUENCY_48000,
    BTIF_A2DP_AAC_OCTET3_VBR_SUPPORTED | ((BTIF_MAX_AAC_BITRATE >> 16) & 0x7f),
    (BTIF_MAX_AAC_BITRATE >> 8) & 0xff,
    (BTIF_MAX_AAC_BITRATE) & 0xff
};

void a2dp_init(void)
{
      btif_a2dp_register_codec_cap(BTIF_AVDTP_CODEC_TYPE_SBC, (uint8_t *)sbc_codec_cap_user, BTIF_A2DP_SBC_OCTET_NUMBER);
      btif_a2dp_register_codec_cap(BTIF_AVDTP_CODEC_TYPE_MPEG2_4_AAC, (uint8_t*)aac_codec_cap_user, BTIF_A2DP_AAC_OCTET_NUMBER);

      btif_a2dp_init();

     for(int i =0; i< BT_DEVICE_NUM; i++ ){
         app_bt_device.a2dp_stream[i] = btif_a2dp_alloc_stream();
     }
#if defined(A2DP_LHDC_ON)
          app_bt_device.a2dp_lhdc_stream  = btif_a2dp_alloc_stream();
#endif
#if defined(ALL_USE_OPUS)
          app_bt_device.a2dp_aac_stream = btif_a2dp_alloc_stream();
#endif
#if defined(MASTER_USE_OPUS) || defined(ALL_USE_OPUS)
          app_bt_device.a2dp_opus_stream = btif_a2dp_alloc_stream();
#endif
#if defined(A2DP_AAC_ON)
          app_bt_device.a2dp_aac_stream = btif_a2dp_alloc_stream();
#endif

    for(uint8_t i=0; i<BT_DEVICE_NUM; i++)
    {
        app_bt_device.a2dp_state[i]=0;
        app_bt_device.a2dp_streamming[i] = 0;
        app_bt_device.avrcp_get_capabilities_rsp[i] = NULL;
        app_bt_device.avrcp_control_rsp[i] = NULL;
        app_bt_device.avrcp_notify_rsp[i] = NULL;
        app_bt_device.avrcp_cmd1[i] = NULL;
        app_bt_device.avrcp_cmd2[i] = NULL;
        app_bt_device.avrcp_custom_cmd[i] = NULL;
    }

    btif_app_a2dp_avrcpadvancedpdu_mempool_init();

    app_bt_device.a2dp_state[BT_DEVICE_ID_1]=0;
    app_bt_device.a2dp_play_pause_flag = 0;
    app_bt_device.curr_a2dp_stream_id= BT_DEVICE_ID_1;

    if (app_avrcp_connect_timer == NULL){
        app_avrcp_connect_timer = osTimerCreate(osTimer(APP_AVRCP_CONNECT), osTimerOnce, NULL);
    }
}

#ifdef __TWS__
typedef uint8_t tx_done_flag;
#define TX_DONE_FLAG_INIT           0
#define TX_DONE_FLAG_SUCCESS    1
#define TX_DONE_FLAG_FAIL           2
#define TX_DONE_FLAG_TXING        3
tx_done_flag TG_tx_done_flag = TX_DONE_FLAG_INIT;



void avrcp_set_slave_volume(uint8_t transid,int8_t volume)
{
   btif_avrcp_channel_t channel;
   btif_avrcp_chnl_handle_t tws_TG = NULL;
    bt_status_t  get_volume_rsp_status;

    if((TG_tx_done_flag == TX_DONE_FLAG_INIT)||(TG_tx_done_flag == TX_DONE_FLAG_SUCCESS)){
        if(is_tws_master_conn_slave_state())
        {
            A2DP_TRACE("!!!avrcp_set_slave_volume transid: 0x%x volumn: %d\n",transid, volume);
            tws_TG = tws_get_avrcp_TGchannel();
            channel.avrcp_channel_handle = tws_TG;

            if(btif_avrcp_adv_cmd_inuse(&channel) && btif_avrcp_state_connected(&channel)){
                if (app_bt_device.avrcp_volume_cmd[0] == NULL)
                    btif_app_a2dp_avrcpadvancedpdu_mempool_calloc(&app_bt_device.avrcp_volume_cmd[0]);
           btif_avrcp_set_volume_cmd(app_bt_device.avrcp_volume_cmd[0], transid, volume );
           app_bt_device.volume_report[0] = BTIF_AVCTP_RESPONSE_INTERIM;

                get_volume_rsp_status = btif_avrcp_ct_get_absolute_volume_rsp(&channel, app_bt_device.avrcp_volume_cmd[0], volume);
                TG_tx_done_flag = TX_DONE_FLAG_TXING;
                A2DP_TRACE("!!!avrcp_set_slave_volume  get_volume_rsp_status:%x\n",get_volume_rsp_status);
                if(get_volume_rsp_status != BT_STS_PENDING){
                    //don't know reason why the status != pending yet
                    TG_tx_done_flag = TX_DONE_FLAG_FAIL;
                }
            }
        }
    }
}


 void avrcp_callback_TG(btif_avrcp_chnl_handle_t chnl, const avrcp_callback_parms_t *parms)
{
    A2DP_TRACE("avrcp_callback_TG : chnl %p, Parms %p\n", chnl, parms);
    A2DP_TRACE("::avrcp_callback_TG Parms->event %d\n", btif_get_avrcp_cb_channel_state(parms));
    btif_avrcp_channel_t*  channel =  btif_get_avrcp_channel(chnl);


#ifdef __BT_ONE_BRING_TWO__
    enum BT_DEVICE_ID_T device_id = (chnl == app_bt_device.avrcp_channel[0]->avrcp_channel_handle)?BT_DEVICE_ID_1:BT_DEVICE_ID_2;
#else
    enum BT_DEVICE_ID_T device_id = BT_DEVICE_ID_1;
#endif
    switch(btif_avrcp_get_callback_event((avrcp_callback_parms_t *)parms))
    {
        case BTIF_AVRCP_EVENT_CONNECT:
        btif_set_avrcp_state(channel,BTIF_AVRCP_STATE_CONNECTED);
#ifdef __TWS__
            if(is_tws_master_conn_slave_state())
            {
                // avrcp_set_slave_volume(0,a2dp_volume_get_tws());
            }
#endif

            if (app_bt_device.avrcp_custom_cmd[device_id] == NULL)
            {
                btif_app_a2dp_avrcpadvancedpdu_mempool_calloc(&app_bt_device.avrcp_custom_cmd[device_id]);
            }
            A2DP_TRACE("::avrcp_callback_TG AVRCP_EVENT_CONNECT %x,device_id=%d\n", btif_get_avrcp_version( channel),device_id);

            log_event_1(EVENT_AVRCP_CONNECTED, (btif_avrcp_get_cmgrhandler_remDev_hciHandle(channel))&0x3);
            break;
        case BTIF_AVRCP_EVENT_DISCONNECT:
            A2DP_TRACE("::avrcp_callback_TG AVRCP_EVENT_DISCONNECT");

            log_event_2(EVENT_AVRCP_DISCONNECTED, (btif_avrcp_get_cmgrhandler_remDev_hciHandle(channel))&0x3,
               btif_get_avrcp_cb_channel_error_code((avrcp_callback_parms_t *)parms));
        btif_set_avrcp_state(channel,BTIF_AVRCP_STATE_DISCONNECTED) ;
            if (app_bt_device.avrcp_get_capabilities_rsp[device_id]){
                btif_app_a2dp_avrcpadvancedpdu_mempool_free(app_bt_device.avrcp_get_capabilities_rsp[device_id]);
                app_bt_device.avrcp_get_capabilities_rsp[device_id] = NULL;
            }
            if (app_bt_device.avrcp_control_rsp[device_id]){
                btif_app_a2dp_avrcpadvancedpdu_mempool_free(app_bt_device.avrcp_control_rsp[device_id]);
                app_bt_device.avrcp_control_rsp[device_id] = NULL;
            }
            if (app_bt_device.avrcp_notify_rsp[device_id]){
                btif_app_a2dp_avrcpadvancedpdu_mempool_free(app_bt_device.avrcp_notify_rsp[device_id]);
                app_bt_device.avrcp_notify_rsp[device_id] = NULL;
            }

            if (app_bt_device.avrcp_cmd1[device_id]){
                btif_app_a2dp_avrcpadvancedpdu_mempool_free(app_bt_device.avrcp_cmd1[device_id]);
                app_bt_device.avrcp_cmd1[device_id] = NULL;
            }
            if (app_bt_device.avrcp_cmd2[device_id]){
                btif_app_a2dp_avrcpadvancedpdu_mempool_free(app_bt_device.avrcp_cmd2[device_id]);
                app_bt_device.avrcp_cmd2[device_id] = NULL;
            }

            if (app_bt_device.avrcp_custom_cmd[device_id]){
                btif_app_a2dp_avrcpadvancedpdu_mempool_free(app_bt_device.avrcp_custom_cmd[device_id]);
                app_bt_device.avrcp_custom_cmd[device_id] = NULL;
            }

            app_bt_device.volume_report[device_id] = 0;


            break;
        case BTIF_AVRCP_EVENT_RESPONSE:
            A2DP_TRACE("::avrcp_callback_TG AVRCP_EVENT_RESPONSE op=%x,status=%x\n", btif_get_avrcp_cb_channel_advOp(parms),
                btif_get_avrcp_cb_channel_state(parms));

            break;
        case BTIF_AVRCP_EVENT_PANEL_CNF:
            A2DP_TRACE("::avrcp_callback_TG AVRCP_EVENT_PANEL_CNF %x,%x,%x",
               btif_get_avrcp_panel_cnf(parms)->response, btif_get_avrcp_panel_cnf(parms)->operation,
                                                              btif_get_avrcp_panel_cnf(parms)->press);
            break;
#ifdef __TWS__
        case BTIF_AVRCP_EVENT_PANEL_PRESS:
            A2DP_TRACE("::avrcp_callback_TG AVRCP_EVENT_PANEL_PRESS %x,%x,device_id=%d",
                 btif_get_avrcp_panel_cnf(parms)->operation,   btif_get_avrcp_panel_ind((avrcp_callback_parms_t *)parms)->operation,   device_id);
            switch(btif_get_avrcp_panel_ind((avrcp_callback_parms_t *)parms)->operation)
            {
                case BTIF_AVRCP_POP_STOP:
                    A2DP_TRACE("avrcp_callback_TG avrcp_key = AVRCP_KEY_STOP");
                    btif_avrcp_set_panel_key(app_bt_device.avrcp_channel[BT_DEVICE_ID_1],BTIF_AVRCP_POP_STOP,TRUE);
                    btif_avrcp_set_panel_key(app_bt_device.avrcp_channel[BT_DEVICE_ID_1],BTIF_AVRCP_POP_STOP,FALSE);

                    app_bt_device.a2dp_play_pause_flag = 0;
                    break;
                case BTIF_AVRCP_POP_PLAY:
                case BTIF_AVRCP_POP_PAUSE:
                    if(app_bt_device.a2dp_play_pause_flag == 0){
                        btif_avrcp_set_panel_key(app_bt_device.avrcp_channel[BT_DEVICE_ID_1],BTIF_AVRCP_POP_PLAY,TRUE);
                        btif_avrcp_set_panel_key(app_bt_device.avrcp_channel[BT_DEVICE_ID_1],BTIF_AVRCP_POP_PLAY,FALSE);
                        app_bt_device.a2dp_play_pause_flag = 1;
                    }else{
                        btif_avrcp_set_panel_key(app_bt_device.avrcp_channel[BT_DEVICE_ID_1],BTIF_AVRCP_POP_PAUSE,TRUE);
                        btif_avrcp_set_panel_key(app_bt_device.avrcp_channel[BT_DEVICE_ID_1],BTIF_AVRCP_POP_PAUSE,FALSE);
                        app_bt_device.a2dp_play_pause_flag = 0;
                    }

                    break;
                case BTIF_AVRCP_POP_FORWARD:
                    A2DP_TRACE("avrcp_callback_TG avrcp_key = AVRCP_KEY_FORWARD");
                    btif_avrcp_set_panel_key(app_bt_device.avrcp_channel[BT_DEVICE_ID_1],BTIF_AVRCP_POP_FORWARD,TRUE);
                    btif_avrcp_set_panel_key(app_bt_device.avrcp_channel[BT_DEVICE_ID_1],BTIF_AVRCP_POP_FORWARD,FALSE);
                    app_bt_device.a2dp_play_pause_flag = 1;
                    break;
                case BTIF_AVRCP_POP_BACKWARD:
                    A2DP_TRACE("avrcp_callback_TG avrcp_key = AVRCP_KEY_BACKWARD");
                    btif_avrcp_set_panel_key(app_bt_device.avrcp_channel[BT_DEVICE_ID_1],BTIF_AVRCP_POP_BACKWARD,TRUE);
                    btif_avrcp_set_panel_key(app_bt_device.avrcp_channel[BT_DEVICE_ID_1],BTIF_AVRCP_POP_BACKWARD,FALSE);
                    app_bt_device.a2dp_play_pause_flag = 1;
                    break;
                case BTIF_AVRCP_POP_VOLUME_UP:
                    A2DP_TRACE("avrcp_callback_TG avrcp_key = AVRCP_KEY_VOLUME_UP");
                    btif_avrcp_set_panel_key(app_bt_device.avrcp_channel[BT_DEVICE_ID_1],BTIF_AVRCP_POP_VOLUME_UP,TRUE);
                    btif_avrcp_set_panel_key(app_bt_device.avrcp_channel[BT_DEVICE_ID_1],BTIF_AVRCP_POP_VOLUME_UP,FALSE);
                    app_bt_stream_volumeup();
                    btapp_hfp_report_speak_gain();
                    btapp_a2dp_report_speak_gain();/* for press volume up key on slave */
                    break;
                case BTIF_AVRCP_POP_VOLUME_DOWN:
                    A2DP_TRACE("avrcp_callback_TG avrcp_key = AVRCP_KEY_VOLUME_DOWN");
                    btif_avrcp_set_panel_key(app_bt_device.avrcp_channel[BT_DEVICE_ID_1],BTIF_AVRCP_POP_VOLUME_DOWN,TRUE);
                    btif_avrcp_set_panel_key(app_bt_device.avrcp_channel[BT_DEVICE_ID_1],BTIF_AVRCP_POP_VOLUME_DOWN,FALSE);
                    app_bt_stream_volumedown();
                    btapp_hfp_report_speak_gain();
                    btapp_a2dp_report_speak_gain();/* for press volume down key on slave */
                    break;
                default :
                    break;
            }
            break;
        case BTIF_AVRCP_EVENT_PANEL_HOLD:
            A2DP_TRACE("::avrcp_callback_TG AVRCP_EVENT_PANEL_HOLD %x,%x",
                btif_get_avrcp_panel_cnf(parms)->operation,btif_get_avrcp_panel_ind(parms)->operation);
            break;
        case BTIF_AVRCP_EVENT_PANEL_RELEASE:
            A2DP_TRACE("::avrcp_callback_TG AVRCP_EVENT_PANEL_RELEASE %x,%x",
               btif_get_avrcp_panel_cnf(parms)->operation,btif_get_avrcp_panel_ind(parms)->operation);
            break;
#endif
        case BTIF_AVRCP_EVENT_ADV_TX_DONE:
            A2DP_TRACE("::avrcp_callback_TG AVRCP_EVENT_ADV_TX_DONE device_id=%d,status=%x,errorcode=%x\n",device_id,
                btif_get_avrcp_cb_channel_state(parms),btif_get_avrcp_cb_channel_error_code(parms));
            A2DP_TRACE("::avrcp_callback_TG AVRCP_EVENT_ADV_TX_DONE op:%d, transid:%x\n", btif_get_avrcp_cb_txPdu_Op(parms),
                btif_get_avrcp_cb_txPdu_transId(parms));
            if (btif_get_avrcp_cb_txPdu_Op(parms) == BTIF_AVRCP_OP_GET_CAPABILITIES){
                if (app_bt_device.avrcp_get_capabilities_rsp[device_id] == btif_get_avrcp_cb_txPdu(parms)){
                    app_bt_device.avrcp_get_capabilities_rsp[device_id] = NULL;
                    btif_app_a2dp_avrcpadvancedpdu_mempool_free(btif_get_avrcp_cb_txPdu(parms));
                }
            }
            TG_tx_done_flag = TX_DONE_FLAG_SUCCESS;
#if 0
            if (Parms->p.adv.txPdu->op == AVRCP_OP_SET_ABSOLUTE_VOLUME){
                if (Parms->p.adv.txPdu->ctype != AVCTP_RESPONSE_INTERIM){
                    if (app_bt_device.avrcp_control_rsp[device_id] == Parms->p.adv.txPdu){
                        app_bt_device.avrcp_control_rsp[device_id] = NULL;
                        app_a2dp_avrcpadvancedpdu_mempool_free(Parms->p.adv.txPdu);
                    }
                }
            }
            if (Parms->p.adv.txPdu->op == AVRCP_OP_REGISTER_NOTIFY){
                if (Parms->p.adv.txPdu->ctype != AVCTP_RESPONSE_INTERIM){
                    if (Parms->p.adv.txPdu->parms[0] == AVRCP_EID_VOLUME_CHANGED){
                        app_bt_device.avrcp_notify_rsp[device_id] = NULL;
                        app_a2dp_avrcpadvancedpdu_mempool_free(Parms->p.adv.txPdu);
                    }
                }
            }
#endif

            break;
        case BTIF_AVRCP_EVENT_COMMAND:
            A2DP_TRACE("::avrcp_callback_TG AVRCP_EVENT_COMMAND device_id=%d,role=%x\n",device_id, btif_get_avrcp_channel_role(channel));
            A2DP_TRACE("::avrcp_callback_TG AVRCP_EVENT_COMMAND ctype=%x,subunitype=%x\n", btif_get_avrcp_cmd_frame(parms)->ctype,
                btif_get_avrcp_cmd_frame(parms)->subunitType);
            A2DP_TRACE("::avrcp_callback_TG AVRCP_EVENT_COMMAND subunitId=%x,opcode=%x\n", btif_get_avrcp_cmd_frame(parms)->subunitId,
                btif_get_avrcp_cmd_frame(parms)->opcode);
            A2DP_TRACE("::avrcp_callback_TG AVRCP_EVENT_COMMAND operands=%x,operandLen=%x\n",
                btif_get_avrcp_cmd_frame(parms)->operands,btif_get_avrcp_cmd_frame(parms)->operandLen);
            A2DP_TRACE("::avrcp_callback_TG AVRCP_EVENT_COMMAND more=%x\n", btif_get_avrcp_cmd_frame(parms)->more);
            if(btif_get_avrcp_cmd_frame(parms)->ctype == BTIF_AVRCP_CTYPE_STATUS)
            {
                uint32_t company_id = *(btif_get_avrcp_cmd_frame(parms)->operands+2) + ((uint32_t)(*(btif_get_avrcp_cmd_frame(parms)->operands+1))<<8) +
                    ((uint32_t)(*(btif_get_avrcp_cmd_frame(parms)->operands))<<16);
                TRACE("::avrcp_callback_TG AVRCP_EVENT_COMMAND company_id=%x\n", company_id);
                if(company_id == 0x001958)  //bt sig
                {
                    avrcp_operation_t op = *(btif_get_avrcp_cmd_frame(parms)->operands+3);
                    uint8_t oplen =  *(btif_get_avrcp_cmd_frame(parms)->operands+6)+ ((uint32_t)(*(btif_get_avrcp_cmd_frame(parms)->operands+5))<<8);
                    A2DP_TRACE("::avrcp_callback_TG AVRCP_EVENT_COMMAND op=%x,oplen=%x\n", op,oplen);
                    switch(op)
                    {
                        case BTIF_AVRCP_OP_GET_CAPABILITIES:
                            {
                                uint8_t event = *(btif_get_avrcp_cmd_frame(parms)->operands+7);
                                if(event==BTIF_AVRCP_CAPABILITY_COMPANY_ID)
                                {
                                    A2DP_TRACE("::avrcp_callback_TG AVRCP_EVENT_COMMAND send support compay id");
                                }
                                else if(event == BTIF_AVRCP_CAPABILITY_EVENTS_SUPPORTED)
                                {
                                    A2DP_TRACE("::avrcp_callback_TG AVRCP_EVENT_COMMAND send support event transId:%d",btif_get_avrcp_cmd_frame(parms)->transId);
#ifdef __AVRCP_EVENT_COMMAND_VOLUME_SKIP__
                                   btif_avrcp_set_channel_adv_event_mask(chnl, 0);   ///volume control
#else
                                    btif_avrcp_set_channel_adv_event_mask(chnl, BTIF_AVRCP_ENABLE_VOLUME_CHANGED);   ///volume control
#endif
                                    if (app_bt_device.avrcp_get_capabilities_rsp[device_id] == NULL)
                                        btif_app_a2dp_avrcpadvancedpdu_mempool_calloc(&app_bt_device.avrcp_get_capabilities_rsp[device_id]);

                                   // app_bt_device.avrcp_get_capabilities_rsp[device_id]->transId = btif_get_avrcp_cmd_frame(parms)->transId;
                                    //app_bt_device.avrcp_get_capabilities_rsp[device_id]->ctype = BTIF_AVCTP_RESPONSE_IMPLEMENTED_STABLE;
                    btif_avrcp_set_capabilities_rsp_cmd( app_bt_device.avrcp_get_capabilities_rsp[device_id],btif_get_avrcp_cmd_frame(parms)->transId, BTIF_AVCTP_RESPONSE_IMPLEMENTED_STABLE);
                                    A2DP_TRACE("::avrcp_callback_TG AVRCP_EVENT_COMMAND send support event transId:%d",  btif_get_app_bt_device_avrcp_notify_rsp_transid(app_bt_device.avrcp_get_capabilities_rsp[device_id]));
                                    btif_avrcp_ct_get_capabilities_rsp(channel,app_bt_device.avrcp_get_capabilities_rsp[device_id],BTIF_AVRCP_CAPABILITY_EVENTS_SUPPORTED,btif_get_avrcp_adv_rem_event_mask(channel));
                                   // AVRCP_CtGetCapabilities_Rsp(chnl,app_bt_device.avrcp_get_capabilities_rsp[device_id],BTIF_AVRCP_CAPABILITY_EVENTS_SUPPORTED,chnl->adv.eventMask);
                                }
                                else
                                {
                                    A2DP_TRACE("::avrcp_callback_TG AVRCP_EVENT_COMMAND send error event value");
                                }
                            }
                            break;
                    }

                }

            }else if(btif_get_avrcp_cmd_frame(parms)->ctype == BTIF_AVCTP_CTYPE_CONTROL){
                A2DP_TRACE("::avrcp_callback_TG AVRCP_EVENT_COMMAND AVCTP_CTYPE_CONTROL\n");
                DUMP8("%02x ", btif_get_avrcp_cmd_frame(parms)->operands, btif_get_avrcp_cmd_frame(parms)->operandLen);
                if (btif_get_avrcp_cmd_frame(parms)->operands[3] == BTIF_AVRCP_OP_SET_ABSOLUTE_VOLUME){
                    A2DP_TRACE("::avrcp_callback_TG AVRCP_EID_VOLUME_CHANGED transId:%d\n", btif_get_avrcp_cmd_frame(parms)->transId);
                    a2dp_volume_set(btif_get_avrcp_cmd_frame(parms)->operands[7]);

                    log_event_2(EVENT_AVRCP_VOLUME_CHANGE_REQ_RECEIVED, (btif_avrcp_get_cmgrhandler_remDev_hciHandle(channel))&0x3,
                        btif_get_avrcp_cmd_frame(parms)->operands[7]);

                    if (app_bt_device.avrcp_control_rsp[device_id] == NULL)
                        btif_app_a2dp_avrcpadvancedpdu_mempool_calloc(&app_bt_device.avrcp_control_rsp[device_id]);

                    //app_bt_device.avrcp_control_rsp[device_id]->transId = btif_get_avrcp_cmd_frame(parms)->transId;
                    //app_bt_device.avrcp_control_rsp[device_id]->ctype = BTIF_AVCTP_RESPONSE_ACCEPTED;
            btif_avrcp_set_control_rsp_cmd(app_bt_device.avrcp_control_rsp[device_id],
                btif_get_avrcp_cmd_frame(parms)->transId, BTIF_AVCTP_RESPONSE_ACCEPTED);
                    DUMP8("%02x ",  btif_get_avrcp_cmd_frame(parms)->operands,  btif_get_avrcp_cmd_frame(parms)->operandLen);

                    //AVRCP_CtAcceptAbsoluteVolume_Rsp(chnl, app_bt_device.avrcp_control_rsp[device_id],  btif_get_avrcp_cmd_frame(parms)->operands[7]);
            btif_avrcp_ct_accept_absolute_volume_rsp(channel, app_bt_device.avrcp_control_rsp[device_id],  btif_get_avrcp_cmd_frame(parms)->operands[7]);
                }
                else if (BTIF_AVRCP_OP_CUSTOM_CMD == btif_get_avrcp_cmd_frame(parms)->operands[3])
                {
                    app_AVRCP_CustomCmd_Received(& btif_get_avrcp_cmd_frame(parms)->operands[7],  btif_get_avrcp_cmd_frame(parms)->operandLen - 7);
                    app_AVRCP_sendCustomCmdRsp(device_id, channel, true,  btif_get_avrcp_cmd_frame(parms)->transId);
                }
            }else if ( btif_get_avrcp_cmd_frame(parms)->ctype == BTIF_AVCTP_CTYPE_NOTIFY){
                bt_status_t status;
                A2DP_TRACE("::avrcp_callback_TG AVRCP_EVENT_COMMAND AVCTP_CTYPE_NOTIFY\n");
                DUMP8("%02x ",  btif_get_avrcp_cmd_frame(parms)->operands, btif_get_avrcp_cmd_frame(parms)->operandLen);
                if ( btif_get_avrcp_cmd_frame(parms)->operands[7] == BTIF_AVRCP_EID_VOLUME_CHANGED){
                    A2DP_TRACE("::avrcp_callback_TG AVRCP_EID_VOLUME_CHANGED transId:%d\n",  btif_get_avrcp_cmd_frame(parms)->transId);
                    if (app_bt_device.avrcp_notify_rsp[device_id] == NULL)
                        btif_app_a2dp_avrcpadvancedpdu_mempool_calloc(&app_bt_device.avrcp_notify_rsp[device_id]);

                    //app_bt_device.avrcp_notify_rsp[device_id]->transId =  btif_get_avrcp_cmd_frame(parms)->transId;
                    //app_bt_device.avrcp_notify_rsp[device_id]->ctype = BTIF_AVCTP_RESPONSE_INTERIM;
            btif_avrcp_set_notify_rsp_cmd(app_bt_device.avrcp_notify_rsp[device_id],btif_get_avrcp_cmd_frame(parms)->transId,  BTIF_AVCTP_RESPONSE_INTERIM) ;
                    app_bt_device.volume_report[device_id] = BTIF_AVCTP_RESPONSE_INTERIM;

                    //status = AVRCP_CtGetAbsoluteVolume_Rsp(chnl, app_bt_device.avrcp_notify_rsp[device_id], a2dp_volume_get());
            status = btif_avrcp_ct_get_absolute_volume_rsp(channel, app_bt_device.avrcp_notify_rsp[device_id], a2dp_volume_get());
                    A2DP_TRACE("::avrcp_callback_TG AVRCP_EVENT_COMMAND AVRCP_EID_VOLUME_CHANGED nRet:%x\n",status);
                }
            }
            break;
        case BTIF_AVRCP_EVENT_ADV_CMD_TIMEOUT:
            A2DP_TRACE("::avrcp_callback_TG AVRCP_EVENT_ADV_CMD_TIMEOUT device_id=%d,role=%x\n",device_id,btif_get_avrcp_channel_role(channel));
            break;
    }
}

#endif

//#ifdef AVRCP_TRACK_CHANGED
//#define MAX_NUM_LETTER 20
//#endif
int app_tws_control_avrcp_media_reinit(void);

enum AVRCP_CALLBACK_GETPLAYSTATUS_CMDSTATUS {
    AVRCP_CALLBACK_GETPLAYSTATUS_CMDSTATUS_NULL,
    AVRCP_CALLBACK_GETPLAYSTATUS_CMDSTATUS_INIT,
    AVRCP_CALLBACK_GETPLAYSTATUS_CMDSTATUS_ONPROC,
    AVRCP_CALLBACK_GETPLAYSTATUS_CMDSTATUS_NORESP,
    AVRCP_CALLBACK_GETPLAYSTATUS_CMDSTATUS_OK,
};

enum AVRCP_CALLBACK_GETPLAYSTATUS_CMDSTATUS avrcp_getplaystatus_cmdstatus = AVRCP_CALLBACK_GETPLAYSTATUS_CMDSTATUS_NULL;

avrcp_advanced_pdu_t *avrcp_play_status_cmd = NULL;
static osTimerId avrcp_play_status_cmd_timer = NULL;

#define AVRCP_PLAY_STATUS_CMD_TIMER_TIMEOUT_MS        (5000)

static void avrcp_callback_getplaystatus_timeout_handler(void const *param);
osTimerDef (AVRCP_PLAY_STATUS_CMD_TIMER, avrcp_callback_getplaystatus_timeout_handler);

static void avrcp_callback_getplaystatus_timeout_handler(void const *param)
{
    avrcp_getplaystatus_cmdstatus = AVRCP_CALLBACK_GETPLAYSTATUS_CMDSTATUS_NORESP;
}


int avrcp_callback_getplaystatus(void)
{
    if (avrcp_getplaystatus_cmdstatus == AVRCP_CALLBACK_GETPLAYSTATUS_CMDSTATUS_NORESP ||
            avrcp_getplaystatus_cmdstatus == AVRCP_CALLBACK_GETPLAYSTATUS_CMDSTATUS_NULL){
        return -1;
    }

    avrcp_getplaystatus_cmdstatus = AVRCP_CALLBACK_GETPLAYSTATUS_CMDSTATUS_ONPROC;

    osTimerStop(avrcp_play_status_cmd_timer);
    osTimerStart(avrcp_play_status_cmd_timer, AVRCP_PLAY_STATUS_CMD_TIMER_TIMEOUT_MS);

    btif_avrcp_ct_get_play_status(app_bt_device.avrcp_channel[0], avrcp_play_status_cmd);
    return 0;
}

int avrcp_callback_getplaystatus_init(void)
{
    A2DP_TRACE("%s",__func__);

    if (avrcp_play_status_cmd_timer == NULL) {
        avrcp_play_status_cmd_timer = osTimerCreate (osTimer(AVRCP_PLAY_STATUS_CMD_TIMER), osTimerOnce, NULL);
    }

    if (avrcp_play_status_cmd == NULL){
        btif_app_a2dp_avrcpadvancedpdu_mempool_calloc(&avrcp_play_status_cmd);
    }

    app_tws_control_avrcp_media_reinit();

    avrcp_getplaystatus_cmdstatus = AVRCP_CALLBACK_GETPLAYSTATUS_CMDSTATUS_INIT;
    avrcp_callback_getplaystatus();

    return 0;
}

int avrcp_callback_getplaystatus_deinit(void)
{
    A2DP_TRACE("%s",__func__);

    avrcp_getplaystatus_cmdstatus = AVRCP_CALLBACK_GETPLAYSTATUS_CMDSTATUS_NULL;
    app_tws_control_avrcp_media_reinit();

    return 0;
}

avrcp_media_status_t    media_status = 0xff;
uint8_t avrcp_get_media_status(void)
{
    A2DP_TRACE("%s %d",__func__, media_status);
    return media_status;
}
extern uint8_t avrcp_ctrl_music_flag;
void avrcp_set_media_status(uint8_t status)
{
    A2DP_TRACE("%s %d",__func__, status);
    if ((status == 1 && avrcp_ctrl_music_flag == 2) || (status == 2 && avrcp_ctrl_music_flag == 1))
        avrcp_ctrl_music_flag = 0;


    media_status = status;
}




void avrcp_callback_CT(btif_avrcp_chnl_handle_t  chnl, const avrcp_callback_parms_t *parms)
{
     btif_avrcp_channel_t*  channel =  btif_get_avrcp_channel(chnl);

     A2DP_TRACE("%s : chnl %p, Parms %p\n", __func__,chnl, parms);
     A2DP_TRACE("::%s Parms->event %d\n", __func__, btif_avrcp_get_callback_event(parms));
#ifdef __TWS__
    //AvrcpChannel *tws_TG = NULL;
#endif
#ifdef __BT_ONE_BRING_TWO__
    enum BT_DEVICE_ID_T device_id = (chnl == app_bt_device.avrcp_channel[0]->avrcp_channel_handle)?BT_DEVICE_ID_1:BT_DEVICE_ID_2;
#else
    enum BT_DEVICE_ID_T device_id = BT_DEVICE_ID_1;
#endif
    switch(btif_avrcp_get_callback_event(parms))
    {
        case BTIF_AVRCP_EVENT_CONNECT_IND:
            A2DP_TRACE("::avrcp_callback_CT AVRCP_EVENT_CONNECT_IND %d\n", btif_avrcp_get_callback_event(parms));
            btif_avrcp_connec_rsp(channel, 1);
            app_bt_device.avrcpPendingKey = AVRCP_KEY_NULL;
            app_bt_device.ptrAvrcpChannel[device_id]->avrcp_channel_handle = chnl;
            break;
        case BTIF_AVRCP_EVENT_CONNECT:
            if(btif_get_avrcp_version( channel) >= 0x103)
            {
                A2DP_TRACE("::AVRCP_GET_CAPABILITY\n");
                if (app_bt_device.avrcp_cmd1[device_id] == NULL)
                   btif_app_a2dp_avrcpadvancedpdu_mempool_calloc(&app_bt_device.avrcp_cmd1[device_id]);

                btif_avrcp_ct_get_capabilities(channel, app_bt_device.avrcp_cmd1[device_id],BTIF_AVRCP_CAPABILITY_EVENTS_SUPPORTED);
                avrcp_set_media_status(0xff);
            }

            app_bt_device.ptrAvrcpChannel[device_id]->avrcp_channel_handle = chnl;
            app_bt_device.avrcpPendingKey = AVRCP_KEY_NULL;
        btif_set_avrcp_state(app_bt_device.avrcp_channel[device_id],BTIF_AVRCP_STATE_CONNECTED);


#ifdef AVRCP_TRACK_CHANGED
            if (app_bt_device.avrcp_cmd2[device_id] == NULL)
                btif_app_a2dp_avrcpadvancedpdu_mempool_calloc(&app_bt_device.avrcp_cmd2[device_id]);
            btif_avrcp_ct_register_notification(channel,app_bt_device.avrcp_cmd2[device_id],AVRCP_EID_TRACK_CHANGED,0);
#endif
            A2DP_TRACE("::avrcp_callback_CT AVRCP_EVENT_CONNECT %x,device_id=%d\n", btif_get_avrcp_version( channel),device_id);

            break;
        case BTIF_AVRCP_EVENT_DISCONNECT:
            A2DP_TRACE("::avrcp_callback_CT AVRCP_EVENT_DISCONNECT");

        btif_set_avrcp_state(app_bt_device.avrcp_channel[device_id],BTIF_AVRCP_STATE_DISCONNECTED);
            if (app_bt_device.avrcp_get_capabilities_rsp[device_id]){
                btif_app_a2dp_avrcpadvancedpdu_mempool_free(app_bt_device.avrcp_get_capabilities_rsp[device_id]);
                app_bt_device.avrcp_get_capabilities_rsp[device_id] = NULL;
            }
            if (app_bt_device.avrcp_control_rsp[device_id]){
                btif_app_a2dp_avrcpadvancedpdu_mempool_free(app_bt_device.avrcp_control_rsp[device_id]);
                app_bt_device.avrcp_control_rsp[device_id] = NULL;
            }
            if (app_bt_device.avrcp_notify_rsp[device_id]){
                btif_app_a2dp_avrcpadvancedpdu_mempool_free(app_bt_device.avrcp_notify_rsp[device_id]);
                app_bt_device.avrcp_notify_rsp[device_id] = NULL;
            }

            if (app_bt_device.avrcp_cmd1[device_id]){
                btif_app_a2dp_avrcpadvancedpdu_mempool_free(app_bt_device.avrcp_cmd1[device_id]);
                app_bt_device.avrcp_cmd1[device_id] = NULL;
            }
            if (app_bt_device.avrcp_cmd2[device_id]){
                btif_app_a2dp_avrcpadvancedpdu_mempool_free(app_bt_device.avrcp_cmd2[device_id]);
                app_bt_device.avrcp_cmd2[device_id] = NULL;
            }
            app_bt_device.volume_report[device_id] = 0;
#ifdef AVRCP_TRACK_CHANGED
            app_bt_device.track_changed[device_id] = 0;
#endif
            app_bt_device.ptrAvrcpChannel[device_id] = NULL;
            avrcp_set_media_status(0xff);
            avrcp_callback_getplaystatus_deinit();
            app_disconnection_handler_before_role_switch();

            osTimerStop(app_avrcp_connect_timer);
            if(!(is_simulating_reconnecting_in_progress()) &&
                    !(CONN_OP_STATE_MACHINE_DISCONNECT_ALL == CURRENT_CONN_OP_STATE_MACHINE()) &&
                    !(CONN_OP_STATE_MACHINE_DISCONNECT_ALL_BEFORE_RE_PAIRING == CURRENT_CONN_OP_STATE_MACHINE()) &&
                    !(CONN_OP_STATE_MACHINE_DISCONNECT_SLAVE == CURRENT_CONN_OP_STATE_MACHINE()) &&
                    !(CONN_OP_STATE_MACHINE_DISCONNECT_ALL_BEFORE_CHANGE_ROLE == CURRENT_CONN_OP_STATE_MACHINE())&&
               (!is_slave_tws_mode() )){

                osTimerStart(app_avrcp_connect_timer, 5000);
            }
            break;
        case BTIF_AVRCP_EVENT_RESPONSE:

            A2DP_TRACE("::avrcp_callback_CT AVRCP_EVENT_RESPONSE op=%x,status=%x\n", btif_get_avrcp_cb_channel_advOp(parms),
                     btif_get_avrcp_cb_channel_state(parms));

            break;
        case BTIF_AVRCP_EVENT_PANEL_CNF:
            A2DP_TRACE("::avrcp_callback_CT AVRCP_EVENT_PANEL_CNF %x,%x,%x",
                btif_get_panel_cnf(parms)->response ,btif_get_panel_cnf(parms)->operation, btif_get_panel_cnf(parms)->press);

            if ((CONN_OP_STATE_MACHINE_ROLE_SWITCH == CURRENT_CONN_OP_STATE_MACHINE()) &&
                    (TO_DISCONNECT_BEFORE_ROLE_SWITCH == currentSimulateReconnectionState))
            {
                app_tws_further_handling_for_role_switch();
            }
            else
            {
                if (AVRCP_KEY_NULL != app_bt_device.avrcpPendingKey)
                {
                    app_start_custom_function_in_bt_thread(app_bt_device.avrcpPendingKey,
                            0, (uint32_t)a2dp_handleKey);
                }
            }
            break;
        case BTIF_AVRCP_EVENT_ADV_RESPONSE:
            TRACE("::avrcp_callback_CT AVRCP_EVENT_ADV_RESPONSE device_id=%d,role=%x\n",device_id,btif_get_avrcp_channel_role(channel));
            TRACE("::avrcp_callback_CT AVRCP_EVENT_ADV_RESPONSE op=%x,status=%x\n", btif_get_avrcp_cb_channel_advOp(parms)
                , btif_get_avrcp_cb_channel_state(parms));

            if(btif_get_avrcp_cb_channel_advOp(parms) == BTIF_AVRCP_OP_GET_PLAY_STATUS && btif_get_avrcp_cb_channel_state(parms) == BT_STS_SUCCESS)
            {
                TRACE("::AVRCP_OP_GET_PLAY_STATUS %d/%d Status:%d, response=%d",
                                btif_get_avrcp_adv_rsp_play_status(parms)->position,
                                btif_get_avrcp_adv_rsp_play_status(parms)->length,
                                btif_get_avrcp_adv_rsp_play_status(parms)->mediaStatus);
                 avrcp_set_media_status(btif_get_avrcp_adv_rsp_play_status(parms)->mediaStatus);
            }

            if(btif_get_avrcp_cb_channel_advOp(parms)  == BTIF_AVRCP_OP_GET_CAPABILITIES && btif_get_avrcp_cb_channel_state(parms) == BT_STS_SUCCESS)
            {
                TRACE("::avrcp_callback_CT AVRCP eventmask=%x\n", btif_get_avrcp_adv_rsp(parms)->capability.info.eventMask);

                btif_set_avrcp_adv_rem_event_mask(channel, btif_get_avrcp_adv_rsp(parms)->capability.info.eventMask);
                if(btif_get_avrcp_adv_rem_event_mask(channel) & BTIF_AVRCP_ENABLE_PLAY_STATUS_CHANGED)
                {
                    TRACE("::avrcp_callback_CT AVRCP send notification\n");
                    if (app_bt_device.avrcp_cmd1[device_id] == NULL)
                        btif_app_a2dp_avrcpadvancedpdu_mempool_calloc(&app_bt_device.avrcp_cmd1[device_id]);
            btif_avrcp_ct_register_notification( channel, app_bt_device.avrcp_cmd1[device_id], BTIF_AVRCP_EID_MEDIA_STATUS_CHANGED,0);
                   // AVRCP_CtRegisterNotification(chnl,app_bt_device.avrcp_cmd1[device_id],AVRCP_EID_MEDIA_STATUS_CHANGED,0);

                }
                if(0)//(chnl->adv.rem_eventMask & AVRCP_ENABLE_PLAY_POS_CHANGED)
                {
                    if (app_bt_device.avrcp_cmd2[device_id] == NULL)
                        btif_app_a2dp_avrcpadvancedpdu_mempool_calloc(&app_bt_device.avrcp_cmd2[device_id]);
            btif_avrcp_ct_register_notification( channel, app_bt_device.avrcp_cmd1[device_id], BTIF_AVRCP_EID_MEDIA_STATUS_CHANGED,1);

                }
            }
            else if(btif_get_avrcp_cb_channel_advOp(parms)  == BTIF_AVRCP_OP_REGISTER_NOTIFY &&btif_get_avrcp_cb_channel_state(parms) == BT_STS_SUCCESS)
            {
                   if(btif_get_avrcp_adv_notify(parms)->event == BTIF_AVRCP_EID_MEDIA_STATUS_CHANGED)
                {
                        TRACE("::avrcp_callback_CT ACRCP notify rsp playback states=%x",btif_get_avrcp_adv_notify(parms)->p.mediaStatus);
                        if(avrcp_get_media_status()!=btif_get_avrcp_adv_notify(parms)->p.mediaStatus){
                            avrcp_set_media_status(btif_get_avrcp_adv_notify(parms)->p.mediaStatus);
                    }
                    // app_bt_device.a2dp_state = Parms->p.adv.notify.p.mediaStatus;
                }
                   else if(btif_get_avrcp_adv_notify(parms)->event == BTIF_AVRCP_EID_PLAY_POS_CHANGED)
                {
                        TRACE("::avrcp_callback_CT ACRCP notify rsp play pos =%x",btif_get_avrcp_adv_notify(parms)->p.position);
                }
                   else if(btif_get_avrcp_adv_notify(parms)->event == BTIF_AVRCP_EID_VOLUME_CHANGED){
                        TRACE("::avrcp_callback_CT ACRCP notify rsp volume =%x",btif_get_avrcp_adv_notify(parms)->p.volume);
                         if (is_slave_tws_mode())
                    {
                        conn_set_flagofA2dpVolConfiguredByMaster(true);
                    }
                        a2dp_volume_set(btif_get_avrcp_adv_notify(parms)->p.volume);
                }
#ifdef AVRCP_TRACK_CHANGED
                    else if(btif_get_avrcp_adv_notify(parms)->event == BTIF_AVRCP_EID_TRACK_CHANGED){
                    //   TRACE("::AVRCP_EID_TRACK_CHANGED transId:%d\n", Parms->p.cmdFrame->transId);
                    if (app_bt_device.avrcp_notify_rsp[device_id] == NULL)
                            btif_app_a2dp_avrcpadvancedpdu_mempool_calloc(&app_bt_device.avrcp_notify_rsp[device_id]);
                    //                     app_bt_device.avrcp_notify_rsp[device_id]->transId = Parms->p.cmdFrame->transId;

                        btif_set_app_bt_device_avrcp_notify_rsp_ctype(app_bt_device.avrcp_notify_rsp[device_id], BTIF_AVCTP_RESPONSE_INTERIM);

                        app_bt_device.track_changed[device_id] = BTIF_AVCTP_RESPONSE_INTERIM;
            btif_avrcp_ct_get_media_Info(channel,app_bt_device.avrcp_notify_rsp[device_id],0x7f)    ;
                       // AVRCP_CtGetMediaInfo(chnl, app_bt_device.avrcp_notify_rsp[device_id],0x7f);
                }
#endif
            }
#ifdef AVRCP_TRACK_CHANGED
            else if(btif_get_avrcp_cb_channel_advOp(parms) == AVRCP_OP_GET_MEDIA_INFO && btif_get_avrcp_cb_channel_state(parms) == BT_STS_SUCCESS){
                //TRACE("::AVRCP_OP_GET_MEDIA_INFO msU32=%x, lsU32=%x\n",Parms->p.adv.notify.p.track.msU32,Parms->p.adv.notify.p.track.lsU32);
                //char song_title[MAX_NUM_LETTER];
                //memcpy(song_title,Parms->p.adv.rsp.element.txt[0].string,Parms->p.adv.rsp.element.txt[0].length);
                TRACE("AVRCP_TRACK_CHANGED numid=%d",btif_get_avrcp_adv_rsp(parms)->element.numIds);
                for(uint8_t i=0;i<7;i++)
                {
                    if(btif_get_avrcp_adv_rsp(parms)->element.txt[i].length>0)
                    {
                        TRACE("Id=%d,%s\n",i,btif_get_avrcp_adv_rsp(parms)->element.txt[i].string);
                    }
                }

            }
#endif
            break;
        case BTIF_AVRCP_EVENT_COMMAND:
            TRACE("::avrcp_callback_CT AVRCP_EVENT_COMMAND device_id=%d,role=%x\n",device_id,btif_get_avrcp_channel_role(channel));
            TRACE("::avrcp_callback_CT AVRCP_EVENT_COMMAND ctype=%x,subunitype=%x\n", btif_get_avrcp_cmd_frame(parms)->ctype,btif_get_avrcp_cmd_frame(parms)->subunitType);
            TRACE("::avrcp_callback_CT AVRCP_EVENT_COMMAND subunitId=%x,opcode=%x\n", btif_get_avrcp_cmd_frame(parms)->subunitId,btif_get_avrcp_cmd_frame(parms)->opcode);
            TRACE("::avrcp_callback_CT AVRCP_EVENT_COMMAND operands=%x,operandLen=%x\n", btif_get_avrcp_cmd_frame(parms)->operands,btif_get_avrcp_cmd_frame(parms)->operandLen);
            TRACE("::avrcp_callback_CT AVRCP_EVENT_COMMAND more=%x\n", btif_get_avrcp_cmd_frame(parms)->more);
            if( btif_get_avrcp_cmd_frame(parms)->ctype == BTIF_AVRCP_CTYPE_STATUS)
            {
                uint32_t company_id = *(btif_get_avrcp_cmd_frame(parms)->operands+2) + ((uint32_t)(*(btif_get_avrcp_cmd_frame(parms)->operands+1))<<8) + ((uint32_t)(*(btif_get_avrcp_cmd_frame(parms)->operands))<<16);
                TRACE("::avrcp_callback_CT AVRCP_EVENT_COMMAND company_id=%x\n", company_id);
                if(company_id == 0x001958)  //bt sig
                {
                    avrcp_operation_t op = *(btif_get_avrcp_cmd_frame(parms)->operands+3);
                    uint8_t oplen =  *(btif_get_avrcp_cmd_frame(parms)->operands+6)+ ((uint32_t)(*(btif_get_avrcp_cmd_frame(parms)->operands+5))<<8);
                    TRACE("::avrcp_callback_CT AVRCP_EVENT_COMMAND op=%x,oplen=%x\n", op,oplen);
                    switch(op)
                    {
                        case BTIF_AVRCP_OP_GET_CAPABILITIES:
                            {
                                uint8_t event = *(btif_get_avrcp_cmd_frame(parms)->operands+7);
                                if(event==BTIF_AVRCP_CAPABILITY_COMPANY_ID)
                                {
                                    TRACE("::avrcp_callback_CT AVRCP_EVENT_COMMAND send support compay id");
                                }
                                else if(event == BTIF_AVRCP_CAPABILITY_EVENTS_SUPPORTED)
                                {
                                    TRACE("::avrcp_callback_CT AVRCP_EVENT_COMMAND send support event transId:%d", btif_get_avrcp_cmd_frame(parms)->transId);
#ifdef __AVRCP_EVENT_COMMAND_VOLUME_SKIP__
                                    //chnl->adv.eventMask = 0;
                                     btif_set_avrcp_adv_rem_event_mask(channel, 0);

#else
                                     btif_set_avrcp_adv_rem_event_mask(channel, BTIF_AVRCP_ENABLE_VOLUME_CHANGED);
                                   // chnl->adv.eventMask = BTIF_AVRCP_ENABLE_VOLUME_CHANGED;   ///volume control
#endif
                                    if (app_bt_device.avrcp_get_capabilities_rsp[device_id] == NULL)
                                        btif_app_a2dp_avrcpadvancedpdu_mempool_calloc(&app_bt_device.avrcp_get_capabilities_rsp[device_id]);

                                    //app_bt_device.avrcp_get_capabilities_rsp[device_id]->transId = btif_get_avrcp_cmd_frame(parms)->transId;
                                    //app_bt_device.avrcp_get_capabilities_rsp[device_id]->ctype = AVCTP_RESPONSE_IMPLEMENTED_STABLE;
                                     btif_avrcp_set_capabilities_rsp_cmd(app_bt_device.avrcp_get_capabilities_rsp[device_id],btif_get_avrcp_cmd_frame(parms)->transId, BTIF_AVCTP_RESPONSE_IMPLEMENTED_STABLE);

                                    TRACE("::avrcp_callback_CT AVRCP_EVENT_COMMAND send support event transId:%d",    btif_get_app_bt_device_avrcp_notify_rsp_transid(app_bt_device.avrcp_get_capabilities_rsp[device_id]));
                                   //AVRCP_CtGetCapabilities_Rsp(chnl,app_bt_device.avrcp_get_capabilities_rsp[device_id],AVRCP_CAPABILITY_EVENTS_SUPPORTED,chnl->adv.eventMask);
                                    btif_avrcp_ct_get_capabilities_rsp(channel,app_bt_device.avrcp_get_capabilities_rsp[device_id],BTIF_AVRCP_CAPABILITY_EVENTS_SUPPORTED,btif_get_avrcp_adv_rem_event_mask(channel));

                    avrcp_callback_getplaystatus_init();
                                }
                                else
                                {
                                    TRACE("::avrcp_callback_CT AVRCP_EVENT_COMMAND send error event value");
                                }
                            }
                            break;
                    }

                }

            }else if(btif_get_avrcp_cmd_frame(parms)->ctype == BTIF_AVCTP_CTYPE_CONTROL){
                TRACE("::avrcp_callback_CT AVRCP_EVENT_COMMAND AVCTP_CTYPE_CONTROL\n");
                DUMP8("%02x ", btif_get_avrcp_cmd_frame(parms)->operands, btif_get_avrcp_cmd_frame(parms)->operandLen);
                if (btif_get_avrcp_cmd_frame(parms)->operands[3] == BTIF_AVRCP_OP_SET_ABSOLUTE_VOLUME){
                    TRACE("::avrcp_callback_CT AVRCP_EID_VOLUME_CHANGED transId:%d\n", btif_get_avrcp_cmd_frame(parms)->transId);
                    a2dp_volume_set(btif_get_avrcp_cmd_frame(parms)->operands[7]);
                    if (app_bt_device.avrcp_control_rsp[device_id] == NULL)
                        btif_app_a2dp_avrcpadvancedpdu_mempool_calloc(&app_bt_device.avrcp_control_rsp[device_id]);

                   // app_bt_device.avrcp_control_rsp[device_id]->transId = btif_get_avrcp_cmd_frame(parms)->transId;
                    //app_bt_device.avrcp_control_rsp[device_id]->ctype = AVCTP_RESPONSE_ACCEPTED;
                     btif_avrcp_set_control_rsp_cmd(app_bt_device.avrcp_control_rsp[device_id],btif_get_avrcp_cmd_frame(parms)->transId,  BTIF_AVCTP_RESPONSE_ACCEPTED);

                    DUMP8("%02x ", btif_get_avrcp_cmd_frame(parms)->operands, btif_get_avrcp_cmd_frame(parms)->operandLen);
                    //AVRCP_CtAcceptAbsoluteVolume_Rsp(chnl, app_bt_device.avrcp_control_rsp[device_id], btif_get_avrcp_cmd_frame(parms)->operands[7]);
            btif_avrcp_ct_accept_absolute_volume_rsp(channel, app_bt_device.avrcp_control_rsp[device_id], btif_get_avrcp_cmd_frame(parms)->operands[7]);
#ifdef __TWS__
                    app_tws_set_slave_volume(btif_get_avrcp_cmd_frame(parms)->operands[7]);
                    // avrcp_set_slave_volume(Parms->p.cmdFrame->transId,Parms->p.cmdFrame->operands[7]);/* for volume up/down on phone */
#endif
                }
                else if (BTIF_AVRCP_OP_CUSTOM_CMD == btif_get_avrcp_cmd_frame(parms)->operands[3])
                {
                    app_AVRCP_CustomCmd_Received(&btif_get_avrcp_cmd_frame(parms)->operands[7], btif_get_avrcp_cmd_frame(parms)->operandLen - 7);
                    app_AVRCP_sendCustomCmdRsp(device_id, channel, true,btif_get_avrcp_cmd_frame(parms)->transId);
                }
            }else if (btif_get_avrcp_cmd_frame(parms)->ctype == BTIF_AVCTP_CTYPE_NOTIFY){
                bt_status_t status;
                TRACE("::avrcp_callback_CT AVRCP_EVENT_COMMAND AVCTP_CTYPE_NOTIFY\n");
                DUMP8("%02x ", btif_get_avrcp_cmd_frame(parms)->operands, btif_get_avrcp_cmd_frame(parms)->operandLen);
                if (btif_get_avrcp_cmd_frame(parms)->operands[7] == BTIF_AVRCP_EID_VOLUME_CHANGED){
                    TRACE("::avrcp_callback_CT AVRCP_EID_VOLUME_CHANGED transId:%d\n",   btif_get_avrcp_cmd_frame(parms)->transId);
                    if (app_bt_device.avrcp_notify_rsp[device_id] == NULL)
                        btif_app_a2dp_avrcpadvancedpdu_mempool_calloc(&app_bt_device.avrcp_notify_rsp[device_id]);

                    //app_bt_device.avrcp_notify_rsp[device_id]->transId = btif_get_avrcp_cmd_frame(parms)->transId;
                    //app_bt_device.avrcp_notify_rsp[device_id]->ctype = AVCTP_RESPONSE_INTERIM;
            btif_avrcp_set_notify_rsp_cmd(app_bt_device.avrcp_notify_rsp[device_id],btif_get_avrcp_cmd_frame(parms)->transId,  BTIF_AVCTP_RESPONSE_INTERIM) ;
                    app_bt_device.volume_report[device_id] = BTIF_AVCTP_RESPONSE_INTERIM;
            status = btif_avrcp_ct_get_absolute_volume_rsp(channel,
                                                      app_bt_device.avrcp_notify_rsp[device_id],a2dp_volume_get());
                //    status = AVRCP_CtGetAbsoluteVolume_Rsp(chnl, app_bt_device.avrcp_notify_rsp[device_id], a2dp_volume_get());

                    TRACE("::avrcp_callback_CT AVRCP_EVENT_COMMAND AVRCP_EID_VOLUME_CHANGED nRet:%x\n",status);
#ifdef __TWS__
                    app_tws_set_slave_volume(a2dp_volume_get_tws());
                    // avrcp_set_slave_volume(Parms->p.cmdFrame->transId,a2dp_volume_get_tws());/* for volume up/down press key */
#endif
                }
            }
            break;
        case BTIF_AVRCP_EVENT_ADV_NOTIFY:
            TRACE("::avrcp_callback_CT AVRCP_EVENT_ADV_NOTIFY  adv.notify.event=%x,device_id=%d,chnl->role=%x\n",btif_get_avrcp_adv_notify(parms)->event,device_id, btif_get_avrcp_cb_channel_role( channel));
            if(btif_get_avrcp_adv_notify(parms)->event == BTIF_AVRCP_EID_VOLUME_CHANGED)
            {
                    TRACE("::avrcp_callback_CT ACRCP notify  vol =%x",btif_get_avrcp_adv_notify(parms)->p.volume);
                    //AVRCP_CtRegisterNotification(chnl,app_bt_device.avrcp_notify_rsp[device_id],BTIF_AVRCP_EID_VOLUME_CHANGED,0);
            btif_avrcp_ct_register_notification(channel,app_bt_device.avrcp_notify_rsp[device_id],BTIF_AVRCP_EID_VOLUME_CHANGED,0);
            }
           else if(btif_get_avrcp_adv_notify(parms)->event == BTIF_AVRCP_EID_MEDIA_STATUS_CHANGED)
            {
                TRACE("::avrcp_callback_CT ACRCP notify  playback states=%x",btif_get_avrcp_adv_notify(parms)->p.mediaStatus);
                if (app_bt_device.avrcp_cmd1[device_id] == NULL)
                    btif_app_a2dp_avrcpadvancedpdu_mempool_calloc(&app_bt_device.avrcp_cmd1[device_id]);
               // AVRCP_CtRegisterNotification(chnl,app_bt_device.avrcp_cmd1[device_id],BTIF_AVRCP_EID_MEDIA_STATUS_CHANGED,0);
        btif_avrcp_ct_register_notification(channel,app_bt_device.avrcp_cmd1[device_id],BTIF_AVRCP_EID_MEDIA_STATUS_CHANGED,0);
                //avrcp_set_media_status(btif_get_avrcp_adv_notify(parms)->p.mediaStatus);
            }
           else if(btif_get_avrcp_adv_notify(parms)->event == BTIF_AVRCP_EID_PLAY_POS_CHANGED)
            {
                TRACE("::avrcp_callback_CT ACRCP notify  play pos =%x",btif_get_avrcp_adv_notify(parms)->p.position);
                if (app_bt_device.avrcp_cmd2[device_id] == NULL)
                    btif_app_a2dp_avrcpadvancedpdu_mempool_calloc(&app_bt_device.avrcp_cmd2[device_id]);
                //AVRCP_CtRegisterNotification(chnl,app_bt_device.avrcp_cmd2[device_id],BTIF_AVRCP_EID_PLAY_POS_CHANGED,1);
        btif_avrcp_ct_register_notification(channel,app_bt_device.avrcp_cmd2[device_id],BTIF_AVRCP_EID_PLAY_POS_CHANGED,1);
            }
#ifdef AVRCP_TRACK_CHANGED
         else if(btif_get_avrcp_adv_notify(parms)->event == BTIF_AVRCP_EID_TRACK_CHANGED){
            TRACE("::AVRCP notify track msU32=%x, lsU32=%x",btif_get_avrcp_adv_notify(parms)->p.track.msU32,btif_get_avrcp_adv_notify(parms)->p.track.lsU32);
                if (app_bt_device.avrcp_cmd2[device_id] == NULL)
                btif_app_a2dp_avrcpadvancedpdu_mempool_calloc(&app_bt_device.avrcp_cmd2[device_id]);
        btif_avrcp_ct_register_notification(channel,app_bt_device.avrcp_cmd2[device_id],BTIF_AVRCP_EID_TRACK_CHANGED,0);
            //AVRCP_CtRegisterNotification(chnl,app_bt_device.avrcp_cmd2[device_id],BTIF_AVRCP_EID_TRACK_CHANGED,0);
            }
#endif
            break;
        case BTIF_AVRCP_EVENT_ADV_TX_DONE:
            TRACE("::avrcp_callback_CT AVRCP_EVENT_ADV_TX_DONE device_id=%d,status=%x,errorcode=%x\n",device_id,
                 btif_get_avrcp_cb_channel_state(parms),btif_get_avrcp_cb_channel_error_code);
            TRACE("::avrcp_callback_CT AVRCP_EVENT_ADV_TX_DONE op:%d, transid:%x\n",btif_get_avrcp_cb_txPdu_Op(parms),
                btif_get_avrcp_cb_txPdu_transId(parms));
            break;
        case BTIF_AVRCP_EVENT_ADV_CMD_TIMEOUT:
            TRACE("::avrcp_callback_CT AVRCP_EVENT_ADV_CMD_TIMEOUT device_id=%d,role=%x\n",device_id,btif_get_avrcp_cb_channel_role(channel));
            break;

    }
}

#else
void a2dp_init(void)
{
    for(uint8_t i=0; i<BT_DEVICE_NUM; i++)
    {
        app_bt_device.a2dp_state[i]=0;
    }

    app_bt_device.a2dp_state[BT_DEVICE_ID_1]=0;
    app_bt_device.a2dp_play_pause_flag = 0;
    app_bt_device.curr_a2dp_stream_id= BT_DEVICE_ID_1;

    if (app_avrcp_connect_timer == NULL){
        app_avrcp_connect_timer = osTimerCreate(osTimer(APP_AVRCP_CONNECT), osTimerOnce, NULL);
    }
}

#if 0
void avrcp_callback(AvrcpChannel *chnl, const AvrcpCallbackParms *Parms)
{
    TRACE("avrcp_callback : chnl %p, Parms %p\n", chnl, Parms);
    TRACE("::Parms->event %d\n", Parms->event);
    switch(Parms->event)
    {
        case AVRCP_EVENT_CONNECT_IND:
            TRACE("::AVRCP_EVENT_CONNECT_IND %d\n", Parms->event);
            AVRCP_ConnectRsp(chnl, 1);
            break;
        case AVRCP_EVENT_CONNECT:
            TRACE("::AVRCP_EVENT_CONNECT %d\n", Parms->event);
            break;
        case AVRCP_EVENT_RESPONSE:
            TRACE("::AVRCP_EVENT_RESPONSE %d\n", Parms->event);

            break;
        case AVRCP_EVENT_PANEL_CNF:
            TRACE("::AVRCP_EVENT_PANEL_CNF %x,%x,%x",
                    Parms->p.panelCnf.response,Parms->p.panelCnf.operation,Parms->p.panelCnf.press);
#if 0
            if((Parms->p.panelCnf.response == AVCTP_RESPONSE_ACCEPTED) && (Parms->p.panelCnf.press == TRUE))
            {
                AVRCP_SetPanelKey(chnl,Parms->p.panelCnf.operation,FALSE);
            }
#endif
            break;
    }
}
#endif
#endif
//void avrcp_init(void)
//{
//  hal_uart_open(HAL_UART_ID_0,NULL);
//    TRACE("avrcp_init...OK\n");
//}
//

static bool a2dp_bdaddr_from_id(uint8_t id, bt_bdaddr_t *bd_addr) {
  ASSERT(id < BT_DEVICE_NUM, "invalid bt device id");
  if(NULL != bd_addr) {
    bt_bdaddr_t *addr = btif_me_get_remote_device_bdaddr(btif_a2dp_get_remote_device(
        app_bt_device.a2dp_connected_stream[id]));
    memset(bd_addr, 0, sizeof(bt_bdaddr_t));
    if (NULL != addr) {
      memcpy(bd_addr, addr->address, BTIF_BD_ADDR_SIZE);
      return true;
    }
  }
  return false;
}

static bool a2dp_bdaddr_cmp(bt_bdaddr_t *bd_addr_1, bt_bdaddr_t *bd_addr_2) {
  if((NULL == bd_addr_1) || (NULL == bd_addr_2)) {
    return false;
  }
  return (memcmp(bd_addr_1->address,
      bd_addr_2->address, BTIF_BD_ADDR_SIZE) == 0);
}

bool a2dp_id_from_bdaddr(bt_bdaddr_t *bd_addr, uint8_t *id) {
  bt_bdaddr_t curr_addr = {0};
  uint8_t curr_id = BT_DEVICE_NUM;

  a2dp_bdaddr_from_id(BT_DEVICE_ID_1, &curr_addr);
  if(a2dp_bdaddr_cmp(&curr_addr, bd_addr)) {
    curr_id = BT_DEVICE_ID_1;
  }

#ifdef __BT_ONE_BRING_TWO__
  a2dp_bdaddr_from_id(BT_DEVICE_ID_2, &curr_addr);
  if(a2dp_bdaddr_cmp(&curr_addr, bd_addr)) {
    curr_id = BT_DEVICE_ID_2;
  }
#endif
  if(id) {
    *id = curr_id;
  }
  return (curr_id < BT_DEVICE_NUM);
}


int store_sbc_buffer(unsigned char *buf, unsigned int len);
int a2dp_audio_sbc_set_frame_info(int rcv_len, int frame_num);


#if defined(__BTIF_BT_RECONNECT__) && defined(__BT_A2DP_RECONNECT__)

struct BT_A2DP_RECONNECT_T bt_a2dp_reconnect;

extern "C" void cancel_a2dp_reconnect(void)
{
    if(bt_a2dp_reconnect.TimerNotifyFunc) {
        osTimerStop(bt_a2dp_reconnect.TimerID);
        osTimerDelete(bt_a2dp_reconnect.TimerID);
        bt_a2dp_reconnect.TimerNotifyFunc= 0;
    }
}

void a2dp_reconnect_timer_callback(void const *n) {
    if(bt_a2dp_reconnect.TimerNotifyFunc)
        bt_a2dp_reconnect.TimerNotifyFunc();
}
osTimerDef(a2dp_reconnect_timer, a2dp_reconnect_timer_callback);
#endif



void btapp_send_pause_key(enum BT_DEVICE_ID_T stream_id)
{
    TRACE("btapp_send_pause_key id = %x",stream_id);
    btif_avrcp_set_panel_key(app_bt_device.avrcp_channel[stream_id], BTIF_AVRCP_POP_PAUSE, TRUE);
    btif_avrcp_set_panel_key(app_bt_device.avrcp_channel[stream_id], BTIF_AVRCP_POP_PAUSE, FALSE);
}


void btapp_a2dp_suspend_music(enum BT_DEVICE_ID_T stream_id)
{
    TRACE("btapp_a2dp_suspend_music id = %x",stream_id);

    btapp_send_pause_key(stream_id);
}



extern enum AUD_SAMPRATE_T a2dp_sample_rate;

#define A2DP_TIMESTAMP_DEBOUNCE_DURATION (1000)
#define A2DP_TIMESTAMP_MODE_SAMPLE_THRESHOLD (2000)

#define A2DP_TIMESTAMP_SYNC_LIMIT_CNT (200)
#define A2DP_TIMESTAMP_SYNC_TIME_THRESHOLD (100)
#define A2DP_TIMESTAMP_SYNC_SAMPLE_THRESHOLD (a2dp_sample_rate/1000*A2DP_TIMESTAMP_SYNC_TIME_THRESHOLD)

#define RICE_THRESHOLD
#define RICE_THRESHOLD

struct A2DP_TIMESTAMP_INFO_T{
    uint16_t rtp_timestamp;
    uint32_t loc_timestamp;
    uint16_t frame_num;
};

enum A2DP_TIMESTAMP_MODE_T{
    A2DP_TIMESTAMP_MODE_NONE,
    A2DP_TIMESTAMP_MODE_SAMPLE,
    A2DP_TIMESTAMP_MODE_TIME,
};

enum A2DP_TIMESTAMP_MODE_T a2dp_timestamp_mode = A2DP_TIMESTAMP_MODE_NONE;

struct A2DP_TIMESTAMP_INFO_T a2dp_timestamp_pre = {0,0,0};
bool a2dp_timestamp_parser_need_sync = false;
int a2dp_timestamp_parser_init(void)
{
    a2dp_timestamp_mode = A2DP_TIMESTAMP_MODE_NONE;
    a2dp_timestamp_pre.rtp_timestamp = 0;
    a2dp_timestamp_pre.loc_timestamp = 0;
    a2dp_timestamp_pre.frame_num = 0;
    a2dp_timestamp_parser_need_sync = false;
    return 0;
}

int a2dp_timestamp_parser_needsync(void)
{
    a2dp_timestamp_parser_need_sync = true;
    return 0;
}

int a2dp_timestamp_parser_run(uint16_t timestamp, uint16_t framenum)
{
    static int skip_cnt = 0;
    struct A2DP_TIMESTAMP_INFO_T curr_timestamp;
    int skipframe = 0;
    int16_t rtpdiff;
    int16_t locdiff;
    bool needsave_timestamp = true;

    curr_timestamp.rtp_timestamp = timestamp;
    curr_timestamp.loc_timestamp = hal_sys_timer_get();
    curr_timestamp.frame_num = framenum;

    switch(a2dp_timestamp_mode) {
        case A2DP_TIMESTAMP_MODE_NONE:

            TRACE("parser rtp:%d loc:%d num:%d prertp:%d preloc:%d\n", curr_timestamp.rtp_timestamp, curr_timestamp.loc_timestamp, curr_timestamp.frame_num,
                    a2dp_timestamp_pre.rtp_timestamp, a2dp_timestamp_pre.loc_timestamp);
            if (a2dp_timestamp_pre.rtp_timestamp){
                locdiff = curr_timestamp.loc_timestamp - a2dp_timestamp_pre.loc_timestamp;
                if (TICKS_TO_MS(locdiff) > A2DP_TIMESTAMP_DEBOUNCE_DURATION){
                    rtpdiff = curr_timestamp.rtp_timestamp - a2dp_timestamp_pre.rtp_timestamp;
                    if (ABS((int16_t)TICKS_TO_MS(locdiff)-rtpdiff)>A2DP_TIMESTAMP_MODE_SAMPLE_THRESHOLD){
                        a2dp_timestamp_mode = A2DP_TIMESTAMP_MODE_SAMPLE;
                        TRACE("A2DP_TIMESTAMP_MODE_SAMPLE\n");
                    }else{
                        a2dp_timestamp_mode = A2DP_TIMESTAMP_MODE_TIME;
                        TRACE("A2DP_TIMESTAMP_MODE_TIME\n");
                    }
                }else{
                    needsave_timestamp = false;
                }
            }
            break;
        case A2DP_TIMESTAMP_MODE_SAMPLE:
            if (a2dp_timestamp_parser_need_sync){
                skip_cnt++;
                rtpdiff = curr_timestamp.rtp_timestamp - a2dp_timestamp_pre.rtp_timestamp;
                locdiff = curr_timestamp.loc_timestamp - a2dp_timestamp_pre.loc_timestamp;
                TRACE("A2DP_TIMESTAMP_MODE_TIME SYNC diff:%d cnt:%d\n", ABS((int16_t)(MS_TO_TICKS(locdiff)*(a2dp_sample_rate/1000)) - rtpdiff), skip_cnt);
                if ((uint32_t)(ABS((int16_t)(TICKS_TO_MS(locdiff)*(a2dp_sample_rate/1000)) - rtpdiff)) < (uint32_t)A2DP_TIMESTAMP_SYNC_SAMPLE_THRESHOLD){
                    skip_cnt = 0;
                    a2dp_timestamp_parser_need_sync = false;
                }else if (skip_cnt > A2DP_TIMESTAMP_SYNC_LIMIT_CNT){
                    skip_cnt = 0;
                    a2dp_timestamp_parser_need_sync = false;
                }else{
                    needsave_timestamp = false;
                    skipframe = 1;
                }
            }
            break;
        case A2DP_TIMESTAMP_MODE_TIME:
            if (a2dp_timestamp_parser_need_sync){
                skip_cnt++;
                rtpdiff = curr_timestamp.rtp_timestamp - a2dp_timestamp_pre.rtp_timestamp;
                locdiff = curr_timestamp.loc_timestamp - a2dp_timestamp_pre.loc_timestamp;
                TRACE("A2DP_TIMESTAMP_MODE_TIME SYNC diff:%d cnt:%d\n", ABS((int16_t)TICKS_TO_MS(locdiff) - rtpdiff), skip_cnt);
                if (ABS((int16_t)TICKS_TO_MS(locdiff) - rtpdiff) < A2DP_TIMESTAMP_SYNC_TIME_THRESHOLD){
                    skip_cnt = 0;
                    a2dp_timestamp_parser_need_sync = false;
                }else if (skip_cnt > A2DP_TIMESTAMP_SYNC_LIMIT_CNT){
                    skip_cnt = 0;
                    a2dp_timestamp_parser_need_sync = false;
                }else{
                    needsave_timestamp = false;
                    skipframe = 1;
                }
            }
            break;
    }

    if (needsave_timestamp){
        a2dp_timestamp_pre.rtp_timestamp = curr_timestamp.rtp_timestamp;
        a2dp_timestamp_pre.loc_timestamp = curr_timestamp.loc_timestamp;
    }
    return skipframe;
}

#ifdef __BT_ONE_BRING_TWO__
uint8_t a2dp_stream_id_distinguish(a2dp_stream_t *Stream, uint8_t event_type)
{
    uint8_t found_device_id = BT_DEVICE_NUM;
    if(Stream == app_bt_device.a2dp_stream[BT_DEVICE_ID_1]->a2dp_stream){
        found_device_id = BT_DEVICE_ID_1;
        stream_id_flag.id = BT_DEVICE_ID_1;
    }
    else if(Stream == app_bt_device.a2dp_connected_stream[BT_DEVICE_ID_2]->a2dp_stream)
    {
        found_device_id = BT_DEVICE_ID_2;
        stream_id_flag.id = BT_DEVICE_ID_2;
    }else/* if(event_type == A2DP_EVENT_STREAM_CLOSED)*/{
        btif_remote_device_t * remDev = 0;
        btif_remote_device_t * connected_remDev[BT_DEVICE_NUM];
        remDev = btif_a2dp_get_remote_device(Stream);
        connected_remDev[0] = btif_a2dp_get_remote_device(app_bt_device.a2dp_connected_stream[BT_DEVICE_ID_1]->a2dp_stream);
        connected_remDev[1] = btif_a2dp_get_remote_device(app_bt_device.a2dp_connected_stream[BT_DEVICE_ID_2]->a2dp_stream);
        if((connected_remDev[0] == remDev) && (remDev!= 0)){
            stream_id_flag.id = BT_DEVICE_ID_1;
            found_device_id = BT_DEVICE_ID_1;
        }
        else if((connected_remDev[1] == remDev) && (remDev !=0)){
            stream_id_flag.id = BT_DEVICE_ID_2;
            found_device_id = BT_DEVICE_ID_2;
        }
    }
    return found_device_id;
}

/*
uint8_t POSSIBLY_UNUSED a2dp_get_streaming_id(void)
{
    uint8_t nRet = 0;
    if(app_bt_device.a2dp_connected_stream[BT_DEVICE_ID_1]->stream.state == AVDTP_STRM_STATE_STREAMING)
        nRet |= 1<<0;
    if(app_bt_device.a2dp_connected_stream[BT_DEVICE_ID_2]->stream.state == AVDTP_STRM_STATE_STREAMING)
        nRet |= 1<<1;
    return nRet;
}
*/

static uint8_t a2dp_stream_locate_the_connected_dev_id(a2dp_stream_t *Stream)
{
    for (uint8_t index = 0;index < BT_DEVICE_NUM;index++)
    {
        if (&(app_bt_device.a2dp_stream[index]) == Stream)
        {
            TRACE("Get a2dp stream index %d", index);
            return index;
        }

    }

    return 0;
}

bool a2dp_stream_push_connected_stream(a2dp_stream_t *Stream)
{
    bool ret = 0;
    btif_remote_device_t *connectedremDev[2];
    btif_remote_device_t *streamremDev;
    streamremDev = btif_a2dp_get_remote_device(Stream);
    uint8_t reserved_device = BT_DEVICE_NUM;
    if(app_bt_device.a2dp_connected_stream[BT_DEVICE_ID_1]->a2dp_stream == Stream ||
            app_bt_device.a2dp_connected_stream[BT_DEVICE_ID_2]->a2dp_stream == Stream){
        TRACE("a2dp_stream_push_connected_stream Stream in record st=0x%x\n", Stream);
        return ret;
    }

    if ((NULL == app_bt_device.a2dp_connected_stream[BT_DEVICE_ID_1]->a2dp_stream) ||
            (NULL == app_bt_device.a2dp_connected_stream[BT_DEVICE_ID_2]->a2dp_stream))
    {
        reserved_device = a2dp_stream_locate_the_connected_dev_id(Stream);
        app_bt_device.a2dp_connected_stream[reserved_device]->a2dp_stream = Stream;
        ret = TRUE;
    }

    TRACE("a2dp_stream_push_connected_stream ret = %d 0x%x st=0x%x 0x%x\n", ret, Stream, app_bt_device.a2dp_connected_stream[0],
            app_bt_device.a2dp_connected_stream[1]);
    connectedremDev[0] = btif_a2dp_get_remote_device(app_bt_device.a2dp_connected_stream[BT_DEVICE_ID_1]->a2dp_stream);
    connectedremDev[1] = btif_a2dp_get_remote_device(app_bt_device.a2dp_connected_stream[BT_DEVICE_ID_2]->a2dp_stream);
    TRACE("streamRemdev = 0x%x connectedRemDev[0]/[1] = 0x%x / 0x%x",streamremDev,connectedremDev[0],connectedremDev[1]);
    if(connectedremDev[0] == connectedremDev[1])
    {
        if(reserved_device == BT_DEVICE_ID_1){
            app_bt_device.a2dp_connected_stream[BT_DEVICE_ID_1]->a2dp_stream = Stream;
            app_bt_device.a2dp_connected_stream[BT_DEVICE_ID_2]->a2dp_stream = NULL;
        }else if(reserved_device == BT_DEVICE_ID_2){
            app_bt_device.a2dp_connected_stream[BT_DEVICE_ID_2]->a2dp_stream = Stream;
            app_bt_device.a2dp_connected_stream[BT_DEVICE_ID_1]->a2dp_stream = NULL;
        }
    }
    return ret;
}

#endif


#if defined( __BTIF_EARPHONE__) && defined(__BTIF_BT_RECONNECT__)

#ifdef __BT_ONE_BRING_TWO__
extern btif_device_record_t record2_copy;
extern uint8_t record2_avalible;
#endif

#endif

#if 0 //new api

#if defined(__TWS__)
static AvdtpCodec   setconfig_codec;
#endif

           // config_codec->elements[7] |= A2DP_LHDC_LLC_ENABLE;
#endif
        ///////null set situation:


uint8_t enum_sample_rate_transfer_to_codec_info(enum AUD_SAMPRATE_T sample_rate)
{
    uint8_t codec_info;
    TRACE("!!!enum_sample_rate_transfer_to_codec_info  sample_rate :%d \n",sample_rate);
    switch (sample_rate)
    {
        case AUD_SAMPRATE_16000:
            codec_info = A2D_SBC_IE_SAMP_FREQ_16;
            break;
        case AUD_SAMPRATE_32000:
            codec_info = A2D_SBC_IE_SAMP_FREQ_32;
            break;
        case AUD_SAMPRATE_48000:
            codec_info = A2D_SBC_IE_SAMP_FREQ_48;
            break;
        case AUD_SAMPRATE_44100:
            codec_info = A2D_SBC_IE_SAMP_FREQ_44;
            break;
#if defined(A2DP_LHDC_ON)
        case AUD_SAMPRATE_96000:
            codec_info = A2D_SBC_IE_SAMP_FREQ_96;
            break;
#endif

        default:
            ASSERT(0, "[%s] 0x%x is invalid", __func__, sample_rate);
            break;
    }
    TRACE("!!!enum_sample_rate_transfer_to_codec_info  codec_info :%x\n",codec_info);
    return codec_info;
}
#if defined(__TWS__)
BOOL app_bt_bypass_hcireceiveddata_cb(void);
typedef  uint32_t (*ext_fn) (void);

void app_a2dp_buff_remaining_monitor(void)
{
    if (tws_if_get_a2dpbuff_available() < 1024){
        //        TRACE("!!!suspend HciBypassProcessReceivedData");
        btif_HciBypassProcessReceivedDataExt((ext_fn)app_bt_bypass_hcireceiveddata_cb);
    }
}
#endif

btif_avdtp_codec_type_t a2dp_get_codec_type(enum BT_DEVICE_ID_T id)
{
    return app_bt_device.codec_type[id];
}

void a2dp_set_codec_type(enum BT_DEVICE_ID_T id, U8 codecType)
{
    app_bt_device.codec_type[id] = codecType;
}

void a2dp_set_sample_bit(enum BT_DEVICE_ID_T id, U8 samplebit)
{
    app_bt_device.sample_bit[id] = samplebit;
}

btif_avdtp_codec_type_t a2dp_get_sample_rate(enum BT_DEVICE_ID_T id)
{
    return app_bt_device.sample_rate[id];
}

void a2dp_update_sample_rate(uint8_t sampleRate)
{
    app_bt_device.sample_rate[BT_DEVICE_ID_1] = sampleRate;
}

#if defined(A2DP_LHDC_ON)
void a2dp_lhdc_config(uint8_t * elements){
    lhdc_info_t info;
    memset(&info,0,sizeof(lhdc_info_t));
    lhdc_info_parse(elements, &info );
    app_bt_device.sample_rate[stream_id_flag.id] = info.sample_rater;
    app_bt_device.sample_bit[stream_id_flag.id] = info.bits;


}
#endif

extern BTIF_TWS_SBC_FRAME_INFO sbc_frame_info;
extern btif_avdtp_media_header_t  media_header;

#ifdef __A2DP_CODEC_ERROR_RESUME__
uint8_t sample_rate_update;
#endif

uint8_t salve_mute_flag = 0;

void a2dp_mobile_audio_data_ind(uint8_t *data_buf,uint32_t len)
{
   // tws_dev_t *twsd = app_tws_get_env_pointer();
    btif_a2dp_callback_parms_t  Info;
    Info.event = BTIF_A2DP_EVENT_STREAM_DATA_IND;
    Info.p.data = data_buf;
    Info.len = len;
    if (!a2dp_sink_of_mobile_dev_callback( tws_get_tws_mobile_sink_stream(), (a2dp_callback_parms_t*)&Info))
    {
        a2dp_sink_common_data_ind_handler(tws_get_tws_mobile_sink_stream(), (a2dp_callback_parms_t*)&Info);
    }
}

void a2dp_sink_common_data_ind_handler(a2dp_stream_t *Stream, const a2dp_callback_parms_t *info)
{
    static uint32_t last_seq = 0;
    int header_len = 0;
    btif_avdtp_media_header_t header;
    int skip_frame=0;
    unsigned int sbc_frame_num = 0;
    btif_a2dp_callback_parms_t *Info = ( btif_a2dp_callback_parms_t *)info;
#ifdef __BT_ONE_BRING_TWO__
    if (app_bt_device.curr_a2dp_stream_id  != stream_id_flag.id) {
       app_bt_device.hf_audio_state[stream_id_flag.id]==HF_AUDIO_DISCON &&
       app_bt_device.hf_audio_state[stream_id_flag.id_other]==HF_AUDIO_DISCON &&
       app_bt_device.hfchan_callSetup[stream_id_flag.id_other] == HF_CALL_SETUP_NONE){

        header_len = AVDTP_ParseMediaHeader(&header, Info->p.data);

        if (app_bt_stream_isrun(APP_BT_STREAM_A2DP_SBC) && (Stream->stream.state == AVDTP_STRM_STATE_STREAMING)){
    }
    if (app_bt_device.hf_audio_state[stream_id_flag.id] != HF_AUDIO_DISCON ||
            app_bt_device.hf_audio_state[stream_id_flag.id_other] != HF_AUDIO_DISCON ||
            app_bt_device.hfchan_callSetup[stream_id_flag.id_other] != HF_CALL_SETUP_NONE){
        TRACE("Skip frame, hfp on");
        return;
    }
#endif

    header_len =btif_avdtp_parse_mediaHeader(&header, Info);

#ifdef __TWS__
    if(!is_tws_slave_conn_master_state())
        skip_frame = a2dp_timestamp_parser_run(header.timestamp,(*(((unsigned char *)Info->p.data) + header_len)));
#else
    skip_frame = a2dp_timestamp_parser_run(header.timestamp,(*(((unsigned char *)Info->p.data) + header_len)));
#endif

#ifdef __TWS__
    if(0)//(skip_frame && (app_tws_get_conn_state() != TWS_SLAVE_CONN_MASTER))
    {
        TRACE("skip sbc packet!!");
        return;
    }
#else
    if(skip_frame)
    {
        TRACE("skip sbc packet!!");
        return;
    }
#endif
    if(is_tws_slave_conn_master_state()){
#if defined(A2DP_LHDC_ON)
        if((a2dp_get_codec_type(stream_id_flag.id) == BTIF_AVDTP_CODEC_TYPE_LHDC)
            ||(app_tws_get_codec_type() == BTIF_AVDTP_CODEC_TYPE_LHDC)){
            //            TRACE("lhdc time=%d,seq=%d \n",header.timestamp,header.sequenceNumber);
        }else
#endif
#if defined(A2DP_AAC_ON)
        if(a2dp_get_codec_type(stream_id_flag.id) == BTIF_AVDTP_CODEC_TYPE_MPEG2_4_AAC){
                //TRACE("aac time=%d,seq=%d \n",header.timestamp,header.sequenceNumber);
            }else
#endif
            {
                //TRACE("time=%d,seq=%d,framnumber=%d,curr t=%d",header.timestamp,header.sequenceNumber,*(((unsigned char *)Info->p.data) + header_len),bt_syn_get_curr_ticks(app_tws_get_tws_conhdl()));
            }
    }
    if (header.sequenceNumber != last_seq + 1) {
        TRACE("media packet last_seq=%d,curr_seq=%d\n", last_seq,header.sequenceNumber);
    }
    last_seq = header.sequenceNumber;

#ifdef __TWS__
    sbc_frame_num = (*(((unsigned char *)Info->p.data) + header_len));
#ifdef __A2DP_CODEC_ERROR_RESUME__
    if(is_tws_master_conn_slave_state() == IF_TWS_MASTER_CONN_SLAVE ||
       is_tws_master_conn_mobile_only_state() == IF_TWS_MASTER_CONN_MOBILE_ONLY)
    {
#if defined(A2DP_LHDC_ON)
        if(a2dp_get_codec_type(stream_id_flag.id) == BTIF_AVDTP_CODEC_TYPE_LHDC && tws_if_get_mobile_codec_type() == BTIF_AVDTP_CODEC_TYPE_MPEG2_4_AAC)
        {

        }
#endif
#if defined(A2DP_AAC_ON)
        if(a2dp_get_codec_type(stream_id_flag.id) == BTIF_AVDTP_CODEC_TYPE_MPEG2_4_AAC && tws_if_get_mobile_codec_type() == BTIF_AVDTP_CODEC_TYPE_MPEG2_4_AAC)
        {
            if(*(uint8_t *)(((unsigned char *)Info->p.data) + header_len)==0x9c)
            {
                TRACE("ERROR !!SBC IN AAC STREAM~~~");
               // tws.mobile_codectype = AVDTP_CODEC_TYPE_SBC;
        tws_if_set_mobile_codec_type(BTIF_AVDTP_CODEC_TYPE_SBC);
                tws_player_set_codec(BTIF_AVDTP_CODEC_TYPE_SBC);
                break;
            }
        }
#endif
    }
#endif

#if defined(A2DP_LHDC_ON)
    if ((a2dp_get_codec_type(stream_id_flag.id) == BTIF_AVDTP_CODEC_TYPE_LHDC)
        ||( tws_if_get_mobile_codec_type() == BTIF_AVDTP_CODEC_TYPE_LHDC)) {
        //TRACE("store lhdc data");
        //        TRACE("lhdc store data len=%d  head len=%d",header_len,Info->len);
        tws_store_sbc_buffer(((unsigned char *)Info->p.data) + header_len , Info->len - header_len,sbc_frame_num);
    }
    else
#endif

#if defined(A2DP_AAC_ON)
    if(a2dp_get_codec_type(stream_id_flag.id) == BTIF_AVDTP_CODEC_TYPE_MPEG2_4_AAC
    || ( is_tws_slave_conn_master_state() && (tws_if_get_mobile_codec_type() == BTIF_AVDTP_CODEC_TYPE_MPEG2_4_AAC)))
    {
            tws_store_sbc_buffer(((unsigned char *)Info->p.data) + header_len , Info->len - header_len,  1);
        }
        else
#endif
        {
        if (is_tws_slave_conn_master_state())
            {
                if (Info->len - header_len == 121)
                {
                    //              salve_mute_flag = 1;
                    salve_mute_flag = 0;
                }
                else
                    salve_mute_flag = 0;
            }
            tws_store_sbc_buffer(((unsigned char *)Info->p.data) + header_len + 1 , Info->len - header_len - 1, sbc_frame_num);
        }
    app_a2dp_buff_remaining_monitor();
#else
    if (app_bt_stream_isrun(APP_BT_STREAM_A2DP_SBC) && (btif_a2dp_get_stream_state(Stream) == BTIF_AVDTP_STRM_STATE_STREAMING)){
        a2dp_audio_sbc_set_frame_info(Info->len - header_len - 1, (*(((unsigned char *)Info->p.data) + header_len)));
#if defined(A2DP_AAC_ON)
        if(a2dp_get_codec_type(stream_id_flag.id) == BTIF_AVDTP_CODEC_TYPE_MPEG2_4_AAC)
            store_sbc_buffer(((unsigned char *)Info->p.data) + header_len , Info->len - header_len);
        else
#endif
        {
            store_sbc_buffer(((unsigned char *)Info->p.data) + header_len + 1 , Info->len - header_len - 1);
        }
    }
#endif
}

void a2dp_restore_sample_rate_after_role_switch(a2dp_stream_t *Stream, const a2dp_callback_parms_t *info)
{
    uint8_t previousA2dpSampleRate = app_tws_get_previous_sample_rate();
    btif_a2dp_callback_parms_t * Info = (btif_a2dp_callback_parms_t *)info;
    // update the a2dp sample rate

    btif_a2dp_get_stream_codecCfg(Stream)->elements[0] &= ~A2D_SBC_IE_SAMP_FREQ_MSK;
    btif_a2dp_get_stream_codecCfg(Stream)->elements[0] |= previousA2dpSampleRate;

    Info->p.codec->elements[0] &= ~A2D_SBC_IE_SAMP_FREQ_MSK;
    Info->p.codec->elements[0] |= previousA2dpSampleRate;
    if(tws.mobile_samplebit == 24)
    {
        btif_a2dp_get_stream_codecCfg(Stream)->elements[1] = A2D_SBC_IE_BIT_NUM_24;
        Info->p.codec->elements[1] = A2D_SBC_IE_BIT_NUM_24;
    }
    else
    {
        btif_a2dp_get_stream_codecCfg(Stream)->elements[1] = A2D_SBC_IE_BIT_NUM_16;
        Info->p.codec->elements[1] = A2D_SBC_IE_BIT_NUM_16;
    }

    a2dp_update_sample_rate(previousA2dpSampleRate);

    TRACE("prev sample rate 0x%x,mobile_samplebit %d", previousA2dpSampleRate,tws.mobile_samplebit);

}

typedef struct _BitStreamInfo {
    unsigned char *bytePtr;
    unsigned int iCache;
    int cachedBits;
    int nBytes;
} BitStreamInfo;


static void RefillBitstreamCache(BitStreamInfo *bsi)
{
    int nBytes = bsi->nBytes;

    /* optimize for common case, independent of machine endian-ness */
    if (nBytes >= 4) {
        bsi->iCache  = (*bsi->bytePtr++) << 24;
        bsi->iCache |= (*bsi->bytePtr++) << 16;
        bsi->iCache |= (*bsi->bytePtr++) <<  8;
        bsi->iCache |= (*bsi->bytePtr++);
        bsi->cachedBits = 32;
        bsi->nBytes -= 4;
    } else {
        bsi->iCache = 0;
        if(nBytes<0 || *bsi->bytePtr == *(int *)0)
        {
            return;
        }
        while (nBytes--) {
            bsi->iCache |= (*bsi->bytePtr++);
            bsi->iCache <<= 8;
        }
        bsi->iCache <<= ((3 - bsi->nBytes)*8);
        bsi->cachedBits = 8*bsi->nBytes;
        bsi->nBytes = 0;
    }
}

static unsigned int GetBits(BitStreamInfo *bsi, int nBits)
{
    unsigned int data, lowBits;

    nBits &= 0x1f;                          /* nBits mod 32 to avoid unpredictable results like >> by negative amount */
    data = bsi->iCache >> (31 - nBits);     /* unsigned >> so zero-extend */
    data >>= 1;                             /* do as >> 31, >> 1 so that nBits = 0 works okay (returns 0) */
    bsi->iCache <<= nBits;                  /* left-justify cache */
    bsi->cachedBits -= nBits;               /* how many bits have we drawn from the cache so far */

    /* if we cross an int boundary, refill the cache */
    if (bsi->cachedBits < 0) {
        lowBits = -bsi->cachedBits;
        //            if(bsi->bytePtr ==0)
        //            ASSERT(bsi->bytePtr !=0,"GETBITS PTR=%x,nbits=%x,cachebits=%x",bsi->bytePtr,nBits,bsi->cachedBits);
        //           ASSERT(bsi->bytePtr !=0x20054000,"GETBITS PTR=%x,nbits=%x,cachebits=%x,bsi=%x",bsi->bytePtr,nBits,bsi->cachedBits,bsi);
        RefillBitstreamCache(bsi);
        data |= bsi->iCache >> (32 - lowBits);      /* get the low-order bits */

        bsi->cachedBits -= lowBits;         /* how many bits have we drawn from the cache so far */
        bsi->iCache <<= lowBits;            /* left-justify cache */
    }

    return data;
}

#if 0
static unsigned int LatmGetValue(BitStreamInfo* bsi)
{
    unsigned char  bytesForValue, valueTmp = 0;
    unsigned int value = 0; /* helper variable 32bit */

    bytesForValue = GetBits(bsi, 2);
    for ( unsigned int i = 0; i <= bytesForValue; i++ ) {
        value <<= 8;
        valueTmp = GetBits(bsi, 8);
        value += valueTmp;
    }
    return value;
}
#endif

static void SetBitstreamPointer(BitStreamInfo *bsi, int nBytes, unsigned char *buf)
{
    /* init bitstream */
    bsi->bytePtr = buf;
    bsi->iCache = 0;        /* 4-byte unsigned int */
    bsi->cachedBits = 0;    /* i.e. zero bits in cache */
    bsi->nBytes = nBytes;
}

static int StreamMuxConfigSlim(BitStreamInfo* bsi)
{
    unsigned char audio_mux_version;
    unsigned char audio_mux_version_A;
    //int taraFullness;
    //unsigned char allStreamSameTimeFraming;
    //short numSubFrames;
    short numPrograms;

    audio_mux_version = GetBits(bsi, 1);
    //muxConfigPresent is 1 for default
    if (audio_mux_version){
        audio_mux_version_A = GetBits(bsi, 1);
    }else{
        audio_mux_version_A = 0;
    }

    if(audio_mux_version_A == 0){
        if(audio_mux_version == 1){
            //taraFullness = LatmGetValue(bsi);
        }

        //allStreamSameTimeFraming = GetBits(bsi, 1);           // allStreamSameTimeFraming
        //numSubFrames             = GetBits(bsi, 6);       // numSubFrames
        numPrograms = GetBits(bsi, 4);                                 // numPrograms

        if (numPrograms){
            return -2;
        }
    }else{
        return -3;
    }

    return 0;
}

static int UnpackLOASHeaderSlim(unsigned char *buf, unsigned char len)
{
    BitStreamInfo bsi;
    unsigned char use_same_mux;
    SetBitstreamPointer(&bsi, len, buf);
    if(bsi.bytePtr ==0 || bsi.bytePtr > (unsigned char *)(buf+len)  || bsi.bytePtr == *(unsigned char **)0)
    {
        return -1;
    }
    if(bsi.nBytes <0)
    {
        return -1;
    }
    use_same_mux = GetBits(&bsi, 1);
    if (!use_same_mux){
        return StreamMuxConfigSlim(&bsi);
    }
    return 0;
}

void a2dp_sink_stream_data_checkhandler(a2dp_stream_t *Stream, const a2dp_callback_parms_t *info)
{
    btif_avdtp_media_header_t header;
    int header_len = 0;
    uint8_t sbc_sync_word;
    bool stream_data_valid = false;
    btif_a2dp_callback_parms_t * Info = (btif_a2dp_callback_parms_t*)info;

   if (Info->event == BTIF_A2DP_EVENT_STREAM_DATA_IND){
        header_len = btif_avdtp_parse_mediaHeader(&header, Info);

        sbc_sync_word = *(uint8_t *)(((unsigned char *)Info->p.data) + header_len+1);

        /*
           TRACE("a2dp ind:%d codec:%d/%d sync:%02x len:%d %02x/%02x",
           Info->len,
           app_bt_device.codec_type[stream_id_flag.id],
           app_tws_get_env_pointer()->mobile_codectype,
           sbc_sync_word,header_len,
         *(uint8_t *)(((unsigned char *)Info->p.data) + header_len),
         *(uint8_t *)(((unsigned char *)Info->p.data) + header_len+1));
         */
#if defined(A2DP_LHDC_ON)
        if ((tws_if_get_mobile_codec_type() == BTIF_AVDTP_CODEC_TYPE_LHDC) &&
                (a2dp_get_codec_type(stream_id_flag.id) == BTIF_AVDTP_CODEC_TYPE_LHDC)){
            stream_data_valid = true;
        }else
#endif
#if defined(A2DP_AAC_ON)
        if ((tws_if_get_mobile_codec_type() == BTIF_AVDTP_CODEC_TYPE_MPEG2_4_AAC) &&
#if !defined(SBC_TRANS_TO_AAC)
                   (a2dp_get_codec_type(stream_id_flag.id) == BTIF_AVDTP_CODEC_TYPE_MPEG2_4_AAC) &&
#endif
                   (sbc_sync_word != 0x9C)){
                stream_data_valid = true;
            }else
#endif
        if ((tws_if_get_mobile_codec_type() == BTIF_AVDTP_CODEC_TYPE_SBC) &&
             (a2dp_get_codec_type(stream_id_flag.id) == BTIF_AVDTP_CODEC_TYPE_SBC) &&
                        (sbc_sync_word == 0x9C)){
                    stream_data_valid = true;
                }

        if (!stream_data_valid){
            TRACE("stream_data invalid need error resume %d/%d",  tws_if_get_mobile_codec_type(), a2dp_get_codec_type(stream_id_flag.id));
        }
    }
}


void a2dp_sink_callback_common_handler(a2dp_stream_t*  Stream, const a2dp_callback_parms_t *info)
{
    //TRACE("Enter %s event is %d", __FUNCTION__, Info->event);
    btif_avdtp_codec_t  * setconfig_codec = btif_a2dp_get_avdtp_setconfig_codec( Stream);
    static u8 tmp_element[10];

#if defined(__TWS__)
    a2dp_stream_t * tws_source_stream = NULL;
    btif_avdtp_codec_t  * tws_set_codec;
    enum AUD_SAMPRATE_T a2dp_sample_rate;
    bt_status_t  status;

#else
//#if defined(__BTIF_BT_RECONNECT__)
//    btif_avdtp_codec_t  * setconfig_codec = btif_a2dp_get_avdtp_setconfig_codec( Stream);
 //   static u8 tmp_element[10];
//#endif
#endif
btif_a2dp_callback_parms_t * Info = (btif_a2dp_callback_parms_t*)info;

btif_a2dp_event_t event = btif_a2dp_get_cb_event(Info);
    // get the mobile device id
#ifdef __BT_ONE_BRING_TWO__
    if((Info->event== BTIF_A2DP_EVENT_STREAM_OPEN)
            || (Info->event== BTIF_A2DP_EVENT_STREAM_OPEN_IND)){
        a2dp_stream_push_connected_stream(Stream);
    }
    a2dp_stream_id_distinguish(Stream, Info->event);
#else
    stream_id_flag.id = BT_DEVICE_ID_1;
#endif

    switch(event) {
        case BTIF_A2DP_EVENT_AVDTP_CONNECT:
            A2DP_TRACE("::A2DP_EVENT_AVDTP_CONNECT %d\n", event);
            break;
        case BTIF_A2DP_EVENT_STREAM_OPEN:
            A2DP_TRACE("::A2DP_EVENT_STREAM_OPEN stream=%x,stream_id:%d, sample_rate codec.elements 0x%x\n", Stream,stream_id_flag.id,Info->p.configReq->codec.elements[0]);
#if defined(A2DP_LHDC_ON)
            if (Info->p.configReq->codec.codecType == BTIF_AVDTP_CODEC_TYPE_LHDC) {
                TRACE("##codecType: LHDC Codec, Element length = %d, AVDTP_MAX_CODEC_ELEM_SIZE = %d\n", Info->p.configReq->codec.elemLen, BTIF_AVDTP_MAX_CODEC_ELEM_SIZE);
                app_bt_device.codec_type[stream_id_flag.id] = BTIF_AVDTP_CODEC_TYPE_LHDC;
                a2dp_lhdc_config(&(Info->p.configReq->codec.elements[0]));
            }else
#endif
#if defined(A2DP_AAC_ON)
            if(Info->p.configReq->codec.codecType == BTIF_AVDTP_CODEC_TYPE_MPEG2_4_AAC)
                {
                    TRACE("::A2DP_EVENT_STREAM_OPEN stream_id:%d, aac sample_rate codec.elements 0x%x\n", stream_id_flag.id,Info->p.configReq->codec.elements[1]);
                app_bt_device.codec_type[stream_id_flag.id] = BTIF_AVDTP_CODEC_TYPE_MPEG2_4_AAC;
                    app_bt_device.sample_bit[stream_id_flag.id] = 16;
                    // convert aac sample_rate to sbc sample_rate format
                if (Info->p.configReq->codec.elements[1] & BTIF_A2DP_AAC_OCTET1_SAMPLING_FREQUENCY_44100) {
                        TRACE("::A2DP_EVENT_STREAM_OPEN stream_id:%d, aac sample_rate 44100\n", stream_id_flag.id);
                        app_bt_device.sample_rate[stream_id_flag.id] = A2D_SBC_IE_SAMP_FREQ_44;
                    }
                else if (Info->p.configReq->codec.elements[2] & BTIF_A2DP_AAC_OCTET2_SAMPLING_FREQUENCY_48000) {
                        TRACE("::A2DP_EVENT_STREAM_OPEN stream_id:%d, aac sample_rate 48000\n", stream_id_flag.id);
                        app_bt_device.sample_rate[stream_id_flag.id] = A2D_SBC_IE_SAMP_FREQ_48;
                    }
                    else {
                        TRACE("::A2DP_EVENT_STREAM_OPEN stream_id:%d, aac sample_rate not 48000 or 44100, set to 44100\n", stream_id_flag.id);
                        app_bt_device.sample_rate[stream_id_flag.id] = A2D_SBC_IE_SAMP_FREQ_44;
                    }
                }else
#endif
                {
                    app_bt_device.codec_type[stream_id_flag.id] = BTIF_AVDTP_CODEC_TYPE_SBC;
                    if(Info->p.codec->elements[1] == A2D_SBC_IE_BIT_NUM_24){
                        app_bt_device.sample_bit[stream_id_flag.id] = 24;
                    }else{
                        app_bt_device.sample_bit[stream_id_flag.id] = 16;
                    }
                    memcpy(app_bt_device.a2dp_codec_elements[stream_id_flag.id],Info->p.configReq->codec.elements,4);
                }
#ifdef __TWS__
        //    if(app_tws_get_conn_state() == TWS_SLAVE_CONN_MASTER || app_tws_get_conn_state() == TWS_MASTER_CONN_SLAVE)
        if(  is_tws_slave_conn_master_state() ||is_tws_master_conn_slave_state())
            {
                TRACE("STREAM OPEN TWSSLAVE OR MASTER\n");
                app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, BTIF_BAM_NOT_ACCESSIBLE,0,0);
            }

            tws_source_stream = tws_get_tws_source_stream();
            tws_set_codec = tws_get_set_config_codec_info();
            TRACE("a2dp_callback moblie codec info = %x,%x,%x,%x",Info->p.codec->elements[0],Info->p.codec->elements[1],
                    Info->p.codec->elements[2],Info->p.codec->elements[3]);
            TRACE("a2dp_callback stream codec info = %x,%x,%x,%x",btif_a2dp_get_stream_codec(Stream)->elements[0],btif_a2dp_get_stream_codec(Stream)->elements[1],
                btif_a2dp_get_stream_codec(Stream)->elements[2],btif_a2dp_get_stream_codec(Stream)->elements[3]);

            TRACE("source stream conn %d codec 0x%x sample rate 0x%x tws rate 0x%x", \
                    app_tws_get_source_stream_connect_status(), \
                    app_bt_device.codec_type[stream_id_flag.id],    \
                    app_bt_device.sample_rate[stream_id_flag.id],\
               btif_a2dp_get_stream_codecCfg( tws_source_stream)->elements[0] &= A2D_SBC_IE_SAMP_FREQ_MSK);

            if(tws_get_source_stream_connect_status()){
                TRACE("!!!Info->p.codec->elements[0]:%x,tws_set_codec.elements[0]:%x\n",Info->p.codec->elements[0],tws_set_codec->elements[0]);
                TRACE("!!!source codec element[0]:%x\n", btif_a2dp_get_stream_codecCfg( tws_source_stream)->elements[0]) ;
#if defined(A2DP_LHDC_ON)
                if(app_bt_device.codec_type[stream_id_flag.id] == BTIF_AVDTP_CODEC_TYPE_LHDC){
                    uint8_t tws_source_sample_rate = btif_a2dp_get_stream_codecCfg( tws_source_stream)->elements[0] &= A2D_SBC_IE_SAMP_FREQ_MSK;
                    if(app_bt_device.sample_rate[stream_id_flag.id] != tws_source_sample_rate){
                       btif_a2dp_get_stream_codecCfg( tws_source_stream)->elements[0] &= ~A2D_SBC_IE_SAMP_FREQ_MSK;
                       btif_a2dp_get_stream_codecCfg( tws_source_stream)->elements[0] |= app_bt_device.sample_rate[stream_id_flag.id];
                        DUMP8("%02x ",&btif_a2dp_get_stream_codecCfg( tws_source_stream)->elements[0],4);
                    }
                    if(app_bt_device.sample_bit[stream_id_flag.id] == 24){
                       btif_a2dp_get_stream_codecCfg( tws_source_stream)->elements[1]=A2D_SBC_IE_BIT_NUM_24;
                    }else if(app_bt_device.sample_bit[stream_id_flag.id] == 16){
                        btif_a2dp_get_stream_codecCfg( tws_source_stream)->elements[1]=A2D_SBC_IE_BIT_NUM_16;
                    }
                    //A2DP_ReconfigStream(tws_source_stream, btif_a2dp_get_stream_codecCfg( tws_source_stream),NULL);
           btif_a2dp_reconfig_stream(tws_source_stream, btif_a2dp_get_stream_codecCfg( tws_source_stream),NULL);
                }else
#endif
#if defined(A2DP_AAC_ON)
                if(app_bt_device.codec_type[stream_id_flag.id] == BTIF_AVDTP_CODEC_TYPE_MPEG2_4_AAC){
                    uint8_t tws_source_sample_rate =  btif_a2dp_get_stream_codecCfg( tws_source_stream)->elements[0] &= A2D_SBC_IE_SAMP_FREQ_MSK;

                        if(app_bt_device.sample_rate[stream_id_flag.id] != tws_source_sample_rate){
                         btif_a2dp_get_stream_codecCfg( tws_source_stream)->elements[0] &= ~A2D_SBC_IE_SAMP_FREQ_MSK;
                         btif_a2dp_get_stream_codecCfg( tws_source_stream)->elements[0] |= app_bt_device.sample_rate[stream_id_flag.id];
                         btif_a2dp_get_stream_codecCfg( tws_source_stream)->elements[1] = A2D_SBC_IE_BIT_NUM_16;
            btif_a2dp_reconfig_stream(tws_source_stream, btif_a2dp_get_stream_codecCfg( tws_source_stream),NULL);

                        }
                    }else
#endif
                    {
                     if((( btif_a2dp_get_stream_codecCfg( tws_source_stream)->elements[0]>>4)&0xf) != ((Info->p.codec->elements[0]>>4)&0xf)){
                            TRACE("!!!A2DP_EVENT_STREAM_OPEN  A2DP_ReconfigStream\n");
                       // A2DP_ReconfigStream(tws_source_stream, Info->p.codec,NULL);
            btif_a2dp_reconfig_stream(tws_source_stream,  Info->p.codec,NULL);
                        }
                    }
            }

            if (!tws_is_role_switching_on())
            {
                if (is_tws_master_conn_mobile_only_state() || (is_tws_master_conn_slave_state()))
                {
                    TRACE("AVRCP WAIT MOBILE CONNECT");
                    osTimerStart(app_avrcp_connect_timer, 5000);
                }
            }
#endif
            app_bt_stream_volume_ptr_update((uint8_t *)btif_a2dp_stream_conn_remDev_bdAddr(Stream)->address);

#ifdef __TWS__
            if(is_tws_master_conn_slave_state())
            {
                //avrcp_set_slave_volume(0,a2dp_volume_get_tws());
            }
#endif

            //            app_bt_stream_a2dpvolume_reset();
            if(Info->p.configReq->codec.codecType == BTIF_AVDTP_CODEC_TYPE_SBC)
            {
                app_bt_device.sample_rate[stream_id_flag.id] = (Info->p.configReq->codec.elements[0] & A2D_SBC_IE_SAMP_FREQ_MSK);
            }
            app_bt_device.a2dp_state[stream_id_flag.id] = 1;
#ifdef __BT_ONE_BRING_TWO__
            if(  btif_a2dp_get_stream_state(app_bt_device.a2dp_stream[stream_id_flag.id_other]->a2dp_stream) != BTIF_AVDTP_STRM_STATE_STREAMING){
                app_bt_device.curr_a2dp_stream_id = stream_id_flag.id;
            }
#endif

#ifndef __TWS__
            btif_avrcp_connect(app_bt_device.avrcp_channel[stream_id_flag.id],  btif_a2dp_stream_conn_remDev_bdAddr(Stream));

#endif
            a2dp_service_connected_set(true);
            break;
        case BTIF_A2DP_EVENT_STREAM_OPEN_IND:
            TRACE("::A2DP_EVENT_STREAM_OPEN_IND %d\n", Info->event);
            //A2DP_OpenStreamRsp(Stream, A2DP_ERR_NO_ERROR, AVDTP_SRV_CAT_MEDIA_TRANSPORT);
            btif_a2dp_open_stream_rsp(Stream, BTIF_A2DP_ERR_NO_ERROR, BTIF_AVDTP_SRV_CAT_MEDIA_TRANSPORT);
            break;
        case BTIF_A2DP_EVENT_STREAM_STARTED:
            TRACE("::A2DP_EVENT_STREAM_STARTED  stream_id:%d error %d\n", stream_id_flag.id, Info->error);
            if (BTIF_BEC_NO_ERROR != Info->error)
            {
                return;
            }

            log_event_2(EVENT_A2DP_STREAMING_STARTED, btif_a2dp_get_stream_type(Stream), 3&btif_a2dp_get_stream_conn_remDev_hciHandle(Stream));


       tws_set_isOnGoingPlayInterruptedByRoleSwitch(false);
            //app_tws_get_env_pointer()->isOnGoingPlayInterruptedByRoleSwitch = false;

#ifdef __TWS__
            if(!is_tws_slave_conn_master_state())
                a2dp_timestamp_parser_init();
#else
                a2dp_timestamp_parser_init();
#endif

#ifndef __TWS__
            app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_START,BT_STREAM_SBC,stream_id_flag.id,MAX_RECORD_NUM,0,0);
#endif



#if defined(__TWS__)
            if(btif_a2dp_is_stream_device_has_delay_reporting(Stream)){
                //A2DP_SetSinkDelay(Stream,350);
        btif_a2dp_set_sink_delay(Stream,350);
            }
#endif
            app_bt_device.a2dp_streamming[stream_id_flag.id] = 1;
            break;
        case BTIF_A2DP_EVENT_STREAM_START_IND:
            TRACE("::A2DP_EVENT_STREAM_START_IND stream=%x stream_id:%d\n",Stream, stream_id_flag.id);
#if defined(__TWS__)
            tws_set_media_suspend(false);
#endif
#ifdef __BT_ONE_BRING_TWO__

           // A2DP_StartStreamRsp(&app_bt_device.a2dp_stream[stream_id_flag.id], A2DP_ERR_NO_ERROR);

        btif_a2dp_start_stream_rsp(app_bt_device.a2dp_stream[stream_id_flag.id]->a2dp_stream, BTIF_A2DP_ERR_NO_ERROR);

            if(btif_a2dp_get_stream_state(app_bt_device.a2dp_stream[stream_id_flag.id_other]->a2dp_stream)!= BTIF_AVDTP_STRM_STATE_STREAMING){
                app_bt_device.curr_a2dp_stream_id = stream_id_flag.id;
            }

#else
#if defined(__TWS__)
            if( !is_tws_master_conn_slave_state())
#endif
            {
               // A2DP_StartStreamRsp(Stream, A2DP_ERR_NO_ERROR);
        btif_a2dp_start_stream_rsp(Stream, BTIF_A2DP_ERR_NO_ERROR);
            }
#endif

#if defined(__TWS__)
            if(btif_a2dp_is_stream_device_has_delay_reporting(Stream)){
                btif_a2dp_set_sink_delay(Stream,350);
            }
#endif

            app_bt_device.a2dp_play_pause_flag = 1;
            break;
        case BTIF_A2DP_EVENT_STREAM_IDLE:
        case BTIF_A2DP_EVENT_STREAM_SUSPENDED:
            TRACE("::A2DP_EVENT_STREAM_SUSPENDED  stream_id:%d\n", stream_id_flag.id);
#if defined(_AUTO_TEST_)
            AUTO_TEST_SEND("Music suspend ok.");
#endif

        #ifdef ADAPTIVE_BLE_CONN_PARAM_ENABLED
            switch_to_high_speed_conn_interval();
        #endif

#ifdef __BT_ONE_BRING_TWO__
            if(btif_a2dp_get_stream_state(app_bt_device.a2dp_stream[stream_id_flag.id_other]->a2dp_stream) == BTIF_AVDTP_STRM_STATE_STREAMING){
                app_bt_device.curr_a2dp_stream_id = stream_id_flag.id_other;
                app_bt_device.a2dp_play_pause_flag = 1;
            }else{
                app_bt_device.a2dp_play_pause_flag = 0;
            }

#ifdef __TWS__
            if(!is_tws_slave_conn_master_state() )
                a2dp_timestamp_parser_init();
#else
                a2dp_timestamp_parser_init();
#endif

            app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_STOP,BT_STREAM_SBC,stream_id_flag.id,0,0,0);
#else
#ifndef __TWS__
            app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_STOP,BT_STREAM_SBC,BT_DEVICE_ID_1,0,0,0);
#endif
            app_bt_device.a2dp_play_pause_flag = 0;
#endif
            app_bt_device.a2dp_streamming[stream_id_flag.id] = 0;
            break;
        case BTIF_A2DP_EVENT_STREAM_DATA_IND:
            a2dp_sink_common_data_ind_handler(Stream, Info);
            break;
        case BTIF_A2DP_EVENT_STREAM_CLOSED:
            TRACE("::A2DP_EVENT_STREAM_CLOSED stream_id:%d, reason = %x\n", stream_id_flag.id,Info->discReason);
            if (!tws_is_role_switching_on())
            {
                if (BTIF_BEC_NO_ERROR == Info->discReason)
                {
                    if (disconn_voice_flag)
                    {
                        disconn_voice_flag = false;
                        if ( !is_slave_tws_mode())
#ifdef MEDIA_PLAYER_SUPPORT
                            app_voice_report(APP_STATUS_INDICATION_DISCONNECTED, 0);
#endif
                        if (is_master_tws_mode())
                        {
                            app_tws_ask_slave_to_play_voice_prompt(APP_STATUS_INDICATION_DISCONNECTED);
                        }
                    }
                }
            }

#ifdef __TWS__
            if(!is_tws_slave_conn_master_state() )
                a2dp_timestamp_parser_init();
#else
            a2dp_timestamp_parser_init();
#endif

#ifdef __BT_ONE_BRING_TWO__
            app_bt_device.curr_a2dp_stream_id = stream_id_flag.id_other;
            if(btif_a2dp_get_stream_state(app_bt_device.a2dp_stream[stream_id_flag.id_other]->a2dp_stream) == BTIF_AVDTP_STRM_STATE_STREAMING){
                app_bt_device.a2dp_play_pause_flag = 1;
            }
            else
            {
                app_bt_device.a2dp_play_pause_flag = 0;
            }
#else
            app_bt_device.a2dp_play_pause_flag = 0;
#endif
            app_bt_device.a2dp_state[stream_id_flag.id] = 0;
            app_bt_device.a2dp_streamming[stream_id_flag.id] = 0;

#ifdef __BT_ONE_BRING_TWO__
            ///a2dp disconnect so check the other stream is playing or not
        if(btif_a2dp_get_stream_state(app_bt_device.a2dp_stream[stream_id_flag.id_other]->a2dp_stream)  != BTIF_AVDTP_STRM_STATE_STREAMING)
            {
                app_bt_device.a2dp_play_pause_flag = 0;
            }
#endif
            a2dp_service_connected_set(false);
            break;
        case BTIF_A2DP_EVENT_CODEC_INFO:
            TRACE("::A2DP_EVENT_CODEC_INFO %d\n", Info->event);
            a2dp_set_config_codec(setconfig_codec,Info);
#ifdef __TWS__
            tws_set_channel_cfg(setconfig_codec);
#endif
            break;
        case BTIF_A2DP_EVENT_GET_CONFIG_IND:
            TRACE("::A2DP_EVENT_GET_CONFIG_IND %d\n", Info->event);

        btif_a2dp_set_stream_config(Stream, setconfig_codec, NULL);
            break;
        case BTIF_A2DP_EVENT_STREAM_RECONFIG_IND:
            TRACE("::A2DP_EVENT_STREAM_RECONFIG_IND %d\n", Info->event);
            DUMP8("%02x ",  &btif_a2dp_get_stream_codecCfg(Stream)->elements[0],4);
            //A2DP_ReconfigStreamRsp(Stream,A2DP_ERR_NO_ERROR,0);
        btif_a2dp_reconfig_stream_rsp( Stream,BTIF_A2DP_ERR_NO_ERROR ,0);
            break;
        case BTIF_A2DP_EVENT_STREAM_RECONFIG_CNF:
            TRACE("::A2DP_EVENT_STREAM_RECONFIG_CNF %d,sample rate=%x\n", Info->event,    btif_a2dp_get_stream_codecCfg( Stream)->elements[0]);
            DUMP8("%02x ",&btif_a2dp_get_stream_codecCfg( Stream)->elements[0],4);
            TRACE("A2DP_EVENT_STREAM_RECONFIG_CNF CODEC TYPE=%02x ",btif_a2dp_get_stream_codecCfg( Stream)->codecType);
#ifdef __TWS__
            tws_source_stream = tws_get_tws_source_stream();
#if defined(A2DP_LHDC_ON)
            if(btif_a2dp_get_stream_codecCfg( Stream)->codecType == BTIF_AVDTP_CODEC_TYPE_LHDC){
                a2dp_lhdc_config(&(Info->p.configReq->codec.elements[0]));
                TRACE("lhdc lhdcsample_rate:%x\n",app_bt_device.sample_rate[stream_id_flag.id]);
                app_bt_device.sample_rate[stream_id_flag.id] = a2dp_lhdc_get_sample_rate(btif_a2dp_get_stream_codecCfg(Stream)->elements);
                if(is_tws_master_conn_slave_state()){
                    tws_player_set_codec(BTIF_AVDTP_CODEC_TYPE_LHDC);
                }
                if(tws_get_source_stream_connect_status()){
                   btif_a2dp_get_stream_codecCfg( tws_source_stream)->elements[0] &= ~A2D_SBC_IE_SAMP_FREQ_MSK;
                   btif_a2dp_get_stream_codecCfg( tws_source_stream)->elements[0] |= app_bt_device.sample_rate[stream_id_flag.id];
                    if(app_bt_device.sample_bit[stream_id_flag.id] == 24){
                        btif_a2dp_get_stream_codecCfg( tws_source_stream)->elements[1] = A2D_SBC_IE_BIT_NUM_24;
                    }else if(app_bt_device.sample_bit[stream_id_flag.id] == 16){
                        btif_a2dp_get_stream_codecCfg( tws_source_stream)->elements[1] = A2D_SBC_IE_BIT_NUM_16;
                    }
                }
            }else
#endif
#ifdef A2DP_AAC_ON
            if(btif_a2dp_get_stream_codecCfg( Stream)->codecType == BTIF_AVDTP_CODEC_TYPE_MPEG2_4_AAC){
                    TRACE("::A2DP_EVENT_STREAM_OPEN stream_id:%d, aac sample_rate codec.elements 0x%x\n", stream_id_flag.id,Info->p.configReq->codec.elements[1]);
                app_bt_device.codec_type[stream_id_flag.id] = BTIF_AVDTP_CODEC_TYPE_MPEG2_4_AAC;
                    app_bt_device.sample_bit[stream_id_flag.id] = 16;
                    // convert aac sample_rate to sbc sample_rate format
                if (Info->p.configReq->codec.elements[1] & BTIF_A2DP_AAC_OCTET1_SAMPLING_FREQUENCY_44100) {
                        TRACE("::A2DP_EVENT_STREAM_OPEN stream_id:%d, aac sample_rate 44100\n", stream_id_flag.id);
                        app_bt_device.sample_rate[stream_id_flag.id] = A2D_SBC_IE_SAMP_FREQ_44;
                    }
                else if (Info->p.configReq->codec.elements[2] & BTIF_A2DP_AAC_OCTET2_SAMPLING_FREQUENCY_48000) {
                        TRACE("::A2DP_EVENT_STREAM_OPEN stream_id:%d, aac sample_rate 48000\n", stream_id_flag.id);
                        app_bt_device.sample_rate[stream_id_flag.id] = A2D_SBC_IE_SAMP_FREQ_48;
                    }
                    else {
                        TRACE("::A2DP_EVENT_STREAM_OPEN stream_id:%d, aac sample_rate not 48000 or 44100, set to 44100\n", stream_id_flag.id);
                        app_bt_device.sample_rate[stream_id_flag.id] = A2D_SBC_IE_SAMP_FREQ_44;
                    }
                if(is_tws_master_conn_slave_state()){
                    tws_player_set_codec(BTIF_AVDTP_CODEC_TYPE_MPEG2_4_AAC);
                    }
                if(tws_get_source_stream_connect_status()){
                   btif_a2dp_get_stream_codecCfg( tws_source_stream)->elements[0] &= ~A2D_SBC_IE_SAMP_FREQ_MSK;
                   btif_a2dp_get_stream_codecCfg( tws_source_stream)->elements[0] |= app_bt_device.sample_rate[stream_id_flag.id];
                   btif_a2dp_get_stream_codecCfg( tws_source_stream)->elements[1] = A2D_SBC_IE_BIT_NUM_16;
                    }
                }else
#endif
                {
                    //slave reconfig same as sbc
                    set_a2dp_reconfig_codec(Stream);
                    a2dp_sample_rate = bt_get_sbc_sample_rate();
                app_bt_device.sample_rate[stream_id_flag.id] = enum_sample_rate_transfer_to_codec_info(a2dp_sample_rate);
                    memcpy(app_bt_device.a2dp_codec_elements[stream_id_flag.id],Info->p.configReq->codec.elements,4);
                if(is_tws_master_conn_slave_state()){
                    tws_player_set_codec(BTIF_AVDTP_CODEC_TYPE_SBC);
                    }
                if(tws_get_source_stream_connect_status()){
                     btif_a2dp_get_stream_codecCfg( tws_source_stream)->elemLen = Info->p.configReq->codec.elemLen;
                    memcpy( btif_a2dp_get_stream_codecCfg( tws_source_stream)->elements, Info->p.configReq->codec.elements, Info->p.configReq->codec.elemLen);
                    }
                }
            if(tws_get_source_stream_connect_status()){
                if (  btif_a2dp_get_stream_state(tws_source_stream) == BTIF_AVDTP_STRM_STATE_OPEN){
                    DUMP8("%02x ",&btif_a2dp_get_stream_codecCfg( tws_source_stream)->elements[0],4);
                    status = btif_a2dp_reconfig_stream(tws_source_stream, btif_a2dp_get_stream_codecCfg( tws_source_stream), NULL);
                    TRACE("A2DP_EVENT_STREAM_RECONFIG_CNF CODEC TYPE-->A2DP_ReconfigStream status:%d state:%d", status, btif_a2dp_get_stream_state(tws_source_stream));
                }else if (btif_a2dp_get_stream_state(tws_source_stream) == BTIF_AVDTP_STRM_STATE_STREAMING){
                    a2dp_source_reconfig_sink_codec_request(Stream);
                }
            }
#endif
            log_event_4(EVENT_A2DP_CONFIG_UPDATED,   btif_a2dp_get_stream_type(Stream), btif_a2dp_get_stream_device_cmgrhandler_remDev_hciHandle(Stream)&3,
                            btif_a2dp_get_stream_codecCfg(Stream)->codecType, app_bt_device.sample_rate[stream_id_flag.id]);
            break;
    }
}

void a2dp_resume_mobile_stream(void)
{
    a2dp_stream_t* streamToResume = app_bt_get_mobile_stream(BT_DEVICE_ID_1);
    log_event_2(EVENT_START_A2DP_STREAMING, btif_a2dp_get_stream_type(streamToResume), 3&  btif_a2dp_get_stream_device_cmgrhandler_remDev_hciHandle(streamToResume));
    //bt_status_t retStatus = A2DP_StartStream(streamToResume);
    bt_status_t retStatus =  btif_a2dp_start_stream(streamToResume);
    TRACE("Resume mobile a2dp ret 0x%x", retStatus);
}

void a2dp_start_stream_handler(a2dp_stream_t *Stream)
{
    if( is_tws_master_conn_slave_state()||
         is_tws_master_conn_mobile_only_state())
    {
#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)
        if(app_get_play_role() == PLAYER_ROLE_SD)
            tws_if_player_stop(BT_STREAM_RBCODEC);
        else if(app_get_play_role() == PLAYER_ROLE_LINEIN)
            tws_if_player_stop(BT_STREAM_LINEIN);
#endif

        tws_if_reset_tws_data();
        if (is_tws_master_conn_slave_state())
        {
            bt_status_t state = BT_STS_FAILED;
            TRACE("A2DP_EVENT_STREAM_STARTED curr op=%d,next op=%d",tws.current_operation,tws.next_operation);
            if (NULL != Stream)
            {
                bt_status_t retStatus = btif_a2dp_start_stream_rsp(Stream,BTIF_A2DP_ERR_NO_ERROR);

                TRACE("Response mobile a2dp start req returns %d", retStatus);
            }

            if(tws_if_get_current_op() == IF_APP_TWS_IDLE_OP)
            {
                a2dp_stream_t * stream = tws_get_tws_source_stream();
                log_event_2(EVENT_START_A2DP_STREAMING, btif_a2dp_get_stream_type(stream), 3& btif_a2dp_get_stream_device_cmgrhandler_remDev_hciHandle(stream));

               state = btif_a2dp_start_stream(stream);
               if(state == BT_STS_PENDING)
                {
                    tws_set_current_op(IF_APP_TWS_START_STREAM);
                }
                else
                {
                    TRACE("%s,start stream error state = %d",__FUNCTION__,state);
                }
            }
            else
            {
               tws_set_next_op(IF_APP_TWS_START_STREAM);
               state = BT_STS_PENDING;
            }
            if(state != BT_STS_PENDING)
            {
               a2dp_stream_t * stream = tws_get_tws_source_stream();
           tws_if_set_stream_reset(1);
             //  state = A2DP_IdleStream(tws.tws_source.stream);
           state = btif_a2dp_idle_stream(stream);
                //reset tws a2dp connect
            }

            TRACE("send start to slave state = 0x%x",state);
        }
    }
#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)
     tws_if_set_mobile_sink_stream_state(IF_TWS_STREAM_START);
#endif
}

bool a2dp_sink_of_mobile_dev_callback(a2dp_stream_t *Stream, const a2dp_callback_parms_t *info)
{
   //TRACE("Enter %s  parmsp:%p   event is %d", __FUNCTION__, info, info->event);
   btif_a2dp_callback_parms_t * Info = (btif_a2dp_callback_parms_t*)info;

   btif_a2dp_event_t event = btif_a2dp_get_cb_event(Info);
    // get the mobile device id
#ifdef __BT_ONE_BRING_TWO__
    a2dp_stream_id_distinguish(Stream, Info->event);
#else
    stream_id_flag.id = BT_DEVICE_ID_1;
#endif

    switch(event) {
        case BTIF_A2DP_EVENT_STREAM_OPEN:
        case BTIF_A2DP_EVENT_STREAM_OPEN_IND:
            a2dp_sink_of_mobile_dev_opened_ind_handler(Stream, Info);
            break;
        case BTIF_A2DP_EVENT_STREAM_CLOSED:
            bt_drv_reg_op_rssi_set(0);
            a2dp_sink_of_mobile_dev_closed_ind_handler(Stream, Info);
            break;
        case BTIF_A2DP_EVENT_STREAM_START_IND:
            {
                TRACE("Get A2DP_EVENT_STREAM_START_IND from mobile.");
                bt_drv_reg_op_rssi_set(0xffff);
                
                app_tws_stop_sniff();
                
            if(( is_tws_master_conn_slave_state()) ||
                (is_tws_master_conn_mobile_only_state()))
            {
                    if (NULL != Stream)
                    {
                    //A2DP_StartStreamRsp(Stream,A2DP_ERR_NO_ERROR);
                    btif_a2dp_start_stream_rsp(Stream,BTIF_A2DP_ERR_NO_ERROR);
                }
                }
                break;
            }
        case BTIF_A2DP_EVENT_STREAM_STARTED:
            TRACE("Mobile gets A2DP_EVENT_STREAM_STARTED  error %d\n", Info->error);
#if defined(_AUTO_TEST_)
             AUTO_TEST_SEND("Music on ok.");
#endif
            if (BTIF_BEC_NO_ERROR != Info->error)
            {
                break;
            }
            TRACE("A2DP_EVENT_STREAM_STARTED event with mobile is received.");
            log_event_2(EVENT_A2DP_STREAMING_STARTED,  btif_a2dp_get_stream_type(Stream),
                3& btif_a2dp_get_stream_conn_remDev_hciHandle(Stream));
            app_bt_set_linkpolicy(app_bt_conn_mgr_get_mobileBtRemDev(), BTIF_BLP_DISABLE_ALL);
            a2dp_start_stream_handler(Stream);
        #ifdef ADAPTIVE_BLE_CONN_PARAM_ENABLED
            switch_to_low_speed_conn_interval();
        #endif
            break;
        case BTIF_A2DP_EVENT_STREAM_IDLE:
        case BTIF_A2DP_EVENT_STREAM_SUSPENDED:
#if defined(_AUTO_TEST_)
            AUTO_TEST_SEND("Music suspend ok.");
#endif
            if (app_bt_device.hf_audio_state[stream_id_flag.id] == BTIF_HF_AUDIO_DISCON){
                app_bt_set_linkpolicy(app_bt_conn_mgr_get_mobileBtRemDev(), BTIF_BLP_MASTER_SLAVE_SWITCH|BTIF_BLP_SNIFF_MODE);
            }

            log_event_2(EVENT_A2DP_STREAMING_SUSPENDED,  btif_a2dp_get_stream_type(Stream),
                3&btif_a2dp_get_stream_device_cmgrhandler_remDev_hciHandle(Stream));

            if (is_tws_master_conn_slave_state())
            {
                uint16_t status;

                tws_set_media_suspend(true);

                TRACE("A2DP_EVENT_STREAM_SUSPENDED curr op=%d,next op=%d", tws_if_get_current_op(), tws_if_get_next_op());
                if(tws_if_get_current_op() == IF_APP_TWS_IDLE_OP)
                {
                    a2dp_stream_t * stream = tws_get_tws_source_stream();

                    log_event_2(EVENT_SUSPEND_A2DP_STREAMING, btif_a2dp_get_stream_type(stream),
                        3& btif_a2dp_get_stream_device_cmgrhandler_remDev_hciHandle(stream));

                    status = btif_a2dp_suspend_stream(stream);

                    if(status == BT_STS_PENDING)
                    {
                        //APP_TWS_SET_CURRENT_OP(APP_TWS_SUSPEND_STREAM);
                        tws_set_current_op(IF_APP_TWS_SUSPEND_STREAM);
                    }
                    else
                    {
                        TRACE("%s,suspend stream error state = %d",__FUNCTION__,status);
                    }
                     //APP_TWS_SET_NEXT_OP(APP_TWS_IDLE_OP);
                      tws_set_next_op(IF_APP_TWS_IDLE_OP);
                }
                else
                {
                  tws_set_next_op(IF_APP_TWS_SUSPEND_STREAM);
                  status = BT_STS_PENDING;
                }

                TRACE("send suspend to slave status=%x,source stream=%x",status,tws_get_tws_source_stream());
            }
            tws_if_player_stop(BT_STREAM_SBC);
#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)
          //  TWS_SET_M_STREAM_STATE(TWS_STREAM_SUSPEND)
            tws_if_set_mobile_sink_stream_state(IF_TWS_STREAM_SUSPEND);

#endif
                break;
        case BTIF_A2DP_EVENT_STREAM_DATA_IND:
            if (!app_bt_stream_isrun(APP_BT_STREAM_HFP_PCM)&&
#ifdef MEDIA_PLAYER_SUPPORT
                    !app_bt_stream_isrun(APP_PLAY_BACK_AUDIO)&&
#endif
                    !app_audio_manager_bt_stream_voice_request_exist() &&
                    (app_audio_manager_get_current_media_type() !=BT_STREAM_VOICE ) &&
                    (app_audio_manager_get_current_media_type() !=BT_STREAM_MEDIA ) &&
                (!tws_if_get_ble_reconnect_pending() ) &&
#ifndef FPGA
                    (app_poweroff_flag == 0) &&
#endif
                 (( is_tws_master_conn_slave_state()) ?
                    ( btif_a2dp_get_stream_state(tws_get_tws_source_stream()) == BTIF_AVDTP_STRM_STATE_STREAMING) : 1)
                    && ( tws_if_get_current_op() == IF_APP_TWS_IDLE_OP)
               )

            {
                if( tws_api_analysis_media_data_from_mobile( Stream, Info))
                {
                    int header_len = 0;
                    u32 sbc_packet_len = 0;
                    U8 frameNum = 0;
                    unsigned char * sbc_buffer = NULL;

                    //header_len = AVDTP_ParseMediaHeader(&media_header, Info->p.data);
                    header_len = btif_avdtp_parse_mediaHeader(tws_if_get_media_header(), Info);
                    app_tws_start_master_player(Stream);
                    tws_if_set_pause_packets(0);

                    sbc_buffer = (unsigned char *)Info->p.data + header_len;
                    sbc_packet_len = Info->len - header_len - 1;
                    frameNum = *(sbc_buffer);

                    tws_if_set_sbc_frame_info(sbc_packet_len/frameNum,frameNum );

                    bt_drv_reg_op_packet_type_checker(tws.tws_conhdl);
                }
            }else{
                if(app_bt_stream_isrun(APP_BT_STREAM_HFP_PCM))
                    TRACE("PCM FILTER");
                if(app_bt_stream_isrun(APP_PLAY_BACK_AUDIO))
                    TRACE("AUDIO FILTER");
                if(app_audio_manager_bt_stream_voice_request_exist())
                    TRACE("VOICE REQ ESIST");
                if(app_audio_manager_get_current_media_type() ==BT_STREAM_VOICE)
                    TRACE("VOICE FILTER");
                if(app_audio_manager_get_current_media_type() ==BT_STREAM_MEDIA)
                    TRACE("MEDIA FILTER");
#ifndef FPGA
                if(app_poweroff_flag)
                    TRACE("power off filter");
#endif
                if(tws.bleReconnectPending)
                    TRACE("ble reconn filter");
                if( is_tws_master_conn_slave_state() && btif_a2dp_get_stream_state(tws_get_tws_source_stream()) != BTIF_AVDTP_STRM_STATE_STREAMING)
                    TRACE("stream filter %d",btif_a2dp_get_stream_state(tws_get_tws_source_stream()) );
                if(tws.current_operation != APP_TWS_IDLE_OP)
                    TRACE("CURR OP FILTER %d",  tws_if_get_current_op());
                // all handlings are done for this event
                return true;
            }

            break;
    }

    return false;
}

void a2dp_suspend_music_force(void)
{
    app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_STOP,BT_STREAM_SBC,stream_id_flag.id,0,0,0);
}

int a2dp_volume_get(void)
{
    int vol = app_bt_stream_volume_get_ptr()->a2dp_vol;

    vol = 8*vol-1;
    if (vol > (0x7f-1))
        vol = 0x7f;

    return (vol);
}


int a2dp_volume_get_tws(void)
{
    int vol = app_bt_stream_volume_get_ptr()->a2dp_vol-1;

    vol = 8*vol;
    if (vol > (0x7f-1))
        vol = 0x7f;

    return (vol);
}

int app_bt_stream_volumeset(int8_t vol);

void a2dp_volume_local_set(int8_t vol)
{
    app_bt_stream_volume_get_ptr()->a2dp_vol = vol;
#ifndef FPGA
    nv_record_touch_cause_flush();
#endif
}

int a2dp_volume_set(U8 vol)
{
    int dest_vol;
    TRACE("#vol:%d",vol);
    dest_vol = (((int)vol&0x7f)<<4)/0x7f + 1;

    if (dest_vol > TGT_VOLUME_LEVEL_15)
        dest_vol = TGT_VOLUME_LEVEL_15;
    if (dest_vol < TGT_VOLUME_LEVEL_0)
        dest_vol = TGT_VOLUME_LEVEL_0;

    a2dp_volume_local_set(dest_vol);
    app_bt_stream_volumeset(dest_vol);

    return (vol);
}

void btapp_a2dp_report_speak_gain(void)
{
#ifdef BTIF_AVRCP_ADVANCED_CONTROLLER
    uint8_t i;
    int vol = a2dp_volume_get();

    for(i=0; i<BT_DEVICE_NUM; i++)
    {
        if (app_bt_device.avrcp_notify_rsp[i] == NULL)
            btif_app_a2dp_avrcpadvancedpdu_mempool_calloc(&app_bt_device.avrcp_notify_rsp[i]);
        TRACE("btapp_a2dp_report_speak_gain transId:%d a2dp_state:%d streamming:%d report:%02x\n",
        btif_get_app_bt_device_avrcp_notify_rsp_transid(app_bt_device.avrcp_notify_rsp[i]),
                app_bt_device.a2dp_state[i],
                app_bt_device.a2dp_streamming[i],
                app_bt_device.volume_report[i]);
        if ((app_bt_device.a2dp_state[i] == 1) &&
                (app_bt_device.a2dp_streamming[i] == 1) &&
            (app_bt_device.volume_report[i] == BTIF_AVCTP_RESPONSE_INTERIM)){
            app_bt_device.volume_report[i] = BTIF_AVCTP_RESPONSE_CHANGED;
            TRACE("btapp_a2dp_report_speak_gain transId:%d\n", btif_get_app_bt_device_avrcp_notify_rsp_transid(app_bt_device.avrcp_notify_rsp[i]));
            if (app_bt_device.avrcp_notify_rsp[i] != NULL){

                btif_set_app_bt_device_avrcp_notify_rsp_ctype(app_bt_device.avrcp_notify_rsp[i],BTIF_AVCTP_RESPONSE_CHANGED);

        btif_avrcp_ct_get_absolute_volume_rsp(app_bt_device.avrcp_channel[i],app_bt_device.avrcp_notify_rsp[i],vol);
            }
        }
    }
#endif
}
extern void app_bt_fill_mobile_a2dp_stream(uint32_t deviceId, a2dp_stream_t *stream);
extern btif_remote_device_t* mobileBtRemDev;


void a2dp_sink_of_mobile_dev_opened_ind_handler(a2dp_stream_t *Stream,  const a2dp_callback_parms_t *info)
{

   btif_remote_device_t *remDev;


    btif_a2dp_callback_parms_t * Info = (btif_a2dp_callback_parms_t*)info;

    if (app_tws_is_to_update_sample_rate_after_role_switch())
    {
        a2dp_restore_sample_rate_after_role_switch(Stream, Info);
        TRACE("set-up the codec type as %d after role switch.",  tws_if_get_mobile_codec_type());
        Info->p.configReq->codec.codecType =  tws_if_get_mobile_codec_type();
    }

    if (BTIF_A2DP_EVENT_STREAM_OPEN == Info->event)
    {
        tws_if_set_mobile_sink_stream(Stream);
        tws_if_set_sink_stream(Stream);
        //twsd->mobile_sink.connected = true;
        tws_if_set_mobile_sink_connnected(true);

        if( is_tws_no_conn_state() )
        {
            tws_if_set_conn_state(IF_TWS_MASTER_CONN_MOBILE_ONLY);
        }
#ifdef __TWS_PAIR_DIRECTLY__
        app_status_indication_set(APP_STATUS_INDICATION_CONNECTED);
        if(app_tws_is_slave_mode())
        {
            TRACE("a2dp:tws_mode_ptr()->mode:%d,open ble scan",tws_mode_ptr()->mode);
            //if slave connect phone ,restart scan for tws, slave to master
            conn_ble_reconnection_machanism_handler(false);
        }
#endif
        app_bt_stream_volume_ptr_update((uint8_t *)  btif_me_get_remote_device_bdaddr(  btif_a2dp_get_stream_conn_remDev( Stream))->address);

        //twsd->unlock_mode(twsd);
        tws_if_unlock_mode();
        TRACE("The codec type is %d", Info->p.configReq->codec.codecType);
#if defined(A2DP_LHDC_ON)
        if(Info->p.configReq->codec.codecType == BTIF_AVDTP_CODEC_TYPE_LHDC){
            btif_a2dp_lhdc_config_tws_audio(info);
        }else
#endif
#ifdef A2DP_AAC_ON
        if(Info->p.configReq->codec.codecType == BTIF_AVDTP_CODEC_TYPE_MPEG2_4_AAC){
            btif_a2dp_aac_config_tws_audio(info);
        }else
#endif
        if(Info->p.configReq->codec.codecType == BTIF_AVDTP_CODEC_TYPE_SBC){
            TRACE("%s %d  parmsp:%p     el:%d sr:%d",__func__,__LINE__,   Info,  Info->p.configReq->codec.elemLen ,Info->p.configReq->codec.elements[0] );
            btif_a2dp_sbc_config_tws_audio(info);
        }

        btif_a2dp_tws_set_mobile_codec_info(info);
        btdrv_rf_bit_offset_track_enable(true);
        btdrv_rf_set_conidx(btdrv_conhdl_to_linkid( btif_a2dp_get_stream_device_cmgrhandler_remDev_hciHandle(Stream)));
        tws_if_set_mobile_conhdl(btif_a2dp_get_stream_device_cmgrhandler_remDev_hciHandle(Stream));

        //remDev = A2DP_GetRemoteDevice(Stream);

        remDev = btif_a2dp_get_remote_device(Stream);

        if (is_btif_me_current_role_bcr_master(remDev)) {
            btdrv_rf_trig_patch_enable(0);
            TRACE("%s : as bt master codec = %d\n", __func__, Info->p.configReq->codec.codecType);
        }
        else {
            btdrv_rf_trig_patch_enable(1);
            TRACE("%s : as bt slave codec = %d\n", __func__, Info->p.configReq->codec.codecType);
        }

#ifdef __TWS_FOLLOW_MOBILE__
            bt_drv_reg_op_afh_follow_mobile_mobileidx_set(btif_a2dp_get_stream_device_cmgrhandler_remDev_hciHandle(Stream));
#endif

#ifdef __TWS_CALL_DUAL_CHANNEL__
        tws_if_set_mobile_addr(btif_a2dp_get_stream_devic_cmgrHandler_remdev_bdAddr(Stream));
       // memcpy(twsd->mobile_addr.addr,Stream->device->cmgrHandler.remDev->bdAddr.addr,6);

        if (!is_simulating_reconnecting_in_progress())
        {
            app_tws_sniffer_magic_config_channel_map();
        }
#endif

#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)
       // TWS_SET_M_STREAM_STATE(IF_TWS_STREAM_OPEN);
        tws_if_set_mobile_sink_stream_state(IF_TWS_STREAM_OPEN)
#endif

    }
    else if (BTIF_A2DP_EVENT_STREAM_OPEN_IND == Info->event)
    {
#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)
  //      TWS_SET_M_STREAM_STATE(TWS_STREAM_OPEN);
     tws_if_set_mobile_sink_stream_state(IF_TWS_STREAM_OPEN)
#endif
#if defined(A2DP_LHDC_ON)
        if(Info->p.configReq->codec.codecType == BTIF_AVDTP_CODEC_TYPE_LHDC){
        btif_a2dp_lhdc_config_tws_audio(Info);
    #if 0
            switch (A2DP_LHDC_SR_DATA(Info->p.configReq->codec.elements[6])) {
                case A2DP_LHDC_SR_96000:
                    twsd->audout.sample_rate = AUD_SAMPRATE_96000;
                    twsd->pcmbuff.sample_rate  = AUD_SAMPRATE_96000;
                    TRACE("LHDC:CodecCfg sample_rate 96000\n", __func__);
                    break;
                case A2DP_LHDC_SR_48000:
                    twsd->audout.sample_rate = AUD_SAMPRATE_48000;
                    twsd->pcmbuff.sample_rate  = AUD_SAMPRATE_48000;
                    TRACE("LHDC:CodecCfg sample_rate 48000\n", __func__);
                    break;
                case A2DP_LHDC_SR_44100:
                    twsd->audout.sample_rate = AUD_SAMPRATE_44100;
                    twsd->pcmbuff.sample_rate  = AUD_SAMPRATE_44100;
                    TRACE("LHDC:CodecCfg sample_rate 44100\n", __func__);
                    break;
            }
                    TRACE("LHDC:CodecCfg bits per sampe = 24", __func__);
                    break;
            }
    #endif
        }else
#endif
#ifdef A2DP_AAC_ON
        if(Info->p.configReq->codec.codecType == BTIF_AVDTP_CODEC_TYPE_MPEG2_4_AAC){
        btif_a2dp_aac_config_tws_audio(Info);
    #if 0
                if (Info->p.configReq->codec.elements[1] & A2DP_AAC_OCTET1_SAMPLING_FREQUENCY_44100){
                    twsd->audout.sample_rate = AUD_SAMPRATE_44100;
                    twsd->pcmbuff.sample_rate  = AUD_SAMPRATE_44100;
                }else if (Info->p.configReq->codec.elements[2] & A2DP_AAC_OCTET2_SAMPLING_FREQUENCY_48000){
                    twsd->audout.sample_rate = AUD_SAMPRATE_48000;
                    twsd->pcmbuff.sample_rate = AUD_SAMPRATE_48000;
            }
     #endif
            }else
#endif
        if(Info->p.configReq->codec.codecType == BTIF_AVDTP_CODEC_TYPE_SBC){
        btif_a2dp_sbc_config_tws_audio(Info);
    #if 0
                    twsd->audout.sample_rate = bt_parse_sbc_sample_rate(Info->p.configReq->codec.elements[0]);
            twsd->pcmbuff.sample_rate = twsd->audout.sample_rate;
    #endif
        }
        btif_a2dp_tws_set_mobile_codec_info(info);
       // twsd->mobile_samplerate = twsd->audout.sample_rate;
        }

    if (!app_tws_is_role_switching_on())
    {
        TRACE("%s mobile profile %d", __func__, mobileDevConnectionState);
        if(IS_JUST_LINK_ON(mobileDevConnectionState))
        {
            if ((app_tws_is_master_mode()  ) && IS_CONNECTED_WITH_TWS())
            {
                app_tws_sniffer_magic_config_channel_map();
                //Me_SetSnifferEnv(1, FORWARD_ROLE, mobileDevBdAddr.addr, tws_mode_ptr()->slaveAddr.addr);
                btif_me_set_sniffer_env(1, FORWARD_ROLE, mobileDevBdAddr.address, tws_mode_ptr()->slaveAddr.address);
            }

#ifndef FPGA
            app_voice_report(APP_STATUS_INDICATION_CONNECTED, 0);
            app_status_indication_set(APP_STATUS_INDICATION_CONNECTED);
            app_tws_ask_slave_to_play_voice_prompt(APP_STATUS_INDICATION_CONNECTED);
#endif
        }
    }

    // update profile active state in NV
    btdevice_profile *btdevice_plf_p =
        (btdevice_profile *)app_bt_profile_active_store_ptr_get((uint8_t *)btif_a2dp_get_stream_devic_cmgrHandler_remdev_bdAddr(Stream));
    if (true != btdevice_plf_p->a2dp_act)
    {
        btdevice_plf_p->a2dp_act = true;
        btdevice_plf_p->a2dp_codectype = Info->p.configReq->codec.codecType;
        TRACE("set codec type as %d", btdevice_plf_p->a2dp_codectype);
#ifndef FPGA
        nv_record_touch_cause_flush();
#endif
        // update the run-time profile active state
        SET_MOBILE_DEV_ACTIVE_PROFILE(BT_PROFILE_A2DP);
    }

    app_bt_fill_mobile_a2dp_stream(BT_DEVICE_ID_1, Stream);

    // update the connection state
    uint32_t formerMobileConnState = mobileDevConnectionState;
    SET_CONN_STATE_A2DP(mobileDevConnectionState);
    //btif_me_get_remote_device_hci_handle(btif_remote_device_t * rdev);
    log_event_2(EVENT_A2DP_CONNECTED, BT_FUNCTION_ROLE_MOBILE, 3&btif_me_get_remote_device_hci_handle(mobileBtRemDev));

    if (formerMobileConnState != mobileDevConnectionState)
    {
        conn_stop_connecting_mobile_supervising();

        mobile_device_connection_state_updated_handler(BTIF_BEC_NO_ERROR);

        if (!is_simulating_reconnecting_in_progress() && !app_tws_is_role_switching_on())
        {
            app_tws_check_max_slot_setting();

            // continue to connect hfp profile
            if (IS_MOBILE_DEV_PROFILE_ACTIVE(BT_PROFILE_HFP) && !IS_CONN_STATE_HFP_ON(mobileDevConnectionState))
            {
                if (!IS_WAITING_FOR_MOBILE_CONNECTION())
                {
                    TRACE("A2DP has been connected, now start connecting HFP.");
                    app_start_custom_function_in_bt_thread(0, 0,(uint32_t)conn_connecting_mobile_handler);
                }
            }
        }
        else
        {
            //app_start_custom_function_in_bt_thread(0,
            //    0, (uint32_t)app_connect_op_after_role_switch);

            app_tws_switch_role_with_slave();
        }
    }
}

void a2dp_sink_of_mobile_dev_closed_ind_common_handler(a2dp_stream_t *Stream, const a2dp_callback_parms_t *Info)
{
    uint8_t status;
    app_bt_hcireceived_data_clear(tws.mobile_conhdl);
    btif_HciBypassProcessReceivedDataExt(NULL);

    tws_if_restore_mobile_channels();
    tws_if_reset_tws_data();
#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)
    //TWS_SET_M_STREAM_STATE(TWS_STREAM_CLOSE)
    tws_if_set_mobile_sink_stream_state(IF_TWS_STREAM_CLOSE)
#endif
    ///if tws is active so check the slave is playing or not
    ///if slave playing,so stop sbc play
    if(( is_tws_master_conn_slave_state()) && !is_tws_if_media_suspend() )
        {
       // tws.media_suspend = true;
    tws_if_set_media_suspend(true);
        if(tws_if_get_current_op() == IF_APP_TWS_IDLE_OP)
            {
            log_event_2(EVENT_SUSPEND_A2DP_STREAMING,   btif_a2dp_get_stream_type(tws_get_tws_source_stream()),
                        3& btif_a2dp_get_stream_device_cmgrhandler_remDev_hciHandle(tws_get_tws_source_stream()));


       //status = A2DP_SuspendStream(tws.tws_source.stream);
       status = btif_a2dp_suspend_stream(tws_get_tws_source_stream());

            if(status == BT_STS_PENDING)
                {
                //APP_TWS_SET_CURRENT_OP(APP_TWS_SUSPEND_STREAM);
        tws_set_current_op(IF_APP_TWS_SUSPEND_STREAM);
                }
                else
                {
                    TRACE("%s,suspend stream error state = %d",__FUNCTION__,status);
                }
           // APP_TWS_SET_NEXT_OP(APP_TWS_IDLE_OP);
        tws_set_current_op(IF_APP_TWS_IDLE_OP);
            }
            else
            {
            //APP_TWS_SET_NEXT_OP(APP_TWS_SUSPEND_STREAM);
        tws_set_next_op(IF_APP_TWS_SUSPEND_STREAM);
        }
        }

#ifdef __TWS_FOLLOW_MOBILE__
    bt_drv_reg_op_afh_follow_mobile_mobileidx_set(0xff); //mobile idx
#endif

    if ( is_tws_master_conn_mobile_only_state())
    {
        tws_if_set_conn_state(IF_TWS_NON_CONNECTED);
    }

    //should be in stream closed end, or the trans thread may send frame to slave in wrong codec type
    //tws.mobile_codectype = AVDTP_CODEC_TYPE_NON_A2DP;
    tws_if_set_mobile_codec_type(BTIF_AVDTP_CODEC_TYPE_NON_A2DP);
    // update the connection state
    CLEAR_CONN_STATE_A2DP(mobileDevConnectionState);
    log_event_3(EVENT_A2DP_DISCONNECTED, BT_FUNCTION_ROLE_MOBILE, 3&  btif_me_get_remote_device_hci_handle(mobileBtRemDev),
       btif_a2dp_get_cb_error((btif_a2dp_callback_parms_t*)Info));
}

void a2dp_sink_of_mobile_dev_closed_ind_handler(a2dp_stream_t *Stream, const a2dp_callback_parms_t *info)
{

    btif_a2dp_callback_parms_t * Info = (btif_a2dp_callback_parms_t*)info;

    log_event_3(EVENT_A2DP_DISCONNECTED,  btif_a2dp_get_stream_type(Stream), 3&btif_me_get_remote_device_hci_handle(mobileBtRemDev), Info->discReason);

    TRACE("The a2dp connection with mobile device is disconnected, reason is 0x%x.", Info->discReason);
    TRACE("Current op state %d", conn_op_state);

    if (!is_simulating_reconnecting_in_progress())
    {
     //   if (app_bt_device.hf_audio_state[stream_id_flag.id] == HF_AUDIO_DISCON){
        //       app_bt_set_linkpolicy(mobileBtRemDev, BLP_MASTER_SLAVE_SWITCH|BLP_SNIFF_MODE);
        //  }
        app_tws_role_switch_cleanup_a2dp(btif_me_get_remote_device_hci_handle(mobileBtRemDev));
    }
    a2dp_sink_of_mobile_dev_closed_ind_common_handler(Stream, info);

    mobile_device_connection_state_updated_handler(Info->discReason);

    if (!is_simulating_reconnecting_in_progress())
    {
        if (!IS_CONNECTED_WITH_MOBILE())
        {
            if (IS_CONNECTING_MOBILE())
            {
                set_conn_op_state(CONN_OP_STATE_IDLE);
            }

            if (is_mobile_connection_creation_supervisor_timer_on_going())
            {
                // put the mobile in the idle state, to assure that if the mobile is connecting
                // in the meantime, the device can be connected
                conn_start_mobile_connection_creation_idle_timer();
            }
            else
            {
                if (!((BTIF_BEC_USER_TERMINATED == Info->discReason) ||
                    (BTIF_BEC_LOCAL_TERMINATED == Info->discReason)))
                {
                    app_start_custom_function_in_bt_thread(0, 0, \
                            (uint32_t)connecting_mobile_timeout_handler);
                }
            }
        }
    }
}

bool a2dp_is_music_ongoing(void)
{
    return app_bt_device.a2dp_streamming[BT_DEVICE_ID_1] || is_tws_if_tws_playing();
}

bool avdtp_Get_aacEnable_Flag( btif_remote_device_t* remDev, btif_avdtp_stream_t *strm)
{
    uint8_t* remoteVersion = btif_a2dp_get_remote_device_version(remDev);
    TRACE("%s,version=0x%x,0x%x",__FUNCTION__,  remoteVersion [1],remoteVersion[2]);
#if defined(A2DP_AAC_ON)
    if(((remoteVersion[1] == 0x0f) && (remoteVersion[2] == 0)) || btif_avdtp_get_stream_codec_type(strm) !=BTIF_AVDTP_CODEC_TYPE_MPEG2_4_AAC){
        return TRUE;
    }else
        return TRUE;
#else
    return TRUE;
#endif
}


void app_AVRCP_SendCustomCmdToMobile(uint8_t* ptrData, uint32_t len)
{
    if (is_slave_tws_mode())
    {
        return;
    }

    btif_avrcp_send_custom_cmd_generic(app_bt_device.avrcp_channel[BT_DEVICE_ID_1], ptrData, len);
}


void app_AVRCP_SendCustomCmdToTws(uint8_t* ptrData, uint32_t len)
{
    if (is_slave_tws_mode())
    {
        btif_avrcp_send_custom_cmd_generic(app_bt_device.avrcp_channel[BT_DEVICE_ID_1], ptrData, len);
    }
    else if (is_master_tws_mode())
    {
        btif_avrcp_channel_t* chnl =  btif_get_avrcp_channel(tws_get_avrcp_channel_hdl());
        btif_avrcp_send_custom_cmd_generic(chnl, ptrData, len);
    }
}


static void app_AVRCP_sendCustomCmdRsp(uint8_t device_id, btif_avrcp_channel_t *chnl, uint8_t isAccept, uint8_t transId)
{
    if (app_bt_device.avrcp_control_rsp[device_id] == NULL)
    {
        btif_app_a2dp_avrcpadvancedpdu_mempool_calloc(&app_bt_device.avrcp_control_rsp[device_id]);
    }

    //app_bt_device.avrcp_control_rsp[device_id]->transId = transId;
    //app_bt_device.avrcp_control_rsp[device_id]->ctype = AVCTP_RES_ACCEPTED;

    btif_avrcp_set_control_rsp_cmd(app_bt_device.avrcp_control_rsp[device_id],transId, BTIF_AVCTP_RESPONSE_ACCEPTED);


    btif_avrcp_ct_accept_custom_cmd_rsp(chnl, app_bt_device.avrcp_control_rsp[device_id], isAccept);
}

static void app_AVRCP_CustomCmd_Received(uint8_t* ptrData, uint32_t len)
{
    TRACE("AVRCP Custom Command Received %d bytes data:", len);
    DUMP8("0x%02x ", ptrData, len);
}

