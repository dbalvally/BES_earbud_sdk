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
/**
 * Default parameter
 * Note: These parameters can not be used as customization
 *       Set customized parameters in tgt_hardware.c
 */
#include "plat_types.h"

#if defined(SPEECH_TX_NS) || defined(SPEECH_RX_NS)
#include "speech_ns.h"
#endif

#if defined(SPEECH_TX_NS2) || defined(SPEECH_RX_NS2)
#include "lc_mmse_ns.h"
#endif

#if defined(SPEECH_TX_NS2FLOAT) || defined(SPEECH_RX_NS2FLOAT)
#include "lc_mmse_ns_float.h"
#endif

#if defined(SPEECH_TX_NS3) || defined(SPEECH_RX_NS3)
#include "ns3.h"
#endif

#if defined(SPEECH_TX_AGC) || defined(SPEECH_RX_AGC)
#include "agc.h"
#endif

#if defined(SPEECH_TX_EQ) || defined(SPEECH_RX_EQ)
#include "speech_eq.h"
#endif

#if defined(SPEECH_TX_POST_GAIN) || defined(SPEECH_RX_POST_GAIN)
#include "speech_gain.h"
#endif

/*--------------------TX configure--------------------*/
#if defined(SPEECH_TX_DC_FILTER)
#include "speech_dc_filter.h"
const SpeechDcFilterConfig WEAK speech_tx_dc_filter_cfg = 
{
    .bypass                 = 0,        // bypass enable or disable. (0/1)
};
#endif

#if defined(SPEECH_TX_AEC)
/****************************************************************************************************
 * Describtion:
 *     Acoustic Echo Cancellation
 * Parameters:
 *     bypass(0/1): bypass enable or disable.
 *     delay(>0): delay samples from mic to speak, unit(sample).
 *     leak_estimate(0~32767): echo supression value. This is a fixed mode. It has relatively large
 *         echo supression and large damage to speech signal.
 *     leak_estimate_shift(0~8): echo supression value. if leak_estimate == 0, then leak_estimate_shift
 *         can take effect. This is a adaptive mode. It has relatively small echo supression and also 
 *         small damage to speech signal.
 * Resource consumption:
 *     RAM:     None
 *     FLASE:   None
 *     MIPS:    fs = 16kHz, 40M;
 * Note:
 *     If possible, use SPEECH_TX_AEC2FLOAT
****************************************************************************************************/
#include "speech_aec.h"
const SpeechAecConfig WEAK speech_tx_aec_cfg = 
{
    .bypass                 = 0,
    .delay                  = 60,
    .leak_estimate          = 16383,
    .leak_estimate_shift    = 4,
};
#endif

#if defined(SPEECH_TX_AEC2)
/****************************************************************************************************
 * Describtion:
 *     Acoustic Echo Cancellation
 * Parameters:
 *     bypass(0/1): bypass enable or disable.
 *     .
 *     .
 *     .
 * Resource consumption:
 *     RAM:     None
 *     FLASE:   None
 *     MIPS:    fs = 16kHz, enNlpEnable = 1, 39M;
 * Note:
 *     If possible, use SPEECH_TX_AEC2FLOAT
****************************************************************************************************/
#include "speech_aec2.h"
const SpeechAec2Config WEAK speech_tx_aec2_cfg = 
{
    .bypass                 = 0,
    .enEAecEnable           = 1,
    .enHpfEnable            = 1,
    .enAfEnable             = 0,
    .enNsEnable             = 0,
    .enNlpEnable            = 1,
    .enCngEnable            = 0,
    .shwDelayLength         = 0,
    .shwNlpBandSortIdx      = 0.75f,
    .shwNlpBandSortIdxLow   = 0.5f,
    .shwNlpTargetSupp       = 3.0f,
    .shwNlpMinOvrd          = 1.0f,
};
#endif

#if defined(SPEECH_TX_AEC2FLOAT)
/****************************************************************************************************
 * Describtion:
 *     Acoustic Echo Cancellation
 * Parameters:
 *     bypass(0/1): bypass enable or disable.
 *     hpf_enabled(0/1): high pass filter enable or disable. Used to remove DC for Near and Ref signals.
 *     af_enabled(0/1): adaptive filter enable or disable. If the echo signal is very large, enable this
 *     nlp_enabled(0/1): non-linear process enable or disable. Enable this by default.
 *     ns_enabled(0/1): noise supression enable or disable. Enable this by default.
 *     cng_enabled(0/1):  comfort noise generator enable or disable.
 *     blocks(1~8): the length of adaptive filter = blocks * frame length
 *     delay(>0): delay samples from mic to speak, unit(sample).
 *     min_ovrd(1~6): supression factor, min_ovrd becomes larger and echo suppression becomes larger.
 *     target_supp(<0): target echo suppression, unit(dB)
 *     noise_supp(-30~0): noise suppression, unit(dB)
 *     cng_type(0/1): noise type(0: White noise; 1: Pink noise)
 *     cng_level(<0): noise gain, unit(dB)
 * Resource consumption:
 *     RAM:     None
 *     FLASE:   None
 *     MIPS:    fs = 16kHz;
 * Note:
 *     This is the recommended AEC
****************************************************************************************************/
#include "echo_canceller.h"
const Ec2FloatConfig WEAK speech_tx_aec2float_cfg = {
    .bypass         = 0,
    .hpf_enabled    = true,
    .af_enabled     = false,
    .nlp_enabled    = true,
    .ns_enabled     = false,
    .cng_enabled    = false,
    .blocks         = 1,
    .delay          = 0,
    .min_ovrd       = 2,
    .target_supp    = -40,
    .noise_supp     = -15,
    .cng_type       = 1,
    .cng_level      = -60,
};
#endif

#if defined(SPEECH_TX_2MIC_NS)
/****************************************************************************************************
 * Describtion:
 *     2 MICs Noise Suppression
 * Parameters:
 *     bypass(0/1): bypass enable or disable.
 * Resource consumption:
 *     RAM:     None
 *     FLASE:   None
 *     MIPS:    fs = 16kHz;
 * Note:
 *     None
****************************************************************************************************/
#include "dual_mic_denoise.h"
const DUAL_MIC_DENOISE_CFG_T WEAK speech_tx_2mic_ns_cfg =
{
    .bypass             = 0,
    .alpha_h            = 0.8,
    .alpha_slow         = 0.9,
    .alpha_fast         = 0.6,
    .thegma             = 0.01,
    .thre_corr          = 0.2,
    .thre_filter_diff   = 0.2,
    .cal_left_gain      = 1.0,
    .cal_right_gain     = 1.0,
    .delay_mono_sample  = 6,
};
#endif

#if defined(SPEECH_TX_2MIC_NS2)
/****************************************************************************************************
 * Describtion:
 *     2 MICs Noise Suppression
 * Parameters:
 *     bypass(0/1): bypass enable or disable.
 *     left_gain: invalid parameter.
 *     right_gain: invalid parameter.
 *     delay_taps(0~4): ceil{[d(MIC1~mouth) - d(MIC2~mouth)] / 2}.
 *         ceil: Returns the largest integer less than or equal to the specified data
 *         d(MIC~mouth): The dinstance from MIC to mouth
 *         e.g. 0: 0~2cm; 1: 2~4; 2: 5~6...
 *     freq_smooth_enable(1): Must enable
 *     wnr_enable(0/1): wind noise reduction enable or disable. This is also useful for improving
 *         noise suppression, but it also has some damage to speech signal. 
 * Resource consumption:
 *     RAM:     None
 *     FLASE:   None
 *     MIPS:    fs = 16kHz, 32M;
 * Note:
 *     None
****************************************************************************************************/
#include "coherent_denoise.h"
const CoherentDenoiseConfig WEAK speech_tx_2mic_ns2_cfg =
{
    .bypass             = 0,
    .left_gain          = 1.0f,
    .right_gain         = 1.0f,
    .delay_taps         = 1,  
    .freq_smooth_enable = 1,
    .wnr_enable         = 0, 
};
#endif

#if defined(SPEECH_TX_2MIC_NS4)
/****************************************************************************************************
 * Describtion:
 *     2 MICs Noise Suppression
 * Parameters:
 *     bypass(0/1): bypass enable or disable.
 * Resource consumption:
 *     RAM:     None
 *     FLASE:   None
 *     MIPS:    fs = 16kHz;
 * Note:
 *     None
****************************************************************************************************/
#include "sensormic_denoise.h"
const SensorMicDenoiseConfig WEAK speech_tx_2mic_ns4_cfg =
{
    .bypass             = 0,
    .left_gain          = 1.0f,
    .right_gain         = 1.0f,
    .delay_tapsM        = 1,
    .delay_tapsS        = 1,
    .delay_tapsC        = 32,
     //////////////////////////{{a0,a1,a2,a3,a4},{b0,b1,b2,b3,b4}}///////////////////////////
    .coefH[0]           = {1.0,-2.9085,3.2629,-1.6697,0.3295},
    .coefH[1]           = {0.5732,-2.2927,3.4390,-2.2927,0.5732},
    .coefL[0]           = {1.0,-2.9085,3.2629,-1.6697,0.3295},
    .coefL[1]           = {0.0009,0.0036,0.0054,0.0036,0.0009},
};
#endif

#if defined(SPEECH_TX_3MIC_NS)
/****************************************************************************************************
 * Describtion:
 *     3 MICs Noise Suppression
 * Parameters:
 *     bypass(0/1): bypass enable or disable.
 *     left_gain: invalid parameter.
 *     right_gain: invalid parameter.
 *     delay_tapsM(0~4): MIC L/R delay samples. Refer to SPEECH_TX_2MIC_NS2 delay_taps
 *     delay_tapsS(0~4): MIC L/S delay samples. Refer to SPEECH_TX_2MIC_NS2 delay_taps
 *     freq_smooth_enable(1): Must enable
 *     wnr_enable(0/1): wind noise reduction enable or disable. This is also useful for improving
 *         noise suppression, but it also has some damage to speech signal. 
 * Resource consumption:
 *     RAM:     None
 *     FLASE:   None
 *     MIPS:    fs = 16kHz;
 * Note:
 *     None
****************************************************************************************************/
#include "speech_3mic_ns.h"
const Speech3MicNsConfig WEAK speech_tx_3mic_ns_cfg =
{
    .bypass             = 0,
    .left_gain          = 1.0f,
    .right_gain         = 1.0f,
    .delay_tapsM        = 1,
    .delay_tapsS        = 4,
    .freq_smooth_enable = 1,
    .wnr_enable         = 0,
};
#endif

#if defined(SPEECH_TX_NS)
/****************************************************************************************************
 * Describtion:
 *     Noise Suppression
 * Parameters:
 *     bypass(0/1): bypass enable or disable.
 * Resource consumption:
 *     RAM:     None
 *     FLASE:   None
 *     MIPS:    fs = 16kHz, 30M;
 * Note:
 *     If possible, use SPEECH_TX_NS2 or SPEECH_TX_NS2FLOAT
****************************************************************************************************/
const SpeechNsConfig WEAK speech_tx_ns_cfg = {
    .bypass     = 0,
    .denoise_dB = -12,
};
#endif

#if defined(SPEECH_TX_NS2)
/****************************************************************************************************
 * Describtion:
 *     Noise Suppression
 * Parameters:
 *     bypass(0/1): bypass enable or disable.
 *     denoise_dB(-30~0): noise suppression, unit(dB). 
 *         e.g. -15: Can reduce 15dB of stationary noise.
 * Resource consumption:
 *     RAM:     fs = 16k:   RAM = 8k; 
 *              fs = 8k:    RAM = 4k;
 *              RAM = frame_size * 30
 *     FLASE:   None
 *     MIPS:    fs = 16kHz, 16M;
 * Note:
 *     None
****************************************************************************************************/
const SpeechNs2Config WEAK speech_tx_ns2_cfg = {
    .bypass     = 0,
    .denoise_dB = -15,
};
#endif

#if defined(SPEECH_TX_NS2FLOAT)
/****************************************************************************************************
 * Describtion:
 *     Noise Suppression
 * Parameters:
 *     bypass(0/1): bypass enable or disable.
 *     denoise_dB(-30~0): noise suppression, unit(dB). 
 *         e.g. -15: Can reduce 15dB of stationary noise.
 * Resource consumption:
 *     RAM:     None
 *     FLASE:   None
 *     MIPS:    fs = 16kHz;
 * Note:
 *     This is a 'float' version for SPEECH_TX_NS2. 
 *     It needs more MIPS and RAM, but can redece quantization noise.
****************************************************************************************************/
const SpeechNs2FloatConfig WEAK speech_tx_ns2float_cfg = {
    .bypass     = 0,
    .denoise_dB = -15,
};
#endif

#if defined(SPEECH_TX_NS3)
/****************************************************************************************************
 * Describtion:
 *     Noise Suppression
 * Parameters:
 *     bypass(0/1): bypass enable or disable.
 *     mode: None
 * Resource consumption:
 *     RAM:     None
 *     FLASE:   None
 *     MIPS:    fs = 16kHz;
 * Note:
 *     None
****************************************************************************************************/
const Ns3Config WEAK speech_tx_ns3_cfg = {
    .bypass = 0,
    .mode   = NS3_SUPPRESSION_HIGH,
};
#endif

#if defined(SPEECH_TX_NOISE_GATE)
/****************************************************************************************************
 * Describtion:
 *     Noise Gate
 * Parameters:
 *     bypass(0/1): bypass enable or disable.
 *     data_threshold(0~32767): distinguish between noise and speech, unit(sample).
 *     data_shift(1~3): 1: -6dB; 2: -12dB; 3: -18dB
 *     factor_up(float): attack time, unit(s)
 *     factor_down(float): release time, unit(s)
 * Resource consumption:
 *     RAM:     None
 *     FLASE:   None
 *     MIPS:    fs = 16kHz;
 * Note:
 *     None
****************************************************************************************************/
#include "speech_noise_gate.h"
const NoisegateConfig WEAK speech_tx_noise_gate_cfg = {
    .bypass         = 0,
    .data_threshold = 640,
    .data_shift     = 1,
    .factor_up      = 0.001f,
    .factor_down    = 0.5f,
};
#endif

#if defined(SPEECH_TX_COMPEXP)
/****************************************************************************************************
 * Describtion:
 *     Compressor and expander
 * Parameters:
 *     bypass(0/1): bypass enable or disable.
 *     ...
 * Resource consumption:
 *     RAM:     None
 *     FLASE:   None
 *     MIPS:    fs = 16kHz;
 * Note:
 *     None
****************************************************************************************************/
#include "compexp.h"
const CompexpConfig WEAK speech_tx_compexp_cfg = {
    .bypass             = 0,
    .comp_threshold     = -10.f,
    .comp_slope         = 0.5f,
    .expand_threshold   = -21.f,
    .expand_slope       = -0.5f,
    .attack_time        = 0.01f,
    .release_time       = 0.1f,
    .makeup_gain        = 6,
    .delay              = 128,
};
#endif

#if defined(SPEECH_TX_AGC)
/****************************************************************************************************
 * Describtion:
 *     Automatic Gain Control
 * Parameters:
 *     bypass(0/1): bypass enable or disable.
 *     target_level(>0): signal can not exceed (-1 * target_level)dB.
 *     compression_gain(>0): excepted gain.
 *     limiter_enable(0/1): 0: target_level does not take effect; 1: target_level takes effect.
 * Resource consumption:
 *     RAM:     None
 *     FLASE:   None
 *     MIPS:    fs = 16kHz, 3M;
 * Note:
 *     None
****************************************************************************************************/
const AgcConfig WEAK speech_tx_agc_cfg = {
    .bypass             = 0,
    .target_level       = 3,
    .compression_gain   = 6,
    .limiter_enable     = 1,
};
#endif

#if defined(SPEECH_TX_EQ)
/****************************************************************************************************
 * Describtion:
 *     Equalizer, implementation with 2 order iir filters
 * Parameters:
 *     bypass(0/1): bypass enable or disable.
 *     gain(float): normalized gain. It is usually less than or equal to 0
 *     num(0~6): the number of EQs
 *     params: {type, frequency, gain, q}. It supports a lot of types, please refer to iirfilt.h file
 * Resource consumption:
 *     RAM:     None
 *     FLASE:   None
 *     MIPS:    fs = 16kHz, 0.5M/section;
 * Note:
 *     None
****************************************************************************************************/
const EqConfig WEAK speech_tx_eq_cfg = {
    .bypass     = 0,
    .gain       = 0.f,
    .num        = 1,
    .params = {
        {IIR_BIQUARD_HPF, 60, 0, 0.707f},
    },
};
#endif

#if defined(SPEECH_TX_POST_GAIN)
/****************************************************************************************************
 * Describtion:
 *     Linear Gain
 * Parameters:
 *     bypass(0/1): bypass enable or disable.
 * Resource consumption:
 *     RAM:     None
 *     FLASE:   None
 *     MIPS:    fs = 16kHz;
 * Note:
 *     None
****************************************************************************************************/
const SpeechGainConfig WEAK speech_tx_post_gain_cfg = {
    .bypass     = 0,
    .gain_dB    = 6.0f,
};
#endif

/*--------------------RX configure--------------------*/

#if defined(SPEECH_RX_NS)
/****************************************************************************************************
 * Describtion:
 *     Acoustic Echo Cancellation
 * Parameters:
 *     Refer to SPEECH_TX_NS
 * Note:
 *     None
****************************************************************************************************/
const SpeechNsConfig WEAK speech_rx_ns_cfg = {
    .bypass     = 0,
    .denoise_dB = -12,
};
#endif

#if defined(SPEECH_RX_NS2)
/****************************************************************************************************
 * Describtion:
 *     Acoustic Echo Cancellation
 * Parameters:
 *     Refer to SPEECH_TX_NS2
 * Note:
 *     None
****************************************************************************************************/
const SpeechNs2Config WEAK speech_rx_ns2_cfg = {
    .bypass     = 0,
    .denoise_dB = -15,
};
#endif

#if defined(SPEECH_RX_NS2FLOAT)
/****************************************************************************************************
 * Describtion:
 *     Acoustic Echo Cancellation
 * Parameters:
 *     Refer to SPEECH_TX_NS2FLOAT
 * Note:
 *     None
****************************************************************************************************/
const SpeechNs2FloatConfig WEAK speech_rx_ns2float_cfg = {
    .bypass     = 0,
    .denoise_dB = -15,
};
#endif

#if defined(SPEECH_RX_NS3)
/****************************************************************************************************
 * Describtion:
 *     Acoustic Echo Cancellation
 * Parameters:
 *     Refer to SPEECH_TX_NS3
 * Note:
 *     None
****************************************************************************************************/
const Ns3Config WEAK speech_rx_ns3_cfg = {
    .bypass = 0,
    .mode   = NS3_SUPPRESSION_HIGH,
};
#endif

#if defined(SPEECH_RX_AGC)
/****************************************************************************************************
 * Describtion:
 *      Automatic Gain Control
 * Parameters:
 *     Refer to SPEECH_TX_AGC
 * Note:
 *     None
****************************************************************************************************/
const AgcConfig WEAK speech_rx_agc_cfg = {
    .bypass             = 0,
    .target_level       = 3,
    .compression_gain   = 6,
    .limiter_enable     = 1,
};
#endif

#if defined(SPEECH_RX_EQ)
/****************************************************************************************************
 * Describtion:
 *     Equalizer, implementation with 2 order iir filters
 * Parameters:
 *     Refer to SPEECH_TX_EQ
 * Note:
 *     None
****************************************************************************************************/
const EqConfig WEAK speech_rx_eq_cfg = {
    .bypass = 0,
    .gain   = 0.f,
    .num    = 1,
    .params = {
        {IIR_BIQUARD_HPF, 60, 0, 0.707f},
    },
};
#endif

#if defined(SPEECH_RX_POST_GAIN)
/****************************************************************************************************
 * Describtion:
 *     Linear Gain
 * Parameters:
 *     Refer to SPEECH_TX_POST_GAIN
 * Note:
 *     None
****************************************************************************************************/
const SpeechGainConfig WEAK speech_rx_post_gain_cfg = {
    .bypass     = 0,
    .gain_dB    = 6.0f,
};
#endif