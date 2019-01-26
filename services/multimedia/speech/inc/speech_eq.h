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
#ifndef SPEECH_EQ_H
#define SPEECH_EQ_H

#include "iirfilt.h"

// use more frame_size * sizeof(float) ram
#define EQ_USE_CMSIS_IIR

#define MAX_VQE_EQ_BAND 6

typedef struct
{
    enum IIR_BIQUARD_TYPE type;
    float f0;
    float gain;
    float q;
} BiquardParamDesign;

typedef struct
{
    enum IIR_BIQUARD_TYPE type;
    float a1;
    float a2;
    float b0;
    float b1;
    float b2;
} BiquardParamRaw;

typedef struct
{
	enum IIR_BIQUARD_TYPE type;
	float v0;
    float v1;
    float v2;
    float v3;
    float v4;
} BiquardParam;

typedef struct
{
    int32_t     bypass;    
    float       gain;
    int32_t     num;
	BiquardParam params[MAX_VQE_EQ_BAND];
} EqConfig;

typedef struct EqState_ EqState;

#ifdef __cplusplus
extern "C" {
#endif

EqState *eq_init(int32_t sample_rate, int32_t frame_size, const EqConfig *cfg);

int32_t eq_destroy(EqState *st);

int32_t eq_set_config(EqState *st, const EqConfig *cfg);

int32_t eq_process(EqState *st, int16_t *pcm_buf, int32_t pcm_len);

#ifdef __cplusplus
}
#endif

#endif