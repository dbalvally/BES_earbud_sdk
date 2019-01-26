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
/*******************************************************************************
** namer：	speech_eq
** description:	fir and iir eq manager
** version：V0.9
** author： Yunjie Huo
** modify：	2016.12.26
** todo: 	1.
*******************************************************************************/
#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_cmu.h"
#include "string.h"
#include "speech_eq.h"
#include "stdbool.h"
#include "hal_location.h"
#include "hw_codec_iir_process.h"
#include "tgt_hardware.h"


extern HW_CODEC_IIR_RAW_CFG_T  hw_codec_iir_adc;


int SRAM_TEXT_LOC speech_eq_run(uint8_t *buf, uint32_t len)
{

    return 0;
}

int speech_eq_open(enum AUD_SAMPRATE_T sample_rate, enum AUD_BITS_T sample_bits, enum AUD_CHANNEL_NUM_T ch_num,void *eq_buf, uint32_t len, HW_CODEC_IIR_TYPE_T hw_iir_type)
{
    TRACE("[%s] sample_rate = %d, sample_bits = %d, hw_iir_type = %d", __func__, sample_rate, sample_bits, hw_iir_type);

    HW_CODEC_IIR_CFG_T *hw_iir_cfg_adc = NULL;
    HW_CODEC_IIR_CFG_T *hw_iir_cfg_dac = NULL;
#ifdef __AUDIO_RESAMPLE__
    enum AUD_SAMPRATE_T sample_rate_hw_iir = AUD_SAMPRATE_50781;
#else
    enum AUD_SAMPRATE_T sample_rate_hw_iir = sample_rate;
#endif

    if(hw_iir_type == HW_CODEC_IIR_ADC)
    {
        hw_codec_iir_open(sample_rate_hw_iir,HW_CODEC_IIR_ADC);

        hw_iir_cfg_adc = hw_codec_iir_get_cfg(sample_rate_hw_iir, HW_CODEC_IIR_ADC, 0);
        ASSERT(hw_iir_cfg_adc != NULL, "[%s] %d codec IIR parameter error!", __func__, hw_iir_cfg_adc);
        hw_codec_iir_set_cfg(hw_iir_cfg_adc, sample_rate_hw_iir, HW_CODEC_IIR_ADC);
        hw_codec_iir_set_coefs(&hw_codec_iir_adc, HW_CODEC_IIR_ADC);
    }
    else
    {
        hw_codec_iir_open(sample_rate_hw_iir,HW_CODEC_IIR_DAC);

        hw_iir_cfg_dac = hw_codec_iir_get_cfg(sample_rate_hw_iir, HW_CODEC_IIR_DAC, 0);
        ASSERT(hw_iir_cfg_dac != NULL, "[%s] %d codec IIR parameter error!", __func__, hw_iir_cfg_dac);

        hw_codec_iir_set_cfg(hw_iir_cfg_dac, sample_rate_hw_iir, HW_CODEC_IIR_DAC);
    }

	return 0;
}

int speech_eq_close(HW_CODEC_IIR_TYPE_T hw_iir_type)
{

    if(hw_iir_type == HW_CODEC_IIR_ADC)
        hw_codec_iir_close(HW_CODEC_IIR_ADC);
    else
        hw_codec_iir_close(HW_CODEC_IIR_DAC);

    return 0;
}

int speech_eq_init(void)
{
    TRACE("[%s]", __func__);

    return 0;
}
