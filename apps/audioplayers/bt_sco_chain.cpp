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
// Speech process macro change table
// TX: Transimt process
// RX: Receive process
// 16k: base 18M
|-------|--------------------|-----------------------------------|----------------------------------|-----------|----------------------|
| TX/RX |        New         |                Old                |           description            |  MIPS(M)  |         Note         |
|       |                    |                                   |                                  | 16k    8k |                      |
|-------|--------------------|-----------------------------------|----------------------------------|-----------|----------------------|
|       | SPEECH_TX_AEC      | SPEECH_ECHO_CANCEL                | Acoustic Echo Cancellation(old)  | 40     \  | HWFFT: 37            |
|       | SPEECH_TX_AEC2     | SPEECH_AEC_FIX                    | Acoustic Echo Cancellation(new)  | 39     \  | enable NLP           |
|       | SPEECH_TX_NS       | SPEECH_NOISE_SUPPRESS             | 1 mic noise suppress(old)        | 30     \  | HWFFT: 19            |
|       | SPEECH_TX_NS2      | LC_MMSE_NOISE_SUPPRESS            | 1 mic noise suppress(new)        | 16     \  | HWFFT: 12            |
| TX    | SPEECH_TX_2MIC_NS  | DUAL_MIC_DENOISE                  | 2 mic noise suppres(long space)  | \         |                      |
|       | SPEECH_TX_2MIC_NS2 | COHERENT_DENOISE                  | 2 mic noise suppres(short space) | 32        |                      |
|       | SPEECH_TX_2MIC_NS3 | FAR_FIELD_SPEECH_ENHANCEMENT      | 2 mic noise suppres(far field)   | \         |                      |
|       | SPEECH_TX_AGC      | SPEECH_AGC                        | Automatic Gain Control           | 3         |                      |
|       | SPEECH_TX_EQ       | SPEECH_WEIGHTING_FILTER_SUPPRESS  | Default EQ                       | 0.5     \ | each section         |
|-------|--------------------|-----------------------------------|----------------------------------|-----------|----------------------|
|       | SPEECH_RX_PLC      | SPEECH_PACKET_LOSS_CONCEALMENT    | packet loss concealment          | 17      \ | Include base process |
|       | SPEECH_RX_NS       | SPEAKER_NOISE_SUPPRESS            | 1 mic noise suppress(old)        | 30      \ |                      |
| RX    | SPEECH_RX_NS2      | LC_MMSE_NOISE_SUPPRESS_SPK        | 1 mic noise suppress(new)        | 16      \ |                      |
|       | SPEECH_RX_AGC      | SPEECH_AGC_SPK                    | Automatic Gain Control           | 3         |                      |
|       | SPEECH_RX_EQ       | SPEAKER_WEIGHTING_FILTER_SUPPRESS | Default EQ                       | 0.5     \ | each section         |
|-------|--------------------|-----------------------------------|----------------------------------|-----------|----------------------|
**/

#include "plat_types.h"
#include "hal_trace.h"
#include "hal_timer.h"
#include "audio_dump.h"
#include "app_audio.h"
#include "ae_math.h"

//#define BT_SCO_CHAIN_PROFILE

extern "C" {
#include "speech_utils.h"
#include "speech_ssat.h"
#include "speech_memory.h"
#include "iir_resample.h"

#if SPEECH_PROCESS_FRAME_MS != 15
#include "frame_resize.h"
FrameResizeState *speech_frame_resize_st = NULL;
#endif

short *aec_echo_buf = NULL;

#if defined(SPEECH_TX_AEC) || defined(SPEECH_TX_AEC2) || defined(SPEECH_TX_AEC2FLOAT)
// Use to free buffer
static short *aec_echo_buf_ptr;
static short *aec_out_buf;
#endif

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

/*--------------------TX state--------------------*/
#if defined(SPEECH_TX_DC_FILTER)
#include "speech_dc_filter.h"
SpeechDcFilterState *speech_tx_dc_filter_st = NULL;
extern const SpeechDcFilterConfig speech_tx_dc_filter_cfg;
#endif

// AEC
#if defined(SPEECH_TX_AEC)
#include "speech_aec.h"
SpeechAecState *speech_tx_aec_st = NULL;
void *speech_tx_aec_lib_st = NULL;
extern const SpeechAecConfig speech_tx_aec_cfg;
#endif

#if defined(SPEECH_TX_AEC2)
#include "speech_aec2.h"
SpeechAec2State *speech_tx_aec2_st = NULL;
extern const SpeechAec2Config speech_tx_aec2_cfg;
#endif

#if defined(SPEECH_TX_AEC2FLOAT)
#include "echo_canceller.h"
static Ec2FloatState *speech_tx_aec2float_st = NULL;
extern const Ec2FloatConfig speech_tx_aec2float_cfg;
#endif

// 2MIC NS
#if defined(SPEECH_TX_2MIC_NS)
#include "dual_mic_denoise.h"
extern const DUAL_MIC_DENOISE_CFG_T speech_tx_2mic_ns_cfg;
// float mic2_ft_caliration[] = {0.395000,0.809000,0.939000,1.748000,1.904000,1.957000,1.944000,1.906000,1.935000,1.940000,1.937000,1.931000,1.929000,1.911000,1.887000,1.871000,1.846000,1.779000,1.748000,2.086000,2.055000,2.002000,1.903000,1.885000,1.854000,1.848000,1.848000,1.844000,1.852000,1.870000,1.866000,1.843000,1.838000,1.824000,1.861000,1.871000,1.866000,1.833000,1.800000,1.769000,1.749000,1.690000,1.664000,1.573000,1.602000,1.692000,1.759000,1.758000,1.698000,1.628000,1.525000,1.509000,1.492000,1.515000,1.530000,1.644000,1.653000,1.617000,1.667000,1.746000,1.663000,1.606000,1.560000,1.500000,1.579000,1.632000,1.623000,1.549000,1.524000,1.512000,1.493000,1.476000,1.421000,1.396000,1.386000,1.459000,1.463000,1.496000,1.568000,1.544000,1.555000,1.547000,1.619000,1.630000,1.574000,1.491000,1.414000,1.383000,1.352000,1.464000,1.474000,1.450000,1.419000,1.425000,1.411000,1.479000,1.517000,1.486000,1.428000,1.389000,1.330000,1.284000,1.267000,1.249000,1.256000,1.215000,1.250000,1.402000,1.386000,1.334000,1.287000,1.329000,1.337000,1.292000,1.226000,1.212000,1.142000,1.087000,1.086000,1.112000,1.145000,1.194000,1.163000,1.131000,1.162000,1.166000,1.259000,1.218000,1.218000,1.322000,1.347000,1.436000,1.890000,1.693000,1.591000,1.518000,1.422000,1.345000,1.331000,1.308000,1.330000,1.305000,1.218000,1.286000,1.340000,1.399000,1.406000,1.353000,1.280000,1.246000,1.185000,1.129000,1.014000,0.985000,0.981000,1.189000,1.533000,1.694000,1.613000,1.464000,1.419000,1.448000,1.449000,1.442000,1.367000,1.283000,1.232000,1.381000,1.484000,1.497000,1.554000,1.438000,1.365000,1.326000,1.332000,1.335000,1.367000,1.301000,1.288000,1.168000,1.103000,1.067000,1.026000,1.076000,1.126000,1.068000,1.045000,0.978000,0.926000,0.939000,0.854000,0.772000,0.902000,0.742000,1.073000,1.220000,1.177000,1.762000,1.573000,1.390000,1.406000,1.148000,1.054000,1.210000,1.344000,1.849000,2.078000,1.756000,1.646000,1.597000,1.447000,1.322000,1.279000,1.007000,0.921000,0.871000,0.864000,1.067000,1.129000,1.128000,1.027000,0.983000,0.889000,0.774000,0.759000,0.724000,0.949000,1.237000,1.499000,1.658000,1.837000,1.492000,1.452000,1.151000,1.100000,0.996000,0.986000,1.023000,1.071000,1.252000,1.295000,1.309000,1.343000,1.220000,1.161000,1.142000,1.041000,0.974000,0.885000,0.799000,0.669000,0.732000,0.953000,0.861000,0.881000,0.988000,0.891000};
#endif

#if defined(SPEECH_TX_2MIC_NS2)
#include "coherent_denoise.h"
CoherentDenoiseState *speech_tx_2mic_ns2_st = NULL;
extern const CoherentDenoiseConfig speech_tx_2mic_ns2_cfg;
#endif

#if defined(SPEECH_TX_2MIC_NS3)
#include "far_field_speech_enhancement.h"
#endif

#if defined(SPEECH_TX_2MIC_NS4)
#include "sensormic_denoise.h"
SensorMicDenoiseState *speech_tx_2mic_ns4_st = NULL;
extern const SensorMicDenoiseConfig speech_tx_2mic_ns4_cfg;
#endif

#if defined(SPEECH_TX_3MIC_NS)
#include "speech_3mic_ns.h"
Speech3MicNsState *speech_tx_3mic_ns_st = NULL;
extern const Speech3MicNsConfig speech_tx_3mic_ns_cfg;
#endif

// NS
#if defined(SPEECH_TX_NS)
SpeechNsState *speech_tx_ns_st = NULL;
extern const SpeechNsConfig speech_tx_ns_cfg;
#endif

#if defined(SPEECH_TX_NS2)
SpeechNs2State *speech_tx_ns2_st = NULL;
extern const SpeechNs2Config speech_tx_ns2_cfg;
#endif

#if defined(SPEECH_TX_NS2FLOAT)
SpeechNs2FloatState *speech_tx_ns2float_st = NULL;
extern const SpeechNs2FloatConfig speech_tx_ns2float_cfg;
#endif

#if defined(SPEECH_TX_NS3)
static Ns3State *speech_tx_ns3_st = NULL;
extern const Ns3Config speech_tx_ns3_cfg;
#endif

#if defined(SPEECH_TX_NOISE_GATE)
#include "speech_noise_gate.h"
static NoisegateState *speech_tx_noise_gate_st = NULL;
extern const NoisegateConfig speech_tx_noise_gate_cfg;
#endif

// Gain
#if defined(SPEECH_TX_COMPEXP)
#include "compexp.h"
static CompexpState *speech_tx_compexp_st = NULL;
extern const CompexpConfig speech_tx_compexp_cfg;
#endif

#if defined(SPEECH_TX_AGC)
static AgcState *speech_tx_agc_st = NULL;
extern const AgcConfig speech_tx_agc_cfg;
#endif

// EQ
#if defined(SPEECH_TX_EQ)
EqState *speech_tx_eq_st = NULL;
extern const EqConfig speech_tx_eq_cfg;
#endif

// GAIN
#if defined(SPEECH_TX_POST_GAIN)
SpeechGainState *speech_tx_post_gain_st = NULL;
extern const SpeechGainConfig speech_tx_post_gain_cfg;
#endif

/*--------------------RX state--------------------*/
// NS
#if defined(SPEECH_RX_NS)
SpeechNsState *speech_rx_ns_st = NULL;
extern const SpeechNsConfig speech_rx_ns_cfg;
#endif

#if defined(SPEECH_RX_NS2)
SpeechNs2State *speech_rx_ns2_st = NULL;
extern const SpeechNs2Config speech_rx_ns2_cfg;
#endif

#if defined(SPEECH_RX_NS2FLOAT)
SpeechNs2FloatState *speech_rx_ns2f_st = NULL;
extern const SpeechNs2FloatConfig speech_rx_ns2float_cfg;
#endif

#if defined(SPEECH_RX_NS3)
static Ns3State *speech_rx_ns3_st = NULL;
extern const Ns3Config speech_rx_ns3_cfg;
#endif

// GAIN
#if defined(SPEECH_RX_AGC)
static AgcState *speech_rx_agc_st = NULL;
extern const AgcConfig speech_rx_agc_cfg;
#endif

// EQ
#if defined(SPEECH_RX_EQ)
EqState *speech_rx_eq_st = NULL;
extern const EqConfig speech_rx_eq_cfg;
#endif

// GAIN
#if defined(SPEECH_RX_POST_GAIN)
SpeechGainState *speech_rx_post_gain_st = NULL;
extern const SpeechGainConfig speech_rx_post_gain_cfg;
#endif
}

static int speech_tx_sample_rate = 16000;
static int speech_rx_sample_rate = 16000;
static int speech_tx_frame_len = 256;
static int speech_rx_frame_len = 256;

static int32_t _speech_tx_process_(int16_t *pcm_buf, int16_t *ref_buf, int32_t *pcm_len);
static int32_t _speech_rx_process_(int16_t *pcm_buf, int32_t *pcm_len);

void *speech_get_ext_buff(int size)
{
    void *pBuff = NULL;
    if (size % 4)
    {
        size = size + (4 - size % 4);
    }

    pBuff = speech_calloc(size, sizeof(uint8_t));
    LOG_PRINT_VOICE_CALL_AUD("[%s] len:%d", __func__, size);

    return pBuff;
}

static short Coefs_2mic[2] = { 5243, 1392 };

#if defined(SPEECH_TX_2MIC_NS)
static int filterState[2] = { 0, 0 };
#if defined(SPEECH_TX_AEC) || defined(SPEECH_TX_AEC2) || defined(SPEECH_TX_AEC2FLOAT)
static int filterState1[2] = { 0, 0 };
#endif
#endif
void LowPassFilter(short* signal_in, short* signal_out, int* filter_state, int in_length)
{
    short tmp16_1 = 0, tmp16_2 = 0;
    int tmp32_1 = filter_state[0];
    int tmp32_2 = filter_state[1];
    int n = 0;
    int half_length = (in_length >> 1);  // Downsampling by 2 gives half length.

    for (n = 0; n < half_length; n++)
    {
        tmp16_1 = (short)((tmp32_1 >> 1) + (((int)Coefs_2mic[0] * (*signal_in)) >> 14));
        tmp32_1 = (int)(*signal_in++) - (((int)Coefs_2mic[0] * tmp16_1) >> 12);
        *signal_out = tmp16_1;
        tmp16_2 = (short)((tmp32_2 >> 1) + (((int)Coefs_2mic[1] * (*signal_in) >> 14)));
        tmp32_2 = (int)(*signal_in++) - (((int)Coefs_2mic[1] * tmp16_2) >> 12);
        *signal_out++ += tmp16_2;
    }
    // Store the filter states.
    filter_state[0] = tmp32_1;
    filter_state[1] = tmp32_2;
}

// Speech Tuning
#ifdef AUDIO_DEBUG_V0_1_0
typedef struct {
#if defined(SPEECH_TX_DC_FILTER)
    SpeechDcFilterConfig    tx_dc_filter;
#endif
#if defined(SPEECH_TX_AEC)
    SpeechAecConfig         tx_aec;
#endif
#if defined(SPEECH_TX_AEC2)
    SpeechAec2Config        tx_aec2;
#endif
#if defined(SPEECH_TX_AEC2FLOAT)
    Ec2FloatConfig          tx_aec2float;
#endif
#if defined(SPEECH_TX_2MIC_NS2)
    CoherentDenoiseConfig   tx_2mic_ns2;
#endif
#if defined(SPEECH_TX_2MIC_NS4)
    SensorMicDenoiseConfig   tx_2mic_ns4;
#endif
#if defined(SPEECH_TX_3MIC_NS)
    Speech3MicNsConfig   tx_3mic_ns;
#endif
#if defined(SPEECH_TX_NS)
    SpeechNsConfig          tx_ns;
#endif
#if defined(SPEECH_TX_NS2)
    SpeechNs2Config         tx_ns2;
#endif
#if defined(SPEECH_TX_NS2FLOAT)
    SpeechNs2FloatConfig    tx_ns2float;
#endif
#if defined(SPEECH_TX_NS3)
    Ns3Config               tx_ns3;
#endif
#if defined(SPEECH_TX_NOISE_GATE)
    NoisegateConfig         tx_noise_gate;
#endif
#if defined(SPEECH_TX_COMPEXP)
    CompexpConfig           tx_compexp;
#endif
#if defined(SPEECH_TX_AGC)
    AgcConfig               tx_agc;
#endif
#if defined(SPEECH_TX_EQ)
    EqConfig                tx_eq;
#endif
#if defined(SPEECH_TX_POST_GAIN)
    SpeechGainConfig        tx_post_gain;
#endif
#if defined(SPEECH_RX_NS)
    SpeechNsConfig          rx_ns;
#endif
#if defined(SPEECH_RX_NS2)
    SpeechNs2Config         rx_ns2;
#endif
#if defined(SPEECH_RX_NS2FLOAT)
    SpeechNs2FloatConfig    rx_ns2float;
#endif
#if defined(SPEECH_RX_NS3)
    Ns3Config               rx_ns3;
#endif
#if defined(SPEECH_RX_NOISE_GATE)
    NoisegateConfig         rx_noise_gate;
#endif
#if defined(SPEECH_RX_COMPEXP)
    CompexpConfig           rx_compexp;
#endif
#if defined(SPEECH_RX_AGC)
    AgcConfig               rx_agc;
#endif
#if defined(SPEECH_RX_EQ)
    EqConfig                rx_eq;
#endif
#if defined(SPEECH_RX_POST_GAIN)
    SpeechGainConfig        rx_post_gain;
#endif
    // Add more process
} SpeechConfig;

static SpeechConfig *speech_cfg = NULL;
static bool speech_tuning_status = false;

int speech_get_config_size(void)
{
    return sizeof(SpeechConfig);
}

int speech_set_config(SpeechConfig *cfg)
{
#if defined(SPEECH_TX_DC_FILTER)
    speech_dc_filter_set_config(speech_tx_dc_filter_st, &cfg->tx_dc_filter);
#endif
#if defined(SPEECH_TX_AEC)
    speech_aec_set_config(speech_tx_aec_st, &cfg->tx_aec);
#endif
#if defined(SPEECH_TX_AEC2)
    speech_aec2_set_config(speech_tx_aec2_st, &cfg->tx_aec2);
#endif
#if defined(SPEECH_TX_AEC2FLOAT)
    ec2float_set_config(speech_tx_aec2float_st, &cfg->tx_aec2float);
#endif
#if defined(SPEECH_TX_2MIC_NS2)
    coheren_denoise_set_config(speech_tx_2mic_ns2_st, &cfg->tx_2mic_ns2);
#endif
#if defined(SPEECH_TX_2MIC_NS4)
    sensormic_denoise_set_config(speech_tx_2mic_ns4_st, &cfg->tx_2mic_ns4);
#endif
#if defined(SPEECH_TX_3MIC_NS)
    speech_3mic_ns_set_config(speech_tx_3mic_ns_st, &cfg->tx_3mic_ns);
#endif
#if defined(SPEECH_TX_NS)
    speech_ns_set_config(speech_tx_ns_st, &cfg->tx_ns);
#endif
#if defined(SPEECH_TX_NS2)
    speech_ns2_set_config(speech_tx_ns2_st, &cfg->tx_ns2);
#endif
#if defined(SPEECH_TX_NS2FLOAT)
    speech_ns2float_set_config(speech_tx_ns2float_st, &cfg->tx_ns2float);
#endif
#if defined(SPEECH_TX_NS3)
    ns3_set_config(speech_tx_ns3_st, &cfg->tx_ns3);
#endif
#if defined(SPEECH_TX_NOISE_GATE)
    speech_noise_gate_set_config(speech_tx_noise_gate_st, &cfg->tx_noise_gate);
#endif
#if defined(SPEECH_TX_COMPEXP)
    compexp_set_config(speech_tx_compexp_st, &cfg->tx_compexp);
#endif
#if defined(SPEECH_TX_AGC)
    agc_set_config(speech_tx_agc_st, &cfg->tx_agc);
#endif
#if defined(SPEECH_TX_EQ)
    eq_set_config(speech_tx_eq_st, &cfg->tx_eq);
#endif
#if defined(SPEECH_TX_POST_GAIN)
    speech_gain_set_config(speech_tx_post_gain_st, &cfg->tx_post_gain);
#endif
#if defined(SPEECH_RX_NS)
    speech_ns_set_config(speech_rx_ns_st, &cfg->rx_ns);
#endif
#if defined(SPEECH_RX_NS2)
    speech_ns2_set_config(speech_rx_ns2_st, &cfg->rx_ns2);
#endif
#if defined(SPEECH_RX_NS2FLOAT)
    speech_ns2float_set_config(speech_rx_ns2float_st, &cfg->rx_ns2float);
#endif
#if defined(SPEECH_RX_NS3)
    ns3_set_config(speech_rx_ns3_st, &cfg->rx_ns3);
#endif
#if defined(SPEECH_RX_NOISE_GATE)
    speech_noise_gate_set_config(speech_rx_noise_gate_st, &cfg->rx_noise_gate);
#endif
#if defined(SPEECH_RX_COMPEXP)
    compexp_set_config(speech_rx_compexp_st, &cfg->rx_compexp);
#endif
#if defined(SPEECH_RX_AGC)
    agc_set_config(speech_rx_agc_st, &cfg->rx_agc);
#endif
#if defined(SPEECH_RX_EQ)
    eq_set_config(speech_rx_eq_st, &cfg->rx_eq);
#endif
#if defined(SPEECH_RX_POST_GAIN)
    speech_gain_set_config(speech_rx_post_gain_st, &cfg->rx_post_gain);
#endif
    // Add more process

    return 0;
}

int speech_tuning_set_status(bool en)
{
    speech_tuning_status = en;

    return 0;
}

bool speech_tuning_get_status(void)
{
    return speech_tuning_status;
}

uint32_t speech_tuning_rx_callback(unsigned char *buf, uint32_t len)
{
    uint32_t res = 0;

    // Check valid
    int config_size = speech_get_config_size(); 
    if (config_size != len)
    {
        LOG_PRINT_VOICE_CALL_AUD("[speech tuning] len(%d) != config_size(%d)", len, config_size);
        res = 1;

        LOG_PRINT_VOICE_CALL_AUD("[Speech Tuning] res : 1; info : len(%d) != config_size(%d);",len, config_size);
    }
    else
    {
        SpeechConfig *cfg = (SpeechConfig *)buf;

        // Save cfg
        *speech_cfg = *cfg;

        // Test parameters
#if defined(SPEECH_TX_2MIC_NS2)
        LOG_PRINT_VOICE_CALL_AUD("[speech tuning] delay_taps: %d", cfg->tx_2mic_ns2.delay_taps);
#endif
#if defined(SPEECH_TX_NOISE_GATE)
        LOG_PRINT_VOICE_CALL_AUD("[speech tuning] data_threshold: %d", cfg->tx_noise_gate.data_threshold);
#endif
#if defined(SPEECH_TX_EQ)
        LOG_PRINT_VOICE_CALL_AUD("[speech tuning] eq num: %d", cfg->tx_eq.num);
        // LOG_PRINT_VOICE_CALL_AUD("[speech tuning] eq[0] type: %d", cfg->tx_eq.params[0].type);
        // LOG_PRINT_VOICE_CALL_AUD("[speech tuning] eq[0] v1: %d", int(cfg->tx_eq.params[0].v1 * 100));
        // LOG_PRINT_VOICE_CALL_AUD("[speech tuning] eq[0] v3: %d", int(cfg->tx_eq.params[0].v3 * 100));
        // LOG_PRINT_VOICE_CALL_AUD("[speech tuning] eq[1] v2: %d", int(cfg->tx_eq.params[1].v2 * 100));
        // LOG_PRINT_VOICE_CALL_AUD("[speech tuning] eq[1] v4: %d", int(cfg->tx_eq.params[1].v4 * 100));
#endif
#if defined(SPEECH_RX_EQ)
        LOG_PRINT_VOICE_CALL_AUD("[speech tuning] RX: eq num: %d", cfg->rx_eq.num);
#endif
        // Set status
        speech_tuning_set_status(true);

        LOG_PRINT_VOICE_CALL_AUD("[speech tuning] res: OK");

        LOG_PRINT_VOICE_CALL_AUD("[Speech Tuning] res : 0;");
    }

    return res;
}

int speech_tuning_init(void)
{
    speech_cfg = (SpeechConfig *)speech_calloc(1, sizeof(SpeechConfig));

#ifdef HAL_TRACE_RX_ENABLE
    hal_trace_rx_register("Speech Tuning", (HAL_TRACE_RX_CALLBACK_T)speech_tuning_rx_callback);
#endif

    speech_tuning_set_status(false);

    return 0;
}

int speech_tuning_deinit(void)
{
    speech_tuning_set_status(false);

    speech_free(speech_cfg);

    return 0;
}

int speech_tuning_set_config(SpeechConfig *cfg)
{
    speech_set_config(cfg);
    speech_tuning_set_status(false);

    return 0;
}
#endif

int speech_tx_init(int sample_rate, int frame_len)
{
    LOG_PRINT_VOICE_CALL_AUD("[%s] Start...", __func__);

#if defined(SPEECH_TX_DC_FILTER)
    int channel_num = SPEECH_CODEC_CAPTURE_CHANNEL_NUM;
    int data_separation = 0;

    speech_tx_dc_filter_st = speech_dc_filter_create(sample_rate, frame_len, &speech_tx_dc_filter_cfg);
    speech_dc_filter_ctl(speech_tx_dc_filter_st, SPEECH_DC_FILTER_SET_CHANNEL_NUM, &channel_num);
    speech_dc_filter_ctl(speech_tx_dc_filter_st, SPEECH_DC_FILTER_SET_DATA_SEPARATION, &data_separation);
#endif

#if defined(SPEECH_TX_AEC) || defined(SPEECH_TX_AEC2) || defined(SPEECH_TX_AEC2FLOAT)
// #if !(defined(__AUDIO_RESAMPLE__) && defined(SW_SCO_RESAMPLE))
    aec_out_buf = (short *)speech_calloc(frame_len, sizeof(short));
    aec_echo_buf = (short *)speech_calloc(frame_len, sizeof(short));
    aec_echo_buf_ptr = aec_echo_buf;
// #endif
#endif

#if defined(SPEECH_TX_AEC)
    speech_tx_aec_st = speech_aec_create(sample_rate, frame_len, &speech_tx_aec_cfg);
    speech_aec_ctl(speech_tx_aec_st, SPEECH_AEC_GET_LIB_ST, &speech_tx_aec_lib_st);
#endif

#if defined(SPEECH_TX_AEC2)
    speech_tx_aec2_st = speech_aec2_create(sample_rate, frame_len, &speech_tx_aec2_cfg);
#endif

#if defined(SPEECH_TX_AEC2FLOAT)
    speech_tx_aec2float_st = ec2float_create(sample_rate, frame_len, &speech_tx_aec2float_cfg);
#endif

#if defined(SPEECH_TX_2MIC_NS)
    dual_mic_denoise_init(NULL, &speech_tx_2mic_ns_cfg);
    // dual_mic_denoise_ctl(DUAL_MIC_DENOISE_SET_CALIBRATION_COEF, mic2_ft_caliration);
#endif

#if defined(SPEECH_TX_2MIC_NS2)
    speech_tx_2mic_ns2_st = coherent_denoise_create(sample_rate, 64, &speech_tx_2mic_ns2_cfg);
#endif

#if defined(SPEECH_TX_2MIC_NS3)
    far_field_speech_enhancement_init();
    far_field_speech_enhancement_start();
#endif

#if defined(SPEECH_TX_2MIC_NS4)
    speech_tx_2mic_ns4_st = sensormic_denoise_create(sample_rate, 64, &speech_tx_2mic_ns4_cfg);
#endif

#if defined(SPEECH_TX_3MIC_NS)
    speech_tx_3mic_ns_st = speech_3mic_ns_create(sample_rate, 64, &speech_tx_3mic_ns_cfg);
#endif

#if defined(SPEECH_TX_NS)
    speech_tx_ns_st = speech_ns_create(sample_rate, frame_len, &speech_tx_ns_cfg);
#if defined(SPEECH_TX_AEC)
    speech_ns_ctl(speech_tx_ns_st, SPEECH_NS_SET_AEC_STATE, speech_tx_aec_lib_st);
    int32_t echo_supress = -39;
    speech_ns_ctl(speech_tx_ns_st, SPEECH_NS_SET_AEC_SUPPRESS, &echo_supress);
#endif
#endif

#if defined(SPEECH_TX_NS2)
    speech_tx_ns2_st = speech_ns2_create(sample_rate, frame_len, &speech_tx_ns2_cfg);
#if defined(SPEECH_TX_AEC)
    speech_ns2_set_echo_state(speech_tx_ns2_st, speech_tx_aec_lib_st);
    speech_ns2_set_echo_suppress(speech_tx_ns2_st, -40);
#endif
#endif

#if defined(SPEECH_TX_NS2FLOAT)
    speech_tx_ns2float_st = speech_ns2float_create(sample_rate, frame_len, &speech_tx_ns2float_cfg);
#if defined(SPEECH_TX_AEC)
    speech_ns2float_set_echo_state(speech_tx_ns2float_st, speech_tx_aec_lib_st);
    speech_ns2float_set_echo_suppress(speech_tx_ns2float_st, -40);
#endif
#endif

#if defined(SPEECH_TX_NS3)
    speech_tx_ns3_st = ns3_create(sample_rate, frame_len, &speech_tx_ns3_cfg);
#endif

#if defined(SPEECH_TX_NOISE_GATE)
    speech_tx_noise_gate_st = speech_noise_gate_create(sample_rate, frame_len, &speech_tx_noise_gate_cfg);
#endif

#if defined(SPEECH_TX_COMPEXP)
    speech_tx_compexp_st = compexp_create(sample_rate, frame_len, &speech_tx_compexp_cfg);
#endif

#if defined(SPEECH_TX_AGC)
    speech_tx_agc_st = agc_state_create(sample_rate, frame_len, &speech_tx_agc_cfg);
#endif

#if defined(SPEECH_TX_EQ)
    speech_tx_eq_st = eq_init(sample_rate, frame_len, &speech_tx_eq_cfg);
#endif

#if defined(SPEECH_TX_POST_GAIN)
    speech_tx_post_gain_st = speech_gain_create(sample_rate, frame_len, &speech_tx_post_gain_cfg);
#endif

    LOG_PRINT_VOICE_CALL_AUD("[%s] End", __func__);

    return 0;
}

int speech_rx_init(int sample_rate, int frame_len)
{
    LOG_PRINT_VOICE_CALL_AUD("[%s] Start...", __func__);

#if defined (SPEECH_RX_NS)
    speech_rx_ns_st = speech_ns_create(sample_rate, frame_len, &speech_rx_ns_cfg);
#endif

#if defined(SPEECH_RX_NS2)
    speech_rx_ns2_st = speech_ns2_create(sample_rate, frame_len, &speech_rx_ns2_cfg);
#endif

#if defined(SPEECH_RX_NS2FLOAT)
    speech_rx_ns2f_st = speech_ns2float_create(sample_rate, frame_len, &speech_rx_ns2float_cfg);
#endif

#if defined(SPEECH_RX_NS3)
    speech_rx_ns3_st = ns3_create(sample_rate, frame_len, &speech_rx_ns3_cfg);
#endif

#if defined(SPEECH_RX_AGC)
    speech_rx_agc_st = agc_state_create(sample_rate, frame_len, &speech_rx_agc_cfg);
#endif

#if defined(SPEECH_RX_EQ)
    speech_rx_eq_st = eq_init(sample_rate, frame_len, &speech_rx_eq_cfg);
#endif

#if defined(SPEECH_RX_POST_GAIN)
    speech_rx_post_gain_st = speech_gain_create(sample_rate, frame_len, &speech_rx_post_gain_cfg);
#endif

    LOG_PRINT_VOICE_CALL_AUD("[%s] End", __func__);

    return 0;
}

int speech_init(int tx_sample_rate, int rx_sample_rate, int frame_ms, uint8_t *buf, int len)
{
    LOG_PRINT_VOICE_CALL_AUD("[%s] Start...", __func__);

    ASSERT(frame_ms == SPEECH_PROCESS_FRAME_MS, "[%s] frame_ms(%d) != SPEECH_PROCESS_FRAME_MS(%d)", __func__,
                                                                                                    frame_ms,
                                                                                                    SPEECH_PROCESS_FRAME_MS);

    speech_tx_sample_rate = tx_sample_rate;
    speech_rx_sample_rate = rx_sample_rate;
    speech_tx_frame_len = SPEECH_FRAME_MS_TO_LEN(tx_sample_rate, frame_ms);
    speech_rx_frame_len = SPEECH_FRAME_MS_TO_LEN(rx_sample_rate, frame_ms);

    speech_heap_init(buf, len);

#if SPEECH_PROCESS_FRAME_MS != 15
    int aec_enable = 0;
#if defined(SPEECH_TX_AEC) || defined(SPEECH_TX_AEC2) || defined(SPEECH_TX_AEC2FLOAT)
    aec_enable = 1;
#endif
    speech_frame_resize_st = frame_resize_create(SPEECH_FRAME_MS_TO_LEN(tx_sample_rate, 15), 
                                                SPEECH_CODEC_CAPTURE_CHANNEL_NUM,
                                                speech_tx_frame_len,
                                                aec_enable,
                                                _speech_tx_process_,
                                                _speech_rx_process_);
#endif
    
    speech_tx_init(speech_tx_sample_rate, speech_tx_frame_len);
    speech_rx_init(speech_rx_sample_rate, speech_rx_frame_len);

    audio_dump_init(speech_tx_frame_len, 1);

#ifdef AUDIO_DEBUG_V0_1_0
    speech_tuning_init();
#endif

    LOG_PRINT_VOICE_CALL_AUD("[%s] End", __func__);

    return 0;
}

int speech_tx_deinit(void)
{
    LOG_PRINT_VOICE_CALL_AUD("[%s] Start...", __func__);

#if defined(SPEECH_TX_POST_GAIN)
    speech_gain_destroy(speech_tx_post_gain_st);
#endif

#if defined(SPEECH_TX_EQ)
    eq_destroy(speech_tx_eq_st);
#endif

#if defined(SPEECH_TX_AGC)
    agc_state_destroy(speech_tx_agc_st);
#endif

#if defined(SPEECH_TX_3MIC_NS)
    speech_3mic_ns_destroy(speech_tx_3mic_ns_st);
#endif

#if defined(SPEECH_TX_2MIC_NS4)
    sensormic_denoise_destroy(speech_tx_2mic_ns4_st);
#endif

#if defined(SPEECH_TX_2MIC_NS3)
    far_field_speech_enhancement_deinit();
#endif

#if defined(SPEECH_TX_2MIC_NS2)
    coheren_denoise_destroy(speech_tx_2mic_ns2_st);
#endif

#if defined(SPEECH_TX_2MIC_NS)
    dual_mic_denoise_deinit();
#endif

#if defined(SPEECH_TX_NS)
    speech_ns_destroy(speech_tx_ns_st);
#endif

#if defined(SPEECH_TX_NS2)
    speech_ns2_destroy(speech_tx_ns2_st);
#endif

#if defined(SPEECH_TX_NS2FLOAT)
    speech_ns2float_destroy(speech_tx_ns2float_st);
#endif

#ifdef SPEECH_TX_NS3
    ns3_destroy(speech_tx_ns3_st);
#endif

#ifdef SPEECH_TX_NOISE_GATE
    speech_noise_gate_destroy(speech_tx_noise_gate_st);
#endif

#ifdef SPEECH_TX_COMPEXP
    compexp_destroy(speech_tx_compexp_st);
#endif

#if defined(SPEECH_TX_AEC)
    speech_aec_destroy(speech_tx_aec_st);
#endif

#if defined(SPEECH_TX_AEC2)
    speech_aec2_destroy(speech_tx_aec2_st);
#endif

#if defined(SPEECH_TX_AEC2FLOAT)
    ec2float_destroy(speech_tx_aec2float_st);
#endif

#if defined(SPEECH_TX_AEC) || defined(SPEECH_TX_AEC2) || defined(SPEECH_TX_AEC2FLOAT)
    speech_free(aec_echo_buf_ptr);
    speech_free(aec_out_buf);
#endif

#if defined(SPEECH_TX_DC_FILTER)
    speech_dc_filter_destroy(speech_tx_dc_filter_st);
#endif

    LOG_PRINT_VOICE_CALL_AUD("[%s] End", __func__);

    return 0;
}

int speech_rx_deinit(void)
{
    LOG_PRINT_VOICE_CALL_AUD("[%s] Start...", __func__);

#if defined(SPEECH_RX_POST_GAIN)
    speech_gain_destroy(speech_rx_post_gain_st);
#endif

#if defined(SPEECH_RX_EQ)
    eq_destroy(speech_rx_eq_st);
#endif

#if defined(SPEECH_RX_AGC)
    agc_state_destroy(speech_rx_agc_st);
#endif

#if defined(SPEECH_RX_NS)
    speech_ns_destroy(speech_rx_ns_st);
#endif

#if defined(SPEECH_RX_NS2)
    speech_ns2_destroy(speech_rx_ns2_st);
#endif

#if defined(SPEECH_RX_NS2FLOAT)
    speech_ns2float_destroy(speech_rx_ns2f_st);
#endif

#ifdef SPEECH_RX_NS3
    ns3_destroy(speech_rx_ns3_st);
#endif

    LOG_PRINT_VOICE_CALL_AUD("[%s] End", __func__);

    return 0;
}

int speech_deinit(void)
{
    LOG_PRINT_VOICE_CALL_AUD("[%s] Start...", __func__);

#ifdef AUDIO_DEBUG_V0_1_0
    speech_tuning_deinit();
#endif

    speech_rx_deinit();
    speech_tx_deinit();

#if SPEECH_PROCESS_FRAME_MS != 15
    frame_resize_destroy(speech_frame_resize_st);
#endif

    size_t total = 0, used = 0, max_used = 0;
    speech_memory_info(&total, &used, &max_used);
    LOG_PRINT_VOICE_CALL_AUD("SPEECH MALLOC MEM: total - %d, used - %d, max_used - %d.", total, used, max_used);
    ASSERT(used == 0, "[%s] used != 0", __func__);

    LOG_PRINT_VOICE_CALL_AUD("[%s] End", __func__);

    return 0;
}

int32_t _speech_tx_process_(int16_t *pcm_buf, int16_t *ref_buf, int32_t *_pcm_len)
{
    int32_t pcm_len = *_pcm_len;

#if defined(BT_SCO_CHAIN_PROFILE)
    uint32_t start_ticks = hal_sys_timer_get();
#endif

    // LOG_PRINT_VOICE_CALL_AUD("[%s] pcm_len = %d", __func__, pcm_len);

#ifdef AUDIO_DEBUG_V0_1_0
    if (speech_tuning_get_status())
    {
        speech_tuning_set_config(speech_cfg);

        // If has MIPS problem, can move this block code into speech_rx_process or return directly
        // return 0
    }
#endif

    audio_dump_clear_up();

#if 0
    // Test MIC: Get one channel data
    // Enable this, should bypass SPEECH_TX_2MIC_NS and SPEECH_TX_2MIC_NS2
    for(uint32_t i=0; i<pcm_len/2; i++)
    {
        pcm_buf[i] = pcm_buf[2*i];      // Left channel
        // pcm_buf[i] = pcm_buf[2*i+1]; // Right channel
    }

    pcm_len >>= 1;
#endif

#if defined(SPEECH_TX_DC_FILTER)
    speech_dc_filter_process(speech_tx_dc_filter_st, pcm_buf, pcm_len);
#endif

#if defined(SPEECH_TX_2MIC_NS)
    dual_mic_denoise_run(pcm_buf, pcm_len, pcm_buf);
    // Channel num: two-->one
    pcm_len >>= 1;
#endif

#if defined(SPEECH_TX_2MIC_NS2)
    coherent_denoise_process(speech_tx_2mic_ns2_st, pcm_buf, pcm_len, pcm_buf);
    // Channel num: two-->one
    pcm_len >>= 1;
#endif

#if defined(SPEECH_TX_2MIC_NS4)
    sensormic_denoise_process(speech_tx_2mic_ns4_st, pcm_buf, pcm_len, pcm_buf);
    // Channel num: two-->one
    pcm_len >>= 1;
#endif

#if defined(SPEECH_TX_3MIC_NS)
    speech_3mic_ns_process(speech_tx_3mic_ns_st, pcm_buf, pcm_len, pcm_buf);
    // Channel num: three-->one
    pcm_len = pcm_len / 3;
#endif

    // Sample rate: 16k-->8k
#if defined(SPEECH_TX_2MIC_NS)
    if (speech_tx_sample_rate == 8000)
    {      
        if (speech_rx_sample_rate == 16000)  
        {
#if defined(SPEECH_TX_AEC) || defined(SPEECH_TX_AEC2) || defined(SPEECH_TX_AEC2FLOAT)
            LowPassFilter(ref_buf, ref_buf, filterState1, pcm_len);
#endif            
        }
        LowPassFilter(pcm_buf, pcm_buf, filterState, pcm_len);
        pcm_len >>= 1;
    }
#endif

    // static uint32_t cnt = 0;
    // for (int i=0; i<pcm_len; i++)
    // {
    //     pcm_buf[i] = cnt++;
    // }

    audio_dump_add_channel_data(0, pcm_buf, pcm_len);
#if defined(SPEECH_TX_AEC) || defined(SPEECH_TX_AEC2) || defined(SPEECH_TX_AEC2FLOAT)
    audio_dump_add_channel_data(1, ref_buf, pcm_len);
#else
    audio_dump_add_channel_data(1, pcm_buf, pcm_len);
#endif

#if defined(SPEECH_TX_AEC)
    speech_aec_process(speech_tx_aec_st, pcm_buf, ref_buf, pcm_len, aec_out_buf);
    speech_copy_int16(pcm_buf, aec_out_buf, pcm_len);
#endif

#if defined(SPEECH_TX_AEC2)
    speech_aec2_process(speech_tx_aec2_st, pcm_buf, ref_buf, pcm_len);
#endif

#if defined(SPEECH_TX_AEC2FLOAT)
    ec2float_process(speech_tx_aec2float_st, pcm_buf, ref_buf, pcm_len, aec_out_buf);
    speech_copy_int16(pcm_buf, aec_out_buf, pcm_len);
#endif

    audio_dump_add_channel_data(2, pcm_buf, pcm_len);

#if defined(SPEECH_TX_NS)
    speech_ns_process(speech_tx_ns_st, pcm_buf, pcm_len);
#endif

#if defined(SPEECH_TX_NS2)
    speech_ns2_process(speech_tx_ns2_st, pcm_buf, pcm_len);
#endif

#if defined(SPEECH_TX_NS2FLOAT)
    speech_ns2float_process(speech_tx_ns2float_st, pcm_buf, pcm_len);
#endif

#if defined(SPEECH_TX_NS3)
    ns3_process(speech_tx_ns3_st, pcm_buf, pcm_len);
#endif

    audio_dump_add_channel_data(3, pcm_buf, pcm_len);

#if defined(SPEECH_TX_NOISE_GATE)
    speech_noise_gate_process(speech_tx_noise_gate_st, pcm_buf, pcm_len);
#endif

    audio_dump_add_channel_data(4, pcm_buf, pcm_len);

#if defined(SPEECH_TX_COMPEXP)
    compexp_process(speech_tx_compexp_st, pcm_buf, pcm_len);
#endif

#if defined(SPEECH_TX_AGC)
    agc_process(speech_tx_agc_st, pcm_buf, pcm_len);
#endif

#if defined(SPEECH_TX_EQ)
    eq_process(speech_tx_eq_st, pcm_buf, pcm_len);
#endif

    // audio_dump_add_channel_data(0, pcm_buf, pcm_len);

#if defined(SPEECH_TX_POST_GAIN)
    speech_gain_process(speech_tx_post_gain_st, pcm_buf, pcm_len);
#endif

    // audio_dump_add_channel_data(1, pcm_buf, pcm_len);

    audio_dump_add_channel_data(5, pcm_buf, pcm_len);
    audio_dump_run();


    *_pcm_len = pcm_len;

#if defined(BT_SCO_CHAIN_PROFILE)
    uint32_t end_ticks = hal_sys_timer_get();
    LOG_PRINT_VOICE_CALL_AUD("[%s] takes %d ticks", __FUNCTION__, end_ticks - start_ticks);
#endif

    return 0;
}

int32_t _speech_rx_process_(int16_t *pcm_buf, int32_t *_pcm_len)
{
    int32_t pcm_len = *_pcm_len;

#if defined(BT_SCO_CHAIN_PROFILE)
    uint32_t start_ticks = hal_sys_timer_get();
#endif

    // audio_dump_add_channel_data(0, pcm_buf, pcm_len);

#if defined(SPEECH_RX_NS)
    speech_ns_process(speech_rx_ns_st, pcm_buf, pcm_len);
#endif

#if defined(SPEECH_RX_NS2) 
    // fix 0dB signal
    for(int i=0; i<pcm_len; i++)
    {
        pcm_buf[i] = (int16_t)(pcm_buf[i] * 0.94);
    }
    speech_ns2_process(speech_rx_ns2_st, pcm_buf, pcm_len);
#endif

#if defined(SPEECH_RX_NS2FLOAT)
    // FIXME
    for(int i=0; i<pcm_len; i++)
    {
        pcm_buf[i] = (int16_t)(pcm_buf[i] * 0.94);
    }
    speech_ns2float_process(speech_rx_ns2f_st, pcm_buf, pcm_len);
#endif

#ifdef SPEECH_RX_NS3
    ns3_process(speech_rx_ns3_st, pcm_buf, pcm_len);
#endif

#if defined(SPEECH_RX_AGC)
    agc_process(speech_rx_agc_st, pcm_buf, pcm_len);
#endif

#if defined(SPEECH_RX_EQ)
    eq_process(speech_rx_eq_st, pcm_buf, pcm_len);
#endif

#if defined(SPEECH_RX_POST_GAIN)
    speech_gain_process(speech_rx_post_gain_st, pcm_buf, pcm_len);
#endif

    // audio_dump_add_channel_data(1, pcm_buf, pcm_len);
    // audio_dump_run();

    *_pcm_len = pcm_len;

#if defined(BT_SCO_CHAIN_PROFILE)
    uint32_t end_ticks = hal_sys_timer_get();
    LOG_PRINT_VOICE_CALL_AUD("[%s] takes %d ticks", __FUNCTION__, end_ticks - start_ticks);
#endif

    return 0;
}

//static short speech_cnt = 0;
int speech_tx_process(short *pcm_buf, short *ref_buf, int *pcm_len)
{
    // for (int i=0; i<pcm_len; i++)
    // {
    //     pcm_buf[i] = speech_cnt++;
    // }

#if SPEECH_PROCESS_FRAME_MS == 15
    _speech_tx_process_(pcm_buf, ref_buf, (int32_t *)pcm_len);
#else

#if 0
    // Test MIC: Get one channel data
    // Enable this, should bypass SPEECH_TX_2MIC_NS and SPEECH_TX_2MIC_NS2
    for(uint32_t i=0; i<pcm_len/2; i++)
    {
        pcm_buf[i] = pcm_buf[2*i];      // Left channel
        // pcm_buf[i] = pcm_buf[2*i+1]; // Right channel
    }

    pcm_len >>= 1;
    *out_pcm_len = pcm_len;
#else
    // MUST use (int32_t *)??????
    frame_resize_process_capture(speech_frame_resize_st, pcm_buf, ref_buf, (int32_t *)pcm_len);
#endif

#endif

    // audio_dump_clear_up();
    // audio_dump_add_channel_data(0, pcm_buf, 240);
    // audio_dump_run();

    return 0;
}

int speech_rx_process(short *pcm_buf, int *pcm_len)
{
    // for (int i=0; i<pcm_len; i++)
    // {
    //     pcm_buf[i] = speech_cnt++;
    // }

#if SPEECH_PROCESS_FRAME_MS == 15
    _speech_rx_process_(pcm_buf, (int32_t *)pcm_len);
#else
    frame_resize_process_playback(speech_frame_resize_st, pcm_buf, (int32_t *)pcm_len);
#endif

    // DUMP16("%d ", pcm_buf, 16);
    // DUMP16("%d ", out_pcm_buf_ptr, 16);

    // int sum = 0;
    // for (int i=0; i<*out_pcm_len; i++)
    // {
    //     sum += out_pcm_buf_ptr[i];
    // }
    // TRACE("inlen = %d, outlen = %d; sum = %d", pcm_len, *out_pcm_len, sum);

    return 0;
}
