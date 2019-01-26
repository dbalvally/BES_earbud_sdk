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
#ifndef __APP_TWS_H__
#define __APP_TWS_H__

#include "cqueue.h"
#include "app_audio.h"
#include "cmsis_os.h"
#include "app_bt.h"
#include "a2dp_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_TWS_SET_CURRENT_OP(op) do { tws.current_operation = (op); TRACE("Set TWS current op to %d", op); } while (0);
#define APP_TWS_SET_NEXT_OP(op) do { tws.next_operation = (op); TRACE("Set TWS next op to %d", op); } while (0);

typedef enum {
	TWSFREE,
	TWSMASTER,
	TWSSLAVE,
} DeviceMode;

typedef enum {
	TWS_MASTER_CONN_SLAVE = 0,	
	TWS_MASTER_CONN_MOBILE_ONLY,	
	TWS_SLAVE_CONN_MASTER,		
	TWS_NON_CONNECTED		
} TWS_CONN_STATE_E;

typedef enum {
    TWS_PLAYER_START = 0,
    TWS_PLAYER_SETUP,
    TWS_PLAYER_STOP,
    TWS_PLAYER_PAUSE,
    TWS_PLAYER_RESTART,
    TWS_PLAYER_NULL,
}TWS_PLAYER_STATUS_T;

enum {
    APP_TWS_CLOSE_BLE_ADV,
    APP_TWS_OPEN_BLE_ADV,
};


enum {
    APP_TWS_START_STREAM,
    APP_TWS_SUSPEND_STREAM,
    APP_TWS_CLOSE_STREAM,
    APP_TWS_OPEN_STREAM,
    APP_TWS_IDLE_OP
};

enum {
    APP_TWS_CLOSE_BLE_SCAN,
    APP_TWS_OPEN_BLE_SCAN,
};

#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)
typedef enum {
    TWS_STREAM_CLOSE = 1 << 0,
    TWS_STREAM_OPEN = 1 << 1,
    TWS_STREAM_START = 1 << 2,
    TWS_STREAM_SUSPEND = 1 << 3,
}TwsStreamState;
#endif

typedef struct {
   uint32_t bt_time_l; //logical count of bt timer 
   uint32_t bt_time_first_l; //logical count of bt timer   
   uint32_t seq_wrap;
}trigger_t;

typedef struct  {
    osSemaphoreId _osSemaphoreId;
    osSemaphoreDef_t _osSemaphoreDef;
#ifdef CMSIS_OS_RTX
    uint32_t _semaphore_data[2];
#endif

} tws_lock_t;

typedef struct	{
    osMutexId _osMutexId;
    osMutexDef_t _osMutexDef;
#ifdef CMSIS_OS_RTX
    int32_t _mutex_data[3];
#endif

} tws_mutex_t;

typedef struct {
    osEvent oe;
    int code;
    int len;
    uint8_t * data;
}scan_msg_t;


typedef struct {
   uint32_t status;
   tws_lock_t _lock;
   uint32_t last_trigger_time;
   APP_AUDIO_CALLBACK_T callback;
   void (*wait)(void *player);
   void (*release)(void *player);
   void (*mutex_lock)(void);
   void (*mutex_unlock)(void);
}player_t;

typedef struct {
    CQueue  queue;
    tws_lock_t buff_lock;
    uint32_t int_lock;
    tws_lock_t wait;
    tws_lock_t wait_data_l;
    uint32_t sample_rate; 
    uint32_t channel;
    void (*wait_space)(void *tws_buff);
    void (*put_space)(void *tws_buff);
    void (*wait_data)(void *tws_buff);
    void (*put_data)(void *tws_buff);
    uint32_t (*lock)(void);
    void (*unlock)(uint32_t pri);
    int  (*write)(void *tws_buff, uint8_t *buff, uint16_t len);
    int  (*read)(void *tws_buff, uint8_t *buff, uint16_t len);
    int  (*len)(void *tws_buff);
    int  (*available)(void *tws_buff);    
    int32_t (*clean)(void *tws_buff);
    int32_t (*playtime)(void *tws_buff);
#ifdef __TWS_USE_PLL_SYNC__         
    uint32_t buffer_count;
#endif
}tws_buffer_t;  //pcm buffer for audout 

typedef struct{
    uint32_t channel;	
    uint32_t sample_rate;	
    uint32_t pcm_counter;	
    uint8_t * pcm_buffer;
    uint32_t size;
    uint32_t pcm_fsz;
    uint32_t pause_point;  // pcm frames from begin
    enum AUD_STREAM_ID_T stream_id;
    enum AUD_STREAM_T stream;
    int32_t (*playtime)(void *audout);
    void (*pcm_useout)(void);
}audout_t;


typedef struct {
    a2dp_stream_t *stream;
    volatile bool connected; //link with peer
    volatile bool counting;
    volatile bool locked; 
    volatile bool blocked;
//#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)
    uint16_t stream_state;
    uint32_t stream_need_reconfig;
//#endif    
    tws_lock_t   wait;
    tws_lock_t   sent_lock;
    tws_lock_t   stream_lock;
    volatile uint32_t inpackets;   //packets counter
    volatile uint32_t outpackets;   //packets counter
    uint32_t trans_speed;
    uint32_t trigger_time;
    uint32_t blocktime;
    
    void (*wait_data)(void *channel);
    void (*put_data)(void *channel);
    int32_t (*wait_sent)(void *channel,uint32_t timeout);
    void (*notify_send)(void *channel);	
    void (*lock_stream)(void *channel);
    void (*unlock_stream)(void *channel);	
}bt_channel_t;

typedef struct {
    trigger_t  dac_timer_trigger;
    TWS_CONN_STATE_E tws_conn_state;  
    volatile bool pcm_playing; 
    osThreadId decoder_tid;
    volatile bool  decoder_running;
    volatile bool  decoder_running_wait;

    
    tws_mutex_t decoder_mutex;
    tws_mutex_t decoder_buffer_mutex;
    void (*lock_decoder_thread)(void *tws);
    void (*unlock_decoder_thread)(void *tws);
    osThreadId trans_tid;
    volatile bool  trans_running;
    tws_mutex_t trans_mutex;
    void (*lock_trans_thread)(void *tws);
    void (*unlock_trans_thread)(void *tws); 	
    tws_buffer_t pcmbuff;
    tws_buffer_t sbcbuff;
    tws_buffer_t a2dpbuff;
#if defined(SLAVE_USE_ENC_SINGLE_SBC) && defined(SLAVE_USE_ENC_SINGLE_SBC_ENQUEUE_ENCODE)
    tws_buffer_t encode_queue;
#endif
    audout_t audout;
    btif_bt_packet_t packet;
    bt_channel_t tws_sink;
    bt_channel_t tws_source;
    bt_channel_t mobile_sink;
    btif_avdtp_codec_type_t mobile_codectype;	
	uint8_t mobile_samplebit;
	uint32_t mobile_samplerate;
	//spp_dev_t *tws_spp_dev;
    uint8_t tws_set_codectype_cmp;
    uint8_t tws_spp_wait_event;
    uint8_t tws_spp_connect;
    osThreadId tws_spp_tid;
    osThreadId tws_ctrl_tid;
    osMailQId tws_mailbox;

    btif_avrcp_chnl_handle_t rcp; //for play,stop,volume up/down
    bt_bdaddr_t peer_addr;
    bt_bdaddr_t connecting_addr;
    bt_bdaddr_t mobile_addr;
    tws_lock_t   query_lock;
    tws_lock_t   decode_lock;
    tws_lock_t   mode_lock;
    uint32_t     prop_lock;
    int32_t      btclkoffset;
    volatile bool has_tws_peer;
    volatile bool isplaying;
    volatile bool decoder_stop;
    volatile bool paused;
    volatile bool tran_blocked;
    volatile bool pausepending;   
    volatile bool wait_pcm_signal;
    volatile bool decode_discard;
    player_t player;
    void (*lock_decoder)(void *tws); 
    void (*unlock_decoder)(void *tws); 
    void (*lock_mode)(void *tws);
    void (*unlock_mode)(void *tws);
    bool  media_suspend;
    bool create_conn_pending;
    bool suspend_media_pending;
    bool start_media_waiting;
    bool start_media_pending;

    uint8_t current_operation;
    uint8_t next_operation;

    
    void (*wait)(void *tws);
    void (*notify)(void *tws);
    void (*lock_prop)(void *tws); 
    void (*unlock_prop)(void *tws); 
    uint8_t decode_channel;
    uint8_t tran_channel;
    uint16_t tws_conhdl;
    uint16_t mobile_conhdl;

    uint8_t stream_reset;
    uint8_t stream_idle;
    uint32_t pause_packets;   //packets counter
    int8_t hfp_volume;
    int8_t tws_player_restart;
    bool slave_player_restart_pending;
#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)
    tws_lock_t wait_stream_reconfig;
    tws_lock_t local_player_wait_restart;
    tws_lock_t wait_stream_started;
    uint8_t local_player_wait_reconfig;
    uint8_t local_player_need_pause;
    uint8_t local_player_sig_flag;
    uint8_t local_player_status;
    uint8_t local_player_stop;
    uint8_t local_player_tran_2_slave_flag;
#endif 
	uint8_t			isWaitingCmdRsp;
	uint8_t			isOnGoingPlayInterruptedByRoleSwitch;
	uint32_t 		currentMs;
	void (*tws_player_stopped_func)(void);
	uint8_t			tws_start_stream_pending;
	uint8_t			bleReconnectPending;
#ifdef __NOT_STOP_AUDIO_IN_UNDERFLOW__
    uint32_t slave_skip_sample_num;
#endif

#ifdef __TWS_USE_PLL_SYNC__
    int8_t   audio_pll_shift;
    int8_t   audio_pll_shift_ok;
    uint8_t audio_pll_syn_enable;
    uint8_t audio_pll_sync_trigger_start;
    uint8_t audio_pll_syn_send_index;
    uint8_t audio_pll_sync_trigger_success;
    //audio_pll_cfg:bit7 enable, bit6:bit4 pll set, bit3:signal bit2:bit:0 adjust direction
    uint8_t audio_pll_cfg;
    uint8_t audio_dma_trigger_start_counter;
    uint8_t audio_dma_trigger_num;
    uint8_t audio_dma_count;
    uint8_t audio_adjust_direction;
    uint8_t audio_pll_monitor_enable;
    uint8_t audio_pll_monitor_cycle_number;
    uint8_t audio_pll_monitor_first_cycle;
    uint8_t audio_pll_pcm_monitor_index;    
    uint32_t audio_pll_pcm_monitor_counter;
    uint32_t audio_pll_first_pcm_mointor_length;
    uint32_t audio_pll_a2dp_buffer_monitor_counter;
    uint32_t audio_pll_first_a2dp_buffer_mointor_length;
#endif
#if defined(__TWS_USE_PLL_SYNC__) || defined(__TWS_SYNC_WITH_DMA_COUNTER__)
    //audio dma irq index
    uint32_t audio_dma_index;
#endif
#ifdef __TWS_SYNC_WITH_DMA_COUNTER__
    uint32_t dma_pcm_samples;
    //remember the last frame counter play ok
    uint32_t audio_sync_last_frame_counter;
    //remember audio drop irq start index
    uint32_t audio_drop_dma_irq_index;
    uint32_t audio_drop_last_dma_irq_index;
    //record the frame number received last time
    uint32_t audio_frame_number;
    uint32_t audio_insert_a2dp_num;
    uint32_t audio_insert_pcm_num;
    uint32_t audio_master_frame_index;
    //as decode, put pcm is not mutext op. should remember the length 
    //for audio drop resync us dma
    uint32_t tws_decoding_pcm_length;
    uint16_t media_trans_frame_index;
    uint16_t master_insert_pcm_length;
    uint16_t master_raw_data_length;
    uint8_t   audio_master_resend_enable;
    uint8_t   master_stop_store_trans_pcm;  
    uint8_t   audio_drop_resync;
    //after data send this flag will be re-set
    uint8_t   tws_trans_timeout;
    uint8_t   dma_pcm_width;
    uint8_t   dma_pcm_chan;
#endif

}tws_dev_t;

typedef struct _app_tws_slave_SbcStreamInfo {
    U8   bitPool;
    uint8_t allocMethod;
}tws_slave_SbcStreamInfo_t;

typedef bt_status_t (*BLE_ADV_SCAN_FUNC)(uint8_t);



typedef struct  {
    uint8_t *buff;
    uint32_t size;
}tws_env_buff_t;

typedef enum
{
	TWS_EARPHONE_LEFT_SIDE = 0,
	TWS_EARPHONE_RIGHT_SIDE
} TWS_EARPHONE_SIDE_E;

typedef struct  {
    tws_env_buff_t env_pcm_buff;
    tws_env_buff_t env_sbc_buff;
    tws_env_buff_t env_a2dp_buff;
    tws_env_buff_t env_encode_queue_buff;
    tws_env_buff_t env_sbsample_buff;
    
    uint32_t env_trigger_delay;
    uint32_t env_trigger_delay_lhdc;
    uint32_t env_trigger_a2dp_buff_thr;
    uint32_t env_trigger_aac_buff_thr;    
    uint32_t env_trigger_lhdc_buff_thr;    
    uint32_t env_slave_trigger_thr;
	TWS_EARPHONE_SIDE_E  earside;
	void (*tws_player_stopped_func)(void);
}tws_env_cfg_t;

typedef struct {
	bt_bdaddr_t peer_addr;
    DeviceMode mode;
}tws_cfg_t;

typedef struct trans_conf_{
uint32_t transesz;
uint32_t framesz;
uint32_t discardframes;
char *opus_temp_buff;
}trans_conf_t;


typedef struct sbcpack{
    btif_a2dp_sbc_packet_t sbcPacket;
    char *buffer;
    int free;
}sbcpack_t;

#define SBCBANK_PACK_NUM (4)

typedef struct sbcbank{
    sbcpack_t sbcpacks[SBCBANK_PACK_NUM];
    int free;
}sbcbank_t;

typedef struct {
    uint8_t codec_type;
    uint8_t sample_bit;
    uint32_t sample_rate;
} __attribute__((packed)) APP_TWS_SET_TWS_CODEC_TYPE_T;

typedef struct {
    int8_t vol;
}__attribute__((packed)) APP_TWS_SET_TWS_HFP_VOLUME_T;

typedef struct {
    int8_t restart;
}__attribute__((packed)) APP_TWS_SET_TWS_PLATER_RESTART_T;

typedef struct {
    int32_t key;
}__attribute__((packed)) APP_TWS_SET_TWS_NOTIFY_KEY_T;

#ifdef LBRT
#define TWS_PLAYER_TRIGGER_DELAY        (100000)
#else
#define TWS_PLAYER_TRIGGER_DELAY        (170000)	//(250000)   //us
#endif
#define TWS_PLAYER_TRIGGER_DELAY_LHDC   (250000)	//(250000)   //us

///change back to 100clk  because vcm on ,then there is no delay in codec open
#define TWS_PLAYER__SLAVE_TRIGGER_THR   (100)   //CLK


#ifdef __TWS_CALL_DUAL_CHANNEL__

#define NONE_ROLE       0

#define SNIFFER_ROLE    1
#define FORWARD_ROLE   2
#endif

#ifdef FPGA
/* pcm buffer size */
#define PCM_BUFFER_SZ_TWS        (48*1024)  // 500ms 
/* media buffer size */
#define A2DP_BUFFER_SZ_TWS       (PCM_BUFFER_SZ_TWS/6)

#if defined(MASTER_USE_OPUS) || defined(SLAVE_USE_OPUS)  || defined(ALL_USE_OPUS)
#define TRANS_BUFFER_SZ_TWS      (A2DP_BUFFER_SZ_TWS*30)
#else
#define TRANS_BUFFER_SZ_TWS      (A2DP_BUFFER_SZ_TWS*15)
#endif

#else

#if defined(SLAVE_USE_ENC_SINGLE_SBC) 
/* pcm buffer size */
#if defined(SLAVE_USE_ENC_SINGLE_SBC_ENQUEUE_ENCODE)
#define PCM_BUFFER_SZ_TWS        (12*2*1024)  //1s
#else
#if defined(__SBC_FUNC_IN_ROM_VBEST2000__) && !defined(__SBC_FUNC_IN_ROM_VBEST2000_ONLYSBC__)
#define PCM_BUFFER_SZ_TWS        (15*2*1024)  //1s
#else
#ifdef __TWS_1CHANNEL_PCM__
#ifdef CHIP_BEST2300
//#define PCM_BUFFER_SZ_TWS        (24*2*1024)  //1s
///make sure that VCM_ON is defined in analog_bes2300.c
#define PCM_BUFFER_SZ_TWS        (11*2*1024)  //1s
#else
#define PCM_BUFFER_SZ_TWS        (7*2*1024) //(10*2*1024)  //1s
#endif
#else
#define PCM_BUFFER_SZ_TWS        (22*2*1024)  //1s
#endif
#endif
#endif

#else
/* pcm buffer size */
#define PCM_BUFFER_SZ_TWS        (12*2*1024)  //1s 
#endif

#ifdef __TWS_USE_PLL_SYNC__
#ifdef __TWS_1CHANNEL_PCM__
#define A2DP_BUFFER_SZ_TWS       (PCM_BUFFER_SZ_TWS/2)
#else
/* media buffer size */
#define A2DP_BUFFER_SZ_TWS       (PCM_BUFFER_SZ_TWS/4)
#endif
#else
#ifdef __TWS_1CHANNEL_PCM__
#define A2DP_BUFFER_SZ_TWS       (PCM_BUFFER_SZ_TWS/3)
#else
/* media buffer size */
#define A2DP_BUFFER_SZ_TWS       (PCM_BUFFER_SZ_TWS/6)
#endif
#endif

#if defined(MASTER_USE_OPUS) || defined(ALL_USE_OPUS)
#define TRANS_BUFFER_SZ_TWS      (A2DP_BUFFER_SZ_TWS*30)
#elif defined(SLAVE_USE_OPUS)
#define TRANS_BUFFER_SZ_TWS      (A2DP_BUFFER_SZ_TWS)
#elif defined(SLAVE_USE_ENC_SINGLE_SBC) 
#if defined(SLAVE_USE_ENC_SINGLE_SBC_ENQUEUE_ENCODE)
#define TRANS_BUFFER_SZ_TWS      (PCM_BUFFER_SZ_TWS)
#else
#ifdef __TWS_1CHANNEL_PCM__
#if defined(SBC_TRANS_TO_AAC)
#define TRANS_BUFFER_SZ_TWS      (PCM_BUFFER_SZ_TWS)
#else
#define TRANS_BUFFER_SZ_TWS      (PCM_BUFFER_SZ_TWS*2)
#endif
#else
#define TRANS_BUFFER_SZ_TWS      (PCM_BUFFER_SZ_TWS)
#endif
#endif
#else
#define TRANS_BUFFER_SZ_TWS      (PCM_BUFFER_SZ_TWS)
#endif

#endif

#define TWS_BUFFER_TOTAL_SIZE (PCM_BUFFER_SZ_TWS+TRANS_BUFFER_SZ_TWS+A2DP_BUFFER_SZ_TWS)

#if defined(SBC_TRANS_TO_AAC)|| defined(MASTER_USE_OPUS) || defined(SLAVE_USE_OPUS) || defined(SLAVE_USE_ENC_SINGLE_SBC) || defined(ALL_USE_OPUS)

#define MIX_2_CHANNELS_WHEN_USED_AS_SINGLE 1
#define OPUS_64K_ENC_320BYTES

#ifdef __3M_PACK__
//#define __3DH5__
#define __3DH3__
#endif

#if defined(SBC_TRANS_TO_AAC)
#define SBCTRANSMITSZ             2048
#define SBCTRANFRAMESZ           2048

#define TRANSDISCARDSFRAMES      (8*SBCTRANSMITSZ/SBCTRANFRAMESZ) //80 fs,200ms  

#elif defined(SLAVE_USE_ENC_SINGLE_SBC)
/* transmit size each time */
//#define SBCTRANSMITSZ            (4608)
//#define SBCTRANSMITSZ            (3072)
#ifdef __3DH3__
#define SBCTRANSMITSZ            (512*7)
#elif defined(__3DH5__)
#define SBCTRANSMITSZ            (512*12)
#else
//nine packet with single chanle bitpool 31 is 647 packet length
#define SBCTRANSMITSZ            (512*9)
#endif

/* transmit frame size */
#define SBCTRANFRAMESZ           (512)

#define TRANSDISCARDSFRAMES      (8*SBCTRANSMITSZ/SBCTRANFRAMESZ) //80 fs,200ms  
//#define TRANSDISCARDSFRAMES      (15*SBCTRANSMITSZ/SBCTRANFRAMESZ) //80 fs,200ms  

#else

#define TRANSDISCARDSFRAMES      (10*SBCTRANSMITSZ/SBCTRANFRAMESZ) //80 fs,200ms  

/* transmit size each time */
#if defined(ALL_USE_OPUS)
#define SBCTRANSMITSZ            (OPUS_FRAME_LEN)
#else
#ifdef SLAVE_OPUS_OPTION_SINGLE_CHANNEL
#ifdef OPUS_64K_ENC_320BYTES
#define SBCTRANSMITSZ            (960*2*2)
#else
#define SBCTRANSMITSZ            (960*2)
#endif
#else
#ifdef OPUS_64K_ENC_320BYTES
#define SBCTRANSMITSZ            (960*2*2*2)
#else
#define SBCTRANSMITSZ            (960*2*2)
#endif
#endif
#endif

/* transmit frame size */
#ifdef OPUS_64K_ENC_320BYTES
#define SBCTRANFRAMESZ           (SBCTRANSMITSZ/2)
#else
#define SBCTRANFRAMESZ           (SBCTRANSMITSZ)
#endif

#endif

#if defined(SBC_TRANS_TO_AAC) && (SBCTRANSMITSZ < 2048)
#error "SBCTRANSMITSZ must >= 2048 when SBC_TRANS_TO_AAC"
#endif

/* pcm length to decode (for opus to encode) */
#if defined(SLAVE_USE_OPUS)
#ifdef SLAVE_OPUS_OPTION_SINGLE_CHANNEL
#define SLAVE_DECODE_PCM_FRAME_LENGTH  (960*2)
#else
#define SLAVE_DECODE_PCM_FRAME_LENGTH  (960*2*2)
#endif
#elif defined(SLAVE_USE_ENC_SINGLE_SBC)
#define SLAVE_DECODE_PCM_FRAME_LENGTH  (1024)
#elif defined(MASTER_USE_OPUS)
#define SLAVE_DECODE_PCM_FRAME_LENGTH  (960*2*2)
#elif defined(ALL_USE_OPUS)
#define SLAVE_DECODE_PCM_FRAME_LENGTH  (960*2*2)
#endif

#if defined(MASTER_USE_OPUS)
#define MASTER_DECODE_PCM_FRAME_LENGTH  (960*2*2)
#elif defined(ALL_USE_OPUS)
#define MASTER_DECODE_PCM_FRAME_LENGTH  (960*2*2)
#else

#if defined(SLAVE_USE_ENC_SINGLE_SBC_ENQUEUE_ENCODE)
#define MASTER_DECODE_PCM_FRAME_LENGTH  (1024)
#else
#define MASTER_DECODE_PCM_FRAME_LENGTH  1024
#endif

#endif

// choose the bigger one
#define DECODE_PCM_FRAME_LENGTH  (MASTER_DECODE_PCM_FRAME_LENGTH>SLAVE_DECODE_PCM_FRAME_LENGTH?MASTER_DECODE_PCM_FRAME_LENGTH:SLAVE_DECODE_PCM_FRAME_LENGTH)

#else
#define DECODE_PCM_FRAME_LENGTH  1024
#define SBCTRANSMITSZ             1024
#define SBCTRANFRAMESZ           512

#define TRANSDISCARDSFRAMES      (20*SBCTRANSMITSZ/SBCTRANFRAMESZ) //80 fs,200ms  

#endif


typedef enum {
#if defined(A2DP_LHDC_ON)
	TWS_CTRL_PLAYER_SETSAMPLE_RATE,
	TWS_CTRL_PLAYER_SETBIT_NUM,
#endif
    TWS_CTRL_PLAYER_SETCODEC,
    TWS_CTRL_PLAYER_SPPCONNECT,
    TWS_CTRL_PLAYER_SET_HFP_VOLUME,
    TWS_CTRL_PLAYER_RESTART_PLAYER,
    TWS_CTRL_SLAVE_KEY_NOTIFY,
    TWS_CTRL_LBRT_PING_REQ,
    TWS_CTRL_ANC_SET_STATUS,
    /*
    TWS_CTRL_ANC_GET_STATUS,
    TWS_CTRL_ANC_NOTIFY_STATUS,
    */
    TWS_CTRL_RING_SYNC,
	#ifdef __TWS_ROLE_SWITCH__
     TWS_CTRL_SEND_PROFILE_DATA,
    #endif
    TWS_CTRL_SHARE_TWS_ESCO_RETX_NB,
}TWS_CTRL_EVENT;

typedef struct {
    uint32_t evt;
    uint32_t arg;
    uint32_t arg1;
} TWS_MSG_BLOCK;


typedef struct{
    U8   bitPool;
    U8   numBlocks;
    U8   numSubBands;
    U8   frameNum;
    U32 frameCount;
    U32 frameLen;
    U32 sampleFreq;
}TWS_SBC_FRAME_INFO;

extern tws_dev_t  tws;
extern tws_cfg_t tws_cfg;
tws_env_cfg_t  *app_tws_get_cfg(void);
int tws_a2dp_callback(a2dp_stream_t *Stream, const btif_a2dp_callback_parms_t *Info);
int tws_store_stream(char *buf, int len);
int32_t init_tws(tws_env_cfg_t *cfg);
int32_t app_tws_active(void);
int32_t app_tws_deactive(void);
int tws_store_sbc_buffer(unsigned char *buf, unsigned int len,unsigned int sbc_frame_num);
void tws_trans_sbc(void);
void  notify_tws_btenv_ready(void);
void tws_set_channel_cfg(btif_avdtp_codec_t * mcfg);
void tws_player_stop_callback(uint32_t status, uint32_t param);
void tws_player_pause_callback(uint32_t status, uint32_t param);
void tws_player_start_callback(uint32_t status, uint32_t param);
void tws_player_restart_callback(uint32_t status, uint32_t param);
int tws_audio_sendrequest(uint8_t stream_type,  TWS_PLAYER_STATUS_T oprate, 
                            uint32_t param,     
                            APP_AUDIO_CALLBACK_T callback);

void app_tws_notipy(void);
uint8_t app_tws_get_mode(void);
bool app_tws_is_master_mode(void);
bool app_tws_is_slave_mode(void);
bool app_tws_is_freeman_mode(void);
uint8_t *app_tws_get_peer_bdaddr(void);
void app_tws_set_have_peer(bool is_have);
void app_tws_set_conn_state(TWS_CONN_STATE_E state);

void app_tws_set_media_suspend(bool enable);

uint8_t is_find_tws_peer_device_onprocess(void);

void find_tws_peer_device_start(void);

void find_tws_peer_device_stop(void);

void set_a2dp_reconfig_codec(a2dp_stream_t *Stream);
btif_avrcp_chnl_handle_t app_tws_get_avrcp_TGchannel(void);
a2dp_stream_t *app_tws_get_tws_source_stream(void);
bool app_tws_get_source_stream_connect_status(void);
btif_avdtp_codec_t* app_tws_get_set_config_codec_info(void);
bool app_tws_reconnect_slave_process(uint8_t a2dp_event, uint8_t avctp_event);
void app_tws_disconnect_slave(void);
void app_tws_disconnect_master(void);
int app_tws_nvrecord_rebuild(void);
void app_tws_creat_tws_link(void);
uint16_t app_tws_get_tws_conhdl(void);
void  app_tws_set_tws_conhdl(uint16_t conhdl);
void app_tws_suspend_tws_stream(void);
bool app_tws_close_tws_stream(void);
void a2dp_source_reconfig_sink_codec_reset(void);
void a2dp_source_reconfig_sink_codec_request(a2dp_stream_t *Stream);
int32_t restore_tws_channels(void);
int32_t restore_mobile_channels(void);
bool app_tws_media_frame_analysis(void *tws, a2dp_stream_t *Stream,const  btif_a2dp_callback_parms_t *info);
void tws_player_stop(uint8_t stream_type);
int app_tws_reset_tws_data(void);
bool app_tws_analysis_media_data_from_mobile(void *tws, a2dp_stream_t *stream, btif_a2dp_callback_parms_t *Info);
void app_tws_start_master_player(a2dp_stream_t *Stream);
a2dp_stream_t* app_tws_get_sink_stream(void);

void a2dp_callback_tws_source(a2dp_stream_t *stream, btif_a2dp_callback_parms_t *Info);

#ifdef __TWS_RECONNECT_USE_BLE__
extern app_tws_ble_reconnect_t app_tws_ble_reconnect;
void app_tws_ble_reconnect_init(void);
#endif
int tws_player_set_codec( btif_avdtp_codec_type_t codec_type);
int tws_player_set_hfp_vol( int8_t vol);
int tws_player_pause_player_req( int8_t req);
int tws_player_set_anc_status(uint32_t status);
/*
int tws_player_notify_anc_status(uint32_t status);
int tws_player_get_anc_status(uint32_t status);
*/
int tws_lbrt_ping_req(uint32_t current_ticks);

int tws_ring_sync(U16 hf_event);
uint32_t tws_media_play_calc_ring_sync_trigger_time(void);


#if defined(A2DP_AAC_ON)
uint8_t app_tws_get_codec_type(void);
#endif
void tws_set_mobile_sink_stream(a2dp_stream_t *stream);

void app_tws_check_pcm_signal(void);


int audout_pcmbuff_thres_monitor(void);

#if  defined(TWS_RBCODEC_PLAYER) || defined (TWS_LINEIN_PLAYER)
void  rb_tws_start_master_player(uint8_t stream_type);
void rb_tws_restart();
uint32_t rb_push_pcm_in_tws_buffer(uint8_t *buff, uint32_t len);
uint32_t linein_push_pcm_in_tws_buffer(uint8_t *buff, uint32_t len);
void rb_set_sbc_encoder_freq_ch(int32_t freq, uint8_t ch);
void rb_start_sink_stream();
void rb_clean_stream_buffer();
void tws_stop_local_player();
void rb_sink_stream_stared();
void tws_local_player_set_tran_2_slave_flag(uint8_t onoff);
uint8_t tws_local_player_need_tran_2_slave();
uint32_t rb_tws_get_trigger_time(void);
uint16_t app_tws_get_master_stream_state(void);
#endif

void app_tws_force_change_role(void);
bool app_tws_store_info_to_nv(DeviceMode mode, uint8_t* peer_addr);

bool a2dp_sink_of_mobile_dev_callback(a2dp_stream_t *Stream, const a2dp_callback_parms_t *Info);
void a2dp_sink_of_mobile_dev_closed_ind_handler(a2dp_stream_t *Stream, const a2dp_callback_parms_t *Info);
void a2dp_sink_of_mobile_dev_opened_ind_handler(a2dp_stream_t *Stream, const a2dp_callback_parms_t *Info);
bool a2dp_sink_of_tws_callback(a2dp_stream_t *Stream, const a2dp_callback_parms_t *Info);
void a2dp_sink_of_tws_closed_ind_handler(a2dp_stream_t *Stream, const a2dp_callback_parms_t *Info);
void a2dp_sink_of_tws_opened_ind_handler(a2dp_stream_t *Stream, const a2dp_callback_parms_t *Info);
void a2dp_source_of_tws_callback(a2dp_stream_t *Stream, const a2dp_callback_parms_t *Info);
void a2dp_source_of_tws_closed_ind_handler(a2dp_stream_t *Stream, const a2dp_callback_parms_t *Info);
void a2dp_source_of_tws_opened_ind_handler(a2dp_stream_t *Stream, const a2dp_callback_parms_t *Info);
void a2dp_sink_common_data_ind_handler(a2dp_stream_t *Stream, const a2dp_callback_parms_t *Info);
void a2dp_sink_callback(a2dp_stream_t *Stream, const a2dp_callback_parms_t *Info);
void a2dp_sink_callback_common_handler(a2dp_stream_t *Stream, const a2dp_callback_parms_t *Info);
bool a2dp_is_music_ongoing(void);
void tws_reset_saved_codectype(void);
void tws_set_saved_codectype(uint8_t codec_type, uint8_t sample_bit, uint32_t sample_rate);
void tws_ctrl_thread_init(void);
#ifdef __TWS_ROLE_SWITCH__
int tws_send_profile_data( uint32_t data_structure);
#endif
int tws_send_esco_retx_nb( uint32_t retx_nb);
void tws_pause_play(void);
#ifdef __NOT_STOP_AUDIO_IN_UNDERFLOW__
void tws_audout_underflow_process(uint32_t len);
void get_stream_sbcqueue_num(a2dp_stream_t *stream);
#endif
#ifdef __TWS_USE_PLL_SYNC__   
void a2dp_reset_all_sync_pll_para(void);
int a2dp_audio_sync_proc(uint8_t status, int shift);
#endif
#ifdef __TWS_SYNC_WITH_DMA_COUNTER__
void a2dp_audio_drop_resync_init(void);
void audio_drop_sync_master();
uint8_t a2dp_audio_drop_check_timeout(void);
void a2dp_audio_drop_set_timeout(uint8_t enable);
uint8_t tws_check_trans_delay(void);
#endif
#ifdef __cplusplus
}
#endif
#endif /**/
