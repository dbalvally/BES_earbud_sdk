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
#ifndef __SPEECH_EQ_H__
#define __SPEECH_EQ_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"
#include "iir_process.h"
#include "hw_codec_iir_process.h"

int speech_eq_init(void);
int speech_eq_open(enum AUD_SAMPRATE_T sample_rate, enum AUD_BITS_T sample_bits,enum AUD_CHANNEL_NUM_T ch_num, void *eq_buf, uint32_t len, HW_CODEC_IIR_TYPE_T hw_iir_type);
int speech_eq_run(uint8_t *buf, uint32_t len);
int speech_eq_close(HW_CODEC_IIR_TYPE_T hw_iir_type);


#ifdef __cplusplus
}
#endif

#endif
