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
#include "tgt_hardware.h"
#include "aud_section.h"
#include "iir_process.h"
#include "fir_process.h"
#include "hw_codec_iir_process.h"

const struct HAL_IOMUX_PIN_FUNCTION_MAP cfg_hw_pinmux_pwl[CFG_HW_PLW_NUM] = {
    //{HAL_IOMUX_PIN_P0_0, HAL_IOMUX_FUNC_AS_GPIO, HAL_IOMUX_PIN_VOLTAGE_VIO, HAL_IOMUX_PIN_PULLUP_ENALBE},
    //{HAL_IOMUX_PIN_P0_1, HAL_IOMUX_FUNC_AS_GPIO, HAL_IOMUX_PIN_VOLTAGE_VIO, HAL_IOMUX_PIN_PULLUP_ENALBE},
};

//adckey define
const uint16_t CFG_HW_ADCKEY_MAP_TABLE[CFG_HW_ADCKEY_NUMBER] = {
    //HAL_KEY_CODE_FN9,HAL_KEY_CODE_FN8,HAL_KEY_CODE_FN7,
    //HAL_KEY_CODE_FN6,HAL_KEY_CODE_FN5,HAL_KEY_CODE_FN4,
    //HAL_KEY_CODE_FN3,HAL_KEY_CODE_FN2,HAL_KEY_CODE_FN1
};

//gpiokey define
#define CFG_HW_GPIOKEY_DOWN_LEVEL          (0)
#define CFG_HW_GPIOKEY_UP_LEVEL            (1)
const struct HAL_KEY_GPIOKEY_CFG_T cfg_hw_gpio_key_cfg[CFG_HW_GPIOKEY_NUM] = {
    {HAL_KEY_CODE_FN1, {HAL_IOMUX_PIN_P0_3, HAL_IOMUX_FUNC_AS_GPIO, HAL_IOMUX_PIN_VOLTAGE_VIO, HAL_IOMUX_PIN_PULLUP_ENALBE}},
    {HAL_KEY_CODE_FN2, {HAL_IOMUX_PIN_P0_2, HAL_IOMUX_FUNC_AS_GPIO, HAL_IOMUX_PIN_VOLTAGE_VIO, HAL_IOMUX_PIN_PULLUP_ENALBE}},
    {HAL_KEY_CODE_FN15,{HAL_IOMUX_PIN_P0_1, HAL_IOMUX_FUNC_AS_GPIO, HAL_IOMUX_PIN_VOLTAGE_VIO, HAL_IOMUX_PIN_PULLUP_ENALBE}},
};

//audio input path define
#define CFG_HW_AUD_INPUT_PATH_MIC_CH0 (AUD_CHANNEL_MAP_CH0 | AUD_VMIC_MAP_VMIC1)
#define CFG_HW_AUD_INPUT_PATH_MIC_CH1 (AUD_CHANNEL_MAP_CH1 | AUD_VMIC_MAP_VMIC2)
const struct AUD_IO_PATH_CFG_T cfg_audio_input_path_cfg[] = {
#if SPEECH_CODEC_CAPTURE_CHANNEL_NUM == 2
	{AUD_INPUT_PATH_MAINMIC, CFG_HW_AUD_INPUT_PATH_MIC_CH0|CFG_HW_AUD_INPUT_PATH_MIC_CH1},
#else
	{AUD_INPUT_PATH_MAINMIC, CFG_HW_AUD_INPUT_PATH_MIC_CH0},
#endif
	{AUD_INPUT_PATH_MICCH0, CFG_HW_AUD_INPUT_PATH_MIC_CH0},
	{AUD_INPUT_PATH_MICCH1, CFG_HW_AUD_INPUT_PATH_MIC_CH1},
};

//bt config
const char *BT_LOCAL_NAME = TO_STRING(BT_DEV_NAME) "\0";
// to support the BLE bisto, the BLE name must be bt name + "-LE"
const char *BLE_DEFAULT_NAME = TO_STRING(BT_DEV_NAME) "-LE";

uint8_t ble_addr[6] = {
#ifdef BLE_DEV_ADDR
	BLE_DEV_ADDR
#else
	0x78, 0x66, 0x95, 0x38, 0x38, 0x38
#endif
};
uint8_t bt_addr[6] = {
#ifdef BT_DEV_ADDR
	BT_DEV_ADDR
#else
	0x78, 0x66, 0x95, 0x33, 0x33, 0x33
#endif
};

//audio config
//freq bands range {[0k:2.5K], [2.5k:5K], [5k:7.5K], [7.5K:10K], [10K:12.5K], [12.5K:15K], [15K:17.5K], [17.5K:20K]}
//gain range -12~+12
int8_t cfg_hw_aud_eq_band_settings[CFG_HW_AUD_EQ_NUM_BANDS] = {0, 0, 0, 0, 0, 0, 0, 0};
const int8_t cfg_aud_eq_sbc_band_settings[CFG_HW_AUD_EQ_NUM_BANDS] = {0, 0, 0, 0, 0, 0, 0, 0};

#define TX_PA_GAIN                          CODEC_TX_PA_GAIN_DEFAULT

const struct CODEC_DAC_VOL_T codec_dac_vol[TGT_VOLUME_LEVEL_QTY] = {
    {TX_PA_GAIN,0x03,-11},
    {TX_PA_GAIN,0x03,-99},
    {TX_PA_GAIN,0x03,-45},
    {TX_PA_GAIN,0x03,-42},
    {TX_PA_GAIN,0x03,-39},
    {TX_PA_GAIN,0x03,-36},
    {TX_PA_GAIN,0x03,-33},
    {TX_PA_GAIN,0x03,-30},
    {TX_PA_GAIN,0x03,-27},
    {TX_PA_GAIN,0x03,-24},
    {TX_PA_GAIN,0x03,-21},
    {TX_PA_GAIN,0x03,-18},
    {TX_PA_GAIN,0x03,-15},
    {TX_PA_GAIN,0x03,-12},
    {TX_PA_GAIN,0x03, -9},
    {TX_PA_GAIN,0x03, -6},
    {TX_PA_GAIN,0x03, -3},
    {TX_PA_GAIN,0x03,  0},  //0dBm
};

const struct HAL_IOMUX_PIN_FUNCTION_MAP app_battery_ext_charger_detecter_cfg = {
    HAL_IOMUX_PIN_NUM, HAL_IOMUX_FUNC_AS_GPIO, HAL_IOMUX_PIN_VOLTAGE_VIO, HAL_IOMUX_PIN_PULLUP_ENALBE
};

const struct HAL_IOMUX_PIN_FUNCTION_MAP app_battery_ext_charger_indicator_cfg = {
    HAL_IOMUX_PIN_NUM, HAL_IOMUX_FUNC_AS_GPIO, HAL_IOMUX_PIN_VOLTAGE_VIO, HAL_IOMUX_PIN_PULLUP_ENALBE
};


/*

//ff


Filter1_B=[     42462788,    -84862242,     42399478];
Filter1_A=[    134217728,   -268358003,    134140286];

Filter2_B=[    135905569,   -267224817,    131334465];
Filter2_A=[    134217728,   -267224817,    133022306];

Filter3_B=[    132936489,   -263935268,    131067941];
Filter3_A=[    134217728,   -263935268,    129786702];

Filter4_B=[    131758190,   -257297054,    126191415];
Filter4_A=[    134217728,   -257297054,    123731878];


  
*/

#define IIR_COUNTER_FF_L (6)
#define IIR_COUNTER_FF_R (0)
#define IIR_COUNTER_FB_L (5)
#define IIR_COUNTER_FB_R (0)


static const struct_anc_cfg POSSIBLY_UNUSED AncFirCoef_50p7k_mode0 = {
    .anc_cfg_ff_l = {
       // .total_gain = 440,
		.total_gain = 350,
		
		.iir_bypass_flag=0,
		.iir_counter=IIR_COUNTER_FF_L,
		
		.iir_coef[0].coef_b={42462788,    -84862242,     42399478},
		.iir_coef[0].coef_a={134217728,   -268358003,    134140286},
		
		.iir_coef[1].coef_b={135905569,   -267224817,    131334465},
		.iir_coef[1].coef_a={134217728,   -267224817,    133022306},
		
		.iir_coef[2].coef_b={132936489,   -263935268,    131067941},
		.iir_coef[2].coef_a={134217728,   -263935268,    129786702},	
		
		.iir_coef[3].coef_b={131758190,   -257297054,    126191415},
		.iir_coef[3].coef_a={134217728,   -257297054,    123731878},
		
		.iir_coef[4].coef_b={0x8000000,0,0},
		.iir_coef[4].coef_a={0x8000000,0,0},
		
		.iir_coef[5].coef_b={0x8000000,0,0},
		.iir_coef[5].coef_a={0x8000000,0,0},
		
/*		.fir_bypass_flag=1,
        .fir_len = AUD_COEF_LEN,
        .fir_coef =
        {
            32767,
        },
*/ 
		.dac_gain_offset=0,
		.adc_gain_offset=(0)*4,
    },
    .anc_cfg_ff_r = {
      //  .total_gain = 382,
		.total_gain = 350,
	
		.iir_bypass_flag=0,
		.iir_counter=IIR_COUNTER_FF_R,
		
		.iir_coef[0].coef_b={42462788,    -84862242,     42399478},
		.iir_coef[0].coef_a={134217728,   -268358003,    134140286},
		
		.iir_coef[1].coef_b={135905569,   -267224817,    131334465},
		.iir_coef[1].coef_a={134217728,   -267224817,    133022306},
		
		.iir_coef[2].coef_b={132936489,   -263935268,    131067941},
		.iir_coef[2].coef_a={134217728,   -263935268,    129786702},	
		
		.iir_coef[3].coef_b={131758190,   -257297054,    126191415},
		.iir_coef[3].coef_a={134217728,   -257297054,    123731878},
		
		.iir_coef[4].coef_b={0x8000000,0,0},
		.iir_coef[4].coef_a={0x8000000,0,0},
		
		.iir_coef[5].coef_b={0x8000000,0,0},
		.iir_coef[5].coef_a={0x8000000,0,0},
		
/*		.fir_bypass_flag=1,
        .fir_len = AUD_COEF_LEN,
        .fir_coef =
        {
            32767,
        },
*/ 
        .dac_gain_offset=0,
	 .adc_gain_offset=(0)*4,
    },


/*

Filter1_B=[     27461831,    -54408898,     27001841];
Filter1_A=[    134217728,   -216605724,     82606056];

Filter2_B=[    138294078,   -267600712,    129323227];
Filter2_A=[    134217728,   -267600712,    133399577];

Filter3_B=[    134500015,   -268177932,    133678688];
Filter3_A=[    134217728,   -268177932,    133960975];

Filter4_B=[    133629164,   -264794659,    131257050];
Filter4_A=[    134217728,   -264794659,    130668486];


*/
            
    .anc_cfg_fb_l = {
        .total_gain = 350,
			
		.iir_bypass_flag=0,
		.iir_counter=IIR_COUNTER_FB_L,
		
		.iir_coef[0].coef_b={  27461831,    -54408898,     27001841},
		.iir_coef[0].coef_a={134217728,   -216605724,     82606056},
		
		.iir_coef[1].coef_b={138294078,   -267600712,    129323227},
		.iir_coef[1].coef_a={134217728,   -267600712,    133399577},
		
		.iir_coef[2].coef_b={134500015,   -268177932,    133678688},
		.iir_coef[2].coef_a={134217728,   -268177932,    133960975},	
		
		.iir_coef[3].coef_b={133629164,   -264794659,    131257050},
		.iir_coef[3].coef_a={134217728,   -264794659,    130668486},	
		
		.iir_coef[4].coef_b={0x8000000,0,0},
		.iir_coef[4].coef_a={0x8000000,0,0},
		
		.iir_coef[5].coef_b={0x8000000,0,0},
		.iir_coef[5].coef_a={0x8000000,0,0},	
		
/*		.fir_bypass_flag=1,
        .fir_len = AUD_COEF_LEN,
        .fir_coef =
        {
            32767,
        },
*/ 





		.dac_gain_offset=0,
		.adc_gain_offset=(0)*4,
    },
    .anc_cfg_fb_r = {
        .total_gain = 350,
			
		.iir_bypass_flag=0,
		.iir_counter=IIR_COUNTER_FB_R,
		
		.iir_coef[0].coef_b={  27461831,    -54408898,     27001841},
		.iir_coef[0].coef_a={134217728,   -216605724,     82606056},
		
		.iir_coef[1].coef_b={138294078,   -267600712,    129323227},
		.iir_coef[1].coef_a={134217728,   -267600712,    133399577},
		
		.iir_coef[2].coef_b={134500015,   -268177932,    133678688},
		.iir_coef[2].coef_a={134217728,   -268177932,    133960975},	
		
		.iir_coef[3].coef_b={133629164,   -264794659,    131257050},
		.iir_coef[3].coef_a={134217728,   -264794659,    130668486},	
		
		.iir_coef[4].coef_b={0x8000000,0,0},
		.iir_coef[4].coef_a={0x8000000,0,0},
		
		.iir_coef[5].coef_b={0x8000000,0,0},
		.iir_coef[5].coef_a={0x8000000,0,0},	
		
		
/*		.fir_bypass_flag=1,
        .fir_len = AUD_COEF_LEN,
        .fir_coef =
        {
            32767,
        },
*/ 
        .dac_gain_offset=0,
	 .adc_gain_offset=(0)*4,
    },


			
};


/*

//ff


Filter1_B=[     42463913,    -84860822,     42396935];
Filter1_A=[    134217728,   -268353516,    134135801];

Filter2_B=[    136002894,   -267154076,    131168209];
Filter2_A=[    134217728,   -267154076,    132953376];

Filter3_B=[    132863566,   -263674901,    130888668];
Filter3_A=[    134217728,   -263674901,    129534506];

Filter4_B=[    131621817,   -256639526,    125746382];
Filter4_A=[    134217728,   -256639526,    123150471];


  
*/

static const struct_anc_cfg POSSIBLY_UNUSED AncFirCoef_48k_mode0 = {
    .anc_cfg_ff_l = {
       // .total_gain = 440,
		.total_gain = 312,
		
		.iir_bypass_flag=0,
		.iir_counter=IIR_COUNTER_FF_L,
		
		.iir_coef[0].coef_b={42463913,    -84860822,     42396935},
		.iir_coef[0].coef_a={134217728,   -268353516,    134135801},
		
		.iir_coef[1].coef_b={136002894,   -267154076,    131168209},
		.iir_coef[1].coef_a={134217728,   -267154076,    132953376},
		
		.iir_coef[2].coef_b={132863566,   -263674901,    130888668},
		.iir_coef[2].coef_a={134217728,   -263674901,    129534506},	
		
		.iir_coef[3].coef_b={131621817,   -256639526,    125746382},
		.iir_coef[3].coef_a={134217728,   -256639526,    123150471},
		
		.iir_coef[4].coef_b={0x8000000,0,0},
		.iir_coef[4].coef_a={0x8000000,0,0},
		
		.iir_coef[5].coef_b={0x8000000,0,0},
		.iir_coef[5].coef_a={0x8000000,0,0},
		
/*		.fir_bypass_flag=1,
        .fir_len = AUD_COEF_LEN,
        .fir_coef =
        {
            32767,
        },
*/       
		.dac_gain_offset=0,
		.adc_gain_offset=(0)*4,
    },
    .anc_cfg_ff_r = {
      //  .total_gain = 382,
		.total_gain = 288,
	
		.iir_bypass_flag=0,
		.iir_counter=IIR_COUNTER_FF_R,
		
		.iir_coef[0].coef_b={42463913,    -84860822,     42396935},
		.iir_coef[0].coef_a={134217728,   -268353516,    134135801},
		
		.iir_coef[1].coef_b={136002894,   -267154076,    131168209},
		.iir_coef[1].coef_a={134217728,   -267154076,    132953376},
		
		.iir_coef[2].coef_b={132863566,   -263674901,    130888668},
		.iir_coef[2].coef_a={134217728,   -263674901,    129534506},	
		
		.iir_coef[3].coef_b={131621817,   -256639526,    125746382},
		.iir_coef[3].coef_a={134217728,   -256639526,    123150471},
		
		.iir_coef[4].coef_b={0x8000000,0,0},
		.iir_coef[4].coef_a={0x8000000,0,0},
		
		.iir_coef[5].coef_b={0x8000000,0,0},
		.iir_coef[5].coef_a={0x8000000,0,0},
/*		.fir_bypass_flag=1,
        .fir_len = AUD_COEF_LEN,
        .fir_coef =
        {
            32767,
        },
*/         
        .dac_gain_offset=0,
	    .adc_gain_offset=(0)*4,
    },


/*

Filter1_B=[     27172676,    -53803459,     26691412];
Filter1_A=[    134217728,   -214195429,     80219070];

Filter2_B=[    138529480,   -267551490,    129040578];
Filter2_A=[    134217728,   -267551490,    133352330];

Filter3_B=[    134516353,   -268162980,    133647489];
Filter3_A=[    134217728,   -268162980,    133946114];

Filter4_B=[    133595549,   -264581113,    131087955];
Filter4_A=[    134217728,   -264581113,    130465777];


*/
            
    .anc_cfg_fb_l = {
        .total_gain = 511,
			
		.iir_bypass_flag=0,
		.iir_counter=IIR_COUNTER_FB_L,
		
		.iir_coef[0].coef_b={  27172676,    -53803459,     26691412},
		.iir_coef[0].coef_a={134217728,   -214195429,     80219070},
		
		.iir_coef[1].coef_b={138529480,   -267551490,    129040578},
		.iir_coef[1].coef_a={134217728,   -267551490,    133352330},
		
		.iir_coef[2].coef_b={134516353,   -268162980,    133647489},
		.iir_coef[2].coef_a={134217728,   -268162980,    133946114},	
		
		.iir_coef[3].coef_b={133595549,   -264581113,    131087955},
		.iir_coef[3].coef_a={134217728,   -264581113,    130465777},	
		
		.iir_coef[4].coef_b={0x8000000,0,0},
		.iir_coef[4].coef_a={0x8000000,0,0},
		
		.iir_coef[5].coef_b={0x8000000,0,0},
		.iir_coef[5].coef_a={0x8000000,0,0},
		
/*		.fir_bypass_flag=1,
        .fir_len = AUD_COEF_LEN,
        .fir_coef =
        {
            32767,
        },
*/         
		.dac_gain_offset=0,
		.adc_gain_offset=(0)*4,
    },
    .anc_cfg_fb_r = {
        .total_gain = 511,
			
		.iir_bypass_flag=0,
		.iir_counter=IIR_COUNTER_FB_R,
		
		.iir_coef[0].coef_b={  27172676,    -53803459,     26691412},
		.iir_coef[0].coef_a={134217728,   -214195429,     80219070},
		
		.iir_coef[1].coef_b={138529480,   -267551490,    129040578},
		.iir_coef[1].coef_a={134217728,   -267551490,    133352330},
		
		.iir_coef[2].coef_b={134516353,   -268162980,    133647489},
		.iir_coef[2].coef_a={134217728,   -268162980,    133946114},	
		
		.iir_coef[3].coef_b={133595549,   -264581113,    131087955},
		.iir_coef[3].coef_a={134217728,   -264581113,    130465777},	
		
		.iir_coef[4].coef_b={0x8000000,0,0},
		.iir_coef[4].coef_a={0x8000000,0,0},
		
		.iir_coef[5].coef_b={0x8000000,0,0},
		.iir_coef[5].coef_a={0x8000000,0,0},	
		
/*		.fir_bypass_flag=1,
        .fir_len = AUD_COEF_LEN,
        .fir_coef =
        {
            32767,
        },
*/         
        .dac_gain_offset=0,
	    .adc_gain_offset=(0)*4,
    },

#if (AUD_SECTION_STRUCT_VERSION == 2)


/*
1.0000000000000000,-1.5858874672928407,0.6974239598044429,0.2832267077115959,-0.3117526885614825,0.1400624733614886,
Filter1_B=[      4751756,     -5230342,      2349858];
Filter1_A=[     16777216,    -26606777,     11700832];


1.0000000000000000,-1.7971697583202608,0.8159624512785459,0.9540998606028980,-1.7971697583202608,0.8618625906756480,
Filter2_B=[     16007139,    -30151505,     14459655];
Filter2_A=[     16777216,    -30151505,     13689578];


1.0000000000000000,-1.9694050640918992,0.9705681145972464,0.3200483744622364,-0.6223829329788905,0.3034976090220014,
Filter3_B=[      5369521,    -10441853,      5091845];
Filter3_A=[     16777216,    -33041134,     16283431];


1.0000000000000000,-1.9921619776276678,0.9921812243512138,0.9968660174712476,-1.9921712178765081,0.9953059666311256,
Filter4_B=[     16724636,    -33423087,     16698463];
Filter4_A=[     16777216,    -33422932,     16646039];
*/

/*

1.0000000000000000,-1.9868580074509832,0.9869011854430232,1.1834688902733632,-2.3614075958038656,1.1779451659756268,
Filter1_B=[     19855313,    -39617845,     19762640];
Filter1_A=[     16777216,    -33333946,     16557454];


1.0000000000000000,-1.0329261527674278,0.0418392318218667,0.5812322628931170,-1.0329261527674278,0.4606069689287498,
Filter2_B=[      9751459,    -17329625,      7727703];
Filter2_A=[     16777216,    -17329625,       701946];


1.0000000000000000,-1.9576081396140492,0.9591185490154677,1.0729914166044796,-1.9576081396140492,0.8861271324109881,
Filter3_B=[     18001809,    -32843215,     14866746];
Filter3_A=[     16777216,    -32843215,     16091339];


1.0000000000000000,-1.9197071583239940,0.9219883336398085,0.7545642546264146,-1.4392920140632206,0.6870089347526202,
Filter4_B=[     12659487,    -24147313,     11526097];
Filter4_A=[     16777216,    -32207342,     15468397];


1.0000000000000000,-1.9102108535747602,0.9139876710346515,0.9829076121866517,-1.9102108535747602,0.9310800588479999,
Filter5_B=[     16490453,    -32048020,     15620931];
Filter5_A=[     16777216,    -32048020,     15334169];
*/




    .anc_cfg_mc_l = {
        .total_gain = 1228,
			
		.iir_bypass_flag=0,
		.iir_counter=5,
		
		.iir_coef[0].coef_b={19855313,    -39617845,     19762640},
		.iir_coef[0].coef_a={16777216,    -33333946,     16557454},
		
		.iir_coef[1].coef_b={9751459,    -17329625,      7727703},
		.iir_coef[1].coef_a={16777216,    -17329625,       701946},
		
		.iir_coef[2].coef_b={18001809,    -32843215,     14866746},
		.iir_coef[2].coef_a={16777216,    -32843215,     16091339},	
		
		.iir_coef[3].coef_b={12659487,    -24147313,     11526097},
		.iir_coef[3].coef_a={16777216,    -32207342,     15468397},	
		
		.iir_coef[4].coef_b={16490453,    -32048020,     15620931},
		.iir_coef[4].coef_a={16777216,    -32048020,     15334169},
		
		.iir_coef[5].coef_b={0x8000000,0,0},
		.iir_coef[5].coef_a={0x8000000,0,0},
		

		.dac_gain_offset=0,
		.adc_gain_offset=(0)*4,
    },
    .anc_cfg_mc_r = {
        .total_gain = 1331,
			
		.iir_bypass_flag=0,
		.iir_counter=5,
		
		.iir_coef[0].coef_b={19855313,    -39617845,     19762640},
		.iir_coef[0].coef_a={16777216,    -33333946,     16557454},
		
		.iir_coef[1].coef_b={9751459,    -17329625,      7727703},
		.iir_coef[1].coef_a={16777216,    -17329625,       701946},
		
		.iir_coef[2].coef_b={18001809,    -32843215,     14866746},
		.iir_coef[2].coef_a={16777216,    -32843215,     16091339},	
		
		.iir_coef[3].coef_b={12659487,    -24147313,     11526097},
		.iir_coef[3].coef_a={16777216,    -32207342,     15468397},	
		
		.iir_coef[4].coef_b={16490453,    -32048020,     15620931},
		.iir_coef[4].coef_a={16777216,    -32048020,     15334169},
		
		.iir_coef[5].coef_b={0x8000000,0,0},
		.iir_coef[5].coef_a={0x8000000,0,0},	
		

        .dac_gain_offset=0,
	    .adc_gain_offset=(0)*4,
    },
#endif
};

/*
//ff


Filter1_B=[     42465729,    -84858529,     42392831];
Filter1_A=[    134217728,   -268346271,    134128558];

Filter2_B=[    136159949,   -267039705,    130899919];
Filter2_A=[    134217728,   -267039705,    132842140];

Filter3_B=[    132746107,   -263254540,    130599907];
Filter3_A=[    134217728,   -263254540,    129128286];

Filter4_B=[    131402980,   -255575175,    125032243];
Filter4_A=[    134217728,   -255575175,    122217496];



*/



static const struct_anc_cfg POSSIBLY_UNUSED AncFirCoef_44p1k_mode0 = {
    .anc_cfg_ff_l = {
       // .total_gain = 440,
		.total_gain =312,
		
		.iir_bypass_flag=0,
		.iir_counter=IIR_COUNTER_FF_L,
		
		.iir_coef[0].coef_b={42465729,    -84858529,     42392831},
		.iir_coef[0].coef_a={134217728,   -268346271,    134128558},
		
		.iir_coef[1].coef_b={136159949,   -267039705,    130899919},
		.iir_coef[1].coef_a={134217728,   -267039705,    132842140},
		
		.iir_coef[2].coef_b={132746107,   -263254540,    130599907},
		.iir_coef[2].coef_a={134217728,   -263254540,    129128286},	
		
		.iir_coef[3].coef_b={131402980,   -255575175,    125032243},
		.iir_coef[3].coef_a={ 134217728,   -255575175,    122217496},
		
		.iir_coef[4].coef_b={0x8000000,0,0},
		.iir_coef[4].coef_a={0x8000000,0,0},
		
		.iir_coef[5].coef_b={0x8000000,0,0},
		.iir_coef[5].coef_a={0x8000000,0,0},
		
/*		.fir_bypass_flag=1,
        .fir_len = AUD_COEF_LEN,
        .fir_coef =
        {
            32767,
        },
*/         
		.dac_gain_offset=0,
		.adc_gain_offset=(0)*4,
    },
    .anc_cfg_ff_r = {
      //  .total_gain = 382,
		.total_gain = 288,
	
		.iir_bypass_flag=0,
		.iir_counter=IIR_COUNTER_FF_R,
		
		.iir_coef[0].coef_b={42465729,    -84858529,     42392831},
		.iir_coef[0].coef_a={134217728,   -268346271,    134128558},
		
		.iir_coef[1].coef_b={136159949,   -267039705,    130899919},
		.iir_coef[1].coef_a={134217728,   -267039705,    132842140},
		
		.iir_coef[2].coef_b={132746107,   -263254540,    130599907},
		.iir_coef[2].coef_a={134217728,   -263254540,    129128286},	
		
		.iir_coef[3].coef_b={131402980,   -255575175,    125032243},
		.iir_coef[3].coef_a={ 134217728,   -255575175,    122217496},
		
		.iir_coef[4].coef_b={0x8000000,0,0},
		.iir_coef[4].coef_a={0x8000000,0,0},
		
		.iir_coef[5].coef_b={0x8000000,0,0},
		.iir_coef[5].coef_a={0x8000000,0,0},
		
/*		.fir_bypass_flag=1,
        .fir_len = AUD_COEF_LEN,
        .fir_coef =
        {
            32767,
        },
*/        
        .dac_gain_offset=0,
	 .adc_gain_offset=(0)*4,
    },

/*

Filter1_B=[     26719020,    -52852829,     26204379];
Filter1_A=[    134217728,   -210410903,     76474119];

Filter2_B=[    138909433,   -267471808,    128584365];
Filter2_A=[    134217728,   -267471808,    133276071];

Filter3_B=[    134542733,   -268138827,    133597115];
Filter3_A=[    134217728,   -268138827,    133922120];

Filter4_B=[    133541379,   -264235686,    130815458];
Filter4_A=[    134217728,   -264235686,    130139109];

*/

            
    .anc_cfg_fb_l = {
        .total_gain = 511,
			
		.iir_bypass_flag=0,
		.iir_counter=IIR_COUNTER_FB_L,
		
		.iir_coef[0].coef_b={26719020,    -52852829,     26204379},
		.iir_coef[0].coef_a={134217728,   -210410903,     76474119},
		
		.iir_coef[1].coef_b={138909433,   -267471808,    128584365},
		.iir_coef[1].coef_a={134217728,   -267471808,    133276071},
		
		.iir_coef[2].coef_b={134542733,   -268138827,    133597115},
		.iir_coef[2].coef_a={134217728,   -268138827,    133922120},	
		
		.iir_coef[3].coef_b={133541379,   -264235686,    130815458},
		.iir_coef[3].coef_a={134217728,   -264235686,    130139109},	
		
		.iir_coef[4].coef_b={0x8000000,0,0},
		.iir_coef[4].coef_a={0x8000000,0,0},
		
		.iir_coef[5].coef_b={0x8000000,0,0},
		.iir_coef[5].coef_a={0x8000000,0,0},	
		
/*		.fir_bypass_flag=1,
        .fir_len = AUD_COEF_LEN,
        .fir_coef =
        {
            32767,
        },
*/ 
		.dac_gain_offset=0,
		.adc_gain_offset=(0)*4,
    },
    .anc_cfg_fb_r = {
        .total_gain = 511,
			
		.iir_bypass_flag=0,
		.iir_counter=IIR_COUNTER_FB_R,
		
		.iir_coef[0].coef_b={26719020,    -52852829,     26204379},
		.iir_coef[0].coef_a={134217728,   -210410903,     76474119},
		
		.iir_coef[1].coef_b={138909433,   -267471808,    128584365},
		.iir_coef[1].coef_a={134217728,   -267471808,    133276071},
		
		.iir_coef[2].coef_b={134542733,   -268138827,    133597115},
		.iir_coef[2].coef_a={134217728,   -268138827,    133922120},	
		
		.iir_coef[3].coef_b={133541379,   -264235686,    130815458},
		.iir_coef[3].coef_a={134217728,   -264235686,    130139109},	
		
		.iir_coef[4].coef_b={0x8000000,0,0},
		.iir_coef[4].coef_a={0x8000000,0,0},
		
		.iir_coef[5].coef_b={0x8000000,0,0},
		.iir_coef[5].coef_a={0x8000000,0,0},	
		
		
/*		.fir_bypass_flag=1,
        .fir_len = AUD_COEF_LEN,
        .fir_coef =
        {
            32767,
        },
*/ 
        .dac_gain_offset=0,
	 .adc_gain_offset=(0)*4,
    },
#if (AUD_SECTION_STRUCT_VERSION == 2)



/*

Filter1_B=[     19847881,    -39594823,     19747071];
Filter1_A=[     16777216,    -33314517,     16538159];

Filter2_B=[      9442890,    -16603187,      7330251];
Filter2_A=[     16777216,    -16603187,        -4075];

Filter3_B=[     18107639,    -32779315,     14701642];
Filter3_A=[     16777216,    -32779315,     16032065];

Filter4_B=[     12666347,    -24058210,     11437046];
Filter4_A=[     16777216,    -32089673,     15357640];

Filter5_B=[     16466312,    -31915122,     15523589];
Filter5_A=[     16777216,    -31915122,     15212684];
*/




    .anc_cfg_mc_l = {
        .total_gain = 1228,
			
		.iir_bypass_flag=0,
		.iir_counter=5,
		
		.iir_coef[0].coef_b={19847881,    -39594823,     19747071},
		.iir_coef[0].coef_a={16777216,    -33314517,     16538159},
		
		.iir_coef[1].coef_b={9442890,    -16603187,      7330251},
		.iir_coef[1].coef_a={16777216,    -16603187,        -4075},
		
		.iir_coef[2].coef_b={18107639,    -32779315,     14701642},
		.iir_coef[2].coef_a={16777216,    -32779315,     16032065},	
		
		.iir_coef[3].coef_b={12666347,    -24058210,     11437046},
		.iir_coef[3].coef_a={16777216,    -32089673,     15357640},	
		
		.iir_coef[4].coef_b={16466312,    -31915122,     15523589},
		.iir_coef[4].coef_a={16777216,    -31915122,     15212684},
		
		.iir_coef[5].coef_b={0x8000000,0,0},
		.iir_coef[5].coef_a={0x8000000,0,0},
		

		.dac_gain_offset=0,
		.adc_gain_offset=(0)*4,
    },
    .anc_cfg_mc_r = {
        .total_gain = 1331,
			
		.iir_bypass_flag=0,
		.iir_counter=5,
		
		.iir_coef[0].coef_b={19847881,    -39594823,     19747071},
		.iir_coef[0].coef_a={16777216,    -33314517,     16538159},
		
		.iir_coef[1].coef_b={9442890,    -16603187,      7330251},
		.iir_coef[1].coef_a={16777216,    -16603187,        -4075},
		
		.iir_coef[2].coef_b={18107639,    -32779315,     14701642},
		.iir_coef[2].coef_a={16777216,    -32779315,     16032065},	
		
		.iir_coef[3].coef_b={12666347,    -24058210,     11437046},
		.iir_coef[3].coef_a={16777216,    -32089673,     15357640},	
		
		.iir_coef[4].coef_b={16466312,    -31915122,     15523589},
		.iir_coef[4].coef_a={16777216,    -31915122,     15212684},
		
		.iir_coef[5].coef_b={0x8000000,0,0},
		.iir_coef[5].coef_a={0x8000000,0,0},	
		

        .dac_gain_offset=0,
	    .adc_gain_offset=(0)*4,
    },
#endif

};

const struct_anc_cfg * anc_coef_list_50p7k[ANC_COEF_LIST_NUM] = {
    &AncFirCoef_50p7k_mode0,
};

const struct_anc_cfg * anc_coef_list_48k[ANC_COEF_LIST_NUM] = {
    &AncFirCoef_48k_mode0,
};

const struct_anc_cfg * anc_coef_list_44p1k[ANC_COEF_LIST_NUM] = {
    &AncFirCoef_44p1k_mode0,
};

const IIR_CFG_T audio_eq_iir_cfg = {
    .gain0 = 0,
    .gain1 = 0,
    .num = 5,
    .param = {
        {IIR_TYPE_PEAK, .0,   200,   2},
        {IIR_TYPE_PEAK, .0,   600,  2},
        {IIR_TYPE_PEAK, .0,   2000.0, 2},
        {IIR_TYPE_PEAK, .0,  6000.0, 2},
        {IIR_TYPE_PEAK, .0,  12000.0, 2}
    }
};

//hardware dac iir eq
const IIR_CFG_T audio_eq_hw_dac_iir_cfg = {
    .gain0 = 0,
    .gain1 = 0,
    .num = 8,
    .param = {
        {IIR_TYPE_PEAK, .0,   120,    2},
        {IIR_TYPE_PEAK, .0,   240.0,  2},
        {IIR_TYPE_PEAK, .0,   480.0,  2},
        {IIR_TYPE_PEAK, .0,   1000.0, 2},
        {IIR_TYPE_PEAK, .0,   2000.0, 2},
        {IIR_TYPE_PEAK, .0,   4000.0, 2},
        {IIR_TYPE_PEAK, .0,   8000.0, 2},
        {IIR_TYPE_PEAK, .0,   16000.0,2},
    }
};

const IIR_CFG_T * const POSSIBLY_UNUSED audio_eq_hw_dac_iir_cfg_list[EQ_DAC_IIR_LIST_NUM]={
    &audio_eq_hw_dac_iir_cfg,
};

//hardware adc iir eq
const IIR_CFG_T audio_eq_hw_adc_iir_adc_cfg = {
    .gain0 = 0,
    .gain1 = 0,
    .num = 0,
    .param = {
        {IIR_TYPE_PEAK, 0.0,   50.0,   0.7},
    }
};

const IIR_CFG_T * const POSSIBLY_UNUSED audio_eq_hw_adc_iir_cfg_list[EQ_ADC_IIR_LIST_NUM]={
    &audio_eq_hw_adc_iir_adc_cfg,
};

const FIR_CFG_T audio_eq_fir_cfg = {
    .gain = 0.f,
    .len = 128,
    .coef = {
        #include "res/eq/EQ_COEF.txt"
    }
};

const FIR_CFG_T speech_spk_eq_16k_cfg = {
    .gain = 0.f,
    .len = 99,
    .coef = {
        8589,15275,1642,-9312,-6090,-3375,-3148,-2896,-2284,-1742,-1330,-968,-650,-381,-157,26,171,284,368,427,465,485,491,484,468,445,416,384,350,315,280,245,212,181,153,127,102,81,62,45,31,19,8,0,-7,-12,-16,-19,-20,-21,-21,-21,-21,-20,-18,-17,-15,-14,-12,-11,-10,-8,-7,-6,-5,-4,-3,-2,-2,-1,-1,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,
    }
};

const FIR_CFG_T speech_spk_eq_8k_cfg = {
    .gain = 0.f,
    .len = 44,
    .coef = {
        26233,-11516,-8568,-5989,-3854,-2175,-924,-47,520,841,979,990,917,796,655,512,379,263,168,93,38,-1,-25,-39,-45,-45,-41,-36,-29,-23,-17,-12,-7,-4,-2,0,1,2,2,2,2,2,1,1,
    }
};
