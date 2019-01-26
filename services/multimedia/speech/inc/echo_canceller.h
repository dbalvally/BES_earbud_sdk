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
#ifndef ECHO_CANCELLER_H
#define ECHO_CANCELLER_H

#include <stdint.h>

typedef struct
{
    int32_t bypass;
    int32_t hpf_enabled;
    int32_t af_enabled;
    int32_t nlp_enabled;
    int32_t ns_enabled;
    int32_t cng_enabled;
    // af config
    int32_t blocks;
    int32_t delay;
    // nlp config
    float   min_ovrd;
    float   target_supp;
    // ns config
    float   noise_supp;
    // cng config
    int32_t cng_type; // 0 - white noise, 1 - pink noise
    float   cng_level;
} Ec2FloatConfig;

struct Ec2FloatState_;

typedef struct Ec2FloatState_ Ec2FloatState;

Ec2FloatState *ec2float_create(int sample_rate, int frame_size, const Ec2FloatConfig *cfg);

int32_t ec2float_destroy(Ec2FloatState *st);

int32_t ec2float_set_config(Ec2FloatState *st, const Ec2FloatConfig *cfg);

int32_t ec2float_process(Ec2FloatState *st, int16_t *pcm_in, int16_t *pcm_ref, int32_t pcm_len, int16_t *pcm_out);

#endif