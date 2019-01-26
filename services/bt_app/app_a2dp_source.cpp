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
#include "cmsis_os.h"
#include "hal_uart.h"
#include "hal_timer.h"
#include "audioflinger.h"
#include "lockcqueue.h"
#include "hal_trace.h"
#include "hal_cmu.h"
#include "analog.h"
#include "bt_drv.h"
#include "app_audio.h"
#include "bt_drv_interface.h"
#include "app_bt_stream.h"
#include "nvrecord.h"
#include "nvrecord_env.h"
#include "a2dp_api.h"
#include "rtos.h"
#include "besbt.h"

#include "cqueue.h"
#include "btapp.h"
#include "apps.h"
#include "resources.h"
#include "app_bt_media_manager.h"
#include "tgt_hardware.h"
#include "app_utils.h"


#ifdef __APP_A2DP_SOURCE__

#define APP_SOURCE_DEBUG
#ifdef APP_SOURCE_DEBUG
#define SOURCE_DBLOG TRACE
#else
#define SOURCE_DBLOG(...)
#endif


extern struct BT_DEVICE_T  app_bt_device;
static btif_avdtp_codec_t sink_config_codec;

#define A2DP_LINEIN_SIZE    (48*2*1024)
#define A2DP_TRANS_SIZE     2048
//#define A2DP_SBCBUFF_SIZE   (A2DP_LINEIN_SIZE/2)
 uint8_t a2dp_linein_buff[A2DP_LINEIN_SIZE];
//uint8_t a2dp_sbcbuff[A2DP_SBCBUFF_SIZE];
static  char a2dp_transmit_buffer[A2DP_TRANS_SIZE];

typedef struct _SCB {
  U8     cb_type;                 /* Control Block Type                      */
  U8     mask;                    /* Semaphore token mask                    */
  U16    tokens;                  /* Semaphore tokens                        */
  struct OS_TCB *p_lnk;           /* Chain of tasks waiting for tokens       */
} *PSCB;


typedef struct  {
    osSemaphoreId _osSemaphoreId;
    osSemaphoreDef_t _osSemaphoreDef;
#ifdef CMSIS_OS_RTX
    uint32_t _semaphore_data[2];
#endif

} a2dp_source_lock_t;



typedef struct{
    CQueue  pcm_queue;
    osThreadId sbc_send_id;
    a2dp_source_lock_t  data_lock;
    a2dp_source_lock_t  sbc_send_lock;
    enum AUD_SAMPRATE_T sample_rate;
    uint8_t sbcen_samplerate;


}A2DP_SOURCE_STRUCT;


typedef struct sbcpack{
    btif_a2dp_sbc_packet_t sbcPacket;
    char buffer[A2DP_TRANS_SIZE];
    int free;
}sbcpack_t;

typedef struct sbcbank{
    sbcpack_t sbcpacks[1];
    int free;
}sbcbank_t;

__SRAMDATA sbcbank_t  sbcbank;

static  sbcpack_t *get_sbcPacket(void)
{
   int index = sbcbank.free;
   sbcbank.free +=1;
   if(sbcbank.free == 1) {
      sbcbank.free = 0;
   }
   return &(sbcbank.sbcpacks[index]);
}


A2DP_SOURCE_STRUCT  a2dp_source;


osMutexId a2dp_source_mutex_id = NULL;
osMutexDef(a2dp_source_mutex);

static void a2dp_source_mutex_lock(void)
{
    osMutexWait(a2dp_source_mutex_id, osWaitForever);
}

static void a2dp_source_mutex_unlock(void)
{
    osMutexRelease(a2dp_source_mutex_id);
}

static void a2dp_source_sem_lock(a2dp_source_lock_t * lock)
{
    osSemaphoreWait(lock->_osSemaphoreId, osWaitForever);
}

static void a2dp_source_sem_unlock(a2dp_source_lock_t * lock)
{

    osSemaphoreRelease(lock->_osSemaphoreId);
}



static void a2dp_source_reset_send_lock(void)
{
    PSCB p_scb =(PSCB)(a2dp_source.sbc_send_lock._osSemaphoreDef.semaphore);
    uint32_t lock = int_lock();
    p_scb->tokens = 0;
    int_unlock(lock);
}


static bool a2dp_source_is_send_wait(void)
{
    bool ret = false;
    uint32_t lock = int_lock();
    PSCB p_scb =(PSCB)(a2dp_source.sbc_send_lock._osSemaphoreDef.semaphore);
    if(p_scb->p_lnk){
        ret = true;
    }
    int_unlock(lock);
    return ret;
}


static void a2dp_source_wait_pcm_data(void)
{

    a2dp_source_lock_t *lock = &(a2dp_source.data_lock);
    PSCB p_scb =(PSCB)(lock->_osSemaphoreDef.semaphore);
    uint32_t iflag = int_lock();
    p_scb->tokens = 0;
    int_unlock(iflag);
    a2dp_source_sem_lock(lock);
}

static void a2dp_source_put_data(void)
{
    a2dp_source_lock_t *lock = &(a2dp_source.data_lock);
    a2dp_source_sem_unlock(lock);
}

#if 0
static void a2dp_source_lock_sbcsending(void * channel)
{
    tws_lock_t *lock = &(a2dp_source.sbc_send_lock);
    sem_lock(lock);
}
static void a2dp_source_unlock_sbcsending(void * channel)
{
    tws_lock_t *lock = &(a2dp_source.sbc_send_lock);
    sem_unlock(lock);
}
#endif

static int32_t a2dp_source_wait_sent(uint32_t timeout)
{
    int32_t ret = 0;
    a2dp_source_lock_t *lock = &(a2dp_source.sbc_send_lock);
    a2dp_source_reset_send_lock();
    ret = osSemaphoreWait(lock->_osSemaphoreId, timeout);
    return ret;

}

static void a2dp_source_notify_send(void)
{
    if (a2dp_source_is_send_wait()){ //task wait lock
     //   TWS_DBLOG("\nNOTIFY SEND\n");
        a2dp_source_sem_unlock(&(a2dp_source.sbc_send_lock));
    }

}



static int a2dp_source_pcm_buffer_write(uint8_t * pcm_buf, uint16_t len)
{
    int status;
    //TWS_DBLOG("\nenter: %s %d\n",__FUNCTION__,__LINE__);
    a2dp_source_mutex_lock();
    status = EnCQueue(&(a2dp_source.pcm_queue), pcm_buf, len);
    a2dp_source_mutex_unlock();
    //TWS_DBLOG("\nexit: %s %d\n",__FUNCTION__,__LINE__);
    return status;
}


static int a2dp_source_pcm_buffer_read(uint8_t *buff, uint16_t len)
{
    uint8_t *e1 = NULL, *e2 = NULL;
    unsigned int len1 = 0, len2 = 0;
    int status;
    a2dp_source_mutex_lock();
    status = PeekCQueue(&(a2dp_source.pcm_queue), len, &e1, &len1, &e2, &len2);
    if (len==(len1+len2)){
        memcpy(buff,e1,len1);
        memcpy(buff+len1,e2,len2);
        DeCQueue(&(a2dp_source.pcm_queue), 0, len);
    }else{
        memset(buff, 0x00, len);
        status = -1;
    }
    a2dp_source_mutex_unlock();
    return status;
}

uint32_t a2dp_source_linein_more_pcm_data(uint8_t * pcm_buf, uint32_t len)
{
    int status;
    status = a2dp_source_pcm_buffer_write(pcm_buf,len);
    if(status !=CQ_OK)
    {
        SOURCE_DBLOG("linin buff overflow!");
    }
    a2dp_source_put_data();
    return len;
}




void a2dp_callback_source(a2dp_stream_t *Stream, const a2dp_callback_parms_t *info)
{
    btif_a2dp_callback_parms_t * Info = (btif_a2dp_callback_parms_t*)info;
    switch(Info->event) {
        case BTIF_A2DP_EVENT_AVDTP_CONNECT:
            SOURCE_DBLOG("::A2DP_EVENT_AVDTP_CONNECT %d\n", Info->event);
            break;
        case BTIF_A2DP_EVENT_STREAM_OPEN:
            SOURCE_DBLOG("::A2DP_EVENT_STREAM_OPEN stream_id:%d, sample_rate codec.elements 0x%x\n", BT_DEVICE_ID_1,Info->p.configReq->codec.elements[0]);
            if(Info->p.configReq->codec.codecType == BTIF_AVDTP_CODEC_TYPE_SBC)
            {
                memcpy(app_bt_device.a2dp_codec_elements[BT_DEVICE_ID_1],Info->p.configReq->codec.elements,4);
            }
            if(Info->p.configReq->codec.elements[1] & A2D_SBC_IE_SUBBAND_MSK == A2D_SBC_IE_SUBBAND_4)
                TRACE("numSubBands is only support 8!");
            a2dp_source.sample_rate =  bt_parse_sbc_sample_rate(btif_a2dp_get_stream_codecCfg(Stream)->elements[0]);
            app_bt_device.sample_rate[BT_DEVICE_ID_1] = (Info->p.configReq->codec.elements[0] & A2D_SBC_IE_SAMP_FREQ_MSK);
            app_bt_device.a2dp_state[BT_DEVICE_ID_1] = 1;

       //     AVRCP_Connect(&app_bt_device.avrcp_channel[BT_DEVICE_ID_1], &Stream->stream.conn.remDev->bdAddr);
            break;
        case BTIF_A2DP_EVENT_STREAM_OPEN_IND:
            SOURCE_DBLOG("::A2DP_EVENT_STREAM_OPEN_IND %d\n", Info->event);
            btif_a2dp_open_stream_rsp(Stream, BTIF_A2DP_ERR_NO_ERROR, BTIF_AVDTP_SRV_CAT_MEDIA_TRANSPORT);
            break;
        case BTIF_A2DP_EVENT_STREAM_STARTED:
            SOURCE_DBLOG("::A2DP_EVENT_STREAM_STARTED  stream_id:%d\n",BT_DEVICE_ID_1);

            app_bt_device.a2dp_streamming[BT_DEVICE_ID_1] = 1;
            break;
        case BTIF_A2DP_EVENT_STREAM_START_IND:
            SOURCE_DBLOG("::A2DP_EVENT_STREAM_START_IND stream_id:%d\n", BT_DEVICE_ID_1);

            btif_a2dp_start_stream_rsp(Stream, BTIF_A2DP_ERR_NO_ERROR);
            app_bt_device.a2dp_play_pause_flag = 1;
            break;
        case BTIF_A2DP_EVENT_STREAM_SUSPENDED:
            SOURCE_DBLOG("::A2DP_EVENT_STREAM_SUSPENDED  stream_id:%d\n",BT_DEVICE_ID_1);
            app_bt_device.a2dp_play_pause_flag = 0;
            app_bt_device.a2dp_streamming[BT_DEVICE_ID_1] = 0;
            break;
        case BTIF_A2DP_EVENT_STREAM_DATA_IND:
            break;
        case BTIF_A2DP_EVENT_STREAM_CLOSED:
            SOURCE_DBLOG("::A2DP_EVENT_STREAM_CLOSED stream_id:%d, reason = 0x%x\n", BT_DEVICE_ID_1,Info->discReason);
//            app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_STOP,BT_STREAM_SBC,stream_id_flag.id,0,0,0);
            app_bt_device.a2dp_state[BT_DEVICE_ID_1] = 0;
            app_bt_device.a2dp_streamming[BT_DEVICE_ID_1] = 0;
            a2dp_source_notify_send();
            break;
        case BTIF_A2DP_EVENT_CODEC_INFO:
            SOURCE_DBLOG("::A2DP_EVENT_CODEC_INFO %d\n", Info->event);
            a2dp_set_config_codec(&sink_config_codec,Info);
            break;
        case BTIF_A2DP_EVENT_GET_CONFIG_IND:
            SOURCE_DBLOG("::A2DP_EVENT_GET_CONFIG_IND %d\n", Info->event);
 //           sink_config_codec.elements[0] = (sink_config_codec.elements[0]&0xef) |  A2D_SBC_IE_SAMP_FREQ_44;
            btif_a2dp_set_stream_config(Stream, &sink_config_codec, NULL);
            break;
        case BTIF_A2DP_EVENT_STREAM_RECONFIG_IND:
            SOURCE_DBLOG("::A2DP_EVENT_STREAM_RECONFIG_IND %d\n", Info->event);
            btif_a2dp_reconfig_stream_rsp(Stream,BTIF_A2DP_ERR_NO_ERROR,0);
            break;
        case BTIF_A2DP_EVENT_STREAM_RECONFIG_CNF:
            SOURCE_DBLOG("::A2DP_EVENT_STREAM_RECONFIG_CNF %d\n", Info->event);
            break;
        case A2DP_EVENT_STREAM_SBC_PACKET_SENT:
            a2dp_source_notify_send();
            break;

    }
}



//__SRAMDATA static BtHandler a2dp_source_handler;
static uint8_t app_a2dp_source_find_process=0;


void app_a2dp_source_stop_find(void)
{
    app_a2dp_source_find_process=0;
//    ME_UnregisterGlobalHandler(&a2dp_source_handler);

}
void app_a2dp_source_set_device_cmgr_handler(btif_cmgr_handler_t* handler )
{
	bt_profile_manager[BT_DEVICE_ID_1].pCmgrHandler = handler;
}

void app_a2dp_source_init_more(void)

{
    a2dp_source_lock_t *lock;

    InitCQueue(&a2dp_source.pcm_queue, A2DP_LINEIN_SIZE, (CQItemType *) a2dp_linein_buff);

    if (a2dp_source_mutex_id == NULL) {
	a2dp_source_mutex_id = osMutexCreate((osMutex(a2dp_source_mutex)));
    }

    lock = &(a2dp_source.data_lock);
    memset(lock, 0, sizeof(a2dp_source_lock_t));
    lock->_osSemaphoreDef.semaphore = lock->_semaphore_data;
    lock->_osSemaphoreId = osSemaphoreCreate(&(lock->_osSemaphoreDef), 0);

    lock = &(a2dp_source.sbc_send_lock);
    memset(lock, 0, sizeof(a2dp_source_lock_t));
    lock->_osSemaphoreDef.semaphore = lock->_semaphore_data;
    lock->_osSemaphoreId = osSemaphoreCreate(&(lock->_osSemaphoreDef), 0);

    a2dp_source.sbc_send_id = osThreadCreate(osThread(send_thread), NULL);
    a2dp_source.sample_rate = AUD_SAMPRATE_44100;


}

static void _find_a2dp_sink_peer_device_start(void)
{
    SOURCE_DBLOG("\nenter: %s %d\n",__FUNCTION__,__LINE__);
    bt_status_t  stat;
    if(app_a2dp_source_find_process ==0 && app_bt_device.a2dp_state[0]==0)
    {
        app_a2dp_source_find_process = 1;

    again:
        stat = btif_me_inquiry(BT_IAC_GIAC, 30, 0);
        SOURCE_DBLOG("\n%s %d\n",__FUNCTION__,__LINE__);
        if (stat != BT_STATUS_PENDING){
            osDelay(500);
            goto again;
        }
        SOURCE_DBLOG("\n%s %d\n",__FUNCTION__,__LINE__);
    }
}

void app_a2dp_source_find_sink(void)
{
    _find_a2dp_sink_peer_device_start();
}


static bool need_init_encoder = true;
static btif_sbc_stream_info_t  StreamInfo = {0};
static btif_sbc_encoder_t sbc_encoder;


static uint8_t app_a2dp_source_samplerate_2_sbcenc_type(enum AUD_SAMPRATE_T sample_rate)
{
    uint8_t rate;
    switch(sample_rate)
    {
        case AUD_SAMPRATE_16000:
            rate =  SBC_CHNL_SAMPLE_FREQ_16;
            break;
        case AUD_SAMPRATE_32000:
            rate =  SBC_CHNL_SAMPLE_FREQ_32;
            break;
        case AUD_SAMPRATE_44100:
            rate =  SBC_CHNL_SAMPLE_FREQ_44_1;
            break;
        case AUD_SAMPRATE_48000:
            rate =  SBC_CHNL_SAMPLE_FREQ_48;
            break;
         default:
            TRACE("error!  sbc enc don't support other samplerate");
            break;
    }
        SOURCE_DBLOG("\n%s %d rate = %x\n",__FUNCTION__,__LINE__,rate);
        return rate;

}


static void a2dp_source_send_sbc_packet(void)
{
    uint32_t frame_size = 512;
    uint32_t frame_num = A2DP_TRANS_SIZE/frame_size;
//    a2dp_source.lock_stream(&(tws.tws_source));
    uint16_t byte_encoded = 0;
//    uint16_t pcm_frame_size = 512/2;
    unsigned short enc_len = 0;
    bt_status_t status = BT_STATUS_FAILED;

    int lock = int_lock();
    sbcpack_t *sbcpack = get_sbcPacket();
    btif_a2dp_sbc_packet_t *sbcPacket = &(sbcpack->sbcPacket);
    sbcPacket->data = (U8 *)sbcpack->buffer;
    memcpy(sbcpack->buffer,a2dp_transmit_buffer,A2DP_TRANS_SIZE);

//    sbcPacket->dataLen = len;
//    sbcPacket->frameSize = len/frame_num;

        SbcPcmData PcmEncData;
    	if(need_init_encoder) {
            SBC_InitEncoder(&sbc_encoder);
            sbc_encoder.streamInfo.numChannels = 2;
            sbc_encoder.streamInfo.channelMode = SBC_CHNL_MODE_STEREO;
            sbc_encoder.streamInfo.bitPool     = 53;
            sbc_encoder.streamInfo.sampleFreq  = app_a2dp_source_samplerate_2_sbcenc_type(a2dp_source.sample_rate);
            sbc_encoder.streamInfo.allocMethod = SBC_ALLOC_METHOD_LOUDNESS;
            sbc_encoder.streamInfo.numBlocks   = 16;
            sbc_encoder.streamInfo.numSubBands = 8;
            sbc_encoder.streamInfo.mSbcFlag    = 0;
            need_init_encoder = 0;
            TRACE("sbc_encoder.streamInfo.sampleFreq = %x",sbc_encoder.streamInfo.sampleFreq);
        }
        PcmEncData.data = (uint8_t *)a2dp_transmit_buffer;
        PcmEncData.dataLen = A2DP_TRANS_SIZE;
        PcmEncData.numChannels = 2;
        PcmEncData.sampleFreq = sbc_encoder.streamInfo.sampleFreq;
        SBC_EncodeFrames(&sbc_encoder, &PcmEncData, &byte_encoded,
                                        (unsigned char*)sbcpack->buffer, &enc_len, A2DP_TRANS_SIZE) ;
        sbcPacket->dataLen = enc_len;
        sbcPacket->frameSize = enc_len/frame_num;

    int_unlock(lock);
    status = btif_a2dp_stream_send_sbc_packet(app_bt_device.a2dp_stream[BT_DEVICE_ID_1].a2dp_stream,sbcPacket,&StreamInfo);
    if(status == BT_STATUS_PENDING)
        a2dp_source_wait_sent(osWaitForever);
    else
        TRACE("SEND ERROR CHECK IT!");
}



#define BT_A2DP_SOURCE_LINEIN_BUFF_SIZE    	(512*5*2*2)

//////////start the audio linein stream for capure the pcm data
int app_a2dp_source_linein_on(bool on)
{
    uint8_t *buff_play = NULL;
    uint8_t *buff_capture = NULL;
    uint8_t *buff_loop = NULL;
    struct AF_STREAM_CONFIG_T stream_cfg;
    static bool isRun =  false;
    SOURCE_DBLOG("app_a2dp_source_linein_on work:%d op:%d", isRun, on);

    if (isRun==on)
        return 0;

    if (on){
#if defined(SLAVE_USE_OPUS) || defined(MASTER_USE_OPUS) || defined(ALL_USE_OPUS)
        app_audio_mempool_init(app_audio_get_basebuf_ptr(APP_MEM_LINEIN_AUDIO), app_audio_get_basebuf_size(APP_MEM_LINEIN_AUDIO));
#else
        app_audio_mempool_init();
#endif
        app_sysfreq_req(APP_SYSFREQ_USER_APP_0, APP_SYSFREQ_208M);

        app_audio_mempool_get_buff(&buff_play, BT_A2DP_SOURCE_LINEIN_BUFF_SIZE);

        memset(&stream_cfg, 0, sizeof(stream_cfg));

        stream_cfg.bits = AUD_BITS_16;
        stream_cfg.channel_num = AUD_CHANNEL_NUM_2;
        stream_cfg.sample_rate = a2dp_source.sample_rate;
#if FPGA==0
        stream_cfg.device = AUD_STREAM_USE_INT_CODEC;
#else
        stream_cfg.device = AUD_STREAM_USE_EXT_CODEC;
#endif
        stream_cfg.trigger_time = 0;

        stream_cfg.vol = 10;
        stream_cfg.io_path = AUD_INPUT_PATH_HP_MIC;
        stream_cfg.handler = a2dp_source_linein_more_pcm_data;
        stream_cfg.data_ptr = buff_play;
        stream_cfg.data_size = BT_A2DP_SOURCE_LINEIN_BUFF_SIZE;
        af_stream_open(AUD_STREAM_ID_0, AUD_STREAM_CAPTURE, &stream_cfg);
        af_stream_start(AUD_STREAM_ID_0, AUD_STREAM_CAPTURE);
        SOURCE_DBLOG("app_factorymode_audioloop on");
    } else {
        af_stream_stop(AUD_STREAM_ID_0, AUD_STREAM_CAPTURE);
        af_stream_close(AUD_STREAM_ID_0, AUD_STREAM_CAPTURE);
        SOURCE_DBLOG("app_factorymode_audioloop off");
        app_sysfreq_req(APP_SYSFREQ_USER_APP_0, APP_SYSFREQ_32K);
    }

    isRun=on;
    return 0;
}



////////////////////////////creat the thread for send sbc data to a2dp sink device ///////////////////
static void send_thread(const void *arg);


__SRAMDATA osThreadDef(send_thread, osPriorityHigh, 1024*2);



static void send_thread(const void *arg)
{
    while(1){
        a2dp_source_wait_pcm_data();
        while(a2dp_source_pcm_buffer_read((uint8_t *)a2dp_transmit_buffer,A2DP_TRANS_SIZE)==0)
        {
            a2dp_source_send_sbc_packet();

        }
    }
}



#endif
