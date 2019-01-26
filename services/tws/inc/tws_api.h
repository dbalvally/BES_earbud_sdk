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
#ifndef __TWS_API_H__
#define __TWS_API_H__

#include "cqueue.h"
#include "app_audio.h"

#include "app_bt.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_TWS_SET_CURRENT_OP(op) do { tws.current_operation = (op); TRACE("Set TWS current op to %d", op); } while (0);
#define APP_TWS_SET_NEXT_OP(op) do { tws.next_operation = (op); TRACE("Set TWS next op to %d", op); } while (0);

typedef enum {
	IF_TWSFREE,
	IF_TWSMASTER,
	IF_TWSSLAVE,
} IF_DeviceMode;

typedef enum {
	IF_TWS_MASTER_CONN_SLAVE = 0,	
	IF_TWS_MASTER_CONN_MOBILE_ONLY,	
	IF_TWS_SLAVE_CONN_MASTER,		
	IF_TWS_NON_CONNECTED		
} IF_TWS_CONN_STATE_E;

typedef enum {
    IF_TWS_PLAYER_START = 0,
    IF_TWS_PLAYER_SETUP,
    IF_TWS_PLAYER_STOP,
    IF_TWS_PLAYER_PAUSE,
    IF_TWS_PLAYER_RESTART,
    IF_TWS_PLAYER_NULL,
}IF_TWS_PLAYER_STATUS_T;

enum {
    IF_APP_TWS_CLOSE_BLE_ADV,
    IF_APP_TWS_OPEN_BLE_ADV,
};


enum {
    IF_APP_TWS_START_STREAM,
    IF_APP_TWS_SUSPEND_STREAM,
    IF_APP_TWS_CLOSE_STREAM,
    IF_APP_TWS_OPEN_STREAM,
    IF_APP_TWS_IDLE_OP
};

enum {
    IF_APP_TWS_CLOSE_BLE_SCAN,
    IF_APP_TWS_OPEN_BLE_SCAN,
};

typedef enum {
    IF_TWS_STREAM_CLOSE = 1 << 0,
    IF_TWS_STREAM_OPEN = 1 << 1,
    IF_TWS_STREAM_START = 1 << 2,
    IF_TWS_STREAM_SUSPEND = 1 << 3,
}IF_TwsStreamState;

typedef struct{
    U8   bitPool;
    U8   numBlocks;
    U8   numSubBands;
    U8   frameNum;
    U32 frameCount;
    U32 frameLen;
    U32 sampleFreq;
}BTIF_TWS_SBC_FRAME_INFO;


void tws_set_current_op(uint8_t op);
void tws_set_next_op(uint8_t op);
void tws_set_current_op(uint8_t op);
void tws_set_next_op(uint8_t op);

uint8_t tws_if_get_current_op(void);
uint8_t tws_if_get_next_op(void);




btif_avrcp_chnl_handle_t tws_get_avrcp_channel_hdl(void);
 bool is_slave_tws_mode(void);
bool is_master_tws_mode(void);
bool is_playing(void );

bool is_tws_master_conn_mobile_only_state(void);

bool is_tws_master_conn_slave_state(void);
bool is_tws_slave_conn_master_state(void);
bool is_tws_no_conn_state(void);

btif_avrcp_chnl_handle_t tws_get_avrcp_TGchannel(void);


btif_avdtp_codec_t*  tws_get_set_config_codec_info(void);

bool tws_get_source_stream_connect_status(void);
bool tws_is_role_switching_on(void);
void  tws_set_isOnGoingPlayInterruptedByRoleSwitch(bool on);
void tws_set_media_suspend(bool enable);

void tws_if_player_stop(uint8_t stream_type);
int tws_if_reset_tws_data(void);
void  tws_if_set_stream_reset(bool enable);
//#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)
void tws_if_set_mobile_sink_stream_state(IF_TwsStreamState state);
//#endif
bool tws_if_get_ble_reconnect_pending(void);
bool tws_api_analysis_media_data_from_mobile(a2dp_stream_t *stream, btif_a2dp_callback_parms_t *Info);
btif_media_header_t* tws_if_get_media_header(void);
void tws_if_set_pause_packets(uint32_t count);
void tws_if_set_sbc_frame_info(uint32_t flen, uint32_t fnum );
btif_avdtp_codec_type_t tws_if_get_mobile_codec_type(void);
void tws_if_set_mobile_sink_stream(a2dp_stream_t *stream);
void tws_if_set_sink_stream(a2dp_stream_t *stream);
void tws_if_set_mobile_sink_connnected(bool connected);
void tws_if_set_conn_state(IF_TWS_CONN_STATE_E state);
void tws_if_unlock_mode(void);
void tws_if_lock_mode(void);
void tws_if_set_mobile_conhdl(uint16_t handle);
void tws_if_set_mobile_addr(uint8_t * addr);

bool is_tws_if_media_suspend(void );
void tws_if_set_media_suspend(bool suspended );
int tws_if_get_a2dpbuff_available(void);
void tws_if_set_stream_state(uint16_t state);
a2dp_stream_t *tws_get_tws_mobile_sink_stream();
void tws_if_restore_mobile_channels(void );
void tws_if_set_mobile_codec_type(btif_avdtp_codec_type_t type);
bool is_tws_if_tws_playing(void);
btif_avrcp_chnl_handle_t tws_get_avrcp_channel_hdl(void);
a2dp_stream_t * tws_get_tws_source_stream(void);


#ifdef __cplusplus
}
#endif
#endif /**/
