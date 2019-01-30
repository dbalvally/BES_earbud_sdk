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
//#include "mbed.h"
#include <stdio.h>
#include <assert.h>

#include "cmsis_os.h"
#include "tgt_hardware.h"
#include "hal_uart.h"
#include "hal_timer.h"
#include "audioflinger.h"
#include "lockcqueue.h"
#include "hal_trace.h"
#include "hal_cmu.h"
#include "hal_chipid.h"
#include "analog.h"
#include "app_bt_stream.h"
#include "app_overlay.h"
#include "app_audio.h"
#include "app_utils.h"
#ifdef ANC_APP
#include "app_anc.h"
#endif
#ifdef __TWS__
#include "app_tws.h"
#include "app_tws_if.h"
#endif
#include "nvrecord.h"
#include "nvrecord_env.h"
#include "hal_codec.h"

#ifdef MEDIA_PLAYER_SUPPORT
#include "resources.h"
#include "app_media_player.h"
#endif
#ifdef __FACTORY_MODE_SUPPORT__
#include "app_factory_audio.h"
#endif

#if VOICE_DATAPATH
#include "app_voicepath.h"
#endif
#include "app_ring_merge.h"

#include "bt_drv.h"
#include "bt_xtal_sync.h"
#include "besbt.h"
#include "app_bt_func.h"

#include "me_api.h"

#include "a2dp_api.h"
#include "avdtp_api.h"
#include "avctp_api.h"
#include "avrcp_api.h"
#include "hfp_api.h"
#include "bt_drv_reg_op.h"

#include "cqueue.h"
#include "btapp.h"
#include "app_hfp.h"

#ifdef TWS_RBCODEC_PLAYER
#include "rbplay.h"
#endif

#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)
#include "app_key.h"
#include "player_role.h"
#endif

#if defined(__SW_IIR_EQ_PROCESS__) || defined(__HW_FIR_EQ_PROCESS__)|| defined(__HW_CODEC_IIR_EQ_PROCESS__)
#include "audio_eq.h"
#endif

#include "ble_tws.h"

extern uint8_t bt_audio_updata_eq_for_anc(uint8_t anc_status);

#include "app_bt_media_manager.h"

#include "string.h"
#include "hal_location.h"
#include "hal_codec.h"
#include "bt_drv_interface.h"

#include "audio_resample_ex.h"
#include "hal_sleep.h"

#define BT_INIT_XTAL_SYNC_MIN (0)
#define BT_INIT_XTAL_SYNC_MAX (0xff - BT_INIT_XTAL_SYNC_MIN)

#if defined(AUDIO_ANC_FB_MC) && defined(ANC_APP) && !defined(__AUDIO_RESAMPLE__)
#include"anc_process.h"

#define DELAY_SAMPLE_MC (36*2)     //  2:ch
static int32_t delay_buf_bt[DELAY_SAMPLE_MC];
#endif

enum PLAYER_OPER_T
{
    PLAYER_OPER_START,
    PLAYER_OPER_STOP,
    PLAYER_OPER_RESTART,
};


extern "C" 
{
    int talk_thru_monitor_init(void);
    int talk_thru_monitor_run(short *pcm_buf_left, short *pcm_buf_right, int pcm_len);
    int talk_thru_monitor_deinit(void);
    void bt_peak_detector_run(short *buf, uint32_t len);
    void bt_peak_detector_sbc_init(void);
}


#if (AUDIO_OUTPUT_VOLUME_DEFAULT < 1) || (AUDIO_OUTPUT_VOLUME_DEFAULT > 17)
#error "AUDIO_OUTPUT_VOLUME_DEFAULT out of range"
#endif
int8_t stream_local_volume = (AUDIO_OUTPUT_VOLUME_DEFAULT);

struct btdevice_volume *btdevice_volume_p;

#ifdef __BT_ANC__
uint8_t bt_sco_samplerate_ratio = 0;
static uint8_t *bt_anc_sco_dec_buf;
extern void us_fir_init(void);
extern uint32_t voicebtpcm_pcm_resample (short* src_samp_buf, uint32_t src_smpl_cnt, short* dst_samp_buf);
#endif

#if defined(AUDIO_ANC_FB_MC) && defined(ANC_APP) && !defined(__AUDIO_RESAMPLE__)
static enum AUD_BITS_T sample_size_play_bt;
static enum AUD_SAMPRATE_T sample_rate_play_bt;
static uint32_t data_size_play_bt;

static uint8_t *playback_buf_bt;
static uint32_t playback_size_bt;
static int32_t playback_samplerate_ratio_bt;

static uint8_t *playback_buf_mc;
static uint32_t playback_size_mc;
static enum AUD_CHANNEL_NUM_T  playback_ch_num_bt;
#endif

//#if defined(__THIRDPARTY)
extern "C" uint8_t is_sbc_mode (void);
static uint8_t bt_sbc_mode;
extern "C" uint8_t __attribute__((section(".fast_text_sram")))  is_sbc_mode(void)
{
    return bt_sbc_mode;
}

extern "C" uint8_t is_sco_mode (void);

static uint8_t bt_sco_mode;
extern "C"   uint8_t __attribute__((section(".fast_text_sram")))  is_sco_mode(void) 
{
    return bt_sco_mode;
}

uint32_t a2dp_audio_more_data(uint8_t *buf, uint32_t len);
int a2dp_audio_init(void);
int a2dp_audio_deinit(void);

enum AUD_SAMPRATE_T a2dp_sample_rate = AUD_SAMPRATE_48000;

#if 0
int bt_sbc_player_setup(uint8_t freq)
{
    struct AF_STREAM_CONFIG_T *stream_cfg = NULL;
    static uint8_t sbc_samp_rate = 0xff;
    uint32_t ret;

    if (sbc_samp_rate == freq)
        return 0;

    switch (freq) {
        case A2D_SBC_IE_SAMP_FREQ_16:
        case A2D_SBC_IE_SAMP_FREQ_32:
        case A2D_SBC_IE_SAMP_FREQ_48:
            a2dp_sample_rate = AUD_SAMPRATE_48000;
            break;
        case A2D_SBC_IE_SAMP_FREQ_44:
            a2dp_sample_rate = AUD_SAMPRATE_44100;
            break;
        default:
            break;
    }

    ret = af_stream_get_cfg(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK, &stream_cfg, true);
    if (ret == 0) {
        stream_cfg->sample_rate = a2dp_sample_rate;
        af_stream_setup(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK, stream_cfg);
    }

#if defined(AUDIO_ANC_FB_MC) && defined(ANC_APP) && !defined(__AUDIO_RESAMPLE__)
    ret = af_stream_get_cfg(AUD_STREAM_ID_2, AUD_STREAM_PLAYBACK, &stream_cfg, true);
    if (ret == 0) {
        stream_cfg->sample_rate = a2dp_sample_rate;
        sample_rate_play_bt=stream_cfg->sample_rate;        
        af_stream_setup(AUD_STREAM_ID_2, AUD_STREAM_PLAYBACK, stream_cfg);
        anc_mc_run_setup(hal_codec_anc_convert_rate(sample_rate_play_bt));
    }
#endif
    sbc_samp_rate = freq;

    return 0;
}
#else

void bt_sbc_player_retrigger(uint32_t trigger_time)
{
    LOG_PRINT_BT_STREAM("[%s] trigger_time = %d", __func__, trigger_time);

    af_stream_pause(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK);
    app_tws_set_trigger_time(trigger_time, true);
    af_stream_restart(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK);
}

void bt_sbc_player_restart(uint32_t trigger_time)
{
    LOG_PRINT_BT_STREAM("[%s] trigger_time = %d", __func__, trigger_time);

    af_stream_pause(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK);
    app_tws_set_trigger_time(trigger_time, true);
#ifdef __TWS_USE_PLL_SYNC__    
    a2dp_reset_all_sync_pll_para();
#endif
    af_stream_restart(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK);
}

void bt_sbc_player_pause()
{
    LOG_PRINT_BT_STREAM("[%s]...", __func__);

    af_stream_pause(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK);
}

int bt_sbc_player_setup(uint8_t opt, uint32_t param)
{
    switch (opt)
    {
        case APP_BT_SBC_PLAYER_TRIGGER:
            bt_sbc_player_retrigger(param);
            break;
        case APP_BT_SBC_PLAYER_RESTART:
            bt_sbc_player_restart(param);
            break;
        case APP_BT_SBC_PLAYER_PAUSE:
            bt_sbc_player_pause();
            break;
        default:
            ASSERT(0, "[%s] is invalid opt", __func__, opt);
            break;
    }

    return 0;
}


#endif


enum AUD_SAMPRATE_T bt_parse_sbc_sample_rate(uint8_t sbc_samp_rate)
{
    enum AUD_SAMPRATE_T sample_rate;
    sbc_samp_rate = sbc_samp_rate & A2D_SBC_IE_SAMP_FREQ_MSK;

    switch (sbc_samp_rate)
    {
        case A2D_SBC_IE_SAMP_FREQ_16:
        case A2D_SBC_IE_SAMP_FREQ_32:
        case A2D_SBC_IE_SAMP_FREQ_48:
            sample_rate = AUD_SAMPRATE_48000;
            break;
        case A2D_SBC_IE_SAMP_FREQ_44:
            sample_rate = AUD_SAMPRATE_44100;
            break;
#if defined(A2DP_LHDC_ON)           
        case A2D_SBC_IE_SAMP_FREQ_96:
            sample_rate = AUD_SAMPRATE_96000;
            break;
#endif          
        default:
           ASSERT(0, "[%s] 0x%x is invalid", __func__, sbc_samp_rate);

            break;
    }
    return sample_rate;
}

void bt_store_sbc_sample_rate(enum AUD_SAMPRATE_T sample_rate)
{
    a2dp_sample_rate = sample_rate;
}

enum AUD_SAMPRATE_T bt_get_sbc_sample_rate(void)
{
    return a2dp_sample_rate;
}

enum AUD_SAMPRATE_T bt_parse_store_sbc_sample_rate(uint8_t sbc_samp_rate)
{
    enum AUD_SAMPRATE_T sample_rate;

    sample_rate = bt_parse_sbc_sample_rate(sbc_samp_rate);
    bt_store_sbc_sample_rate(sample_rate);

    return sample_rate;
}

void merge_stereo_to_mono_16bits(int16_t *src_buf, int16_t *dst_buf,  uint32_t src_len)
{
    uint32_t i = 0;
    for (i = 0; i < src_len; i+=2) {
        dst_buf[i] = (src_buf[i]>>1) + (src_buf[i+1]>>1);
        dst_buf[i+1] = dst_buf[i];
    }
}

#ifdef __TWS__
extern uint32_t tws_audout_pcm_more_data(uint8_t * buf, uint32_t len);

extern  tws_dev_t  tws;
extern "C" void OS_NotifyEvm(void);


#if defined(__AUDIO_RESAMPLE__) && defined(SW_PLAYBACK_RESAMPLE)
#define TWS_RESAMPLE_ITER_NUM               512
static struct APP_RESAMPLE_T *tws_resample;

static int tws_resample_iter(uint8_t *buf, uint32_t len)
{
    int ret;

    ret = tws.pcmbuff.read(&(tws.pcmbuff),buf,len);

    return ret;
}

struct APP_RESAMPLE_T*  tws_get_resample(void)
{
    return tws_resample;
}
#else
struct APP_RESAMPLE_T*  tws_get_resample(void)
{
    return NULL;
}
#endif

#if defined(TWS_SBC_PATCH_SAMPLES)
static uint32_t curr_sample_counter = 0;
#endif

#ifdef __TWS_USE_PLL_SYNC__
SRAM_TEXT_LOC void tws_set_pll_adjust_para(uint8_t shift_dirction)
{
    uint8_t shift_signal = 0;
    uint8_t pll_shift_num = 0;
    tws.audio_adjust_direction = shift_dirction;
    tws.audio_dma_trigger_start_counter = 0;
    
    if(shift_dirction == A2DPPLAY_SYNC_ADJ_INC){
        //for tws should set shift max feild
        if(tws.audio_pll_shift < A2DPPLAY_SYNC_TWS_MAX_SHIFT)
            tws.audio_pll_shift += 2;
    }
    else if(shift_dirction == A2DPPLAY_SYNC_ADJ_DEC){
        if(tws.audio_pll_shift > -A2DPPLAY_SYNC_TWS_DEC_MAX_SHIFT)
            tws.audio_pll_shift -= 1;
    }else{
        if(tws.audio_pll_shift > 0)
            tws.audio_pll_shift -= 1;
        else if(tws.audio_pll_shift < 0)
            tws.audio_pll_shift += 1;
    }
    
    if(tws.audio_pll_shift < 0){
        shift_signal = 1;
        pll_shift_num = -tws.audio_pll_shift;
    }else
        pll_shift_num  = tws.audio_pll_shift;
    
    tws.audio_dma_trigger_num = tws.audio_dma_count + A2DPPLAY_SYNC_DELAY_NUMBER;
    tws.audio_pll_cfg = 0x80 | ((A2DPPLAY_SYNC_STATUS_SET << 4) & 0x70) |((shift_signal?1:0) << 3) | (pll_shift_num & 0x07); 
    tws.audio_pll_monitor_enable = 0;
    tws.audio_pll_pcm_monitor_counter = 0;
    tws.audio_pll_first_pcm_mointor_length = 0;
    tws.audio_pll_first_a2dp_buffer_mointor_length = 0;
    tws.audio_pll_shift_ok = 0;
    tws.audio_pll_monitor_cycle_number = 0;
    tws.audio_pll_syn_enable = 1;
}

SRAM_TEXT_LOC void tws_pll_adjust_monitor_action(uint8_t shift_dirction)
{
    uint8_t shift_signal = 0;
    uint8_t pll_shift_num = 0;
    if(tws.audio_pll_monitor_enable){
        if(!(tws.audio_pll_cfg & 0x80)){
            //moniter pcm length each 30 irq
            if(tws.audio_pll_pcm_monitor_index < A2DPPLAY_SYNC_MONITOR_ADJ_IRQ_NUM){
                tws.audio_pll_pcm_monitor_counter  += LengthOfCQueue(&tws.pcmbuff.queue);
                if(tws.mobile_codectype == BTIF_AVDTP_CODEC_TYPE_SBC){
                    tws.audio_pll_a2dp_buffer_monitor_counter  += LengthOfCQueue(&tws.a2dpbuff.queue);
                }else{
                    tws.audio_pll_a2dp_buffer_monitor_counter  += tws.a2dpbuff.buffer_count;
                }
                tws.audio_pll_pcm_monitor_index ++;
            }else{
                //one monitor cycle
                tws.audio_pll_monitor_cycle_number += 1;
                
                if((tws.audio_pll_pcm_monitor_counter > 0) && tws.audio_pll_first_pcm_mointor_length > 0){
                    if(shift_dirction == A2DPPLAY_SYNC_ADJ_INC){
                        if((tws.audio_pll_pcm_monitor_counter > tws.audio_pll_first_pcm_mointor_length)){
                            //adjust pll ok
                            tws.audio_pll_first_pcm_mointor_length = tws.audio_pll_pcm_monitor_counter;
                            tws.audio_pll_shift_ok += 1;
                        }else{
                            if(tws.audio_pll_pcm_monitor_counter == tws.audio_pll_first_pcm_mointor_length){
                                //check a2dp buffer
                                if(tws.audio_pll_a2dp_buffer_monitor_counter > tws.audio_pll_first_a2dp_buffer_mointor_length){
                                    tws.audio_pll_first_a2dp_buffer_mointor_length = tws.audio_pll_a2dp_buffer_monitor_counter;
                                    tws.audio_pll_shift_ok += 1;
                                }
                                else{
                                    //adjust pll failed
                                    tws.audio_pll_shift_ok -= 1;
                                }
                            }else{
                                //adjust pll failed
                                tws.audio_pll_shift_ok -= 1;
                            }
                        }
                    }else{
                        if(tws.audio_pll_pcm_monitor_counter < tws.audio_pll_first_pcm_mointor_length){
                            //adjust pll ok
                            tws.audio_pll_shift_ok += 1;
                            tws.audio_pll_first_pcm_mointor_length = tws.audio_pll_pcm_monitor_counter;
                        }else{
                            //check a2dp buffer
                            if(tws.audio_pll_pcm_monitor_counter == tws.audio_pll_first_pcm_mointor_length){
                                if(tws.audio_pll_a2dp_buffer_monitor_counter < tws.audio_pll_first_a2dp_buffer_mointor_length){
                                    tws.audio_pll_first_a2dp_buffer_mointor_length = tws.audio_pll_a2dp_buffer_monitor_counter;
                                    tws.audio_pll_shift_ok += 1;
                                }
                                else
                                    tws.audio_pll_shift_ok -= 1;
                            }else{
                                //adjust pll failed
                                tws.audio_pll_shift_ok -= 1;
                        }
                    }
                }
            }
#if 0                
            TRACE("shift monitor %d %d %d %d %d %d %d",LengthOfCQueue(&tws.pcmbuff.queue),  LengthOfCQueue(&tws.a2dpbuff.queue),
                                                                tws.audio_pll_first_pcm_mointor_length, 
                                                                tws.audio_pll_pcm_monitor_counter ,
                                                                tws.audio_pll_first_a2dp_buffer_mointor_length, 
                                                                tws.audio_pll_a2dp_buffer_monitor_counter,
                                                                tws.audio_dma_index);
#endif
            //remember first pcm moniter
            if(tws.audio_pll_monitor_first_cycle == 0){
                tws.audio_pll_first_pcm_mointor_length = tws.audio_pll_pcm_monitor_counter;
                tws.audio_pll_first_a2dp_buffer_mointor_length = tws.audio_pll_a2dp_buffer_monitor_counter;
                tws.audio_pll_monitor_first_cycle = 1;
            }
            
            tws.audio_pll_pcm_monitor_index = 0;
            tws.audio_pll_pcm_monitor_counter = 0;
            tws.audio_pll_a2dp_buffer_monitor_counter = 0;
        }

            if(tws.audio_pll_monitor_cycle_number >= A2DPPLAY_SYNC_MONITOR_IRQ_CIRCLE_NUM){
                //moniter pll adjust failed, should agression action
                if(tws.audio_pll_shift_ok < 0){
                    if(shift_dirction == A2DPPLAY_SYNC_ADJ_INC){
                        if(tws.audio_pll_shift < A2DPPLAY_SYNC_TWS_MAX_SHIFT)
                            tws.audio_pll_shift += 1;
                    }else{
                        if(tws.audio_pll_shift > -A2DPPLAY_SYNC_TWS_DEC_MAX_SHIFT)
                            tws.audio_pll_shift -= 1;                        
                    }
                    
                    if(tws.audio_pll_shift < 0){
                        shift_signal = 1;
                        pll_shift_num = -tws.audio_pll_shift;
                    }else
                        pll_shift_num  = tws.audio_pll_shift;

                    tws.audio_dma_trigger_num = tws.audio_dma_count + A2DPPLAY_SYNC_DELAY_NUMBER;
                    tws.audio_pll_cfg = 0x80 | ((A2DPPLAY_SYNC_STATUS_SET << 4) & 0x70) |((shift_signal?1:0) << 3) | (pll_shift_num & 0x07); 
                    //reset trigger start counter
                    tws.audio_dma_trigger_start_counter = 0;
                }
                LOG_PRINT_BT_STREAM("tshift circle %d", tws.audio_pll_shift_ok);
                //reset to start new monitor cycle
                tws.audio_pll_shift_ok = 0;
                tws.audio_pll_monitor_cycle_number = 0;
                tws.audio_pll_pcm_monitor_counter = 0;
                tws.audio_pll_syn_enable = 1;
            }
        }
    }  else{
        tws.audio_pll_monitor_first_cycle = 0;
    }
}

SRAM_TEXT_LOC void single_set_pll_adjust_para(uint8_t shift_dirction)
{
    tws.audio_adjust_direction = shift_dirction;
    tws.audio_pll_cfg = 0x80;
    tws.audio_dma_trigger_start_counter = 0;
    
    if(shift_dirction == A2DPPLAY_SYNC_ADJ_INC){
        if(tws.audio_pll_shift < A2DPPLAY_SYNC_TWS_MAX_SHIFT)
            tws.audio_pll_shift += 1;
    }
    else if(shift_dirction == A2DPPLAY_SYNC_ADJ_DEC){
        if(tws.audio_pll_shift > 1){
            tws.audio_pll_shift = 1;
        }else if(tws.audio_pll_shift > -A2DPPLAY_SYNC_TWS_DEC_MAX_SHIFT)
            tws.audio_pll_shift -= 1;
    }else{
        //idle
        if(tws.audio_pll_shift > 0)
            tws.audio_pll_shift -= 1;
        else if(tws.audio_pll_shift < 0)
            tws.audio_pll_shift += 1;
    }

    tws.audio_pll_monitor_enable = 0;
    tws.audio_pll_pcm_monitor_counter = 0;
    tws.audio_pll_first_pcm_mointor_length = 0;
    tws.audio_pll_first_a2dp_buffer_mointor_length = 0;
    tws.audio_pll_shift_ok = 0;
    tws.audio_pll_monitor_cycle_number = 0;   
}

SRAM_TEXT_LOC void single_pll_adjust_monitor_action(uint8_t shift_dirction)
{         
    if(tws.audio_pll_monitor_enable){
    //moniter pcm length each 30 irq
    if(tws.audio_pll_pcm_monitor_index < A2DPPLAY_SYNC_SINGLE_MONITOR_ADJ_IRQ_NUM){
        tws.audio_pll_pcm_monitor_counter  += LengthOfCQueue(&tws.pcmbuff.queue);
        if(tws.mobile_codectype == BTIF_AVDTP_CODEC_TYPE_SBC){
            tws.audio_pll_a2dp_buffer_monitor_counter  += LengthOfCQueue(&tws.a2dpbuff.queue);
        }else{
            tws.audio_pll_a2dp_buffer_monitor_counter  += tws.a2dpbuff.buffer_count;
        }
        tws.audio_pll_pcm_monitor_index ++;
    }else{
        //one monitor cycle
        tws.audio_pll_monitor_cycle_number += 1;
        
        if((tws.audio_pll_pcm_monitor_counter > 0) && tws.audio_pll_first_pcm_mointor_length > 0){
            if(shift_dirction == A2DPPLAY_SYNC_ADJ_INC){
                if((tws.audio_pll_pcm_monitor_counter > tws.audio_pll_first_pcm_mointor_length)){
                    //adjust pll ok
                    tws.audio_pll_shift_ok += 1;
                    tws.audio_pll_first_pcm_mointor_length = tws.audio_pll_pcm_monitor_counter;
                }else{
                    if(tws.audio_pll_pcm_monitor_counter == tws.audio_pll_first_pcm_mointor_length){
                        //check a2dp buffer
                        if(tws.audio_pll_a2dp_buffer_monitor_counter > tws.audio_pll_first_a2dp_buffer_mointor_length){
                            tws.audio_pll_shift_ok += 1;
                            tws.audio_pll_first_a2dp_buffer_mointor_length = tws.audio_pll_a2dp_buffer_monitor_counter;
                        }
                        else{
                            //adjust pll failed
                            tws.audio_pll_shift_ok -= 1;
                        }
                    }else{
                        //adjust pll failed
                        tws.audio_pll_shift_ok -= 1;
                    }
                }
            }else{
                if(tws.audio_pll_pcm_monitor_counter < tws.audio_pll_first_pcm_mointor_length){
                    //adjust pll ok
                    tws.audio_pll_first_pcm_mointor_length = tws.audio_pll_pcm_monitor_counter;
                    tws.audio_pll_shift_ok += 1;
                }else{
                    //check a2dp buffer
                    if(tws.audio_pll_pcm_monitor_counter == tws.audio_pll_first_pcm_mointor_length){
                        if(tws.audio_pll_a2dp_buffer_monitor_counter < tws.audio_pll_first_a2dp_buffer_mointor_length){
                            tws.audio_pll_shift_ok += 1;
                            tws.audio_pll_first_a2dp_buffer_mointor_length = tws.audio_pll_a2dp_buffer_monitor_counter;
                        }
                        else
                            tws.audio_pll_shift_ok -= 1;
                    }else{
                        //adjust pll failed
                        tws.audio_pll_shift_ok -= 1;
                    }
                }
            }
        }
#if 0        
        TRACE("shift monitor %d %d %d %d %d %d",LengthOfCQueue(&tws.pcmbuff.queue),  LengthOfCQueue(&tws.a2dpbuff.queue),
                                                                        tws.audio_pll_first_pcm_mointor_length, 
                                                                        tws.audio_pll_pcm_monitor_counter ,
                                                                        tws.audio_pll_first_a2dp_buffer_mointor_length, 
                                                                        tws.audio_pll_a2dp_buffer_monitor_counter);
#endif
        //remember first pcm moniter
        if(tws.audio_pll_monitor_first_cycle == 0){
            tws.audio_pll_first_pcm_mointor_length = tws.audio_pll_pcm_monitor_counter;
            tws.audio_pll_first_a2dp_buffer_mointor_length = tws.audio_pll_a2dp_buffer_monitor_counter;
            tws.audio_pll_monitor_first_cycle = 1;
        }
        
        tws.audio_pll_pcm_monitor_index = 0;
        tws.audio_pll_pcm_monitor_counter = 0;
        tws.audio_pll_a2dp_buffer_monitor_counter = 0;
    }

        if(tws.audio_pll_monitor_cycle_number >= A2DPPLAY_SYNC_SINGLE_MONITOR_IRQ_CIRCLE_NUM){
            //moniter pll adjust failed, should agression action
            if(tws.audio_pll_shift_ok < 0){
                if(shift_dirction == A2DPPLAY_SYNC_ADJ_INC){
                    if(tws.audio_pll_shift < A2DPPLAY_SYNC_TWS_MAX_SHIFT)
                        tws.audio_pll_shift += 1;
                }else{
                    if(tws.audio_pll_shift > -A2DPPLAY_SYNC_TWS_DEC_MAX_SHIFT)
                        tws.audio_pll_shift -= 1;                    
                }
                tws.audio_pll_cfg = 0x80;
                tws.audio_dma_trigger_start_counter = 0;
            }
            LOG_PRINT_BT_STREAM("sshift circle %d", tws.audio_pll_shift_ok);
            //reset to start new monitor cycle
            tws.audio_pll_shift_ok = 0;
            tws.audio_pll_monitor_cycle_number = 0;
            tws.audio_pll_pcm_monitor_counter = 0;           
        }
    }  else{
        tws.audio_pll_monitor_first_cycle = 0;
    }
}

#endif
extern btif_remote_device_t* mobileBtRemDev;
uint32_t tws_audout_pcm_more_data(uint8_t * buf, uint32_t len)
{
#if VOICE_DATAPATH
    if (app_voicepath_get_stream_pending_state(VOICEPATH_STREAMING))
    {          
        af_stream_start(AUD_STREAM_ID_0, AUD_STREAM_CAPTURE);
    #if (MIX_MIC_DURING_MUSIC)    
        app_voicepath_enable_hw_sidetone(0, HW_SIDE_TONE_MAX_ATTENUATION_COEF);
    #endif  
        app_voicepath_set_stream_state(VOICEPATH_STREAMING, true);
        app_voicepath_set_pending_started_stream(VOICEPATH_STREAMING, false);
    }
#endif   
    int ret = 0;
#ifdef __TWS_USE_PLL_SYNC__    
    int8_t pll_shift = 0;
    uint32_t a2dp_buffer_threshold_dec = 0;
    uint32_t a2dp_buffer_threshold_inc = 0;
    uint32_t a2dp_buffer_counter = 0;
#endif
    
    if(tws.wait_pcm_signal == true){
        tws.lock_prop(&tws);
        tws.wait_pcm_signal = false;
        tws.unlock_prop(&tws);
    }
#if __TWS_SYNC_WITH_DMA_COUNTER__
    LOG_PRINT_DMA_RESYNC("m:%d %d a:%d %d ins:%d d:%d %d a:%d %d t:%d\n", len, 
                LengthOfCQueue(&tws.pcmbuff.queue), 
                LengthOfCQueue(&tws.a2dpbuff.queue), 
                LengthOfCQueue(&tws.sbcbuff.queue),
                tws.audio_insert_pcm_num,
                tws.audio_dma_index,
                tws.audio_master_frame_index,
                tws.a2dpbuff.buffer_count,
                tws.sbcbuff.buffer_count,
                bt_syn_get_curr_ticks(app_tws_get_tws_conhdl())
                );
#endif      
#if defined(TWS_SBC_PATCH_SAMPLES)
    if ((tws.mobile_codectype == AVDTP_CODEC_TYPE_SBC)
            && (curr_sample_counter < TWS_SBC_PATCH_SAMPLES_CNT_NUM) && ((curr_sample_counter + len) >= TWS_SBC_PATCH_SAMPLES_CNT_NUM)) {
        ASSERT(len > TWS_SBC_PATCH_SAMPLES_PAT_NUM, "%s:len should > TWS_SBC_PATCH_SAMPLES_PAT_NUM", __func__);
        len -= TWS_SBC_PATCH_SAMPLES_PAT_NUM;
#ifdef __TWS_SYNC_WITH_DMA_COUNTER__
        if(tws->audio_insert_pcm_num > 0){
            if(tws->audio_insert_pcm_num >= len){
                memset(buf, 0, len);
                tws.audio_insert_pcm_num -= len;
            }else{
                memset(buf, 0, tws->audio_insert_pcm_num);
                ret = tws.pcmbuff.read(&(tws.pcmbuff),buf + tws->audio_insert_pcm_num, len - tws->audio_insert_pcm_num);
                tws.audio_insert_pcm_num = 0;
            }
        }else
            ret = tws.pcmbuff.read(&(tws.pcmbuff),buf,len);
#else
            ret = tws.pcmbuff.read(&(tws.pcmbuff),buf,len);
#endif
        memcpy(buf + len , buf + len - TWS_SBC_PATCH_SAMPLES_PAT_NUM, TWS_SBC_PATCH_SAMPLES_PAT_NUM);
        curr_sample_counter += len;
        curr_sample_counter %= TWS_SBC_PATCH_SAMPLES_CNT_NUM;
    }
    else
#endif
    {
#if defined(TWS_SBC_PATCH_SAMPLES)
        curr_sample_counter += len;
#endif
#if defined(__AUDIO_RESAMPLE__) && defined(SW_PLAYBACK_RESAMPLE)
#if defined(A2DP_AAC_ON)
        if(app_tws_get_codec_type() == BTIF_AVDTP_CODEC_TYPE_MPEG2_4_AAC)
        {
#ifdef __TWS_SYNC_WITH_DMA_COUNTER__
           if(tws.audio_insert_pcm_num > 0){
                if(tws.audio_insert_pcm_num >= len){
                    memset(buf, 0, len);
                    tws.audio_insert_pcm_num -= len;
                }else{
                    memset(buf, 0, tws.audio_insert_pcm_num);
                    ret = tws.pcmbuff.read(&(tws.pcmbuff),buf + tws.audio_insert_pcm_num, len - tws.audio_insert_pcm_num);
                    tws.audio_insert_pcm_num = 0;
                }
            }else
                ret = tws.pcmbuff.read(&(tws.pcmbuff),buf,len);
#else
                ret = tws.pcmbuff.read(&(tws.pcmbuff),buf,len);
#endif
        }
        else
#endif
        {
            ret = app_playback_resample_run(tws_resample, buf, len);
        }
#else
#ifdef __TWS_SYNC_WITH_DMA_COUNTER__
        if(tws.audio_insert_pcm_num > 0){
            if(tws.audio_insert_pcm_num >= len){
                memset(buf, 0, len);
                tws.audio_insert_pcm_num -= len;
            }else{
                memset(buf, 0, tws.audio_insert_pcm_num);
                ret = tws.pcmbuff.read(&(tws.pcmbuff),buf + tws.audio_insert_pcm_num, len - tws.audio_insert_pcm_num);
                tws.audio_insert_pcm_num = 0;
            }
        }else
            ret = tws.pcmbuff.read(&(tws.pcmbuff),buf,len);
#else
         ret = tws.pcmbuff.read(&(tws.pcmbuff),buf,len);
#endif

#endif
    }
    tws.pcmbuff.put_space(&(tws.pcmbuff));
    OS_NotifyEvm();

    if(ret) {
#if defined(__TWS_SYNC_WITH_DMA_COUNTER__)   
         if (TWS_SLAVE_CONN_MASTER == app_tws_get_conn_state() && tws.slave_player_restart_pending == false)
        {
          //audio drop slave should cal re-sync again
            if(!tws.audio_drop_resync){
                tws.audio_drop_resync = 1;
                tws.audio_drop_dma_irq_index = tws.audio_dma_index + 1;
                audio_drop_sync_master();           
            }
            memset(buf, 0, len);
        }else if(TWS_MASTER_CONN_SLAVE == app_tws_get_conn_state()){
              tws_pause_play();
#if defined(A2DP_LHDC_ON)
            if(tws.mobile_codectype == AVDTP_CODEC_TYPE_LHDC)
            {
                a2dp_audio_lhdc_init();
            }
#endif
            TRACE("audout underflow \n");
        }
#else
#if defined(__NOT_STOP_AUDIO_IN_UNDERFLOW__)
        tws_audout_underflow_process(len);
        ret = 0;
#else
        if (TWS_SLAVE_CONN_MASTER == app_tws_get_conn_state() && tws.slave_player_restart_pending == false)
        {
            tws.lock_prop(&tws);
            app_tws_reset_slave_medianum();
            tws.unlock_prop(&tws);
        }
        LOG_PRINT_BT_STREAM("audout underflow \n");
        ret = 0;
#endif //__NOT_STOP_AUDIO_IN_UNDERFLOW__
#endif

    }else{

#ifdef BT_XTAL_SYNC
#ifdef BT_XTAL_SYNC_NEW_METHOD
        uint32_t bitoffset = 0;
        if(TWS_SLAVE_CONN_MASTER == app_tws_get_conn_state())
        {
            bitoffset = btdrv_rf_bitoffset_get(btdrv_conhdl_to_linkid(app_tws_get_tws_conhdl()));
            bt_xtal_sync_new(bitoffset,BT_XTAL_SYNC_MODE_WITH_TWS);
        }
#ifdef __MASTER_FOLLOW_MOBILE__
        else if(TWS_MASTER_CONN_SLAVE == app_tws_get_conn_state())
        {
            bitoffset = btdrv_rf_bitoffset_get(btdrv_conhdl_to_linkid(btif_me_get_remote_device_hci_handle(mobileBtRemDev)));
            bt_xtal_sync_new(bitoffset,BT_XTAL_SYNC_MODE_WITH_MOBILE);
        }
#endif
#else
        bt_xtal_sync(BT_XTAL_SYNC_MODE_MUSIC);
#endif
#endif    
        if(tws.paused == false){
            audout_pcmbuff_thres_monitor();
        }

        ret = len;
    }
#if 0    
    if(tws.tws_mode == TWSSLAVE && slave_packet_count >=2000 && tws.slave_player_restart_pending == false)
    {
     //   tws.lock_prop(&tws);
        slave_packet_count = 0;
     //   app_tws_reset_slave_medianum();
     //   tws.unlock_prop(&tws);
        tws_player_pause_player_req(1);
        tws.slave_player_restart_pending = true;     
//        tws_player_stop();   
    }
#endif
#ifdef __TWS_USE_PLL_SYNC__
#if 0
    TRACE("m:%d %d a:%d %d %d c:%x %d %d %d m:%d\n", len, 
                LengthOfCQueue(&tws.pcmbuff.queue), 
                LengthOfCQueue(&tws.a2dpbuff.queue), 
                LengthOfCQueue(&tws.sbcbuff.queue),
                tws.a2dpbuff.buffer_count,
                tws.audio_pll_cfg,
                tws.audio_dma_trigger_num, 
                tws.audio_dma_count,  
                tws.audio_dma_trigger_start_counter,
                tws.audio_pll_monitor_enable);
#endif

    //in case aud close but irq not cancel. 
    {
        //tws mode
        if(app_tws_mode_is_master() || app_tws_mode_is_slave()){
            if(app_tws_is_master_mode()){
#if defined(A2DP_AAC_ON)                    
                if(tws.mobile_codectype == BTIF_AVDTP_CODEC_TYPE_MPEG2_4_AAC){
                    a2dp_buffer_threshold_inc = 8;   //1024samples*8
                    a2dp_buffer_threshold_dec = ((5*a2dp_buffer_threshold_inc)/2);
                    a2dp_buffer_counter = tws.a2dpbuff.buffer_count; 
                }else
#endif
#if defined(A2DP_LHDC_ON)
                if(tws.mobile_codectype == BTIF_AVDTP_CODEC_TYPE_LHDC){
                    a2dp_buffer_threshold_inc = 8;   //1024samples*8
                    a2dp_buffer_threshold_dec = ((5*a2dp_buffer_threshold_inc)/2);
                    a2dp_buffer_counter = tws.a2dpbuff.buffer_count; 
                }else
#endif
                {
                    a2dp_buffer_threshold_inc = (A2DP_BUFFER_SZ_TWS/2);   //1024samples*8
                    a2dp_buffer_threshold_dec = (3*A2DP_BUFFER_SZ_TWS/4);
                    a2dp_buffer_counter = LengthOfCQueue(&tws.a2dpbuff.queue);
                }
                    
                if(a2dp_buffer_counter < a2dp_buffer_threshold_inc){
                    //should play slow                    
                    if(!(tws.audio_pll_cfg & 0x80) && (tws.audio_adjust_direction != A2DPPLAY_SYNC_ADJ_INC)){
                        tws_set_pll_adjust_para(A2DPPLAY_SYNC_ADJ_INC);                  
                        LOG_PRINT_BT_STREAM("enable A2DPPLAY_SYNC_ADJ_INC");     
                    }
                    tws_pll_adjust_monitor_action(A2DPPLAY_SYNC_ADJ_INC);
                }else if((AvailableOfCQueue(&tws.pcmbuff.queue) <= 2*len) && 
                    (a2dp_buffer_counter >= a2dp_buffer_threshold_dec)){
                    //should play fast
                    if(!(tws.audio_pll_cfg & 0x80) && (tws.audio_adjust_direction != A2DPPLAY_SYNC_ADJ_DEC)){
                        tws_set_pll_adjust_para(A2DPPLAY_SYNC_ADJ_DEC);
                        LOG_PRINT_BT_STREAM("enable A2DPPLAY_SYNC_ADJ_DEC");
                    }
                    tws_pll_adjust_monitor_action(A2DPPLAY_SYNC_ADJ_DEC);
                }else{
                    //play common
                     if(!(tws.audio_pll_cfg & 0x80) && (tws.audio_adjust_direction != A2DPPLAY_SYNC_ADJ_IDLE)){
                        tws_set_pll_adjust_para(A2DPPLAY_SYNC_ADJ_IDLE);
                        LOG_PRINT_BT_STREAM("enable A2DPPLAY_SYNC_ADJ_IDLE");
                    }
                     //idle no need monitor
                }
            }
            //if audio_dma_count == audio_dma_trigger_num  should start adjust pll
            if((tws.audio_dma_count == tws.audio_dma_trigger_num)){
                LOG_PRINT_BT_STREAM("pll_cfg=%x", tws.audio_pll_cfg);
                if(tws.audio_pll_cfg & 0x80){
                    if(((app_tws_is_master_mode()) && (tws.audio_pll_sync_trigger_success == 1))
                        || (app_tws_is_slave_mode())){
                        uint8_t pll_shift_para = 0;                
                        pll_shift_para = tws.audio_pll_cfg & 0xf;
                        LOG_PRINT_BT_STREAM("psync:%d c:%d s:%d sf:%x", tws.audio_dma_index, tws.audio_dma_count, (tws.audio_pll_cfg >> 4)&0x07, pll_shift_para);
                        //check sinal bit
                        if(pll_shift_para & 0x8){
                            pll_shift = -(pll_shift_para&0x07);
                        }else
                            pll_shift = (pll_shift_para&0x07);
                        a2dp_audio_sync_proc((tws.audio_pll_cfg >> 4)&0x07, pll_shift);
                        //reset monitor counter
                        tws.audio_pll_pcm_monitor_index = 0;
                        tws.audio_pll_pcm_monitor_counter = 0;
                        tws.audio_pll_shift_ok = 0;
                        tws.audio_pll_monitor_cycle_number = 0;
                        tws.audio_pll_monitor_enable = 1;                    
                        //reset dma start count
                        tws.audio_dma_trigger_start_counter = 0;
                    }
                }
            }
            
            if(tws.audio_pll_cfg & 0x80){
                if(tws.audio_dma_trigger_start_counter > A2DPPLAY_SYNC_ADJ_MINI_INTERVAL_NUMBER){
                    if(((app_tws_is_master_mode()) && (tws.audio_pll_sync_trigger_success == 1))
                        || (app_tws_is_slave_mode())){
                        //reset to start a new adjust process
                        tws.audio_pll_cfg = 0;
                        tws.audio_dma_trigger_start_counter = 0;
                    }
                }else
                    tws.audio_dma_trigger_start_counter++;
            }
            //dma count will swap each 0xff
            tws.audio_dma_count ++;           
        }else{
            //single mode
#if defined(A2DP_AAC_ON)                    
                if(tws.mobile_codectype == BTIF_AVDTP_CODEC_TYPE_MPEG2_4_AAC){
                    a2dp_buffer_threshold_inc = 4;   //1024samples*4
                    a2dp_buffer_threshold_dec = ((5*a2dp_buffer_threshold_inc)/2);
                    a2dp_buffer_counter = tws.a2dpbuff.buffer_count; 
                }else
#endif
#if defined(A2DP_LHDC_ON)
                if(tws.mobile_codectype == BTIF_AVDTP_CODEC_TYPE_LHDC){
                    a2dp_buffer_threshold_inc = 4;   //1024samples*4
                    a2dp_buffer_threshold_dec = ((5*a2dp_buffer_threshold_inc)/2);
                    a2dp_buffer_counter = tws.a2dpbuff.buffer_count; 
                }else
#endif
                {
                    a2dp_buffer_threshold_inc = (A2DP_BUFFER_SZ_TWS/2);   //1024samples*8
                    a2dp_buffer_threshold_dec = (3*A2DP_BUFFER_SZ_TWS/4);
                    a2dp_buffer_counter = LengthOfCQueue(&tws.a2dpbuff.queue);
                }
                            
            if(a2dp_buffer_counter < a2dp_buffer_threshold_inc){
                 if(!(tws.audio_pll_cfg & 0x80) &&  (tws.audio_adjust_direction != A2DPPLAY_SYNC_ADJ_INC)){
                    single_set_pll_adjust_para(A2DPPLAY_SYNC_ADJ_INC);
                    LOG_PRINT_BT_STREAM("enable A2DPPLAY_SYNC_ADJ_INC");     
                 }        
                 single_pll_adjust_monitor_action(A2DPPLAY_SYNC_ADJ_INC);
            }else if((AvailableOfCQueue(&tws.pcmbuff.queue) <= 2*len) && 
                    (a2dp_buffer_counter >= a2dp_buffer_threshold_dec)){
                 if(!(tws.audio_pll_cfg & 0x80) &&  (tws.audio_adjust_direction != A2DPPLAY_SYNC_ADJ_DEC)){
                    single_set_pll_adjust_para(A2DPPLAY_SYNC_ADJ_DEC);
                    LOG_PRINT_BT_STREAM("enable A2DPPLAY_SYNC_ADJ_DEC");     
                 }
                  single_pll_adjust_monitor_action(A2DPPLAY_SYNC_ADJ_DEC);
            }else{
                if(!(tws.audio_pll_cfg & 0x80) &&  (tws.audio_adjust_direction != A2DPPLAY_SYNC_ADJ_IDLE)){
                    single_set_pll_adjust_para(A2DPPLAY_SYNC_ADJ_IDLE);
                    LOG_PRINT_BT_STREAM("enable A2DPPLAY_SYNC_ADJ_IDLE");     
                }       
            }

            if(tws.audio_pll_cfg & 0x80){
                if(!tws.audio_pll_monitor_enable){
                    if(tws.audio_dma_trigger_start_counter > A2DPPLAY_SYNC_SINGLE_ADJ_MINI_INTERVAL_NUMBER){
                        tws.audio_pll_cfg = 0;
                        //reset monitor counter
                        tws.audio_pll_pcm_monitor_index = 0;
                        tws.audio_pll_pcm_monitor_counter = 0;
                        tws.audio_pll_shift_ok = 0;
                        tws.audio_pll_monitor_cycle_number = 0;
                        tws.audio_pll_monitor_enable = 1;
                        a2dp_audio_sync_proc(A2DPPLAY_SYNC_STATUS_SET, tws.audio_pll_shift);
                        tws.audio_dma_trigger_start_counter = 0;
                    }else
                        tws.audio_dma_trigger_start_counter ++;
                }else{
                    //if mointor need pll config set at once
                    tws.audio_pll_cfg = 0;
                    a2dp_audio_sync_proc(A2DPPLAY_SYNC_STATUS_SET, tws.audio_pll_shift);
                }
            }
        } 
    }
#endif

#if defined(__TWS_USE_PLL_SYNC__) || defined(__TWS_SYNC_WITH_DMA_COUNTER__)
    tws.audio_dma_index ++;
#endif
    return ret;
}


#endif

FRAM_TEXT_LOC uint32_t bt_sbc_player_more_data(uint8_t *buf, uint32_t len)
{
#ifdef BT_XTAL_SYNC
#ifndef __TWS__
    bt_xtal_sync(BT_XTAL_SYNC_MODE_MUSIC);
#endif
#endif

#ifdef A2DP_EQ_24BIT
    // Change len to 16-bit sample buffer length
    len = len / (sizeof(int32_t) / sizeof(int16_t));
#endif

    //--------------------------------------------------------------
    // Start of normal 16-bit processing
    //--------------------------------------------------------------

#ifdef __TWS__
    tws_audout_pcm_more_data(buf, len);
    app_ring_merge_more_data(buf, len);
#else
    a2dp_audio_more_data(buf, len);
    app_ring_merge_more_data(buf, len);
#ifdef __AUDIO_OUTPUT_MONO_MODE__
    merge_stereo_to_mono_16bits((int16_t *)buf, (int16_t *)buf, len/2);
#endif
#endif

    //--------------------------------------------------------------
    // End of normal 16-bit processing
    //--------------------------------------------------------------

#ifdef A2DP_EQ_24BIT
    int samp_cnt = len / sizeof(int16_t);
    int16_t *samp16 = (int16_t *)buf;
    int32_t *samp32 = (int32_t *)buf;

    // Convert 16-bit sample to 24-bit sample
    for (int i = samp_cnt - 1; i >= 0; i--) {
        samp32[i] = samp16[i] << 8;
    }

    // Restore len to 24-bit sample buffer length
    len = len * (sizeof(int32_t) / sizeof(int16_t));
#endif

#ifdef __HEAR_THRU_PEAK_DET__
#if 0
#ifdef ANC_APP
    if(app_anc_work_status())
#endif
    {
        if(bt_peak_detector_run((short *)buf, len/2) == PDK_RUN_STATUS_RUN){            
#if 0
#ifdef _FB_ANTI_HOWL_
            if (anc_anti_howl_start  == false){
                fb_anti_how_init_buff();
                fb_anti_howl_start();
            }
#endif
#endif
        }
    }
#else
    bt_peak_detector_run((short *)buf, len/2);
#endif
#endif

#if defined(__SW_IIR_EQ_PROCESS__) || defined(__HW_FIR_EQ_PROCESS__) || defined(__AUDIO_DRC__) || defined(__HW_CODEC_IIR_EQ_PROCESS__)
#ifdef ANC_APP
    bt_audio_updata_eq_for_anc(app_anc_work_status());
#endif
    audio_eq_run(buf, len);
#endif

#ifndef __TWS__
    OS_NotifyEvm();
#endif

    return len;
}

#if defined(AUDIO_ANC_FB_MC) && defined(ANC_APP) && !defined(__AUDIO_RESAMPLE__)
static uint32_t audio_mc_data_playback_a2dp(uint8_t *buf, uint32_t mc_len_bytes)
{
   uint32_t begin_time;
   //uint32_t end_time;
   begin_time = hal_sys_timer_get();
   LOG_PRINT_BT_STREAM("music cancel: %d",begin_time);

   float left_gain;
   float right_gain;
   int playback_len_bytes,mc_len_bytes_8;
   int i,j,k;   
   int delay_sample;

   hal_codec_get_dac_gain(&left_gain,&right_gain);

   //LOG_PRINT_BT_STREAM("playback_samplerate_ratio:  %d",playback_samplerate_ratio);
   
  // LOG_PRINT_BT_STREAM("left_gain:  %d",(int)(left_gain*(1<<12)));
  // LOG_PRINT_BT_STREAM("right_gain: %d",(int)(right_gain*(1<<12)));
  
   playback_len_bytes=mc_len_bytes/playback_samplerate_ratio_bt;

   mc_len_bytes_8=mc_len_bytes/8;

    if (sample_size_play_bt == AUD_BITS_16)
    {
        int16_t *sour_p=(int16_t *)(playback_buf_bt+playback_size_bt/2);
        int16_t *mid_p=(int16_t *)(buf);
        int16_t *mid_p_8=(int16_t *)(buf+mc_len_bytes-mc_len_bytes_8);
        int16_t *dest_p=(int16_t *)buf;

        if(buf == playback_buf_mc)
        {
            sour_p=(int16_t *)playback_buf_bt;
        }
        
        delay_sample=DELAY_SAMPLE_MC;
            
        for(i=0,j=0;i<delay_sample;i=i+2)
        {
            mid_p[j++]=delay_buf_bt[i];
            mid_p[j++]=delay_buf_bt[i+1];
        }

        for(i=0;i<playback_len_bytes/2-delay_sample;i=i+2)
        {
            mid_p[j++]=sour_p[i];
            mid_p[j++]=sour_p[i+1];
        }  

        for(j=0;i<playback_len_bytes/2;i=i+2)
        {
            delay_buf_bt[j++]=sour_p[i];
            delay_buf_bt[j++]=sour_p[i+1];
        }

        if(playback_samplerate_ratio_bt<=8)
        {
            for(i=0,j=0;i<playback_len_bytes/2;i=i+2*(8/playback_samplerate_ratio_bt))
            {
                mid_p_8[j++]=mid_p[i];
                mid_p_8[j++]=mid_p[i+1];
            }
        }
        else
        {
            for(i=0,j=0;i<playback_len_bytes/2;i=i+2)
            {
                for(k=0;k<playback_samplerate_ratio_bt/8;k++)
                {
                    mid_p_8[j++]=mid_p[i];
                    mid_p_8[j++]=mid_p[i+1];
                }
            }
        }
        
        anc_mc_run_stereo((uint8_t *)mid_p_8,mc_len_bytes_8,left_gain,right_gain,AUD_BITS_16);

        for(i=0,j=0;i<(int)(mc_len_bytes_8)/2;i=i+2)
        {
            for(k=0;k<8;k++)
            {
                dest_p[j++]=mid_p_8[i];
                dest_p[j++]=mid_p_8[i+1];
            }
        }
        
    }
    else if (sample_size_play_bt == AUD_BITS_24)
    {
        int32_t *sour_p=(int32_t *)(playback_buf_bt+playback_size_bt/2);
        int32_t *mid_p=(int32_t *)(buf);   
        int32_t *mid_p_8=(int32_t *)(buf+mc_len_bytes-mc_len_bytes_8);        
        int32_t *dest_p=(int32_t *)buf;

        if(buf == (playback_buf_mc))
        {
            sour_p=(int32_t *)playback_buf_bt;
        }
        
        delay_sample=DELAY_SAMPLE_MC;
            
        for(i=0,j=0;i<delay_sample;i=i+2)
        {
            mid_p[j++]=delay_buf_bt[i];
            mid_p[j++]=delay_buf_bt[i+1];
         
        }

         for(i=0;i<playback_len_bytes/4-delay_sample;i=i+2)
        {
            mid_p[j++]=sour_p[i];
            mid_p[j++]=sour_p[i+1];
        }  

         for(j=0;i<playback_len_bytes/4;i=i+2)
        {
            delay_buf_bt[j++]=sour_p[i];
            delay_buf_bt[j++]=sour_p[i+1];
        }
         
        if(playback_samplerate_ratio_bt<=8)
        {
            for(i=0,j=0;i<playback_len_bytes/4;i=i+2*(8/playback_samplerate_ratio_bt))
            {
                mid_p_8[j++]=mid_p[i];
                mid_p_8[j++]=mid_p[i+1];
            }
        }
        else
        {
            for(i=0,j=0;i<playback_len_bytes/4;i=i+2)
            {
                for(k=0;k<playback_samplerate_ratio_bt/8;k++)
                {
                    mid_p_8[j++]=mid_p[i];
                    mid_p_8[j++]=mid_p[i+1];
                }
            }
        }

        anc_mc_run_stereo((uint8_t *)mid_p_8,mc_len_bytes_8,left_gain,right_gain,AUD_BITS_24);

        for(i=0,j=0;i<(mc_len_bytes_8)/4;i=i+2)
        {
            for(k=0;k<8;k++)
            {
                dest_p[j++]=mid_p_8[i];
                dest_p[j++]=mid_p_8[i+1];
            }
        }

    }

  //  end_time = hal_sys_timer_get();

 //   LOG_PRINT_BT_STREAM("%s:run time: %d", __FUNCTION__, end_time-begin_time);

    return 0;
}
#endif

uint8_t bt_audio_updata_eq_for_anc(uint8_t anc_status)
{
#ifdef ANC_APP
    static uint8_t anc_status_record = 0xff;

    anc_status = app_anc_work_status();
    if(anc_status_record != anc_status)
    {
        anc_status_record = anc_status;
        LOG_PRINT_BT_STREAM("[%s]anc_status = %d", __func__, anc_status);
/*
#ifdef __SW_IIR_EQ_PROCESS__
        bt_audio_set_eq(AUDIO_EQ_TYPE_SW_IIR,bt_audio_get_eq_index(AUDIO_EQ_TYPE_SW_IIR,anc_status));
#endif

#ifdef __HW_FIR_EQ_PROCESS__
        bt_audio_set_eq(AUDIO_EQ_TYPE_HW_FIR,bt_audio_get_eq_index(AUDIO_EQ_TYPE_HW_FIR,anc_status));
#endif

#ifdef __HW_DAC_IIR_EQ_PROCESS__
        bt_audio_set_eq(AUDIO_EQ_TYPE_HW_DAC_IIR,bt_audio_get_eq_index(AUDIO_EQ_TYPE_HW_DAC_IIR,anc_status));
#endif

#ifdef __HW_IIR_EQ_PROCESS__
        bt_audio_set_eq(AUDIO_EQ_TYPE_HW_IIR,bt_audio_get_eq_index(AUDIO_EQ_TYPE_HW_IIR,anc_status));
#endif
*/
    }
#endif
    return 0;
}

bool is_sbc_play(void)
{
    return app_bt_stream_isrun(APP_BT_STREAM_A2DP_SBC);
}

int bt_sbc_player(enum PLAYER_OPER_T on, enum APP_SYSFREQ_FREQ_T freq,uint32_t trigger_ticks)
{
    struct AF_STREAM_CONFIG_T stream_cfg;
    enum AUD_SAMPRATE_T sample_rate;
    static bool isRun =  false;
    uint8_t* bt_audio_buff = NULL;
#if defined(__SW_IIR_EQ_PROCESS__) || defined(__HW_FIR_EQ_PROCESS__) || defined(__HW_CODEC_IIR_EQ_PROCESS__)
    uint8_t* bt_eq_buff = NULL;
    uint32_t eq_buff_size;
    uint8_t POSSIBLY_UNUSED play_samp_size;
#endif

    LOG_PRINT_BT_STREAM("bt_sbc_player work:%d op:%d freq:%d,samplerate=%d,ticks=%d", isRun, on, freq,a2dp_sample_rate,trigger_ticks);

    if ((isRun && on == PLAYER_OPER_START) || (!isRun && on == PLAYER_OPER_STOP))
        return 0;

    if (on == PLAYER_OPER_STOP || on == PLAYER_OPER_RESTART) {
        af_stream_stop(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK);
#if defined(AUDIO_ANC_FB_MC) && defined(ANC_APP) && !defined(__AUDIO_RESAMPLE__)
        af_stream_stop(AUD_STREAM_ID_2, AUD_STREAM_PLAYBACK);
#endif

#if defined(__SW_IIR_EQ_PROCESS__) || defined(__HW_FIR_EQ_PROCESS__) || defined(__HW_CODEC_IIR_EQ_PROCESS__)
        audio_eq_close();
#endif
        af_stream_close(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK);
#if defined(AUDIO_ANC_FB_MC) && defined(ANC_APP) && !defined(__AUDIO_RESAMPLE__)
        af_stream_close(AUD_STREAM_ID_2, AUD_STREAM_PLAYBACK);
#endif


#if defined(__THIRDPARTY)
        bt_sbc_mode = 0;
#endif
#if VOICE_DATAPATH
        app_voicepath_set_stream_state(AUDIO_OUTPUT_STREAMING, false);
        app_voicepath_set_pending_started_stream(AUDIO_OUTPUT_STREAMING, false);
#endif
        if (on == PLAYER_OPER_STOP) {
#ifndef FPGA
#ifdef BT_XTAL_SYNC
//            osDelay(50);
            bt_term_xtal_sync(false);
#if 1//ndef BT_XTAL_SYNC_NEW_METHOD
            bt_term_xtal_sync_default();
#endif
#endif
#endif
            a2dp_audio_deinit();
#ifdef __TWS__
            app_tws_deactive();
#if defined(__AUDIO_RESAMPLE__) && defined(SW_PLAYBACK_RESAMPLE)
            app_playback_resample_close(tws_resample);
            tws_resample = NULL;
#endif
#endif
            app_overlay_unloadall();

            app_sysfreq_req(APP_SYSFREQ_USER_APP_0, APP_SYSFREQ_32K);
            af_set_priority(osPriorityAboveNormal);
        }
    }

    if (on == PLAYER_OPER_START || on == PLAYER_OPER_RESTART) {
#ifdef __TWS_USE_PLL_SYNC__
        a2dp_reset_all_sync_pll_para();
#endif
        af_set_priority(osPriorityHigh);
        stream_local_volume = btdevice_volume_p->a2dp_vol;
        app_audio_mempool_init();

#ifdef __BT_ONE_BRING_TWO__
        if (btif_me_get_activeCons>1){
            if (freq < APP_SYSFREQ_104M) {
                freq = APP_SYSFREQ_104M;
            }
        }
#endif

#ifdef __PC_CMD_UART__
        if (freq < APP_SYSFREQ_104M) {
            freq = APP_SYSFREQ_104M;
        }
#endif

#if defined(__SW_IIR_EQ_PROCESS__)&&defined(__HW_FIR_EQ_PROCESS__)
#ifdef __TWS__
        if(freq < APP_SYSFREQ_78M) {
            freq = APP_SYSFREQ_78M;
        }
#endif
        if (audio_eq_fir_cfg.len > 128){
            freq = (freq > APP_SYSFREQ_104M) ? freq : APP_SYSFREQ_104M;
        }
#else // !EQ
#ifdef __TWS__
#if defined(A2DP_AAC_ON)
        if(app_tws_get_codec_type() == BTIF_AVDTP_CODEC_TYPE_MPEG2_4_AAC)
        {
            if(freq < APP_SYSFREQ_52M) {
                freq = APP_SYSFREQ_52M;
            }
        }
        else
#endif     
        if(app_tws_mode_is_only_mobile())
        {
            freq = APP_SYSFREQ_52M;
        }
        else
        {
#if defined(__AUDIO_RESAMPLE__) && defined(SW_PLAYBACK_RESAMPLE)
            freq = APP_SYSFREQ_104M;
#else        
            freq = APP_SYSFREQ_52M;
#endif
        }
#endif // __TWS__
#endif // !EQ
#if defined(APP_MUSIC_26M) && !defined(__SW_IIR_EQ_PROCESS__)
        if (freq < APP_SYSFREQ_26M) {
            freq = APP_SYSFREQ_26M;
        }
#else
        if (freq < APP_SYSFREQ_52M) {
            freq = APP_SYSFREQ_52M;
        }
#endif

#if defined(AUDIO_ANC_FB_MC) && defined(ANC_APP) && !defined(__AUDIO_RESAMPLE__)
        if (freq < APP_SYSFREQ_104M) {
            freq = APP_SYSFREQ_104M;
        }
#endif
#if defined(A2DP_LHDC_ON)
        if ((a2dp_get_codec_type(BT_DEVICE_ID_1) == BTIF_AVDTP_CODEC_TYPE_LHDC) ||
            (app_tws_get_codec_type() == BTIF_AVDTP_CODEC_TYPE_LHDC)){
            freq = APP_SYSFREQ_104M;
        }else
#endif
        if ((a2dp_get_codec_type(BT_DEVICE_ID_1) == BTIF_AVDTP_CODEC_TYPE_SBC) ||
            (app_tws_get_codec_type() == BTIF_AVDTP_CODEC_TYPE_SBC)){
            freq = APP_SYSFREQ_104M;
        }

        app_sysfreq_req(APP_SYSFREQ_USER_APP_0, freq);
        TRACE("bt_sbc_player: app_sysfreq_req %d", freq);

        if (on == PLAYER_OPER_START) {
#ifndef FPGA
#if defined(A2DP_LHDC_ON)
            if (a2dp_get_codec_type(BT_DEVICE_ID_1) == BTIF_AVDTP_CODEC_TYPE_LHDC
                || app_tws_get_codec_type() == BTIF_AVDTP_CODEC_TYPE_LHDC){
                app_overlay_select(APP_OVERLAY_A2DP_LHDC);
            }else
#endif
#if defined(A2DP_AAC_ON)
            if (a2dp_get_codec_type(BT_DEVICE_ID_1) == BTIF_AVDTP_CODEC_TYPE_MPEG2_4_AAC
                    || app_tws_get_codec_type() == BTIF_AVDTP_CODEC_TYPE_MPEG2_4_AAC) {
                app_overlay_select(APP_OVERLAY_A2DP_AAC);
            }
            else
#endif
            {
                app_overlay_select(APP_OVERLAY_A2DP);
            }
#endif

#ifdef __TWS__
            app_tws_active();

#if defined (LBRT) && defined(__LBRT_PAIR__)
            bt_drv_reg_op_fix_tx_pwr(app_tws_get_tws_conhdl());
#endif    

#if defined(__AUDIO_RESAMPLE__) && defined(SW_PLAYBACK_RESAMPLE)
#if defined(A2DP_AAC_ON)
            if(app_tws_get_codec_type() == BTIF_AVDTP_CODEC_TYPE_MPEG2_4_AAC)
            {
            }
            else
#endif
            {
                enum AUD_CHANNEL_NUM_T chans;

#ifdef __TWS_1CHANNEL_PCM__
                chans = AUD_CHANNEL_NUM_1;
#else
                chans = AUD_CHANNEL_NUM_2;
#endif

#ifdef RESAMPLE_ANY_SAMPLE_RATE
                tws_resample = app_playback_resample_any_open(chans, tws_resample_iter, TWS_RESAMPLE_ITER_NUM * chans,
                    (float)a2dp_sample_rate / AUD_SAMPRATE_50781);
#else
                tws_resample = app_playback_resample_open(a2dp_sample_rate, chans, tws_resample_iter, TWS_RESAMPLE_ITER_NUM * chans);
#endif
            }
#endif
#endif // __TWS__

#ifdef BT_XTAL_SYNC
#if 0
            if (app_tws_mode_is_slave()) {
                btdrv_rf_bit_offset_track_enable(true);
            }
            else {
                btdrv_rf_bit_offset_track_enable(false);
            }
#endif

#ifdef __TWS__
            if(app_tws_mode_is_only_mobile())
            {
                btdrv_rf_bit_offset_track_enable(false);
            }
            else
#endif
            {
                btdrv_rf_bit_offset_track_enable(true);
            }
            bt_init_xtal_sync(BT_XTAL_SYNC_MODE_MUSIC, BT_INIT_XTAL_SYNC_MIN, BT_INIT_XTAL_SYNC_MAX);
#endif // BT_XTAL_SYNC

#ifdef  __FPGA_FLASH__
            app_overlay_select(APP_OVERLAY_A2DP);
#endif
#if defined(__THIRDPARTY)
            bt_sbc_mode = 1;
#endif
        }

#if defined(__AUDIO_RESAMPLE__) && defined(SW_PLAYBACK_RESAMPLE)

#if defined(A2DP_AAC_ON)
        if(app_tws_get_codec_type() == BTIF_AVDTP_CODEC_TYPE_MPEG2_4_AAC)
        {
            sample_rate = a2dp_sample_rate;
        }
        else
#endif
        {
            sample_rate = AUD_SAMPRATE_50781;
        }
#else
        sample_rate = a2dp_sample_rate;
#endif

        memset(&stream_cfg, 0, sizeof(stream_cfg));

//        stream_cfg.channel_num = AUD_CHANNEL_NUM_2;
        stream_cfg.sample_rate = sample_rate;
#ifdef FPGA
        stream_cfg.device = AUD_STREAM_USE_EXT_CODEC;
#else
        stream_cfg.device = AUD_STREAM_USE_INT_CODEC;
#endif
        stream_cfg.io_path = AUD_OUTPUT_PATH_SPEAKER;
#ifdef __TWS_1CHANNEL_PCM__

        if(CFG_HW_AUD_OUTPUT_PATH_SPEAKER_DEV == (AUD_CHANNEL_MAP_CH0 | AUD_CHANNEL_MAP_CH1))
        {
            ASSERT(0,"CAN'T SET STEREO CHANNEL IN THIS FUNCION");
        }
        else
        {
            stream_cfg.channel_num = AUD_CHANNEL_NUM_1;
        }
#else
        stream_cfg.channel_num = AUD_CHANNEL_NUM_2;
#endif
        
        LOG_PRINT_BT_STREAM("SBC START CHANNEL NUM=%x",stream_cfg.channel_num);
        stream_cfg.vol = stream_local_volume;
        stream_cfg.handler = bt_sbc_player_more_data;

        stream_cfg.data_size = BT_AUDIO_BUFF_SIZE;
#ifdef A2DP_EQ_24BIT
        stream_cfg.data_size *= 2;
        stream_cfg.bits = AUD_BITS_24;
#else
        stream_cfg.bits = AUD_BITS_16;
#endif
#if VOICE_DATAPATH
        app_voicepath_set_audio_output_sample_rate(stream_cfg.sample_rate);
        app_voicepath_set_audio_output_data_buf_size(stream_cfg.data_size);
#endif

        app_audio_mempool_get_buff(&bt_audio_buff, stream_cfg.data_size);
        stream_cfg.data_ptr = BT_AUDIO_CACHE_2_UNCACHE(bt_audio_buff);


#if defined(AUDIO_ANC_FB_MC) && defined(ANC_APP) && !defined(__AUDIO_RESAMPLE__)
        sample_size_play_bt=stream_cfg.bits;
        sample_rate_play_bt=stream_cfg.sample_rate;
        data_size_play_bt=stream_cfg.data_size;
        playback_buf_bt=stream_cfg.data_ptr;
        playback_size_bt=stream_cfg.data_size;  
        playback_samplerate_ratio_bt=8;
        playback_ch_num_bt=stream_cfg.channel_num;
#endif

        af_stream_open(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK, &stream_cfg);

#if 0//def __TWS__
        struct HAL_CODEC_CONFIG_T codec_cfg;
        float shift_level;
        codec_cfg.sample_rate = stream_cfg.sample_rate;

#if defined(A2DP_AAC_ON)
        
        if(app_tws_get_codec_type() == AVDTP_CODEC_TYPE_SBC)
        {
            shift_level = A2DP_SMAPLE_RATE_SHIFT_LEVEL;
            LOG_PRINT_BT_STREAM("bt stream sbc level ");            
        }
        else if(app_tws_get_codec_type() == AVDTP_CODEC_TYPE_MPEG2_4_AAC)
        {
            shift_level = AAC_SMAPLE_RATE_SHIFT_LEVEL;
            LOG_PRINT_BT_STREAM("bt stream aac level ");                        
        }
#else
        shift_level = A2DP_SMAPLE_RATE_SHIFT_LEVEL;
#endif
        hal_codec_setup_stream_smaple_rate_shift(HAL_CODEC_ID_0, AUD_STREAM_PLAYBACK, &codec_cfg, shift_level);
#endif

#if defined(__SW_IIR_EQ_PROCESS__) || defined(__HW_FIR_EQ_PROCESS__ )||defined(__HW_CODEC_IIR_EQ_PROCESS__)
        audio_eq_init();
#ifdef __HW_FIR_EQ_PROCESS__
        play_samp_size = (stream_cfg.bits <= AUD_BITS_16) ? 2 : 4;
#if defined(CHIP_BEST2000)
        eq_buff_size = stream_cfg.data_size * sizeof(int32_t) / play_samp_size;
#else
        eq_buff_size = stream_cfg.data_size * sizeof(int16_t) / play_samp_size;
#endif
        app_audio_mempool_get_buff(&bt_eq_buff, eq_buff_size);
#else
        eq_buff_size = 0;
        bt_eq_buff = NULL;
#endif
        audio_eq_open(stream_cfg.sample_rate, stream_cfg.bits, stream_cfg.channel_num, bt_eq_buff, eq_buff_size);
#endif

#ifdef __TWS_RESAMPLE_ADJUST__
#if defined(__AUDIO_RESAMPLE__) && !defined(SW_PLAYBACK_RESAMPLE)
///to do by mql for 2300 new version
        if(hal_get_chip_metal_id() >= HAL_CHIP_METAL_ID_2)
        {
#if defined(A2DP_AAC_ON)
            if(app_tws_get_codec_type() == BTIF_AVDTP_CODEC_TYPE_MPEG2_4_AAC)
            {
                af_codec_tune_resample_rate(AUD_STREAM_PLAYBACK, 1);
            }
            else
#endif
#if defined(A2DP_LHDC_ON)
            if(app_tws_get_codec_type() == BTIF_AVDTP_CODEC_TYPE_LHDC)
            {
                af_codec_tune_resample_rate(AUD_STREAM_PLAYBACK, 1);
            }
            else
#endif
            {
#if defined(SBC_TRANS_TO_AAC)
            af_codec_tune_resample_rate(AUD_STREAM_PLAYBACK, 1);
#else
                if(sample_rate == 44100)
                    af_codec_tune_resample_rate(AUD_STREAM_PLAYBACK, 0.99988);
                else if(sample_rate == 48000)
                    af_codec_tune_resample_rate(AUD_STREAM_PLAYBACK, 0.99989);
#endif
            }
        }
#endif
#else
        af_codec_tune_resample_rate(AUD_STREAM_PLAYBACK, 1);
#endif

        if (on == PLAYER_OPER_START)
        {
            // This might use all of the rest buffer in the mempool,
            // so it must be the last configuration before starting stream.
            a2dp_audio_init();
        }

#if defined(AUDIO_ANC_FB_MC) && defined(ANC_APP) && !defined(__AUDIO_RESAMPLE__)
        stream_cfg.bits = sample_size_play_bt;
        stream_cfg.channel_num = playback_ch_num_bt;
        stream_cfg.sample_rate = sample_rate_play_bt;
        stream_cfg.device = AUD_STREAM_USE_MC;
        stream_cfg.vol = 0;
        stream_cfg.handler = audio_mc_data_playback_a2dp;
        stream_cfg.io_path = AUD_OUTPUT_PATH_SPEAKER;

        app_audio_mempool_get_buff(&bt_audio_buff, data_size_play_bt*8);
        stream_cfg.data_ptr = BT_AUDIO_CACHE_2_UNCACHE(bt_audio_buff);      
        stream_cfg.data_size = data_size_play_bt*8;
        
        playback_buf_mc=stream_cfg.data_ptr;
        playback_size_mc=stream_cfg.data_size;  

        anc_mc_run_init(hal_codec_anc_convert_rate(sample_rate_play_bt));

        af_stream_open(AUD_STREAM_ID_2, AUD_STREAM_PLAYBACK, &stream_cfg);
        //ASSERT(ret == 0, "af_stream_open playback failed: %d", ret);
#endif

        app_tws_set_trigger_time(trigger_ticks, true);

        af_stream_start(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK);

#if defined(AUDIO_ANC_FB_MC) && defined(ANC_APP) && !defined(__AUDIO_RESAMPLE__)
        af_stream_start(AUD_STREAM_ID_2, AUD_STREAM_PLAYBACK);
#endif

#ifdef __HEAR_THRU_PEAK_DET__
      bt_peak_detector_sbc_init();

      //  bt_peak_detector_init(a2dp_sample_rate, PKD_FACTOR_UP, PKD_FACTOR_DOWN);
#endif
    }

    isRun = (on != PLAYER_OPER_STOP);
    return 0;
}

#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)
void bt_sbc_player_retrigger(uint32_t trigger_time);
int bt_sbc_local_player_init(bool on, uint32_t trigger_ticks)
{
    struct AF_STREAM_CONFIG_T stream_cfg;
    static bool isRun =  false;
    static uint8_t* bt_audio_buff = NULL;
    static uint8_t* bt_eq_buff = NULL;

    if (bt_audio_buff == NULL) {
        app_audio_mempool_get_buff(&bt_audio_buff, BT_AUDIO_BUFF_SIZE);
    }

    LOG_PRINT_BT_STREAM("bt_sbc_local_player_init work:%d op:%d %d", isRun, on, trigger_ticks);
    if(on && isRun){
        bt_sbc_player_retrigger(trigger_ticks);
        return 0;
    }
    // app_bt_stream_volumeset(stream_volume.a2dp_vol);
    if (on)
    {
        LOG_PRINT_BT_STREAM("TODO: get volume?");
        // stream_local_volume = btdevice_volume_p->a2dp_vol;

#ifdef __TWS__
        app_tws_active();

        a2dp_audio_init();
#endif  //__TWS__

#ifdef BT_XTAL_SYNC
        bt_init_xtal_sync(BT_XTAL_SYNC_MODE_MUSIC, BT_INIT_XTAL_SYNC_MIN, BT_INIT_XTAL_SYNC_MAX);
#endif
        memset(bt_audio_buff, 0, sizeof(bt_audio_buff));
        memset(&stream_cfg, 0, sizeof(stream_cfg));

        stream_cfg.bits = AUD_BITS_16;
        stream_cfg.channel_num = AUD_CHANNEL_NUM_2;
        stream_cfg.sample_rate = bt_get_sbc_sample_rate();;
        stream_cfg.device = AUD_STREAM_USE_INT_CODEC;

        stream_cfg.io_path = AUD_OUTPUT_PATH_SPEAKER;

        stream_cfg.vol = stream_local_volume;
        stream_cfg.handler = bt_sbc_player_more_data;
        stream_cfg.data_ptr = BT_AUDIO_CACHE_2_UNCACHE(bt_audio_buff);
        stream_cfg.data_size = BT_AUDIO_BUFF_SIZE;
        stream_cfg.trigger_time = trigger_ticks;

        af_stream_open(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK, &stream_cfg);

#if defined(__SW_IIR_EQ_PROCESS__) || defined(__HW_FIR_EQ_PROCESS__ )||defined(__HW_CODEC_IIR_EQ_PROCESS__)
        audio_eq_init();
        if (bt_eq_buff == NULL) {
            app_audio_mempool_get_buff(&bt_eq_buff, BT_AUDIO_BUFF_SIZE*2);
        }
#if 0//def __AUDIO_RESAMPLE__
        audio_eq_open(AUD_SAMPRATE_50700, AUD_BITS_16,AUD_CHANNEL_NUM_2, bt_eq_buff, BT_AUDIO_BUFF_SIZE*2);
#else
        audio_eq_open(AUD_SAMPRATE_44100, AUD_BITS_16,AUD_CHANNEL_NUM_2, bt_eq_buff, BT_AUDIO_BUFF_SIZE*2);
#endif
#endif

#if VOICE_DATAPATH
        if (app_voicepath_get_stream_state(VOICEPATH_STREAMING))
        {
            app_voicepath_set_pending_started_stream(AUDIO_OUTPUT_STREAMING, true);
        }
        else
        {
            app_voicepath_set_stream_state(AUDIO_OUTPUT_STREAMING, true);
            af_stream_start(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK);
        }
#else
        af_stream_start(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK);
#endif
    }else{
        af_stream_stop(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK);
#if defined(__SW_IIR_EQ_PROCESS__) || defined(__HW_FIR_EQ_PROCESS__)
        audio_eq_close();
#endif
        af_stream_close(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK);
#ifdef BT_XTAL_SYNC
        osDelay(50);
        bt_term_xtal_sync(false);
#ifndef BT_XTAL_SYNC_NEW_METHOD
        bt_term_xtal_sync_default();
#endif
#endif
        a2dp_audio_deinit();
#ifdef __TWS__
        app_tws_deactive();
#endif

        app_sysfreq_req(APP_SYSFREQ_USER_APP_0, APP_SYSFREQ_32K);
    }

    isRun=on;
    return 0;
}
#endif


#ifdef SPEECH_SIDETONE
extern "C" {
void hal_codec_sidetone_enable(int ch, int gain);
void hal_codec_sidetone_disable(void);
}
#endif

void speech_tx_aec_set_frame_len(int len);
int voicebtpcm_pcm_echo_buf_queue_init(uint32_t size);
void voicebtpcm_pcm_echo_buf_queue_reset(void);
void voicebtpcm_pcm_echo_buf_queue_deinit(void);
int voicebtpcm_pcm_audio_init(void);
int voicebtpcm_pcm_audio_deinit(void);
uint32_t voicebtpcm_pcm_audio_data_come(uint8_t *buf, uint32_t len);
uint32_t voicebtpcm_pcm_audio_more_data(uint8_t *buf, uint32_t len);
int store_voicebtpcm_m2p_buffer(unsigned char *buf, unsigned int len);
int get_voicebtpcm_p2m_frame(unsigned char *buf, unsigned int len);
uint8_t btapp_hfp_need_mute(void);

static enum AUD_CHANNEL_NUM_T sco_play_chan_num;
static enum AUD_CHANNEL_NUM_T sco_cap_chan_num;

bool bt_sco_codec_is_msbc(void)
{
    bool en = false;
#ifdef HFP_1_6_ENABLE
    if (bt_sco_get_current_codecid() == BTIF_HF_SCO_CODEC_MSBC)
    {
        en = true;
    }
    else
#endif
    {
        en = false;
    }

    return en;
}

#if defined(__AUDIO_RESAMPLE__) && defined(SW_SCO_RESAMPLE)

#ifdef CHIP_BEST1000
#error "Unsupport SW_SCO_RESAMPLE on best1000 by now"
#endif
#ifdef NO_SCO_RESAMPLE
#error "Conflicted config: NO_SCO_RESAMPLE and SW_SCO_RESAMPLE"
#endif

// The decoded playback data in the first irq is output to DAC after the second irq (PING-PONG buffer)
#define SCO_PLAY_RESAMPLE_ALIGN_CNT     2

static uint8_t sco_play_irq_cnt;
static bool sco_dma_buf_err;
static struct APP_RESAMPLE_T *sco_capture_resample;
static struct APP_RESAMPLE_T *sco_playback_resample;

FRAM_TEXT_LOC static int bt_sco_capture_resample_iter(uint8_t *buf, uint32_t len)
{
    voicebtpcm_pcm_audio_data_come(buf, len);
    return 0;
}

FRAM_TEXT_LOC static int bt_sco_playback_resample_iter(uint8_t *buf, uint32_t len)
{
    voicebtpcm_pcm_audio_more_data(buf, len);
    return 0;
}

#endif

// #define BT_SCO_I2S_ENABLE

#if defined(BT_SCO_I2S_ENABLE)
#define I2S_STREAM_ID       AUD_STREAM_ID_2

#define FRAME_LEN       (256)
#define CHANNEL_NUM     (2)
#define TEMP_BUF_SIZE   (FRAME_LEN * CHANNEL_NUM * 2 * 2)

// static uint32_t codec_capture_buf_len = TEMP_BUF_SIZE;
// static uint32_t codec_playback_buf_len = TEMP_BUF_SIZE;
static uint32_t i2s_capture_buf_len = TEMP_BUF_SIZE;
static uint32_t i2s_playback_buf_len = TEMP_BUF_SIZE;

// static uint8_t codec_capture_buf[TEMP_BUF_SIZE];
// static uint8_t codec_playback_buf[TEMP_BUF_SIZE];
static uint8_t i2s_capture_buf[TEMP_BUF_SIZE];
static uint8_t i2s_playback_buf[TEMP_BUF_SIZE];

// static uint8_t codec_queue_buf[TEMP_BUF_SIZE];

static uint32_t codec_capture_cnt = 0;
static uint32_t codec_playback_cnt = 0;
static uint32_t i2s_capture_cnt = 0;
static uint32_t i2s_playback_cnt = 0;

static uint32_t i2s_capture_callback(uint8_t *buf, uint32_t len)
{
    short *pcm_buf = (short*)buf;
    int pcm_len = len / 2;

    LOG_PRINT_BT_STREAM("[%s] cnt = %d", __func__, i2s_capture_cnt++);

    // memcpy(buf, codec_queue_buf, len);


    return pcm_len * 2;
}

static uint32_t i2s_playback_callback(uint8_t *buf, uint32_t len)
{
    short *pcm_buf = (short*)buf;
    int pcm_len = len / 2;

    LOG_PRINT_BT_STREAM("[%s] cnt = %d", __func__, i2s_playback_cnt++);

    // memcpy(buf, codec_queue_buf, len);


    return pcm_len * 2;
}

static uint32_t bt_sco_i2s_open(void)
{
    int ret = 0;
    struct AF_STREAM_CONFIG_T stream_cfg;

    memset(&stream_cfg, 0, sizeof(stream_cfg));
    stream_cfg.bits = AUD_BITS_16;
    stream_cfg.channel_num = AUD_CHANNEL_NUM_2;
    stream_cfg.channel_map = (enum AUD_CHANNEL_MAP_T)(AUD_CHANNEL_MAP_CH0 | AUD_CHANNEL_MAP_CH1);
    stream_cfg.sample_rate = AUD_SAMPRATE_16000;
    stream_cfg.vol = 12;
    stream_cfg.device = AUD_STREAM_USE_I2S_MASTER;
    stream_cfg.io_path = AUD_IO_PATH_NULL;
    stream_cfg.data_size = i2s_playback_buf_len;
    stream_cfg.data_ptr = i2s_playback_buf;
    stream_cfg.handler = i2s_playback_callback;
    // stream_cfg.handler = NULL;

    LOG_PRINT_BT_STREAM("i2s playback sample_rate:%d, data_size:%d",stream_cfg.sample_rate,stream_cfg.data_size);
    ret = af_stream_open(I2S_STREAM_ID, AUD_STREAM_PLAYBACK, &stream_cfg);
    ASSERT(ret == 0, "i2s playback failed: %d", ret);


    memset(&stream_cfg, 0, sizeof(stream_cfg));
    stream_cfg.i2s_master_clk_wait = true;
    stream_cfg.bits = AUD_BITS_16;
    stream_cfg.channel_num = AUD_CHANNEL_NUM_2;
    stream_cfg.channel_map = (enum AUD_CHANNEL_MAP_T)(AUD_CHANNEL_MAP_CH0 | AUD_CHANNEL_MAP_CH1);
    stream_cfg.sample_rate = AUD_SAMPRATE_16000;
    stream_cfg.vol = 12;
    stream_cfg.device = AUD_STREAM_USE_I2S_MASTER;
    stream_cfg.io_path = AUD_IO_PATH_NULL;
    stream_cfg.data_size = i2s_capture_buf_len;
    stream_cfg.data_ptr = i2s_capture_buf;
    stream_cfg.handler = i2s_capture_callback;
    LOG_PRINT_BT_STREAM("i2s capture sample_rate:%d, data_size:%d",stream_cfg.sample_rate,stream_cfg.data_size);
    ret = af_stream_open(I2S_STREAM_ID, AUD_STREAM_CAPTURE, &stream_cfg);
    ASSERT(ret == 0, "i2s capture failed: %d", ret);

    return 0;
}

static uint32_t bt_sco_i2s_close(void)
{
    af_stream_close(I2S_STREAM_ID, AUD_STREAM_CAPTURE);
    af_stream_close(I2S_STREAM_ID, AUD_STREAM_PLAYBACK);

    return 0;
}

static uint32_t bt_sco_i2s_start(void)
{
    af_stream_start(I2S_STREAM_ID, AUD_STREAM_PLAYBACK);
    af_stream_start(I2S_STREAM_ID, AUD_STREAM_CAPTURE);

    return 0;
}

static uint32_t bt_sco_i2s_stop(void)
{
    af_stream_stop(I2S_STREAM_ID, AUD_STREAM_CAPTURE);       
    af_stream_stop(I2S_STREAM_ID, AUD_STREAM_PLAYBACK); 

    return 0;
}
#endif

//( codec:mic-->btpcm:tx
// codec:mic
FRAM_TEXT_LOC static uint32_t bt_sco_codec_capture_data(uint8_t *buf, uint32_t len)
{
#if defined(BT_SCO_I2S_ENABLE)
    LOG_PRINT_BT_STREAM("[%s] cnt = %d", __func__, codec_capture_cnt++);
#endif

#if defined(__AUDIO_RESAMPLE__) && defined(SW_SCO_RESAMPLE)
    if(hal_cmu_get_audio_resample_status())
    {
        if (af_stream_buffer_error(AUD_STREAM_ID_0, AUD_STREAM_CAPTURE)) {
            sco_dma_buf_err = true;
        }
        // The decoded playback data in the first irq is output to DAC after the second irq (PING-PONG buffer),
        // so it is aligned with the capture data after 2 playback irqs.
        if (sco_play_irq_cnt < SCO_PLAY_RESAMPLE_ALIGN_CNT) {
            // Skip processing
            return len;
        }
        app_capture_resample_run(sco_capture_resample, buf, len);
    }
    else
#endif
    {
        voicebtpcm_pcm_audio_data_come(buf, len);
    }

    return len;
}

// btpcm:tx


uint8_t msbc_received=0;


static void tws_stop_sco_callback(uint32_t status, uint32_t param);

uint8_t trigger_test=0;

enum SCO_PLAYER_RESTART_STATUS_E
{
    SCO_PLAYER_RESTART_STATUS_IDLE,
    SCO_PLAYER_RESTART_STATUS_STARTING,
    SCO_PLAYER_RESTART_STATUS_ONPROCESS,
}sco_player_restart_status;

int bt_sco_player_restart_requeset(bool needrestart)
{
    switch (sco_player_restart_status)
    {
        case SCO_PLAYER_RESTART_STATUS_IDLE:
            if (needrestart){
                sco_player_restart_status = SCO_PLAYER_RESTART_STATUS_STARTING;
            }else{
                sco_player_restart_status = SCO_PLAYER_RESTART_STATUS_IDLE;
            }
            break;
        case SCO_PLAYER_RESTART_STATUS_STARTING:
        case SCO_PLAYER_RESTART_STATUS_ONPROCESS:
        default:            
            if (needrestart){
                // do nothing
            }else{
                sco_player_restart_status = SCO_PLAYER_RESTART_STATUS_IDLE;
            }
            break;
    }

    return 0;
}

inline bool bt_sco_player_need_restart(void)
{
    bool nRet = false;
    if (sco_player_restart_status == SCO_PLAYER_RESTART_STATUS_STARTING){
        sco_player_restart_status = SCO_PLAYER_RESTART_STATUS_ONPROCESS;
        nRet = true;
    }
    return nRet;
}

FRAM_TEXT_LOC static uint32_t bt_sco_btpcm_playback_data(uint8_t *buf, uint32_t len)
{
    get_voicebtpcm_p2m_frame(buf, len);

    if (btapp_hfp_need_mute())
    {
        memset(buf, 0, len);
    }

#if  defined(HFP_1_6_ENABLE)
    if ((bt_sco_get_current_codecid() == BTIF_HF_SCO_CODEC_MSBC) && msbc_received == 1) {
        if (bt_drv_reg_op_sco_tx_buf_restore(&trigger_test)){
#ifndef APB_PCM     
//            bt_sco_player_restart_requeset(true);
#endif            
        }        
    }
    if (bt_sco_player_need_restart()){
        app_tws_close_emsack_mode();
        app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_STOP,BT_STREAM_VOICE,BT_DEVICE_ID_1,MAX_RECORD_NUM,0,(uint32_t)tws_stop_sco_callback);
    }
#endif

#ifdef __3RETX_SNIFF__     
    app_acl_sniff_sco_conflict_check();
#endif

    return len;
}
//)

#if IS_WITH_SCO_PACKET_STATISTICS
#define MSBC_QUALITY_RECORD_CNT     0x100
static uint32_t record_good_packet_rate_index = 0;
static uint8_t is_record_good_packet_rate_role_back = false;
static uint16_t good_msbc_packet_one_thousandth[MSBC_QUALITY_RECORD_CNT];
static void record_msbc_packet_quality(uint16_t one_thousandth)
{
    good_msbc_packet_one_thousandth[record_good_packet_rate_index++] = one_thousandth;

    if (MSBC_QUALITY_RECORD_CNT == record_good_packet_rate_index)
    {
        is_record_good_packet_rate_role_back = true;
        record_good_packet_rate_index = 0;
    }
}

static uint32_t record_msbc_packet_trend_index = 0;
typedef struct
{
    uint16_t isGood : 1;
    uint16_t count  : 15;
} msbc_packet_trend_t;

#define MSBC_QUALITY_TREND_CNT     0x400
static uint8_t is_recorded_packet_trend_role_back = false;
static msbc_packet_trend_t recordedTrend[MSBC_QUALITY_TREND_CNT];
static msbc_packet_trend_t currentMsbcTrend = {1, 0};
static void record_msbc_packet_trend(bool isGood)
{
    if (isGood ^ currentMsbcTrend.isGood)
    {
        recordedTrend[record_msbc_packet_trend_index++] = currentMsbcTrend;
        if (MSBC_QUALITY_TREND_CNT == record_msbc_packet_trend_index)
        {
            is_recorded_packet_trend_role_back = true;
            record_msbc_packet_trend_index = 0;
        }

        currentMsbcTrend.count = 1;
        currentMsbcTrend.isGood = isGood;
    }
    else
    {
        currentMsbcTrend.count++;
    }
}

void msbc_dump_init(void)
{
    memset((uint8_t *)&recordedTrend, 0, sizeof(recordedTrend));   
    memset((uint8_t *)&good_msbc_packet_one_thousandth, 0, sizeof(good_msbc_packet_one_thousandth)); 
}

void print_msbc_packet_quality(void)
{
    TRACE_HIGHPRIO("msbc quality:");
    uint32_t printedEntryCnt = 0;
    uint32_t printedCount = 0;
    uint32_t index;
    if (is_record_good_packet_rate_role_back)
    {
        index = record_good_packet_rate_index;
    }
    else
    {
        index = 0;
    }

    uint32_t entriesToPrint;
    
    for (;;)
    {
        if ((index + 16) >= MSBC_QUALITY_RECORD_CNT)
        {
            entriesToPrint = MSBC_QUALITY_RECORD_CNT - index;
        }
        else
        {
            entriesToPrint = 16;
        }

        printedCount += entriesToPrint;
        for (uint32_t i = 0;i < entriesToPrint;i++)
        {
            TRACE_HIGHPRIO("%d", good_msbc_packet_one_thousandth[index+i]);
        }

        index += entriesToPrint;
        if (MSBC_QUALITY_RECORD_CNT == index)
        {
            index = 0;
        }

        if (printedCount >=  MSBC_QUALITY_RECORD_CNT)
        {
            break;
        }
    }
}

void print_msbc_packet_trend(void)
{
    TRACE_HIGHPRIO("msbc trend:");
    uint32_t printedEntryCnt = 0;
    uint32_t printedCount = 0;
    uint32_t index;
    if (is_recorded_packet_trend_role_back)
    {
        index = record_msbc_packet_trend_index;
    }
    else
    {
        index = 0;
    }
    
    for (;;)
    {
        if (MSBC_QUALITY_TREND_CNT == index)
        {
            index = 0;
        }

        index++;
        
        TRACE_HIGHPRIO("%d %d", recordedTrend[index].isGood, recordedTrend[index].count);
        printedCount++;

        if (printedCount >=  MSBC_QUALITY_TREND_CNT)
        {
            break;
        }
    }
}
#endif

uint8_t header_pos=0xff;
uint8_t header_sn;
uint64_t total_count=0;
uint64_t good_count=0;
uint64_t bad_count=0;

uint8_t msbc_mute_flag = 0;
uint8_t msbc_find_msbc_header(uint8_t *buff,uint8_t count,uint8_t max_count)
{
    uint8_t i;
#if IS_WITH_SCO_PACKET_STATISTICS
    if((total_count%100) ==0)
    {
        TRACE_LVL(HAL_TRACE_LEVEL_ENV, "GOOD FRAME RATE=%d/100",good_count*100/total_count);
    }
    if((total_count%1000) ==0)
    {

        TRACE_LVL(HAL_TRACE_LEVEL_ENV, "GOOD FRAME RATE=%d/1000",good_count*1000/total_count);
        record_msbc_packet_quality(good_count);
        good_count = 0;
        total_count = 0;
        bad_count = 0;        
    } 
#endif
    total_count++;

    for(i=0;i<60;i++)
    {
        if(buff[i]==0x1 && buff[i+2] == 0xad)
        {
            //TRACE_LVL(HAL_TRACE_LEVEL_ENV, "msbc rx seq=%x,i=%d",buff[i+1],i);
            header_pos = i;
            header_sn = buff[i+1];
            if(msbc_received == 0)
            {
                msbc_received = 1;
            }
            good_count++;
#if IS_WITH_SCO_PACKET_STATISTICS           
            record_msbc_packet_trend(true);
#endif
            return i;
        }
    }
    LOG_PRINT_MEDIA_MGR("NO SEQ");
    
    if(header_sn == 0x8)
        header_sn = 0x38;
    else if(header_sn ==0x38)
        header_sn = 0xc8;
    else if(header_sn ==0xc8)
        header_sn = 0xf8;
    else if(header_sn ==0xf8)
        header_sn = 0x08;
    if((count == max_count) && (header_pos+2)>=60)
        return 0;
    if(buff[header_pos+2] == 0xad)
    {
#if IS_WITH_SCO_PACKET_STATISTICS
        //DUMP8("%02x ",buff+header_pos,10);
#endif
        buff[header_pos] = 0x01;
        buff[header_pos+1] = header_sn;
        good_count++;
#if IS_WITH_SCO_PACKET_STATISTICS
        record_msbc_packet_trend(true);
#endif
    }
    else
    {
        bad_count++;
#if IS_WITH_SCO_PACKET_STATISTICS       
        record_msbc_packet_trend(false);
        //DUMP8("%02x ",buff+header_pos,10);
#endif
//        hal_trace_tportset(0);
//        hal_trace_tportclr(0);        
    }
 
    return 0;
}

//( btpcm:rx-->codec:spk
// btpcm:rx

#ifdef __TWS_CALL_DUAL_CHANNEL__
uint32_t sco_trigger_time=0;
#endif


typedef struct 
{
    uint32_t btpcm_count;
    uint32_t sco_play_count;
    uint32_t btpcm_rx_clk;
    uint32_t btpcm_play_clk;
    uint32_t curr_btpcm_rx_clk;
    uint32_t curr_btpcm_play_clk;
    uint32_t playback_time;
    uint8_t btpcm_type;
    
    
}BTPCM_TIME_STRUCT;


BTPCM_TIME_STRUCT btpcm_time;

extern "C" void sco_store_pcm_irq_clk(void)
{
    if(bt_sco_mode ==1)
    {
            btpcm_time.btpcm_rx_clk = hal_sys_timer_get();//bt_syn_get_curr_ticks(app_tws_get_tws_conhdl());
    }
}

extern "C" void sco_store_playback_irq_clk(void)
{
    if(bt_sco_mode ==1)
    {
         btpcm_time.playback_time = btpcm_time.btpcm_play_clk;
            btpcm_time.btpcm_play_clk = hal_sys_timer_get();//bt_syn_get_curr_ticks(app_tws_get_tws_conhdl());
            btpcm_time.playback_time = btpcm_time.btpcm_play_clk -btpcm_time.playback_time;
    }
}


void bt_sco_btpcm_check_frame_type(void)
{
    if(btpcm_time.btpcm_count== 0)
    {
        uint32_t clk = btpcm_time.btpcm_rx_clk - btpcm_time.curr_btpcm_play_clk;
        uint32_t playtime_interval = btpcm_time.playback_time;
    if(playtime_interval>1000)
    {
        LOG_PRINT_BT_STREAM("playtime error %d",playtime_interval);
        playtime_interval = 320;
    }
    LOG_PRINT_BT_STREAM("frame type check pcmclk=%d,playclk=%d playtime=%d",btpcm_time.btpcm_rx_clk,btpcm_time.curr_btpcm_play_clk,btpcm_time.playback_time);
        if((btpcm_time.sco_play_count-1) %2 ==0)  ///pcm irq is after even playback irq
        {
            if(clk>=playtime_interval/2)
            {
                btpcm_time.btpcm_type = 1;
            }
            else
            {
                btpcm_time.btpcm_type = 4;
            }
        }
        else
        {
        //    uint32_t clk = btpcm_time.btpcm_rx_clk - btpcm_time.curr_btpcm_play_clk;
            if(clk>=playtime_interval/2)
            {
                btpcm_time.btpcm_type = 3;
            }
         else
            {
                btpcm_time.btpcm_type = 2;
            }            
        }
        LOG_PRINT_BT_STREAM("PCM IRQ TYPE =%d",btpcm_time.btpcm_type);
    }
}

extern "C" uint32_t bt_sco_btpcm_get_playback_count(void)
{
    return btpcm_time.sco_play_count;
}

extern "C" uint32_t bt_sco_btpcm_btpcm_type(void)
{
    return btpcm_time.btpcm_type;
}

uint8_t msbc_sn_rx_ok=0;
uint8_t msbc_sn_rx[4];

FRAM_TEXT_LOC  static uint32_t bt_sco_btpcm_capture_data(uint8_t *buf, uint32_t len)
{
#ifdef __TWS_CALL_DUAL_CHANNEL__


    if(app_tws_get_btpcm_wait_triggle() ==1)
    {
        bt_sco_btpcm_check_frame_type();

        LOG_PRINT_BT_STREAM("CLEAR PCM WAIT TRIGGLER ct=%x",btdrv_syn_get_curr_ticks());
        app_tws_set_btpcm_wait_triggle(0);
        btdrv_syn_clr_trigger();
        btdrv_syn_trigger_codec_en(0);        
        sco_trigger_time = 0;
        good_count = 0;
        total_count = 0;
        bad_count = 0;
        return 0;

    }
#endif

#if defined(HFP_1_6_ENABLE)
    if (bt_sco_get_current_codecid() == BTIF_HF_SCO_CODEC_MSBC) {
        uint16_t * temp_buf = NULL;
        temp_buf=(uint16_t *)buf;
        len /= 2;
        for(uint32_t i=0; i<len; i++) {
            buf[i]=temp_buf[i]>>8;
        }

//    LOG_PRINT_BT_STREAM("CAP: len=%d",len);
        for(uint8_t i=0;i<len/60;i++)
        {
            msbc_find_msbc_header(buf+i*60,i,len/60-1);    
        }

    if(buf[header_pos+1] !=0  && buf[120+header_pos+1] !=0 &&
        buf[60+header_pos+1] !=0 && buf[180+header_pos+1] !=0)
    {

        if(buf[header_pos+1]  != buf[60+header_pos+1] && 
            buf[60+header_pos+1]  != buf[120+header_pos+1] &&
            buf[120+header_pos+1] !=buf[180+header_pos+1]
            )
        {
            LOG_PRINT_BT_STREAM("SN IS OK");
            msbc_sn_rx_ok =1;
            msbc_sn_rx[0] = buf[header_pos+1];
            msbc_sn_rx[1] = buf[60+header_pos+1];
            msbc_sn_rx[2] = buf[120+header_pos+1];
            msbc_sn_rx[3] = buf[180+header_pos+1];
        }
    }
#if 0
     if(msbc_sn_rx_ok == 1)
     {
         if(buf[header_pos+1] !=0 && buf[header_pos+1] !=msbc_sn_rx[0]  )
         {
             LOG_PRINT_BT_STREAM("DUPLICATE 0");
             memset(&buf[header_pos] ,0,60);
         }
         if(buf[60+header_pos+1] !=0 && buf[60+header_pos+1] !=msbc_sn_rx[1]  )
         {
             LOG_PRINT_BT_STREAM("DUPLICATE 1");
             memset(&buf[60+header_pos] ,0,60);
         }
         if(buf[120+header_pos+1] !=0 && buf[120+header_pos+1] !=msbc_sn_rx[2]  )
         {
             LOG_PRINT_BT_STREAM("DUPLICATE 2");
             memset(&buf[120+header_pos] ,0,60);
         }
         if(buf[180+header_pos+1] !=0 && buf[180+header_pos+1] !=msbc_sn_rx[3]  )
         {
             LOG_PRINT_BT_STREAM("DUPLICATE 3");
             memset(&buf[180+header_pos] ,0,60);
         }
     }
#endif
    }
#endif
    store_voicebtpcm_m2p_buffer(buf, len);
    btpcm_time.curr_btpcm_rx_clk= btpcm_time.btpcm_rx_clk;
    btpcm_time.btpcm_count++;

    return len;
}

#ifdef __BT_ANC__
static void bt_anc_sco_down_sample_16bits(int16_t *dst, int16_t *src, uint32_t dst_cnt)
{
    for (uint32_t i = 0; i < dst_cnt; i++) {
        dst[i] = src[i * bt_sco_samplerate_ratio * sco_play_chan_num];
    }
}
#endif

extern CQueue voicebtpcm_m2p_queue;


extern uint8_t slave_sco_active;
extern struct BT_DEVICE_T  app_bt_device;


static void tws_restart_voice_callback(uint32_t status, uint32_t param)
{

    uint8_t sco_state;
    uint32_t lock = int_lock();
    if(app_tws_mode_is_slave())
    {
        sco_state = slave_sco_active;
    }
    else
    {
        sco_state = (app_bt_device.hf_audio_state[0] == BTIF_HF_AUDIO_CON)?1:0;
    }
    int_unlock(lock);
  LOG_PRINT_BT_STREAM("tws_restart_voice_callback audio state=%d",sco_state);

  
  if(sco_state == 0)
  {
        app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_STOP,BT_STREAM_VOICE,BT_DEVICE_ID_1,MAX_RECORD_NUM,0,0);  
  }
}



static void tws_stop_sco_callback(uint32_t status, uint32_t param)
{
    uint8_t sco_state;
    uint32_t lock = int_lock();
    if(app_tws_mode_is_slave())
    {
        sco_state = slave_sco_active;
    }
    else
    {
        sco_state = (app_bt_device.hf_audio_state[0] == BTIF_HF_AUDIO_CON)?1:0;
    }
    int_unlock(lock);

  LOG_PRINT_BT_STREAM("tws_stop_sco_callback audio state=%d",sco_state);

  
  if(sco_state)
  {
//      osDelay(30);
      app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_START,BT_STREAM_VOICE,BT_DEVICE_ID_1,MAX_RECORD_NUM,1,(uint32_t)tws_restart_voice_callback);
  }
}

#define __TWS_RETRIGGER_VOICE__

void bt_sco_check_pcm(void)
{
#ifdef __TWS_RETRIGGER_VOICE__
    if(sco_trigger_time == 0)
        return;
    uint32_t curr_time = btdrv_syn_get_curr_ticks();
    uint8_t sco_state;
    if(app_tws_mode_is_slave())
    {
        sco_state = slave_sco_active;
    }
    else
    {
        sco_state = (app_bt_device.hf_audio_state[0] == BTIF_HF_AUDIO_CON)?1:0;
    }
    if(sco_state  && ((curr_time - sco_trigger_time)>0x400))
    {
        sco_trigger_time =0;
#ifdef __TWS_CALL_DUAL_CHANNEL__
        app_bt_resume_sniffer_sco_for_trigger(TRIGGER_TIMEOUT);
#endif
        app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_STOP,BT_STREAM_VOICE,BT_DEVICE_ID_1,MAX_RECORD_NUM,0,(uint32_t)tws_stop_sco_callback);
    }
#endif
}



extern uint32_t sco_active_bak;
extern btif_remote_device_t* mobileBtRemDev;
extern uint16_t sniff_sco_hcihandle;
extern btif_remote_device_t* masterBtRemDev;
FRAM_TEXT_LOC static uint32_t bt_sco_codec_playback_data(uint8_t *buf, uint32_t len)
{
#ifdef __TWS_CALL_DUAL_CHANNEL__
    if(app_tws_get_pcm_wait_triggle() ==1)
    {
         LOG_PRINT_BT_STREAM("SET PCM WAIT TRIGGLER,ct=%x",bt_syn_get_curr_ticks(app_tws_get_tws_conhdl()));
        msbc_received=0;
        if(app_tws_mode_is_slave())
        {
            bt_drv_reg_op_sco_status_restore();
        }
        app_bt_resume_sniffer_sco_for_trigger(TRIGGER_SUCCESS);
        btdrv_syn_clr_trigger();
        app_tws_set_pcm_wait_triggle(0);
#ifdef CHIP_BEST2300
        btdrv_set_bt_pcm_triggler_en(0);
        btdrv_set_bt_pcm_en(1);
#else
        app_tws_set_pcm_triggle();
#endif
        app_tws_set_btpcm_wait_triggle(1);
        memset(buf, 0, len);
        sco_trigger_time = btdrv_syn_get_curr_ticks();
        
    }
#endif

    uint8_t *dec_buf;
    uint32_t mono_len;

#ifdef BT_XTAL_SYNC
#ifdef BT_XTAL_SYNC_NEW_METHOD
        if(TWS_MASTER_CONN_SLAVE == app_tws_get_conn_state())
        {
        //    uint32_t bitoffset = btdrv_rf_bitoffset_get(btdrv_conhdl_to_linkid(mobileBtRemDev->hciHandle));
            //LOG_PRINT_BT_STREAM("xtal_sync: rxbit=%d",rxbit);
     //       bt_xtal_sync_new(bitoffset);
        }
        else if(TWS_SLAVE_CONN_MASTER == app_tws_get_conn_state())
        {
            uint32_t bitoffset = bt_drv_reg_op_get_bitoffset( btif_me_get_remote_device_hci_handle(masterBtRemDev));
            //LOG_PRINT_BT_STREAM("xtal_sync: rxbit=%d",rxbit);
            bt_xtal_sync_new(bitoffset,BT_XTAL_SYNC_MODE_WITH_TWS);
        }
#else
        bt_xtal_sync(BT_XTAL_SYNC_MODE_VOICE);
#endif
#endif

#ifdef __BT_ANC__
    mono_len = len / sco_play_chan_num / bt_sco_samplerate_ratio;
    dec_buf = bt_anc_sco_dec_buf;
#else
    mono_len = len / sco_play_chan_num;
    dec_buf = buf;
#endif

#if defined(__AUDIO_RESAMPLE__) && defined(SW_SCO_RESAMPLE)
    if(hal_cmu_get_audio_resample_status())
    {
        if (sco_play_irq_cnt < SCO_PLAY_RESAMPLE_ALIGN_CNT) {
            sco_play_irq_cnt++;
        }
        if (sco_dma_buf_err || af_stream_buffer_error(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK)) {
            sco_dma_buf_err = false;
            sco_play_irq_cnt = 0;
            app_resample_reset(sco_playback_resample);
            app_resample_reset(sco_capture_resample);
            // voicebtpcm_pcm_echo_buf_queue_reset();
            LOG_PRINT_BT_STREAM("%s: DMA buffer error: reset resample", __func__);
        }
        app_playback_resample_run(sco_playback_resample, dec_buf, mono_len);
    }
    else
#endif
    {
#ifdef __BT_ANC__
        bt_anc_sco_down_sample_16bits((int16_t *)dec_buf, (int16_t *)buf, mono_len / 2);
#else
        if (sco_play_chan_num == AUD_CHANNEL_NUM_2) {
            // Convert stereo data to mono data (to save into echo_buf)
            app_bt_stream_copy_track_two_to_one_16bits((int16_t *)dec_buf, (int16_t *)buf, mono_len / 2);
        }
#endif
        voicebtpcm_pcm_audio_more_data(dec_buf, mono_len);
    }

#ifdef __BT_ANC__
    voicebtpcm_pcm_resample((int16_t *)dec_buf, mono_len / 2, (int16_t *)buf);
#endif
    btpcm_time.curr_btpcm_play_clk = btpcm_time.btpcm_play_clk;
    btpcm_time.sco_play_count++;

    if (sco_play_chan_num == AUD_CHANNEL_NUM_2) {
        // Convert mono data to stereo data
        app_bt_stream_copy_track_one_to_two_16bits((int16_t *)buf, (int16_t *)buf, len / 2 / 2);
    }

    return len;
}

#if defined(AUDIO_ANC_FB_MC) && defined(ANC_APP) && !defined(__AUDIO_RESAMPLE__)
static uint32_t audio_mc_data_playback_sco(uint8_t *buf, uint32_t mc_len_bytes)
{
  // uint32_t begin_time;
   //uint32_t end_time;
  // begin_time = hal_sys_timer_get();
  // LOG_PRINT_BT_STREAM("phone cancel: %d",begin_time);

   float left_gain;
   float right_gain;
   int32_t playback_len_bytes,mc_len_bytes_8;
   int32_t i,j,k; 
   int delay_sample;

   mc_len_bytes_8=mc_len_bytes/8;

   hal_codec_get_dac_gain(&left_gain,&right_gain);

   LOG_PRINT_BT_STREAM("playback_samplerate_ratio:  %d,ch:%d,sample_size:%d.",playback_samplerate_ratio_bt,playback_ch_num_bt,sample_size_play_bt);
   LOG_PRINT_BT_STREAM("mc_len_bytes:  %d",mc_len_bytes);
   
  // LOG_PRINT_BT_STREAM("left_gain:  %d",(int)(left_gain*(1<<12)));
  // LOG_PRINT_BT_STREAM("right_gain: %d",(int)(right_gain*(1<<12)));
  
   playback_len_bytes=mc_len_bytes/playback_samplerate_ratio_bt;

    if (sample_size_play_bt == AUD_BITS_16)
    {
        int16_t *sour_p=(int16_t *)(playback_buf_bt+playback_size_bt/2);
        int16_t *mid_p=(int16_t *)(buf);
        int16_t *mid_p_8=(int16_t *)(buf+mc_len_bytes-mc_len_bytes_8);        
        int16_t *dest_p=(int16_t *)buf;

        if(buf == playback_buf_mc)
        {
            sour_p=(int16_t *)playback_buf_bt;
        }

        if(playback_ch_num_bt==AUD_CHANNEL_NUM_2)
        {
            delay_sample=DELAY_SAMPLE_MC;
                
            for(i=0,j=0;i<delay_sample;i=i+2)
            {
                mid_p[j++]=delay_buf_bt[i];
                mid_p[j++]=delay_buf_bt[i+1];
            }

            for(i=0;i<playback_len_bytes/2-delay_sample;i=i+2)
            {
                mid_p[j++]=sour_p[i];
                mid_p[j++]=sour_p[i+1];
            }  

            for(j=0;i<playback_len_bytes/2;i=i+2)
            {
                delay_buf_bt[j++]=sour_p[i];
                delay_buf_bt[j++]=sour_p[i+1];
            }

            if(playback_samplerate_ratio_bt<=8)
            {
                for(i=0,j=0;i<playback_len_bytes/2;i=i+2*(8/playback_samplerate_ratio_bt))
                {
                    mid_p_8[j++]=mid_p[i];
                    mid_p_8[j++]=mid_p[i+1];
                }
            }
            else
            {
                for(i=0,j=0;i<playback_len_bytes/2;i=i+2)
                {
                    for(k=0;k<playback_samplerate_ratio_bt/8;k++)
                    {
                        mid_p_8[j++]=mid_p[i];
                        mid_p_8[j++]=mid_p[i+1];
                    }
                }
            }
            
            anc_mc_run_stereo((uint8_t *)mid_p_8,mc_len_bytes_8,left_gain,right_gain,AUD_BITS_16);

            for(i=0,j=0;i<(mc_len_bytes_8)/2;i=i+2)
            {
                for(k=0;k<8;k++)
                {
                    dest_p[j++]=mid_p_8[i];
                    dest_p[j++]=mid_p_8[i+1];
                }
            }
            
        }
        else if(playback_ch_num_bt==AUD_CHANNEL_NUM_1)
        {
            delay_sample=DELAY_SAMPLE_MC/2;
                
            for(i=0,j=0;i<delay_sample;i=i+1)
            {
                mid_p[j++]=delay_buf_bt[i];
            }

            for(i=0;i<playback_len_bytes/2-delay_sample;i=i+1)
            {
                mid_p[j++]=sour_p[i];
            }  

            for(j=0;i<playback_len_bytes/2;i=i+1)
            {
                delay_buf_bt[j++]=sour_p[i];
            }

            if(playback_samplerate_ratio_bt<=8)
            {
                for(i=0,j=0;i<playback_len_bytes/2;i=i+1*(8/playback_samplerate_ratio_bt))
                {
                    mid_p_8[j++]=mid_p[i];
                }
            }
            else
            {
                for(i=0,j=0;i<playback_len_bytes/2;i=i+1)
                {
                    for(k=0;k<playback_samplerate_ratio_bt/8;k++)
                    {
                        mid_p_8[j++]=mid_p[i];
                    }
                }
            }
            
            anc_mc_run_mono((uint8_t *)mid_p_8,mc_len_bytes_8,left_gain,AUD_BITS_16);

            for(i=0,j=0;i<(mc_len_bytes_8)/2;i=i+1)
            {
                for(k=0;k<8;k++)
                {
                    dest_p[j++]=mid_p_8[i];
                }
            }    
        }
   
    }
    else if (sample_size_play_bt == AUD_BITS_24)
    {
        int32_t *sour_p=(int32_t *)(playback_buf_bt+playback_size_bt/2);
        int32_t *mid_p=(int32_t *)(buf);
        int32_t *mid_p_8=(int32_t *)(buf+mc_len_bytes-mc_len_bytes_8);        
        int32_t *dest_p=(int32_t *)buf;

        if(buf == playback_buf_mc)
        {
            sour_p=(int32_t *)playback_buf_bt;
        }

        if(playback_ch_num_bt==AUD_CHANNEL_NUM_2)
        {
            delay_sample=DELAY_SAMPLE_MC;
                
            for(i=0,j=0;i<delay_sample;i=i+2)
            {
                mid_p[j++]=delay_buf_bt[i];
                mid_p[j++]=delay_buf_bt[i+1];
            }

            for(i=0;i<playback_len_bytes/4-delay_sample;i=i+2)
            {
                mid_p[j++]=sour_p[i];
                mid_p[j++]=sour_p[i+1];
            }  

            for(j=0;i<playback_len_bytes/4;i=i+2)
            {
                delay_buf_bt[j++]=sour_p[i];
                delay_buf_bt[j++]=sour_p[i+1];
            }

            if(playback_samplerate_ratio_bt<=8)
            {
                for(i=0,j=0;i<playback_len_bytes/4;i=i+2*(8/playback_samplerate_ratio_bt))
                {
                    mid_p_8[j++]=mid_p[i];
                    mid_p_8[j++]=mid_p[i+1];
                }
            }
            else
            {
                for(i=0,j=0;i<playback_len_bytes/4;i=i+2)
                {
                    for(k=0;k<playback_samplerate_ratio_bt/8;k++)
                    {
                        mid_p_8[j++]=mid_p[i];
                        mid_p_8[j++]=mid_p[i+1];
                    }
                }
            }
            
            anc_mc_run_stereo((uint8_t *)mid_p_8,mc_len_bytes_8,left_gain,right_gain,AUD_BITS_16);

            for(i=0,j=0;i<(mc_len_bytes_8)/4;i=i+2)
            {
                for(k=0;k<8;k++)
                {
                    dest_p[j++]=mid_p_8[i];
                    dest_p[j++]=mid_p_8[i+1];
                }
            }
            
        }
        else if(playback_ch_num_bt==AUD_CHANNEL_NUM_1)
        {
            delay_sample=DELAY_SAMPLE_MC/2;
                
            for(i=0,j=0;i<delay_sample;i=i+1)
            {
                mid_p[j++]=delay_buf_bt[i];
            }

            for(i=0;i<playback_len_bytes/4-delay_sample;i=i+1)
            {
                mid_p[j++]=sour_p[i];
            }  

            for(j=0;i<playback_len_bytes/4;i=i+1)
            {
                delay_buf_bt[j++]=sour_p[i];
            }

            if(playback_samplerate_ratio_bt<=8)
            {
                for(i=0,j=0;i<playback_len_bytes/4;i=i+1*(8/playback_samplerate_ratio_bt))
                {
                    mid_p_8[j++]=mid_p[i];
                }
            }
            else
            {
                for(i=0,j=0;i<playback_len_bytes/4;i=i+1)
                {
                    for(k=0;k<playback_samplerate_ratio_bt/8;k++)
                    {
                        mid_p_8[j++]=mid_p[i];
                    }
                }
            }
            
            anc_mc_run_mono((uint8_t *)mid_p_8,mc_len_bytes_8,left_gain,AUD_BITS_16);

            for(i=0,j=0;i<(mc_len_bytes_8)/4;i=i+1)
            {
                for(k=0;k<8;k++)
                {
                    dest_p[j++]=mid_p_8[i];
                }
            }    
        }
       
    }

  //  end_time = hal_sys_timer_get();

 //   TRACE("%s:run time: %d", __FUNCTION__, end_time-begin_time);

    return 0;
}
#endif

int speech_get_frame_size(int fs, int ch, int ms)
{
    return (fs / 1000 * ch * ms);
}

int speech_get_af_buffer_size(int fs, int ch, int ms)
{
    return speech_get_frame_size(fs, ch, ms) * 2 * 2;
}

enum AUD_SAMPRATE_T speech_sco_get_sample_rate(void)
{
    enum AUD_SAMPRATE_T sample_rate;

#if defined(HFP_1_6_ENABLE)
    if (bt_sco_codec_is_msbc())
    {
        sample_rate = AUD_SAMPRATE_16000;
    }
    else
#endif
    {
        sample_rate = AUD_SAMPRATE_8000;
    }

    return sample_rate;
}

enum AUD_SAMPRATE_T speech_codec_get_sample_rate(void)
{
    enum AUD_SAMPRATE_T sample_rate;

#if defined(MSBC_8K_SAMPLE_RATE)
    sample_rate = AUD_SAMPRATE_8000;
#else
    if (bt_sco_codec_is_msbc())
    {

        sample_rate = AUD_SAMPRATE_16000;
    }
    else
    {
        sample_rate = AUD_SAMPRATE_8000;
    }
#endif

    return sample_rate;
}

#if defined(TWS_AF_ADC_I2S_SYNC)
void bt_sco_bt_trigger_callback(void)
{
    LOG_PRINT_BT_STREAM("[%s] Start...", __func__);

#if defined(BT_SCO_I2S_ENABLE)
    bt_sco_i2s_start();
#endif
}
#endif

int bt_sco_player(bool on, enum APP_SYSFREQ_FREQ_T freq,uint32_t trigger_ticks)
{
    struct AF_STREAM_CONFIG_T stream_cfg;
    static bool isRun =  false;
    uint8_t * bt_audio_buff = NULL;
    enum AUD_SAMPRATE_T sample_rate;
    int8_t temp_volume;

    LOG_PRINT_BT_STREAM("bt_sco_player work:%d op:%d freq:%d", isRun, on, freq);

    if (isRun==on)
        return 0;

    if (on){
#ifdef IAG_BLE_INCLUDE    
        ble_tws_stop_all_activities();
#endif

        af_set_priority(osPriorityHigh);
        app_audio_manager_sco_status_checker();
#ifdef  __3RETX_SNIFF__   
         if(app_tws_mode_is_master()){
            bt_drv_reg_op_force_retrans(true);
        }else{
            bt_drv_reg_op_force_retrans(false);
        }
#endif

#if defined( __TWS__) && defined(__TWS_CALL_DUAL_CHANNEL__)
        memset((uint8_t *)&btpcm_time,0,sizeof(BTPCM_TIME_STRUCT));
        if(app_tws_mode_is_slave())
        {
            stream_local_volume = app_tws_get_tws_hfp_vol()+2;
        }
        else
        {
            stream_local_volume = btdevice_volume_p->hfp_vol;
        }
#else
         stream_local_volume = btdevice_volume_p->hfp_vol;
#endif
    LOG_PRINT_BT_STREAM("bt_sco_player volume=%d",stream_local_volume);

        if (freq < APP_SYSFREQ_104M)
        {
            freq = APP_SYSFREQ_104M;
        }

#if defined(SPEECH_TX_AEC2FLOAT)
        if (freq < APP_SYSFREQ_208M) {
            freq = APP_SYSFREQ_208M;
        }
#endif

#if defined(SPEECH_TX_2MIC_NS3)
        if (freq < APP_SYSFREQ_208M)
        {
            freq = APP_SYSFREQ_208M;
        }
#endif

#if defined(AUDIO_ANC_FB_MC) && defined(ANC_APP) && !defined(__AUDIO_RESAMPLE__)
        if (freq < APP_SYSFREQ_208M) {
            freq = APP_SYSFREQ_208M;
        }
#endif

        app_sysfreq_req(APP_SYSFREQ_USER_APP_0, freq);
        LOG_PRINT_BT_STREAM("bt_sco_player: app_sysfreq_req %d", freq);
        LOG_PRINT_BT_STREAM("sys freq calc : %d\n", hal_sys_timer_calc_cpu_freq(100, 0));
        bt_sco_player_restart_requeset(false);
#ifdef __TWS__
        app_tws_deactive();
#endif
#ifndef FPGA
        app_overlay_select(APP_OVERLAY_HFP);
#ifdef BT_XTAL_SYNC
        bt_init_xtal_sync(BT_XTAL_SYNC_MODE_VOICE, BT_INIT_XTAL_SYNC_MIN, BT_INIT_XTAL_SYNC_MAX);
#endif
#endif
        btdrv_rf_bit_offset_track_enable(true);

        int aec_frame_len = speech_get_frame_size(speech_codec_get_sample_rate(), 1, SPEECH_SCO_FRAME_MS);
        speech_tx_aec_set_frame_len(aec_frame_len);

        app_audio_mempool_use_mempoolsection_init();

#ifndef _SCO_BTPCM_CHANNEL_
        /*
        memset(&hf_sendbuff_ctrl, 0, sizeof(hf_sendbuff_ctrl));
        */
        app_hfp_clear_ctrl_buffer();
#endif

        sample_rate = speech_codec_get_sample_rate();

    sco_cap_chan_num = (enum AUD_CHANNEL_NUM_T)SPEECH_CODEC_CAPTURE_CHANNEL_NUM;

#ifdef __TWS_1CHANNEL_PCM__
        if(CFG_HW_AUD_OUTPUT_PATH_SPEAKER_DEV == (AUD_CHANNEL_MAP_CH0 | AUD_CHANNEL_MAP_CH1))
        {
            ASSERT(0,"CAN'T SET STEREO CHANNEL IN THIS FUNCION");
        }
        else
        {
            sco_play_chan_num = AUD_CHANNEL_NUM_1;
        }
#else
        sco_play_chan_num = AUD_CHANNEL_NUM_2;
#endif

#if defined(BT_SCO_I2S_ENABLE)
        // Initialize cnt        
        codec_capture_cnt = 0;
        codec_playback_cnt = 0;
        i2s_capture_cnt = 0;
        i2s_playback_cnt = 0;

        bt_sco_i2s_open();
#endif

        memset(&stream_cfg, 0, sizeof(stream_cfg));

        // codec:mic
        stream_cfg.channel_num = sco_cap_chan_num;
        stream_cfg.data_size = speech_get_af_buffer_size(sample_rate, sco_cap_chan_num, SPEECH_SCO_FRAME_MS);

#if defined(__AUDIO_RESAMPLE__) && defined(NO_SCO_RESAMPLE)
        // When __AUDIO_RESAMPLE__ is defined,
        // resample is off by default on best1000, and on by default on other platforms
#ifndef CHIP_BEST1000
        hal_cmu_audio_resample_disable();
#endif
#endif

#if defined(__AUDIO_RESAMPLE__) && defined(SW_SCO_RESAMPLE)
        if (sample_rate == AUD_SAMPRATE_8000)
        {
            stream_cfg.sample_rate = AUD_SAMPRATE_8463;
        }
        else if (sample_rate == AUD_SAMPRATE_16000)
        {
            stream_cfg.sample_rate = AUD_SAMPRATE_16927;
        }
        sco_capture_resample = app_capture_resample_open(sample_rate, stream_cfg.channel_num,
                            bt_sco_capture_resample_iter, stream_cfg.data_size / 2);

        uint32_t mono_cap_samp_cnt = stream_cfg.data_size / 2 / 2 / stream_cfg.channel_num;
        uint32_t cap_irq_cnt_per_frm = ((mono_cap_samp_cnt * stream_cfg.sample_rate + (sample_rate - 1)) / sample_rate +
            (aec_frame_len - 1)) / aec_frame_len;
        if (cap_irq_cnt_per_frm == 0) {
            cap_irq_cnt_per_frm = 1;
        }
#else
        stream_cfg.sample_rate = sample_rate;
#endif

        stream_cfg.bits = AUD_BITS_16;
        stream_cfg.vol = stream_local_volume;

#ifdef FPGA
        stream_cfg.device = AUD_STREAM_USE_EXT_CODEC;
#else
        stream_cfg.device = AUD_STREAM_USE_INT_CODEC;
#endif
        stream_cfg.io_path = AUD_INPUT_PATH_MAINMIC;
        stream_cfg.handler = bt_sco_codec_capture_data;
        app_audio_mempool_get_buff(&bt_audio_buff, stream_cfg.data_size);
        stream_cfg.data_ptr = BT_AUDIO_CACHE_2_UNCACHE(bt_audio_buff);

        LOG_PRINT_BT_STREAM("capture sample_rate:%d, data_size:%d",stream_cfg.sample_rate,stream_cfg.data_size);

        af_stream_open(AUD_STREAM_ID_0, AUD_STREAM_CAPTURE, &stream_cfg);
        if(trigger_test)
        {
            btdrv_set_bt_pcm_triggler_delay(trigger_test);
            btdrv_set_bt_pcm_en(0);
            trigger_test = 0;
        }
        else
        {

#ifdef CHIP_BEST2300
#ifdef APB_PCM  
            bt_drv_reg_enable_pcm_tx_hw_cal();
            btdrv_set_bt_pcm_en(0);
#else   
            btdrv_set_bt_pcm_triggler_delay(59);
            btdrv_set_bt_pcm_en(0);
#endif          
#else
            btdrv_set_bt_pcm_triggler_delay(55);
#endif
        }
        app_tws_set_trigger_time(trigger_ticks, false);

#if defined(TWS_AF_ADC_I2S_SYNC)
        af_codec_bt_trigger_config(true, bt_sco_bt_trigger_callback);
#endif
        
#ifdef CHIP_BEST2300        
#ifdef __TWS_CALL_DUAL_CHANNEL__
        //for 2300 trigger by luofei
        app_bt_block_sniffer_sco_for_trigger();
#endif //__TWS_CALL_DUAL_CHANNEL__
#endif

#ifdef __TWS_FORCE_CRC_ERROR__   
        btdrv_set_pcm_data_ignore_crc();
#endif

        // codec:spk
        sample_rate = speech_codec_get_sample_rate();  
        // sco_play_chan_num = AUD_CHANNEL_NUM_2;
        stream_cfg.channel_num = sco_play_chan_num;
        // stream_cfg.data_size = BT_AUDIO_SCO_BUFF_SIZE * stream_cfg.channel_num;
        stream_cfg.data_size = speech_get_af_buffer_size(sample_rate, sco_play_chan_num, SPEECH_SCO_FRAME_MS);
#if defined(__AUDIO_RESAMPLE__) && defined(SW_SCO_RESAMPLE)
        if (sample_rate == AUD_SAMPRATE_8000)
        {
            stream_cfg.sample_rate = AUD_SAMPRATE_8463;
        }
        else if (sample_rate == AUD_SAMPRATE_16000)
        {
            stream_cfg.sample_rate = AUD_SAMPRATE_16927;
        }
        sco_playback_resample = app_playback_resample_open(sample_rate, AUD_CHANNEL_NUM_1,
                            bt_sco_playback_resample_iter, stream_cfg.data_size / stream_cfg.channel_num / 2);
        sco_play_irq_cnt = 0;
        sco_dma_buf_err = false;

        uint32_t mono_play_samp_cnt = stream_cfg.data_size / 2 / 2 / stream_cfg.channel_num;
        uint32_t play_irq_cnt_per_frm = ((mono_play_samp_cnt * stream_cfg.sample_rate + (sample_rate - 1)) / sample_rate +
            (aec_frame_len - 1)) / aec_frame_len;
        if (play_irq_cnt_per_frm == 0) {
            play_irq_cnt_per_frm = 1;
        }
        uint32_t play_samp_cnt_per_frm = mono_play_samp_cnt * play_irq_cnt_per_frm;
        uint32_t cap_samp_cnt_per_frm = mono_cap_samp_cnt * cap_irq_cnt_per_frm;
        uint32_t max_samp_cnt_per_frm = (play_samp_cnt_per_frm >= cap_samp_cnt_per_frm) ? play_samp_cnt_per_frm : cap_samp_cnt_per_frm;
        uint32_t echo_q_samp_cnt = (((max_samp_cnt_per_frm + mono_play_samp_cnt * SCO_PLAY_RESAMPLE_ALIGN_CNT) *
            // convert to 8K/16K sample cnt
             sample_rate +(stream_cfg.sample_rate - 1)) / stream_cfg.sample_rate +
            // aligned with aec_frame_len
            (aec_frame_len - 1)) / aec_frame_len * aec_frame_len;
        if (echo_q_samp_cnt == 0) {
            echo_q_samp_cnt = aec_frame_len;
        }
        voicebtpcm_pcm_echo_buf_queue_init(echo_q_samp_cnt * 2);
#else
        stream_cfg.sample_rate = sample_rate;
#endif

#ifdef __BT_ANC__
        // Mono channel decoder buffer (8K or 16K sample rate)
        app_audio_mempool_get_buff(&bt_anc_sco_dec_buf, stream_cfg.data_size / 2 / sco_play_chan_num);
        // The playback size for the actual sample rate
        bt_sco_samplerate_ratio = 6/(sample_rate/AUD_SAMPRATE_8000);
        stream_cfg.data_size *= bt_sco_samplerate_ratio;
#if defined(__AUDIO_RESAMPLE__) && defined(SW_SCO_RESAMPLE)
        stream_cfg.sample_rate = AUD_SAMPRATE_50781;
#else
        stream_cfg.sample_rate = AUD_SAMPRATE_48000;
#endif

        //damic_init();
        //init_amic_dc_bt();
        //ds_fir_init();
        us_fir_init();
#endif
        bt_sco_mode = 1;

        stream_cfg.io_path = AUD_OUTPUT_PATH_SPEAKER;
        stream_cfg.handler = bt_sco_codec_playback_data;

        app_audio_mempool_get_buff(&bt_audio_buff, stream_cfg.data_size);
        stream_cfg.data_ptr = BT_AUDIO_CACHE_2_UNCACHE(bt_audio_buff);

        LOG_PRINT_BT_STREAM("playback sample_rate:%d, data_size:%d",stream_cfg.sample_rate,stream_cfg.data_size);

#if defined(AUDIO_ANC_FB_MC) && defined(ANC_APP) && !defined(__AUDIO_RESAMPLE__)
        sample_size_play_bt=stream_cfg.bits;
        sample_rate_play_bt=stream_cfg.sample_rate;
        data_size_play_bt=stream_cfg.data_size;
        playback_buf_bt=stream_cfg.data_ptr;
        playback_size_bt=stream_cfg.data_size;  

#ifdef __BT_ANC__
        playback_samplerate_ratio_bt=8;
#else        
        if (sample_rate_play_bt == AUD_SAMPRATE_8000)
        {
            playback_samplerate_ratio_bt=8*3*2;
        }
        else if (sample_rate_play_bt == AUD_SAMPRATE_16000)
        {
            playback_samplerate_ratio_bt=8*3;
        }
#endif
        
        playback_ch_num_bt=stream_cfg.channel_num;
#endif
        af_stream_open(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK, &stream_cfg);

#if defined(AUDIO_ANC_FB_MC) && defined(ANC_APP) && !defined(__AUDIO_RESAMPLE__)
        stream_cfg.bits = sample_size_play_bt;
        stream_cfg.channel_num = playback_ch_num_bt;
        stream_cfg.sample_rate = sample_rate_play_bt;
        stream_cfg.device = AUD_STREAM_USE_MC;
        stream_cfg.vol = 0;
        stream_cfg.handler = audio_mc_data_playback_sco;
        stream_cfg.io_path = AUD_OUTPUT_PATH_SPEAKER;

        app_audio_mempool_get_buff(&bt_audio_buff, data_size_play_bt*playback_samplerate_ratio_bt);
        stream_cfg.data_ptr = BT_AUDIO_CACHE_2_UNCACHE(bt_audio_buff);      
        stream_cfg.data_size = data_size_play_bt*playback_samplerate_ratio_bt;
        
        playback_buf_mc=stream_cfg.data_ptr;
        playback_size_mc=stream_cfg.data_size;  

        anc_mc_run_init(hal_codec_anc_convert_rate(sample_rate_play_bt));

        af_stream_open(AUD_STREAM_ID_2, AUD_STREAM_PLAYBACK, &stream_cfg);
#endif

        // Must call this function before af_stream_start
        // Get all free app audio buffer except SCO_BTPCM used(2k)
        voicebtpcm_pcm_audio_init();

#if defined(AUDIO_ANC_FB_MC) && defined(ANC_APP) && !defined(__AUDIO_RESAMPLE__)
        af_stream_start(AUD_STREAM_ID_2, AUD_STREAM_PLAYBACK);
#endif

#if !defined(TWS_AF_ADC_I2S_SYNC) && defined(BT_SCO_I2S_ENABLE)
        bt_sco_i2s_start();
#endif

        af_stream_start(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK);
        af_stream_start(AUD_STREAM_ID_0, AUD_STREAM_CAPTURE);

#ifdef SPEECH_SIDETONE
        // channel {0: left channel; 1: right channel}
        // reduce gain, step 2dB, {for example, 0x0a: -20dB}
        hal_codec_sidetone_enable(0, 0x0a);
#endif            

#ifdef _SCO_BTPCM_CHANNEL_
        stream_cfg.sample_rate = speech_sco_get_sample_rate();
        stream_cfg.channel_num = AUD_CHANNEL_NUM_1;
        // stream_cfg.data_size = BT_AUDIO_SCO_BUFF_SIZE * stream_cfg.channel_num;
        stream_cfg.data_size = speech_get_af_buffer_size(stream_cfg.sample_rate, stream_cfg.channel_num, SPEECH_SCO_FRAME_MS);
        // btpcm:rx
        stream_cfg.device = AUD_STREAM_USE_BT_PCM;
        stream_cfg.handler = bt_sco_btpcm_capture_data;
        app_audio_mempool_get_buff(&bt_audio_buff, stream_cfg.data_size);
        stream_cfg.data_ptr = BT_AUDIO_CACHE_2_UNCACHE(bt_audio_buff);

        LOG_PRINT_BT_STREAM("sco btpcm sample_rate:%d, data_size:%d",stream_cfg.sample_rate,stream_cfg.data_size);

        af_stream_open(AUD_STREAM_ID_1, AUD_STREAM_CAPTURE, &stream_cfg);

        // btpcm:tx
        stream_cfg.device = AUD_STREAM_USE_BT_PCM;
        stream_cfg.handler = bt_sco_btpcm_playback_data;
        app_audio_mempool_get_buff(&bt_audio_buff, stream_cfg.data_size);
        stream_cfg.data_ptr = BT_AUDIO_CACHE_2_UNCACHE(bt_audio_buff);
        af_stream_open(AUD_STREAM_ID_1, AUD_STREAM_PLAYBACK, &stream_cfg);

        af_stream_start(AUD_STREAM_ID_1, AUD_STREAM_PLAYBACK);
        af_stream_start(AUD_STREAM_ID_1, AUD_STREAM_CAPTURE);

#ifdef CHIP_BEST2300
        btdrv_pcm_enable();
        if (bt_sco_get_current_codecid() == BTIF_HF_SCO_CODEC_CVSD) {
            btdrv_linear_format_16bit_set();
        }
#endif
#endif
#if defined( __TWS__) && defined(__TWS_CALL_DUAL_CHANNEL__)
        if(app_tws_mode_is_slave())
        {
            temp_volume = app_tws_get_tws_hfp_vol()+2;
        }
        else
        {
            temp_volume = btdevice_volume_p->hfp_vol;
        }
#else
        temp_volume = btdevice_volume_p->hfp_vol;
#endif
        if (stream_local_volume != temp_volume){
            stream_local_volume = temp_volume;
            app_bt_stream_volumeset(stream_local_volume);
        }

#ifdef FPGA
        app_bt_stream_volumeset(stream_local_volume);
#endif
        LOG_PRINT_BT_STREAM("bt_sco_player on");
    }
    else
    {
#ifdef __TWS_CALL_DUAL_CHANNEL__
        sco_trigger_time=0;
        msbc_received = 0;
        btpcm_time.sco_play_count = 0;
        btpcm_time.btpcm_count = 0;
        msbc_sn_rx_ok = 0;
#ifdef CHIP_BEST2300
        btdrv_set_bt_pcm_triggler_en(1);
#endif
#endif
        af_stream_stop(AUD_STREAM_ID_0, AUD_STREAM_CAPTURE);
        af_stream_stop(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK);
#if defined(AUDIO_ANC_FB_MC) && defined(ANC_APP) && !defined(__AUDIO_RESAMPLE__)
        af_stream_stop(AUD_STREAM_ID_2, AUD_STREAM_PLAYBACK);
#endif

#ifdef _SCO_BTPCM_CHANNEL_
        af_stream_stop(AUD_STREAM_ID_1, AUD_STREAM_CAPTURE);
        af_stream_stop(AUD_STREAM_ID_1, AUD_STREAM_PLAYBACK);

        af_stream_close(AUD_STREAM_ID_1, AUD_STREAM_CAPTURE);
        af_stream_close(AUD_STREAM_ID_1, AUD_STREAM_PLAYBACK);
#endif
        af_stream_close(AUD_STREAM_ID_0, AUD_STREAM_CAPTURE);
        af_stream_close(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK);
#if defined(AUDIO_ANC_FB_MC) && defined(ANC_APP) && !defined(__AUDIO_RESAMPLE__)
        af_stream_close(AUD_STREAM_ID_2, AUD_STREAM_PLAYBACK);
#endif

#if defined(BT_SCO_I2S_ENABLE)
        bt_sco_i2s_stop();
        bt_sco_i2s_close();
#endif
        
        osDelay(10);

#ifdef SPEECH_SIDETONE
        hal_codec_sidetone_disable();
#endif           

#if defined(__AUDIO_RESAMPLE__) && defined(SW_SCO_RESAMPLE)
        app_capture_resample_close(sco_capture_resample);
        sco_capture_resample = NULL;
        app_capture_resample_close(sco_playback_resample);
        sco_playback_resample = NULL;
#endif

#if defined(__AUDIO_RESAMPLE__) && defined(NO_SCO_RESAMPLE)
#ifndef CHIP_BEST1000
        // When __AUDIO_RESAMPLE__ is defined,
        // resample is off by default on best1000, and on by default on other platforms
        hal_cmu_audio_resample_enable();
#endif
#endif

        bt_sco_mode = 0;

#ifdef __BT_ANC__
        bt_anc_sco_dec_buf = NULL;
        //damic_deinit();
        //app_cap_thread_stop();
#endif
        voicebtpcm_pcm_audio_deinit();

#ifndef FPGA
#ifdef BT_XTAL_SYNC
//        osDelay(50);
        bt_term_xtal_sync(false);
#if 1//ndef BT_XTAL_SYNC_NEW_METHOD
        bt_term_xtal_sync_default();
#endif
#endif
#endif
        LOG_PRINT_BT_STREAM("bt_sco_player off");
        app_overlay_unloadall();
        app_sysfreq_req(APP_SYSFREQ_USER_APP_0, APP_SYSFREQ_32K);
        af_set_priority(osPriorityAboveNormal);

        //bt_syncerr set to default(0x07)
 //       BTDIGITAL_REG_SET_FIELD(REG_BTCORE_BASE_ADDR, 0x0f, 0, 0x07);
    }

    isRun=on;
    return 0;
}

uint16_t gStreamplayer = APP_BT_STREAM_INVALID;
int app_bt_stream_open(APP_AUDIO_STATUS* status)
{
    int nRet = -1;
    uint16_t player = (uint16_t)status->id;
    APP_AUDIO_STATUS next_status;
    enum APP_SYSFREQ_FREQ_T freq = (enum APP_SYSFREQ_FREQ_T)status->freq;

    LOG_PRINT_BT_STREAM("app_bt_stream_open prev:%d cur:%d freq:%d,param=%d", gStreamplayer, player, freq,status->param);

    if (gStreamplayer != APP_BT_STREAM_INVALID)
    {
    #if !ISOLATED_AUDIO_STREAM_ENABLED
        LOG_PRINT_BT_STREAM("Close prev bt stream before opening");
        nRet = app_bt_stream_close(gStreamplayer);
        if (nRet)
        {
            return -1;
        }
        else
        {
            app_audio_list_rmv_callback(status, &next_status);
        }
    #else
        if (gStreamplayer & player)
        {
            return -1;
        }
        
        if (player >= APP_BT_STREAM_BORDER_INDEX)
        {
            if (APP_BT_INPUT_STREAM_INDEX(gStreamplayer) > 0)
            {
                LOG_PRINT_BT_STREAM("Close prev input bt stream before opening");
                nRet = app_bt_stream_close(APP_BT_INPUT_STREAM_INDEX(gStreamplayer));
                if (nRet)
                {
                    return -1;
                }
                else
                {
                    app_audio_list_rmv_callback(status, &next_status);
                }
            }
        }
        else
        {
            if (APP_BT_OUTPUT_STREAM_INDEX(gStreamplayer) > 0)
            {
                LOG_PRINT_BT_STREAM("Close prev output bt stream before opening");
                nRet = app_bt_stream_close(APP_BT_OUTPUT_STREAM_INDEX(gStreamplayer));
                if (nRet)
                {
                    return -1;
                }
                else
                {
                    app_audio_list_rmv_callback(status, &next_status);
                }
            }
        }          
    #endif
    }

    switch (player) {
        case APP_BT_STREAM_HFP_PCM:
        case APP_BT_STREAM_HFP_CVSD:
        case APP_BT_STREAM_HFP_VENDOR:
            nRet = bt_sco_player(true, freq,status->param);
            break;
        case APP_BT_STREAM_A2DP_SBC:
        case APP_BT_STREAM_A2DP_AAC:
        case APP_BT_STREAM_A2DP_VENDOR:
            nRet = bt_sbc_player(PLAYER_OPER_START, freq,status->param);
            break;
#ifdef __FACTORY_MODE_SUPPORT__
        case APP_FACTORYMODE_AUDIO_LOOP:
            nRet = app_factorymode_audioloop(true, freq);
            break;
#endif
   #ifdef MEDIA_PLAYER_SUPPORT
        case APP_PLAY_BACK_AUDIO:
            nRet = app_play_audio_onoff(true, status);
            break;
    #endif
#ifdef TWS_RBCODEC_PLAYER
        case APP_BT_STREAM_RBCODEC:
            nRet = bt_sbc_local_player_init(true, status->param);
            break;
#endif
#if VOICE_DATAPATH
        case APP_BT_STREAM_VOICEPATH:
            nRet = app_voicepath_start_audio_stream();
            break;
#endif
#ifdef TWS_LINEIN_PLAYER
        case APP_BT_STREAM_LINEIN:
            nRet = app_linein_audio_onoff(true, status->param);
            break;
#endif
        default:
            nRet = -1;
            break;
    }

    if (!nRet)
    {
        gStreamplayer |= player;
    }

    return nRet;
}

int app_bt_stream_close(uint16_t player)
{
    int nRet = -1;
    LOG_PRINT_BT_STREAM("app_bt_stream_close prev:%d cur:%d", gStreamplayer, player);

    if ((gStreamplayer & player) != player)
    {
        return -1;
    }

    switch (player) {
        case APP_BT_STREAM_HFP_PCM:
        case APP_BT_STREAM_HFP_CVSD:
        case APP_BT_STREAM_HFP_VENDOR:
            nRet = bt_sco_player(false, APP_SYSFREQ_32K,0);
            break;
        case APP_BT_STREAM_A2DP_SBC:
        case APP_BT_STREAM_A2DP_AAC:
        case APP_BT_STREAM_A2DP_VENDOR:
            nRet = bt_sbc_player(PLAYER_OPER_STOP, APP_SYSFREQ_32K,0);
            break;
#ifdef __FACTORY_MODE_SUPPORT__
        case APP_FACTORYMODE_AUDIO_LOOP:
            nRet = app_factorymode_audioloop(false, APP_SYSFREQ_32K);
            break;
#endif
#ifdef MEDIA_PLAYER_SUPPORT
       case APP_PLAY_BACK_AUDIO:
            nRet = app_play_audio_onoff(false, NULL);
            break;
#endif
#ifdef TWS_RBCODEC_PLAYER
        case APP_BT_STREAM_RBCODEC:
            nRet = bt_sbc_local_player_init(false, 0);
            break;
#endif
#if VOICE_DATAPATH
        case APP_BT_STREAM_VOICEPATH:
            nRet = app_voicepath_stop_audio_stream();
            break;
#endif
#ifdef TWS_LINEIN_PLAYER
        case APP_BT_STREAM_LINEIN:
            bt_sbc_local_player_init(false, 0);
            nRet = app_linein_audio_onoff(false, 0);
            break;
#endif
        default:
            nRet = -1;
            break;
    }
    if (!nRet)
    {
        gStreamplayer &= (~player);
    }
    return nRet;
}



int app_bt_stream_setup(uint16_t player, uint32_t param)
{
    int nRet = -1;
    uint8_t opt;

//    opt = APP_BT_SBC_PLAYER_TRIGGER;
//    opt = param>>28;
//    param &= 0x0fffffff;
    if(param)
    {
        opt = APP_BT_SBC_PLAYER_RESTART;
    }
    else
    {
        opt = APP_BT_SBC_PLAYER_PAUSE;
    }

    switch (player)
    {
#ifdef TWS_RBCODEC_PLAYER    
        case APP_BT_STREAM_RBCODEC:
#endif
#ifdef TWS_LINEIN_PLAYER
        case APP_BT_STREAM_LINEIN:
#endif            
        case APP_BT_STREAM_A2DP_SBC:
            nRet = bt_sbc_player_setup(opt, param);
            break;
        default:
            ASSERT(0, "[%s] %d is invalid player", __func__, player);
            break;
    }

    return nRet;
}

int app_bt_stream_restart(APP_AUDIO_STATUS* status)
{
    int nRet = -1;
    uint16_t player = status->id;
    enum APP_SYSFREQ_FREQ_T freq = (enum APP_SYSFREQ_FREQ_T)status->freq;

    LOG_PRINT_BT_STREAM("app_bt_stream_restart prev:%d cur:%d freq:%d", gStreamplayer, player, freq);

    if ((gStreamplayer & player) != player)
    {
        return -1;
    }

    switch (player) {
        case APP_BT_STREAM_HFP_PCM:
        case APP_BT_STREAM_HFP_CVSD:
        case APP_BT_STREAM_HFP_VENDOR:
            break;
        case APP_BT_STREAM_A2DP_SBC:
        case APP_BT_STREAM_A2DP_AAC:
        case APP_BT_STREAM_A2DP_VENDOR:
            nRet = bt_sbc_player(PLAYER_OPER_RESTART, freq,status->param);
            break;
#ifdef __FACTORY_MODE_SUPPORT__
        case APP_FACTORYMODE_AUDIO_LOOP:
            break;
#endif
#ifdef MEDIA_PLAYER_SUPPORT
        case APP_PLAY_BACK_AUDIO:
            break;
#endif
        default:
            nRet = -1;
            break;
    }

    return nRet;
}

void app_bt_stream_volumeup(void)
{
    if (app_bt_stream_isrun(APP_BT_STREAM_A2DP_SBC))
    {
        LOG_PRINT_BT_STREAM("%s set audio volume", __func__);
        btdevice_volume_p->a2dp_vol++;
        app_bt_stream_volumeset(btdevice_volume_p->a2dp_vol);
    }

    if (app_bt_stream_isrun(APP_BT_STREAM_HFP_PCM))
    {
        LOG_PRINT_BT_STREAM("%s set hfp volume", __func__);
        btdevice_volume_p->hfp_vol++;
        app_bt_stream_volumeset(btdevice_volume_p->hfp_vol);
    }

    if (app_bt_stream_isrun(APP_BT_STREAM_INVALID))
    {
        LOG_PRINT_BT_STREAM("%s set idle volume", __func__);
        btdevice_volume_p->a2dp_vol++;
    }

    if (btdevice_volume_p->a2dp_vol > TGT_VOLUME_LEVEL_15)
    {
        btdevice_volume_p->a2dp_vol = TGT_VOLUME_LEVEL_15;
#ifdef MEDIA_PLAYER_SUPPORT
        media_PlayAudio(AUD_ID_BT_WARNING,0);
#endif
    }

    if (btdevice_volume_p->hfp_vol > TGT_VOLUME_LEVEL_15)
    {
        btdevice_volume_p->hfp_vol = TGT_VOLUME_LEVEL_15;
#ifdef MEDIA_PLAYER_SUPPORT
        media_PlayAudio(AUD_ID_BT_WARNING,0);
#endif
    }
    #ifndef FPGA
    nv_record_touch_cause_flush();
    #endif

    LOG_PRINT_BT_STREAM("%s a2dp: %d", __func__, btdevice_volume_p->a2dp_vol);
    LOG_PRINT_BT_STREAM("%s hfp: %d", __func__, btdevice_volume_p->hfp_vol);
}

void app_bt_stream_volumedown(void)
{
    if (app_bt_stream_isrun(APP_BT_STREAM_A2DP_SBC))
    {
        LOG_PRINT_BT_STREAM("%s set audio volume", __func__);
        btdevice_volume_p->a2dp_vol--;
        app_bt_stream_volumeset(btdevice_volume_p->a2dp_vol);
    }

    if (app_bt_stream_isrun(APP_BT_STREAM_HFP_PCM))
    {
        LOG_PRINT_BT_STREAM("%s set hfp volume", __func__);
        btdevice_volume_p->hfp_vol--;
        app_bt_stream_volumeset(btdevice_volume_p->hfp_vol);
    }

    if (app_bt_stream_isrun(APP_BT_STREAM_INVALID))
    {
        LOG_PRINT_BT_STREAM("%s set idle volume", __func__);
        btdevice_volume_p->a2dp_vol--;
    }

    if (btdevice_volume_p->a2dp_vol < TGT_VOLUME_LEVEL_0)
    {
        btdevice_volume_p->a2dp_vol = TGT_VOLUME_LEVEL_0;
#ifdef MEDIA_PLAYER_SUPPORT
        media_PlayAudio(AUD_ID_BT_WARNING,0);
#endif
    }

    if (btdevice_volume_p->hfp_vol < TGT_VOLUME_LEVEL_0)
    {
        btdevice_volume_p->hfp_vol = TGT_VOLUME_LEVEL_0;
#ifdef MEDIA_PLAYER_SUPPORT
        media_PlayAudio(AUD_ID_BT_WARNING,0);
#endif
    }
    #ifndef FPGA
    nv_record_touch_cause_flush();
    #endif

    LOG_PRINT_BT_STREAM("%s a2dp: %d", __func__, btdevice_volume_p->a2dp_vol);
    LOG_PRINT_BT_STREAM("%s hfp: %d", __func__, btdevice_volume_p->hfp_vol);
}

int app_bt_stream_volumeset(int8_t vol)
{
    uint32_t ret;
    struct AF_STREAM_CONFIG_T *stream_cfg = NULL;

    if (vol > TGT_VOLUME_LEVEL_15)
        vol = TGT_VOLUME_LEVEL_15;
    if (vol < TGT_VOLUME_LEVEL_0)
        vol = TGT_VOLUME_LEVEL_0;
    LOG_PRINT_BT_STREAM("app_bt_stream_volumeset vol=%d", vol);

    stream_local_volume = vol;
    if (!app_bt_stream_isrun(APP_PLAY_BACK_AUDIO))
    {
        ret = af_stream_get_cfg(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK, &stream_cfg, true);
        if (ret == 0) {
            stream_cfg->vol = vol;
            af_stream_setup(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK, stream_cfg);
        }
#if defined(AUDIO_ANC_FB_MC) && defined(ANC_APP) && !defined(__AUDIO_RESAMPLE__)
        ret = af_stream_get_cfg(AUD_STREAM_ID_2, AUD_STREAM_PLAYBACK, &stream_cfg, true);
        if (ret == 0) {
            stream_cfg->vol = vol;
            af_stream_setup(AUD_STREAM_ID_2, AUD_STREAM_PLAYBACK, stream_cfg);
        }
#endif

        
    }
    return 0;
}

int app_bt_stream_local_volume_get(void)
{
    return stream_local_volume;
}

void app_bt_stream_a2dpvolume_reset(void)
{
     btdevice_volume_p->a2dp_vol = NVRAM_ENV_STREAM_VOLUME_A2DP_VOL_DEFAULT ;
}

void app_bt_stream_hfpvolume_reset(void)
{
     btdevice_volume_p->hfp_vol = NVRAM_ENV_STREAM_VOLUME_HFP_VOL_DEFAULT;
}

#ifdef __TWS_ROLE_SWITCH__
extern"C" bool app_tws_is_role_switching_on(void);
#endif
void app_bt_stream_volume_ptr_update(uint8_t *bdAddr)
{
    static struct btdevice_volume stream_volume = {NVRAM_ENV_STREAM_VOLUME_A2DP_VOL_DEFAULT,NVRAM_ENV_STREAM_VOLUME_HFP_VOL_DEFAULT};
#ifdef __TWS_ROLE_SWITCH__
    if(app_tws_is_role_switching_on())
    {
        TRACE("app_bt_stream_volume_ptr_update");
        return ;
    }
#endif
#ifndef FPGA
    nvrec_btdevicerecord *record = NULL;
    if (!nv_record_btdevicerecord_find((bt_bdaddr_t*)bdAddr,&record)){
        btdevice_volume_p = &(record->device_vol);
        LOG_PRINT_ENV_DUMP8("0x%02x ", bdAddr, BTIF_BD_ADDR_SIZE);
        LOG_PRINT_ENV("%s a2dp_vol:%d hfp_vol:%d ptr:0x%x", __func__, btdevice_volume_p->a2dp_vol, btdevice_volume_p->hfp_vol,btdevice_volume_p);
    }else
#endif
    {
        btdevice_volume_p = &stream_volume;
        LOG_PRINT_ENV("%s default", __func__);
    }
}

struct btdevice_volume * app_bt_stream_volume_get_ptr(void)
{
    return btdevice_volume_p;
}

bool app_bt_stream_isrun(uint16_t player)
{
    if((player == APP_BT_STREAM_INVALID) && (gStreamplayer != APP_BT_STREAM_INVALID))
    {
        return false;
    }
    else if ((gStreamplayer & player) == player)
    {
        return true;
    }
    else
    {
        return false;
    }
}

int app_bt_stream_closeall()
{
    LOG_PRINT_BT_STREAM("app_bt_stream_closeall");

    bt_sco_player(false, APP_SYSFREQ_32K,0);
    bt_sbc_player(PLAYER_OPER_STOP, APP_SYSFREQ_32K,0);

    #ifdef MEDIA_PLAYER_SUPPORT
    app_play_audio_onoff(false, NULL);
    #endif
#if VOICE_DATAPATH
    app_voicepath_stop_audio_stream();
#endif
    gStreamplayer = APP_BT_STREAM_INVALID;
    return 0;
}

void app_bt_stream_copy_track_one_to_two_16bits(int16_t *dst_buf, int16_t *src_buf, uint32_t src_len)
{
    // Copy from tail so that it works even if dst_buf == src_buf
    for (int i = (int)(src_len - 1); i >= 0; i--)
    {
        dst_buf[i*2 + 0] = dst_buf[i*2 + 1] = src_buf[i];
    }
}

void app_bt_stream_copy_track_two_to_one_16bits(int16_t *dst_buf, int16_t *src_buf, uint32_t dst_len)
{
    for (uint32_t i = 0; i < dst_len; i++)
    {
        dst_buf[i] = src_buf[i*2];
    }
}

// =======================================================
// APP RESAMPLE
// =======================================================

#if !(MIX_MIC_DURING_MUSIC)
#include "resample_coef.h"
#endif

static float ratio_step_factor = 1.0f;

static APP_RESAMPLE_BUF_ALLOC_CALLBACK resamp_buf_alloc = app_audio_mempool_get_buff;

static void memzero_int16(void *dst, uint32_t len)
{
    int16_t *dst16 = (int16_t *)dst;
    int16_t *dst16_end = dst16 + len / 2;

    while (dst16 < dst16_end)
    {
        *dst16++ = 0;
    }
}

static struct APP_RESAMPLE_T *app_resample_open(enum AUD_STREAM_T stream, const struct RESAMPLE_COEF_T *coef, enum AUD_CHANNEL_NUM_T chans,
        APP_RESAMPLE_ITER_CALLBACK cb, uint32_t iter_len,
        float ratio_step)
{
    LOG_PRINT_BT_STREAM("app resample is opened.");
    struct APP_RESAMPLE_T *resamp;
    struct RESAMPLE_CFG_T cfg;
    enum RESAMPLE_STATUS_T status;
    uint32_t size, resamp_size;
    uint8_t *buf;

    resamp_size = audio_resample_ex_get_buffer_size(chans, AUD_BITS_16, coef->phase_coef_num);

    size = sizeof(struct APP_RESAMPLE_T);
    size += ALIGN(iter_len, 4);
    size += resamp_size;

    resamp_buf_alloc(&buf, size);

    resamp = (struct APP_RESAMPLE_T *)buf;
    buf += sizeof(*resamp);
    resamp->stream = stream;
    resamp->cb = cb;
    resamp->iter_buf = buf;
    buf += ALIGN(iter_len, 4);
    resamp->iter_len = iter_len;
    resamp->offset = iter_len;
    resamp->ratio_step = ratio_step;

    memset(&cfg, 0, sizeof(cfg));
    cfg.chans = chans;
    cfg.bits = AUD_BITS_16;
    if (stream == AUD_STREAM_PLAYBACK) {
        cfg.ratio_step = ratio_step * ratio_step_factor;
    } else {
        cfg.ratio_step = ratio_step / ratio_step_factor;
    }
    cfg.coef = coef;
    cfg.buf = buf;
    cfg.size = resamp_size;

    status = audio_resample_ex_open(&cfg, (RESAMPLE_ID *)&resamp->id);
    ASSERT(status == RESAMPLE_STATUS_OK, "%s: Failed to open resample: %d", __func__, status);

#ifdef CHIP_BEST1000
    hal_cmu_audio_resample_enable();
#endif

    return resamp;
}

static int app_resample_close(struct APP_RESAMPLE_T *resamp)
{
#ifdef CHIP_BEST1000
    hal_cmu_audio_resample_disable();
#endif

    if (resamp)
    {
        audio_resample_ex_close((RESAMPLE_ID *)resamp->id);
    }

    return 0;
}

struct APP_RESAMPLE_T *app_playback_resample_open(enum AUD_SAMPRATE_T sample_rate, enum AUD_CHANNEL_NUM_T chans,
        APP_RESAMPLE_ITER_CALLBACK cb, uint32_t iter_len)
{
    const struct RESAMPLE_COEF_T *coef = NULL;

    if (sample_rate == AUD_SAMPRATE_8000)
    {
        coef = &resample_coef_8k_to_8p4k;
    }
    else if (sample_rate == AUD_SAMPRATE_16000)
    {
        coef = &resample_coef_8k_to_8p4k;
    }
    else if (sample_rate == AUD_SAMPRATE_32000)
    {
        coef = &resample_coef_32k_to_50p7k;
    }
    else if (sample_rate == AUD_SAMPRATE_44100)
    {
        coef = &resample_coef_44p1k_to_50p7k;
    }
    else if (sample_rate == AUD_SAMPRATE_48000)
    {
        coef = &resample_coef_48k_to_50p7k;
    }
    else
    {
        ASSERT(false, "%s: Bad sample rate: %u", __func__, sample_rate);
    }

    return app_resample_open(AUD_STREAM_PLAYBACK, coef, chans, cb, iter_len, 0);
}

struct APP_RESAMPLE_T *app_playback_resample_any_open(enum AUD_CHANNEL_NUM_T chans,
        APP_RESAMPLE_ITER_CALLBACK cb, uint32_t iter_len,
        float ratio_step)
{
    const struct RESAMPLE_COEF_T *coef = &resample_coef_any_up256;

    return app_resample_open(AUD_STREAM_PLAYBACK, coef, chans, cb, iter_len, ratio_step);
}

#ifdef PLAYBACK_FORCE_48K
static int app_force48k_resample_iter(uint8_t *buf, uint32_t len)
{
    uint8_t codec_type = BTIF_AVDTP_CODEC_TYPE_SBC;
    codec_type = bt_sbc_player_get_codec_type();
    a2dp_audio_more_data(codec_type, buf, len);
    return 0;
}

struct APP_RESAMPLE_T *app_force48k_resample_any_open(enum AUD_CHANNEL_NUM_T chans,
        APP_RESAMPLE_ITER_CALLBACK cb, uint32_t iter_len,
        float ratio_step)
{
    const struct RESAMPLE_COEF_T *coef = &resample_coef_any_up256;

    return app_resample_open(AUD_STREAM_PLAYBACK, coef, chans, cb, iter_len, ratio_step);
}
#endif

int app_playback_resample_close(struct APP_RESAMPLE_T *resamp)
{
    return app_resample_close(resamp);
}

int app_playback_resample_run(struct APP_RESAMPLE_T *resamp, uint8_t *buf, uint32_t len)
{
    uint32_t in_size, out_size;
    struct RESAMPLE_IO_BUF_T io;
    enum RESAMPLE_STATUS_T status;
    int ret;
    //uint32_t lock;

    if (resamp == NULL)
    {
        goto _err_exit;
    }

    io.out_cyclic_start = NULL;
    io.out_cyclic_end = NULL;

    if (resamp->offset < resamp->iter_len)
    {
        io.in = resamp->iter_buf + resamp->offset;
        io.in_size = resamp->iter_len - resamp->offset;
        io.out = buf;
        io.out_size = len;

        //lock = int_lock();
        status = audio_resample_ex_run((RESAMPLE_ID *)resamp->id, &io, &in_size, &out_size);
        //int_unlock(lock);
        if (status != RESAMPLE_STATUS_OUT_FULL && status != RESAMPLE_STATUS_IN_EMPTY &&
            status != RESAMPLE_STATUS_DONE)
        {
            goto _err_exit;
        }

        buf += out_size;
        len -= out_size;
        resamp->offset += in_size;

        ASSERT(len == 0 || resamp->offset == resamp->iter_len,
            "%s: Bad resample offset: len=%d offset=%u iter_len=%u",
            __func__, len, resamp->offset, resamp->iter_len);
    }

    while (len)
    {
        ret = resamp->cb(resamp->iter_buf, resamp->iter_len);
        if (ret)
        {
            goto _err_exit;
        }

        io.in = resamp->iter_buf;
        io.in_size = resamp->iter_len;
        io.out = buf;
        io.out_size = len;

        //lock = int_lock();
        status = audio_resample_ex_run((RESAMPLE_ID *)resamp->id, &io, &in_size, &out_size);
        //int_unlock(lock);
        if (status != RESAMPLE_STATUS_OUT_FULL && status != RESAMPLE_STATUS_IN_EMPTY &&
            status != RESAMPLE_STATUS_DONE)
        {
            goto _err_exit;
        }

        ASSERT(out_size <= len, "%s: Bad resample out_size: out_size=%u len=%d", __func__, out_size, len);
        ASSERT(in_size <= resamp->iter_len, "%s: Bad resample in_size: in_size=%u iter_len=%u", __func__, in_size, resamp->iter_len);

        buf += out_size;
        len -= out_size;
        if (in_size != resamp->iter_len)
        {
            resamp->offset = in_size;

            ASSERT(len == 0, "%s: Bad resample len: len=%d out_size=%u", __func__, len, out_size);
        }
    }

    return 0;

_err_exit:
    if (resamp)
    {
        app_resample_reset(resamp);
    }

    memzero_int16(buf, len);

    return 1;
}

struct APP_RESAMPLE_T *app_capture_resample_open(enum AUD_SAMPRATE_T sample_rate, enum AUD_CHANNEL_NUM_T chans,
        APP_RESAMPLE_ITER_CALLBACK cb, uint32_t iter_len)
{
    const struct RESAMPLE_COEF_T *coef = NULL;

    if (sample_rate == AUD_SAMPRATE_8000)
    {
        coef = &resample_coef_8p4k_to_8k;
    }
    else if (sample_rate == AUD_SAMPRATE_16000)
    {
        // Same coef as 8K sample rate
        coef = &resample_coef_8p4k_to_8k;
    }
    else
    {
        ASSERT(false, "%s: Bad sample rate: %u", __func__, sample_rate);
    }

    return app_resample_open(AUD_STREAM_CAPTURE, coef, chans, cb, iter_len, 0);

}

struct APP_RESAMPLE_T *app_capture_resample_any_open(enum AUD_CHANNEL_NUM_T chans,
        APP_RESAMPLE_ITER_CALLBACK cb, uint32_t iter_len,
        float ratio_step)
{
    const struct RESAMPLE_COEF_T *coef = &resample_coef_any_up256;
    return app_resample_open(AUD_STREAM_CAPTURE, coef, chans, cb, iter_len, ratio_step);
}

int app_capture_resample_close(struct APP_RESAMPLE_T *resamp)
{
    return app_resample_close(resamp);
}

int app_capture_resample_run(struct APP_RESAMPLE_T *resamp, uint8_t *buf, uint32_t len)
{
    uint32_t in_size, out_size;
    struct RESAMPLE_IO_BUF_T io;
    enum RESAMPLE_STATUS_T status;
    int ret;

    if (resamp == NULL)
    {
        goto _err_exit;
    }

    io.out_cyclic_start = NULL;
    io.out_cyclic_end = NULL;

    if (resamp->offset < resamp->iter_len)
    {
        io.in = buf;
        io.in_size = len;
        io.out = resamp->iter_buf + resamp->offset;
        io.out_size = resamp->iter_len - resamp->offset;

        status = audio_resample_ex_run((RESAMPLE_ID *)resamp->id, &io, &in_size, &out_size);
        if (status != RESAMPLE_STATUS_OUT_FULL && status != RESAMPLE_STATUS_IN_EMPTY &&
            status != RESAMPLE_STATUS_DONE)
        {
            goto _err_exit;
        }

        buf += in_size;
        len -= in_size;
        resamp->offset += out_size;

        ASSERT(len == 0 || resamp->offset == resamp->iter_len,
            "%s: Bad resample offset: len=%d offset=%u iter_len=%u",
            __func__, len, resamp->offset, resamp->iter_len);

        if (resamp->offset == resamp->iter_len)
        {
            ret = resamp->cb(resamp->iter_buf, resamp->iter_len);
            if (ret)
            {
                goto _err_exit;
            }
        }
    }

    while (len)
    {
        io.in = buf;
        io.in_size = len;
        io.out = resamp->iter_buf;
        io.out_size = resamp->iter_len;

        status = audio_resample_ex_run((RESAMPLE_ID *)resamp->id, &io, &in_size, &out_size);
        if (status != RESAMPLE_STATUS_OUT_FULL && status != RESAMPLE_STATUS_IN_EMPTY &&
            status != RESAMPLE_STATUS_DONE)
        {
            goto _err_exit;
        }

        ASSERT(in_size <= len, "%s: Bad resample in_size: in_size=%u len=%u", __func__, in_size, len);
        ASSERT(out_size <= resamp->iter_len, "%s: Bad resample out_size: out_size=%u iter_len=%u", __func__, out_size, resamp->iter_len);

        buf += in_size;
        len -= in_size;
        if (out_size == resamp->iter_len)
        {
            ret = resamp->cb(resamp->iter_buf, resamp->iter_len);
            if (ret)
            {
                goto _err_exit;
            }
        }
        else
        {
            resamp->offset = out_size;

            ASSERT(len == 0, "%s: Bad resample len: len=%u in_size=%u", __func__, len, in_size);
        }
    }

    return 0;

_err_exit:
    if (resamp)
    {
        app_resample_reset(resamp);
    }

    memzero_int16(buf, len);

    return 1;
}

void app_resample_reset(struct APP_RESAMPLE_T *resamp)
{
    audio_resample_ex_flush((RESAMPLE_ID *)resamp->id);
    resamp->offset = resamp->iter_len;
}

void app_resample_tune(struct APP_RESAMPLE_T *resamp, uint32_t rate, int32_t sample, uint32_t ms)
{
    float freq_offset;
    float new_step;

    if (resamp == NULL)
    {
        return;
    }

    freq_offset = (float)sample * 1000 / (rate * ms);
    ratio_step_factor *= 1 - freq_offset;

    LOG_PRINT_BT_STREAM("%s: ppb=%d", __FUNCTION__, (int)(freq_offset * 1000 * 1000 * 1000));
    if (resamp->stream == AUD_STREAM_PLAYBACK) {
        new_step = resamp->ratio_step * ratio_step_factor;
    } else {
        new_step = resamp->ratio_step / ratio_step_factor;
    }
    audio_resample_ex_set_ratio_step(resamp->id, new_step);
}

void app_resample_set_tune_factor(struct APP_RESAMPLE_T *resamp, float factor)
{
    float new_step;

    ratio_step_factor = factor;

    if (resamp == NULL)
    {
        return;
    }

    if (resamp->stream == AUD_STREAM_PLAYBACK) {
        new_step = resamp->ratio_step * ratio_step_factor;
    } else {
        new_step = resamp->ratio_step / ratio_step_factor;
    }

    audio_resample_ex_set_ratio_step(resamp->id, new_step);
}

float app_resample_get_tune_factor(void)
{
    return ratio_step_factor;
}

APP_RESAMPLE_BUF_ALLOC_CALLBACK app_resample_set_buf_alloc_callback(APP_RESAMPLE_BUF_ALLOC_CALLBACK cb)
{
    APP_RESAMPLE_BUF_ALLOC_CALLBACK old_cb;

    old_cb = resamp_buf_alloc;
    resamp_buf_alloc = cb;

    return old_cb;
}

