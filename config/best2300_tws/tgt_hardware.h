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
#ifndef __TGT_HARDWARE__
#define __TGT_HARDWARE__

#ifdef __cplusplus
extern "C" {
#endif

#include "hal_iomux.h"
#include "hal_gpio.h"
#include "hal_key.h"
#include "hal_aud.h"

//eq config
#define EQ_DAC_IIR_LIST_NUM                     1
#define EQ_ADC_IIR_LIST_NUM                     1

//pwl
#define CFG_HW_PLW_NUM (0)
extern const struct HAL_IOMUX_PIN_FUNCTION_MAP cfg_hw_pinmux_pwl[CFG_HW_PLW_NUM];

//adckey define
#define CFG_HW_ADCKEY_NUMBER 0
#define CFG_HW_ADCKEY_BASE 0
#define CFG_HW_ADCKEY_ADC_MAXVOLT 1000
#define CFG_HW_ADCKEY_ADC_MINVOLT 0
#define CFG_HW_ADCKEY_ADC_KEYVOLT_BASE 130
extern const uint16_t CFG_HW_ADCKEY_MAP_TABLE[CFG_HW_ADCKEY_NUMBER];

#if defined(__TWS_1CHANNEL_PCM__)
#define TWS_SBC_PATCH_SAMPLES_PAT_NUM (1*2)
#define TWS_SBC_PATCH_SAMPLES_CNT_NUM (5000*2)
#else
#define TWS_SBC_PATCH_SAMPLES_PAT_NUM (1*2*2)
#define TWS_SBC_PATCH_SAMPLES_CNT_NUM (5000*2*2)
#endif

#if defined(TWS_SBC_PATCH_SAMPLES) 
// 0.04 * 0.005 = 2/10000 = 1/5000 sw shift
// 0.00225 * 0.005 = 1.1125/100000 hw shift
#define A2DP_SMAPLE_RATE_44100_SHIFT_LEVEL (0.0424f)

#define A2DP_SMAPLE_RATE_48000_SHIFT_LEVEL (0.0455f)

#elif defined(__AUDIO_RESAMPLE_CONFIG__)

#ifdef RESAMPLE_ANY_SAMPLE_RATE_CONFIG
#define A2DP_SMAPLE_RATE_44100_SHIFT_LEVEL (0.032f)
#define A2DP_SMAPLE_RATE_48000_SHIFT_LEVEL (0.032f)
#else
#define A2DP_SMAPLE_RATE_44100_SHIFT_LEVEL (0.0024f)
#define A2DP_SMAPLE_RATE_48000_SHIFT_LEVEL (0.0055f)

#define A2DP_SMAPLE_RATE_44100_SHIFT_LEVEL_C (0.0261f) ////1
#define A2DP_SMAPLE_RATE_48000_SHIFT_LEVEL_C (0.0282f)   ///2

//#define A2DP_SMAPLE_RATE_44100_SHIFT_LEVEL_C (0.0024f) ////1
//#define A2DP_SMAPLE_RATE_48000_SHIFT_LEVEL_C (0.0055f)   ///2

#endif
#else
#define A2DP_SMAPLE_RATE_SHIFT_LEVEL (0)
#endif

#define AAC_SMAPLE_RATE_SHIFT_LEVEL (0)
#if defined(__AUDIO_RESAMPLE_CONFIG__)
#define AAC_SMAPLE_RATE_44100_SHIFT_LEVEL (0.0024f)

#define AAC_SMAPLE_RATE_48000_SHIFT_LEVEL (0.0055f)
#endif

#if defined(SLAVE_USE_ENC_SINGLE_SBC)
#define TRANS_BTA_AV_CO_SBC_MAX_BITPOOL  31
#endif

#if defined(SLAVE_USE_OPUS)
#define BTA_AV_CO_SBC_MAX_BITPOOL  53
#else
#define BTA_AV_CO_SBC_MAX_BITPOOL  53
#define BTA_AV_CO_SBC_MAX_BITPOOL_48K  45
#endif

#define MAX_AAC_BITRATE (264630)
#define MAX_AAC_BITRATE_48K (196608)



//gpiokey define
#define CFG_HW_GPIOKEY_NUM (3)
extern const struct HAL_KEY_GPIOKEY_CFG_T cfg_hw_gpio_key_cfg[CFG_HW_GPIOKEY_NUM];

//audio path define
extern const struct AUD_IO_PATH_CFG_T cfg_audio_input_path_cfg[];

#if defined(__TWS_1CHANNEL_PCM__)
#define CFG_HW_AUD_OUTPUT_PATH_SPEAKER_DEV (AUD_CHANNEL_MAP_CH0)
#else
#define CFG_HW_AUD_OUTPUT_PATH_SPEAKER_DEV (AUD_CHANNEL_MAP_CH0 | AUD_CHANNEL_MAP_CH1)
#endif

//bt config
extern const char *BT_LOCAL_NAME;
extern const char *BLE_DEFAULT_NAME;

extern uint8_t ble_addr[6];
extern uint8_t bt_addr[6];

#define ANALOG_ADC_A_GAIN_DB (30)
#define ANALOG_ADC_B_GAIN_DB (30)


//5db per step max is 7
#define ANALOG_ADC_GAIN1    0x5
#define ANALOG_ADC_GAIN2    0x4


#define CODEC_SADC_VOL (12)

#define ZERODB_DIG_DAC_REGVAL (21)

extern const struct CODEC_DAC_VOL_T codec_dac_vol[TGT_VOLUME_LEVEL_QTY];

//range -12~+12
#define CFG_HW_AUD_EQ_NUM_BANDS (8)
extern const int8_t cfg_aud_eq_sbc_band_settings[CFG_HW_AUD_EQ_NUM_BANDS];
#define CFG_AUD_EQ_IIR_NUM_BANDS (4)
extern  int8_t cfg_hw_aud_eq_band_settings[CFG_HW_AUD_EQ_NUM_BANDS];

//battery info
#define APP_BATTERY_MIN_MV (3200)
#define APP_BATTERY_PD_MV   (3100)

#define APP_BATTERY_MAX_MV (4200)

extern const struct HAL_IOMUX_PIN_FUNCTION_MAP app_battery_ext_charger_detecter_cfg;
extern const struct HAL_IOMUX_PIN_FUNCTION_MAP app_battery_ext_charger_indicator_cfg;

#ifdef __cplusplus
}
#endif

#endif
