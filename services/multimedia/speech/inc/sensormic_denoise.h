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
#ifndef __SENSORMIC_DENOISE_H__
#define __SENSORMIC_DENOISE_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int32_t     bypass;
    float       left_gain;              // MIC Left增益补偿
    float       right_gain;             // MIC Right增益补偿
    int32_t     delay_tapsM;             // MIC L/R delay samples. 0: 适用于麦克距离为<2cm; 1: 适用于麦克距离为2cm左右; 2: 适用于麦克距离为4cm左右
    int32_t     delay_tapsS;
	int32_t     delay_tapsC;
	float       coefH[2][5];
	float       coefL[2][5];
} SensorMicDenoiseConfig;

struct SensorMicDenoiseState_;

typedef struct SensorMicDenoiseState_ SensorMicDenoiseState;

SensorMicDenoiseState *sensormic_denoise_create(int32_t sample_rate, int32_t frame_size, const SensorMicDenoiseConfig *cfg);

int32_t sensormic_denoise_destroy(SensorMicDenoiseState *st);

int32_t sensormic_denoise_set_config(SensorMicDenoiseState *st, const SensorMicDenoiseConfig *cfg);

int32_t sensormic_denoise_process(SensorMicDenoiseState *st, short *pcm_buf, int32_t pcm_len, short *out_buf);

#ifdef __cplusplus
}
#endif

#endif
