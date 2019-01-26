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
#ifndef APP_TWS_IF_H
#define APP_TWS_IF_H
#include "app_tws.h"
#ifdef __cplusplus
#define EXTERN_C                        extern "C"
#else
#define EXTERN_C
#endif

#define app_tws_get_conn_state()	(tws.tws_conn_state)

void app_tws_set_sink_stream(a2dp_stream_t *stream);

a2dp_stream_t* app_tws_get_sink_stream(void);

EXTERN_C char app_tws_mode_is_master(void);

EXTERN_C char app_tws_mode_is_slave(void);

char app_tws_mode_is_only_mobile(void);

EXTERN_C uint16_t app_tws_get_tws_conhdl(void);

void app_tws_set_tws_conhdl(uint16_t conhdl);

btif_avrcp_chnl_handle_t  app_tws_get_avrcp_TGchannel(void);

a2dp_stream_t *app_tws_get_tws_source_stream(void);

a2dp_stream_t *app_tws_get_tws_sink_stream(void);

a2dp_stream_t* app_tws_get_mobile_sink_stream(void);

bool app_tws_get_source_stream_connect_status(void);

bool app_tws_get_sink_stream_connect_status(void);

uint8_t *app_tws_get_peer_bdaddr(void);

void app_tws_set_have_peer(bool is_have);

void app_tws_set_tws_mode(DeviceMode mode);

DeviceMode app_tws_get_tws_mode(void);

void app_tws_set_media_suspend(bool enable);

int app_tws_get_a2dpbuff_available(void);

void app_tws_set_eq_band_settings(int8_t *eq_band_settings);

void app_tws_get_eq_band_gain(float **eq_band_gain);

void app_tws_get_eq_band_init(void);

float app_tws_sample_rate_hacker(uint32_t sample_rate);

void app_tws_reset_slave_medianum(void);
uint16_t app_tws_get_slave_medianum(void);

a2dp_stream_t *app_tws_get_main_sink_stream(void);

EXTERN_C void app_tws_env_init(tws_env_cfg_t *cfg);

void app_tws_switch_role_in_nv(void);

void app_tws_store_peer_ble_addr_to_nv(uint8_t* peer_ble_addr);
uint8_t app_tws_get_codec_type(void);

uint8_t app_tws_get_tws_hfp_vol(void);

tws_dev_t* app_tws_get_env_pointer(void);

int tws_player_notify_key( unsigned short key,unsigned short event);

EXTERN_C void app_tws_buff_alloc(void);
EXTERN_C void app_tws_buff_free(void);
EXTERN_C uint8_t *app_tws_get_transmitter_buff(void);
a2dp_stream_t *app_tws_get_main_sink_stream(void);
#if defined(SLAVE_USE_ENC_SINGLE_SBC)
void app_tws_adjust_encode_bitpool(uint8_t updown);
void app_tws_reset_cadun_count(void);
void app_tws_kadun_count(void);
#endif

#ifdef __cplusplus
extern "C" {
#endif
	btif_cmgr_handler_t* app_tws_get_cmgr_handler(void);



#ifdef __cplusplus
}
#endif

#ifdef __TWS_CALL_DUAL_CHANNEL__
#ifdef __cplusplus
extern "C" {
#endif
void app_tws_set_pcm_wait_triggle(uint8_t triggle_en);

uint8_t app_tws_get_pcm_wait_triggle(void);

void app_tws_set_pcm_triggle(void);
void app_tws_set_trigger_time(uint32_t trigger_time, bool play_only);


void app_tws_set_btpcm_wait_triggle(uint8_t triggle_en);

uint8_t app_tws_get_btpcm_wait_triggle(void);
#ifdef __cplusplus
}
#endif
#endif

#endif

