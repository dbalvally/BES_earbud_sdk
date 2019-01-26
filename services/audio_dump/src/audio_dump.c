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
#include "hal_trace.h"
#include "audio_dump.h"
#include "string.h"

#ifdef AUDIO_DEBUG_V0_1_0

#ifdef AUDIO_DUMP
#define AUDIO_DUMP_HEAD_STR             ("[audio dump]")
#define AUDIO_DUMP_HEAD_LEN             (sizeof(AUDIO_DUMP_HEAD_STR)-1)
#define AUDIO_DUMP_DATA_LEN             (4)
#define AUDIO_DUMP_SAMPLE_BITS          (sizeof(short))
#define AUDIO_DUMP_MAX_FRAME_LEN        (512)
#define AUDIO_DUMP_MAX_CHANNEL_NUM      (8)
#define AUDIO_DUMP_MAX_DATA_SIZE        (AUDIO_DUMP_SAMPLE_BITS * AUDIO_DUMP_MAX_FRAME_LEN * AUDIO_DUMP_MAX_CHANNEL_NUM)
#define AUDIO_DUMP_BUFFER_SIZE          (AUDIO_DUMP_HEAD_LEN + AUDIO_DUMP_DATA_LEN + AUDIO_DUMP_MAX_DATA_SIZE)

static int audio_dump_frame_len = 256;
static int audio_dump_channel_num = 2;
static int audio_dump_data_size = 0;
static int audio_dump_buf_size = 0;
static short *audio_dump_data_ptr = NULL;
static char audio_dump_buf[AUDIO_DUMP_BUFFER_SIZE];
#endif

void audio_dump_clear_up(void)
{
#ifdef AUDIO_DUMP
    memset(audio_dump_data_ptr, 0, audio_dump_data_size);
#endif
}

void audio_dump_init(int frame_len, int channel_num)
{
#ifdef AUDIO_DUMP
    ASSERT(frame_len <= AUDIO_DUMP_MAX_FRAME_LEN, "[%s] frame_len(%d) is invalid", __func__, frame_len);
    ASSERT(channel_num <= AUDIO_DUMP_MAX_CHANNEL_NUM, "[%s] channel_num(%d) is invalid", __func__, channel_num);

    char *buf_ptr = audio_dump_buf;
    audio_dump_frame_len = frame_len;
    audio_dump_channel_num = channel_num;

    memcpy(buf_ptr, AUDIO_DUMP_HEAD_STR, AUDIO_DUMP_HEAD_LEN);

    buf_ptr += AUDIO_DUMP_HEAD_LEN;


    int *data_len = (int *)buf_ptr;
    *data_len = frame_len * channel_num * AUDIO_DUMP_SAMPLE_BITS;
    audio_dump_data_size = *data_len;
    audio_dump_buf_size = AUDIO_DUMP_HEAD_LEN + AUDIO_DUMP_DATA_LEN + audio_dump_data_size;

    buf_ptr += AUDIO_DUMP_DATA_LEN;

    audio_dump_data_ptr = (short *)buf_ptr;

    audio_dump_clear_up();
#endif
}

void audio_dump_add_channel_data(int channel_id, short *pcm_buf, int pcm_len)
{
#ifdef AUDIO_DUMP
    ASSERT(audio_dump_frame_len == pcm_len, "[%s] pcm_len(%d) is not valid", __func__, pcm_len);

    if (channel_id >= audio_dump_channel_num)
    {
        return;
    }

    for(int i=0; i<pcm_len; i++)
    {
        audio_dump_data_ptr[audio_dump_channel_num * i + channel_id] = pcm_buf[i];
    }
#endif
}

void audio_dump_run(void)
{
#ifdef AUDIO_DUMP
    AUDIO_DEBUG_DUMP((const unsigned char *)audio_dump_buf, audio_dump_buf_size);
#endif
}

#else       // AUDIO_DEBUG_V0_1_0

#ifdef AUDIO_DUMP
#define AUDIO_DUMP_MAX_FRAME_LEN      (512)
#define AUDIO_DUMP_MAX_CHANNEL_NUM    (8)
#define AUDIO_DUMP_BUFFER_SIZE        (AUDIO_DUMP_MAX_FRAME_LEN * AUDIO_DUMP_MAX_CHANNEL_NUM)

static int audio_dump_frame_len = 256;
static int audio_dump_channel_num = 2;
static short audio_dump_dump_buf[AUDIO_DUMP_BUFFER_SIZE];
#endif

void audio_dump_clear_up(void)
{
#ifdef AUDIO_DUMP
    memset(audio_dump_dump_buf, 0, sizeof(audio_dump_dump_buf));
#endif
}

void audio_dump_init(int frame_len, int channel_num)
{
#ifdef AUDIO_DUMP
    ASSERT(frame_len <= AUDIO_DUMP_MAX_FRAME_LEN, "[%s] frame_len(%d) is invalid", __func__, frame_len);
    ASSERT(channel_num <= AUDIO_DUMP_MAX_CHANNEL_NUM, "[%s] channel_num(%d) is invalid", __func__, channel_num);

    audio_dump_frame_len = frame_len;
    audio_dump_channel_num = channel_num;
    audio_dump_clear_up();
#endif
}

void audio_dump_add_channel_data(int channel_id, short *pcm_buf, int pcm_len)
{
#ifdef AUDIO_DUMP
    ASSERT(audio_dump_frame_len == pcm_len, "[%s] pcm_len(%d) is not valid", __func__, pcm_len);

    if (channel_id >= audio_dump_channel_num)
    {
        return;
    }

    for(int i=0; i<pcm_len; i++)
    {
        audio_dump_dump_buf[audio_dump_channel_num * i + channel_id] = pcm_buf[i];
    }
#endif
}

void audio_dump_run(void)
{
#ifdef AUDIO_DUMP
    AUDIO_DEBUG_DUMP((const unsigned char *)audio_dump_dump_buf, audio_dump_frame_len * audio_dump_channel_num * sizeof(short));
#endif
}
#endif