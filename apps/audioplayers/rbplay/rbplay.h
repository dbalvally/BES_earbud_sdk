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
/* rbplay header */
/* playback control & rockbox codec porting & codec thread */
#ifndef RBPLAY_HEADER
#define RBPLAY_HEADER

#ifdef __cplusplus
extern "C" {
#endif

/* ---- APIs ---- */
typedef enum _RB_CTRL_CMD_T {
    RB_CTRL_CMD_NONE = 0,
    RB_CTRL_CMD_SCAN_SONGS,
    RB_CTRL_CMD_CODEC_PARSE_FILE,
    RB_CTRL_CMD_CODEC_INIT,
    RB_CTRL_CMD_CODEC_RUN,
    RB_CTRL_CMD_CODEC_PAUSE,
    RB_CTRL_CMD_CODEC_STOP,
    RB_CTRL_CMD_CODEC_DEINIT,

    RB_CTRL_CMD_CODEC_SEEK, /* 5 */

    RB_CTRL_CMD_CODEC_MAX,
} RB_CTRL_CMD_T;

typedef enum _RB_ST_EVT_T {
    RB_ST_EVT_NONE = 0,
    RB_ST_EVT_CODEC_ERR,

    RB_ST_EVT_MAX,
} RB_ST_EVT_T;

typedef void (*RB_ST_Callback)(RB_ST_EVT_T evt, void *param);

void rb_play_init(RB_ST_Callback cb);
int rbcodec_init(void );

int app_rbplay_open(void );
int app_rbplay_sd_open(void );
int app_rbmodule_open(void);

int app_rbplay_usb_open(void );
bool rb_play_prepare_file(const char* file_path);
void rb_play_prepare_mem(const char* mem_address);
void rb_play_prepare_file_raw(const char* file_path, unsigned int len, int type);
void rb_play_prepare_mem_raw(const char *mem_address, unsigned int len, int type);
void rb_play_start(void);
void rb_play_pause(void);
void rb_play_stop(void);
void rb_play_deinit(void);
void rbplay_usbhost_event_put(void);
int rb_codec_running(void);
void app_rbplay_stop_audioplayer(void );

void rb_pcm_player_open_with_ci(void);

#ifdef __cplusplus
}
#endif

#endif /* RBPLAY_HEADER */
