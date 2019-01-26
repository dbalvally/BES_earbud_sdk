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
#include "cmsis_os.h"
#include "hal_trace.h"
#include "resources.h"
#include "app_bt_stream.h"
#include "app_media_player.h"
#include "app_factory.h"
#include "string.h"

// for audio
#include "audioflinger.h"
#include "app_audio.h"
#include "app_utils.h"



#ifdef __FACTORY_MODE_SUPPORT__

#define BT_AUDIO_FACTORMODE_BUFF_SIZE    	(1024*2)
static enum APP_AUDIO_CACHE_T a2dp_cache_status = APP_AUDIO_CACHE_QTY;
static int16_t *app_audioloop_play_cache = NULL;

static uint32_t app_factorymode_data_come(uint8_t *buf, uint32_t len)
{
    app_audio_pcmbuff_put(buf, len);
    if (a2dp_cache_status == APP_AUDIO_CACHE_QTY){
        a2dp_cache_status = APP_AUDIO_CACHE_OK;
    }
    return len;
}

static uint32_t app_factorymode_more_data(uint8_t *buf, uint32_t len)
{
    if (a2dp_cache_status != APP_AUDIO_CACHE_QTY){
        app_audio_pcmbuff_get((uint8_t *)app_audioloop_play_cache, len/2);
        app_bt_stream_copy_track_one_to_two_16bits((int16_t *)buf, app_audioloop_play_cache, len/2/2);
    }
    return len;
}

#if defined(__AUDIO_RESAMPLE__) && defined(SW_CAPTURE_RESAMPLE)
static struct APP_RESAMPLE_T *sco_play_resample;
static int bt_sco_resample_iter(uint8_t *buf, uint32_t len)
{
    app_factorymode_data_come(buf, len);
    return 0;
}
#endif

int app_factorymode_audioloop(bool on, enum APP_SYSFREQ_FREQ_T freq)
{
    uint8_t *buff_play = NULL;
    uint8_t *buff_capture = NULL;
    uint8_t *buff_loop = NULL;
    struct AF_STREAM_CONFIG_T stream_cfg;
    static bool isRun =  false;

    enum AUD_SAMPRATE_T sample_rate;

    
    APP_FACTORY_TRACE("app_factorymode_audioloop work:%d op:%d freq:%d", isRun, on, freq);

    if (isRun==on)
        return 0;

    if (on){
        if (freq < APP_SYSFREQ_52M) {
            freq = APP_SYSFREQ_52M;
        }
        app_sysfreq_req(APP_SYSFREQ_USER_APP_0, freq);

        a2dp_cache_status = APP_AUDIO_CACHE_QTY;
        app_audio_mempool_init();
        app_audio_mempool_get_buff(&buff_capture, BT_AUDIO_FACTORMODE_BUFF_SIZE);
        app_audio_mempool_get_buff(&buff_play, BT_AUDIO_FACTORMODE_BUFF_SIZE*2);
        app_audio_mempool_get_buff((uint8_t **)&app_audioloop_play_cache, BT_AUDIO_FACTORMODE_BUFF_SIZE*2/2/2);
        app_audio_mempool_get_buff(&buff_loop, BT_AUDIO_FACTORMODE_BUFF_SIZE<<2);
        app_audio_pcmbuff_init(buff_loop, BT_AUDIO_FACTORMODE_BUFF_SIZE<<2);
        memset(&stream_cfg, 0, sizeof(stream_cfg));
        stream_cfg.bits = AUD_BITS_16;
        stream_cfg.channel_num = AUD_CHANNEL_NUM_1;
#if FPGA==0
        stream_cfg.device = AUD_STREAM_USE_INT_CODEC;
#else
        stream_cfg.device = AUD_STREAM_USE_EXT_CODEC;
#endif
        sample_rate = AUD_SAMPRATE_8000;
#if defined(__AUDIO_RESAMPLE__) && defined(SW_CAPTURE_RESAMPLE)
        if (sample_rate == AUD_SAMPRATE_8000) {
            stream_cfg.sample_rate = AUD_SAMPRATE_8463;
        } else if (sample_rate == AUD_SAMPRATE_16000) {
            stream_cfg.sample_rate = AUD_SAMPRATE_16927;
        }
        sco_play_resample = app_capture_resample_open(sample_rate, stream_cfg.channel_num,
            bt_sco_resample_iter, BT_AUDIO_FACTORMODE_BUFF_SIZE);
#else
        stream_cfg.sample_rate = sample_rate;
#endif



        stream_cfg.vol = 10;
        stream_cfg.io_path = AUD_INPUT_PATH_MAINMIC;
        stream_cfg.handler = app_factorymode_data_come;

        stream_cfg.data_ptr = BT_AUDIO_CACHE_2_UNCACHE(buff_capture);
        stream_cfg.data_size = BT_AUDIO_FACTORMODE_BUFF_SIZE;
        af_stream_open(AUD_STREAM_ID_0, AUD_STREAM_CAPTURE, &stream_cfg);
        stream_cfg.sample_rate  = sample_rate;
        stream_cfg.channel_num = AUD_CHANNEL_NUM_2;
        stream_cfg.io_path = AUD_OUTPUT_PATH_SPEAKER;
        stream_cfg.handler = app_factorymode_more_data;

        stream_cfg.data_ptr = BT_AUDIO_CACHE_2_UNCACHE(buff_play);
        stream_cfg.data_size = BT_AUDIO_FACTORMODE_BUFF_SIZE*2;
        af_stream_open(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK, &stream_cfg);

        af_stream_start(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK);
        af_stream_start(AUD_STREAM_ID_0, AUD_STREAM_CAPTURE);
        APP_FACTORY_TRACE("app_factorymode_audioloop on");
    } else {
        af_stream_stop(AUD_STREAM_ID_0, AUD_STREAM_CAPTURE);
        af_stream_stop(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK);
        af_stream_close(AUD_STREAM_ID_0, AUD_STREAM_CAPTURE);
        af_stream_close(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK);
        APP_FACTORY_TRACE("app_factorymode_audioloop off");

        app_sysfreq_req(APP_SYSFREQ_USER_APP_0, APP_SYSFREQ_32K);
    }

    isRun=on;
    return 0;
}

#if defined(__audio_test_suite__)

#include "cqueue.h"
#define APP_AUDIO_TEST_BUFF_SIZE  (512*2)
#define APP_AUDIO_UART_BUFF_SIZE (512)

typedef struct {
    CQueue  queue;
    struct audio_fifo_mutex_t {
        osMutexId _osMutexId;
        osMutexDef_t _osMutexDef;
#ifdef CMSIS_OS_RTX
        int32_t _mutex_data[3];
#endif
    }mutex;
}audio_fifo_t;

static int app_audio_test_buffer_init(audio_fifo_t *fifo, uint8_t *buff, uint16_t len)
{
    memset(fifo, 0, sizeof(audio_fifo_t));
    fifo->mutex._osMutexDef.mutex = fifo->mutex._mutex_data;
    fifo->mutex._osMutexId = osMutexCreate(&(fifo->mutex._osMutexDef));

    osMutexWait(fifo->mutex._osMutexId, osWaitForever);
    InitCQueue(&(fifo->queue), len, buff);
    memset(buff, 0x00, len);
    osMutexRelease(fifo->mutex._osMutexId);

    return 0;
}

static int app_audio_test_buffer_deinit(audio_fifo_t *fifo)
{
    osMutexDelete(fifo->mutex._osMutexId);
    
    return 0;
}

static int app_audio_test_buffer_length(audio_fifo_t *fifo)
{
    int len;

    osMutexWait(fifo->mutex._osMutexId, osWaitForever);
    len = LengthOfCQueue(&(fifo->queue));
    osMutexRelease(fifo->mutex._osMutexId);

    return len;
}

static int app_audio_test_buffer_available(audio_fifo_t *fifo)
{
    int len;

    osMutexWait(fifo->mutex._osMutexId, osWaitForever);
    len = AvailableOfCQueue(&(fifo->queue));
    osMutexRelease(fifo->mutex._osMutexId);

    return len;
}

static int app_audio_test_buffer_write(audio_fifo_t *fifo, uint8_t *buff, uint16_t len)
{
    int status;

    osMutexWait(fifo->mutex._osMutexId, osWaitForever);
    status = EnCQueue(&(fifo->queue), buff, len);
    osMutexRelease(fifo->mutex._osMutexId);

    return status;
}

static int app_audio_test_buffer_read(audio_fifo_t *fifo, uint8_t *buff, uint16_t len)
{
    uint8_t *e1 = NULL, *e2 = NULL;
    unsigned int len1 = 0, len2 = 0;
    int status;

    osMutexWait(fifo->mutex._osMutexId, osWaitForever);
    status = PeekCQueue(&(fifo->queue), len, &e1, &len1, &e2, &len2);
    if (len==(len1+len2)){
        memcpy(buff,e1,len1);
        memcpy(buff+len1,e2,len2);
        DeCQueue(&(fifo->queue), 0, len);
    }else{
        memset(buff, 0x00, len);
        status = -1;
    }
    osMutexRelease(fifo->mutex._osMutexId);
    
    return status;
}

int app_audio_test_buffer_discard(audio_fifo_t *fifo, uint16_t len)
{
    int status;

    osMutexWait(fifo->mutex._osMutexId, osWaitForever);
    status = DeCQueue(&(fifo->queue), 0, len);
    osMutexRelease((fifo->mutex._osMutexId));

    return status;
}

typedef struct {
    uint16_t *buf;
    uint32_t len;
    uint32_t cuur_buf_pos;
}audio_test_pcmpatten_t;

int app_audio_test_output_pcmpatten(audio_test_pcmpatten_t *pcmpatten, uint8_t *buf, uint32_t len)
{
    uint32_t remain_size = len;
    uint32_t curr_size = 0;

    if (remain_size > pcmpatten->len)
    {
        do{
            if (pcmpatten->cuur_buf_pos)
            {
                curr_size = pcmpatten->len-pcmpatten->cuur_buf_pos;
                memcpy(buf,&(pcmpatten->buf[pcmpatten->cuur_buf_pos/2]), curr_size);
                remain_size -= curr_size;
                pcmpatten->cuur_buf_pos = 0;
            }
            else if (remain_size>pcmpatten->len)
            {
                memcpy(buf+curr_size, pcmpatten->buf, pcmpatten->len);
                curr_size += pcmpatten->len;
                remain_size -= pcmpatten->len;
            }
            else
            {
                memcpy(buf+curr_size,pcmpatten->buf, remain_size);
                pcmpatten->cuur_buf_pos = remain_size;
                remain_size = 0;
            }
        }while(remain_size);
    }
    else
    {
        if ((pcmpatten->len - pcmpatten->cuur_buf_pos) >= len)
        {
            memcpy(buf, &(pcmpatten->buf[pcmpatten->cuur_buf_pos/2]),len);
            pcmpatten->cuur_buf_pos += len;
        }
        else 
        {
            curr_size = pcmpatten->len-pcmpatten->cuur_buf_pos;
            memcpy(buf, &(pcmpatten->buf[pcmpatten->cuur_buf_pos/2]),curr_size);
            pcmpatten->cuur_buf_pos = len - curr_size;
            memcpy(buf+curr_size, pcmpatten->buf, pcmpatten->cuur_buf_pos);                
        }
    }
    
    return 0;
}

uint8_t mic_buff_a[APP_AUDIO_TEST_BUFF_SIZE*2];
audio_fifo_t mic_fifo_a = {0,};

uint8_t mic_buff_b[APP_AUDIO_TEST_BUFF_SIZE*2];
audio_fifo_t mic_fifo_b = {0,};

uint8_t mic_buff_proc[APP_AUDIO_TEST_BUFF_SIZE*2];
audio_fifo_t mic_fifo_proc = {0,};

uint8_t mic_buff_bone[APP_AUDIO_TEST_BUFF_SIZE*2];
audio_fifo_t mic_fifo_bone = {0,};

osMessageQDef(app_audio_test_msg, 20, NULL);
osMessageQId app_audio_test_msg_id;

enum APP_AUDIO_TEST_MSG_T {
    APP_AUDIO_TEST_MSG_MIC_A = 0,
    APP_AUDIO_TEST_MSG_MIC_B = 1,
    APP_AUDIO_TEST_MSG_MIC_PROC = 2,
    APP_AUDIO_TEST_MSG_MIC_BONE = 3,
    APP_AUDIO_TEST_MSG_UART = 4,

    APP_AUDIO_TEST_MSG_NUM
};

static void app_audio_test_thread(void const *argument)
{
    osEvent evt;
    uint8_t uart_buf[APP_AUDIO_UART_BUFF_SIZE];
    bool capture_valid = false;

    app_audio_test_buffer_init(&mic_fifo_a, mic_buff_a, sizeof(mic_buff_a));
    app_audio_test_buffer_init(&mic_fifo_b, mic_buff_b, sizeof(mic_buff_b));
    app_audio_test_buffer_init(&mic_fifo_proc, mic_buff_proc, sizeof(mic_buff_proc));
    app_audio_test_buffer_init(&mic_fifo_bone, mic_buff_bone, sizeof(mic_buff_bone));

    app_audio_test_msg_id =  osMessageCreate(osMessageQ(app_audio_test_msg), NULL);

    while(1){
        evt = osMessageGet(app_audio_test_msg_id, osWaitForever);
        if (evt.status == osEventMessage) {
            switch ((uint32_t)evt.value.p) {
                case APP_AUDIO_TEST_MSG_MIC_A:
                    if (!app_audio_test_buffer_read(&mic_fifo_a, uart_buf, sizeof(uart_buf))){
                        capture_valid = true;
                    }
                    break;
                case APP_AUDIO_TEST_MSG_MIC_B:
                    if (!app_audio_test_buffer_read(&mic_fifo_b, uart_buf, sizeof(uart_buf))){
                        capture_valid = true;
                    }
                    break;
                case APP_AUDIO_TEST_MSG_MIC_PROC:
                    if (!app_audio_test_buffer_read(&mic_fifo_proc, uart_buf, sizeof(uart_buf))){
                        capture_valid = true;
                    }
                    break;
                case APP_AUDIO_TEST_MSG_MIC_BONE:
                    if (!app_audio_test_buffer_read(&mic_fifo_bone, uart_buf, sizeof(uart_buf))){
                        capture_valid = true;
                    }
                    break;
                case APP_AUDIO_TEST_MSG_UART:

                    break;
                default:
                    break;
            }
            if (capture_valid){
                capture_valid = false;
            }
        }
    }
}


static osThreadId app_audio_test_tid = NULL;
osThreadDef(app_audio_test_thread, osPriorityHigh, 2048);

void app_audio_test_init(void)
{
    if(!app_audio_test_tid){
        app_audio_test_tid = osThreadCreate(osThread(app_audio_test_thread), NULL);
    }
}

void app_audio_test_chnlsel_16bits(uint8_t chnlsel, uint16_t *src_buf, uint16_t *dst_buf,  uint32_t src_len)
{
    uint32_t i = 0;
    for (i = 0; i < src_len; i+=2)
    {
        dst_buf[i/2] = src_buf[i + chnlsel];
    }
}

void  app_audio_test_copy_track_one_to_two_16bits(int16_t *dst_buf, int16_t *src_buf, uint32_t src_len)
{
    // Copy from tail so that it works even if dst_buf == src_buf
    for (int i = (int)(src_len - 1); i >= 0; i--)
    {
        dst_buf[i*2 + 0] = dst_buf[i*2 + 1] = src_buf[i];
    }
}

int16_t app_audio_test_capture_cache[128];

static uint32_t app_audio_test_capture(uint8_t *buf, uint32_t len)
{
    uint16_t sample_size = len/2;

    app_audio_pcmbuff_put(buf, len);
    app_audio_test_chnlsel_16bits(0, (uint16_t *)buf, (uint16_t *)app_audio_test_capture_cache,  sample_size);
    app_audio_test_buffer_write(&mic_fifo_a, (uint8_t *)app_audio_test_capture_cache, sizeof(app_audio_test_capture_cache));
    if (app_audio_test_buffer_length(&mic_fifo_a)>APP_AUDIO_UART_BUFF_SIZE){
        osMessagePut(app_audio_test_msg_id, APP_AUDIO_TEST_MSG_MIC_A, 0);
    }
    
    app_audio_test_chnlsel_16bits(1, (uint16_t *)buf, (uint16_t *)app_audio_test_capture_cache,  sample_size);
    app_audio_test_buffer_write(&mic_fifo_b, (uint8_t *)app_audio_test_capture_cache, sizeof(app_audio_test_capture_cache));
    if (app_audio_test_buffer_length(&mic_fifo_b)>APP_AUDIO_UART_BUFF_SIZE){
        osMessagePut(app_audio_test_msg_id, APP_AUDIO_TEST_MSG_MIC_B, 0);
    }

    return len;
}

static uint32_t app_audio_test_playback(uint8_t *buf, uint32_t len)
{
    uint16_t sample_size = len/2;
    app_audio_pcmbuff_get(buf, len);

    app_audio_test_chnlsel_16bits(0, (uint16_t *)buf, (uint16_t *)app_audio_test_capture_cache,  sample_size);
    app_audio_test_buffer_write(&mic_fifo_proc, (uint8_t *)app_audio_test_capture_cache, sizeof(app_audio_test_capture_cache));
    if (app_audio_test_buffer_length(&mic_fifo_proc)>APP_AUDIO_UART_BUFF_SIZE){
        osMessagePut(app_audio_test_msg_id, APP_AUDIO_TEST_MSG_MIC_PROC, 0);
    }
    return len;
}

int app_audio_test_suite(bool on)
{
    static bool isRun =  false;
    uint8_t *buff_play = NULL;
    uint8_t *buff_capture = NULL;
    uint8_t *buff_loop = NULL;
    struct AF_STREAM_CONFIG_T stream_cfg;
    enum AUD_SAMPRATE_T sample_rate;

    
    TRACE("app_audio_test_suite work:%d op:%d", isRun, on);

    if (isRun==on)
        return 0;

    if (on){

        app_sysfreq_req(APP_SYSFREQ_USER_APP_0, APP_SYSFREQ_104M);

        app_audio_test_init();
        app_audio_mempool_init();
        app_audio_mempool_get_buff(&buff_capture, APP_AUDIO_TEST_BUFF_SIZE);
        app_audio_mempool_get_buff(&buff_play, APP_AUDIO_TEST_BUFF_SIZE);
        app_audio_mempool_get_buff(&buff_loop, APP_AUDIO_TEST_BUFF_SIZE<<2);
        app_audio_pcmbuff_init(buff_loop, APP_AUDIO_TEST_BUFF_SIZE<<2);
        memset(&stream_cfg, 0, sizeof(stream_cfg));
        stream_cfg.bits = AUD_BITS_16;
        stream_cfg.channel_num = AUD_CHANNEL_NUM_2;
        stream_cfg.device = AUD_STREAM_USE_INT_CODEC;
        stream_cfg.sample_rate = AUD_SAMPRATE_16000;

        stream_cfg.vol = 16;
        stream_cfg.io_path = AUD_INPUT_PATH_MAINMIC;
        stream_cfg.handler = app_audio_test_capture;

        stream_cfg.data_ptr = BT_AUDIO_CACHE_2_UNCACHE(buff_capture);
        stream_cfg.data_size = APP_AUDIO_TEST_BUFF_SIZE;
        af_stream_open(AUD_STREAM_ID_0, AUD_STREAM_CAPTURE, &stream_cfg);
        
        stream_cfg.sample_rate  = AUD_SAMPRATE_16000;
        stream_cfg.channel_num = AUD_CHANNEL_NUM_2;
        stream_cfg.io_path = AUD_OUTPUT_PATH_SPEAKER;
        stream_cfg.handler = app_audio_test_playback;

        stream_cfg.data_ptr = BT_AUDIO_CACHE_2_UNCACHE(buff_play);
        stream_cfg.data_size = APP_AUDIO_TEST_BUFF_SIZE;
        af_stream_open(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK, &stream_cfg);

        af_stream_start(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK);
        af_stream_start(AUD_STREAM_ID_0, AUD_STREAM_CAPTURE);
        TRACE("app_audio_test_suite on");
    } else {
        af_stream_stop(AUD_STREAM_ID_0, AUD_STREAM_CAPTURE);
        af_stream_stop(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK);
        af_stream_close(AUD_STREAM_ID_0, AUD_STREAM_CAPTURE);
        af_stream_close(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK);
        APP_FACTORY_TRACE("app_audio_test_suite off");

        app_sysfreq_req(APP_SYSFREQ_USER_APP_0, APP_SYSFREQ_32K);
    }

    isRun=on;
    return 0;
}

#endif

#include "fft128dot.h"

#define N 64
#define NFFT 128

struct mic_st_t{
    FftTwiddle_t w[N];
    FftTwiddle_t w128[N*2];
    FftData_t x[N*2];
    FftData_t data_odd[N];
    FftData_t data_even[N];
    FftData_t data_odd_d[N];
    FftData_t data_even_d[N];
    FftData_t data[N*2];
    signed long out[N];
};

int app_factorymode_mic_cancellation_run(void * mic_st, signed short *inbuf, int sample)
{
    struct mic_st_t *st = (struct mic_st_t *)mic_st;
    int i,k,jj,ii;
    //int dataWidth = 16;     // input word format is 16 bit twos complement fractional format 1.15
    int twiddleWidth = 16;  // input word format is 16 bit twos complement fractional format 2.14
    FftMode_t ifft = FFT_MODE;

    make_symmetric_twiddles(st->w,N,twiddleWidth);
    make_symmetric_twiddles(st->w128,N*2,twiddleWidth);
    // input data
    for (i=0; i<sample; i++){
        st->x[i].re = inbuf[i];
        st->x[i].im = 0;
    }

    for(ii = 0; ii < 1; ii++)
    {
      k = 0;
        for (jj = 0; jj < N*2; jj+=2)
        {
            FftData_t tmp;

            tmp.re     = st->x[jj].re;
            tmp.im     = st->x[jj].im;

              st->data_even[k].re = tmp.re;//(int) (double(tmp.re)*double(1 << FFTR4_INPUT_FORMAT_Y)) ;
              st->data_even[k].im = tmp.im;//(int) (double(tmp.im)*double(1 << FFTR4_INPUT_FORMAT_Y)) ;
              tmp.re     = st->x[jj+1].re;
              tmp.im     = st->x[jj+1].im;
              st->data_odd[k].re  = tmp.re;//(int) (double(tmp.re)*double(1 << FFTR4_INPUT_FORMAT_Y)) ;
              st->data_odd[k].im  = tmp.im;//(int) (double(tmp.im)*double(1 << FFTR4_INPUT_FORMAT_Y)) ;
              k++;
        }

        fftr4(NFFT/2, st->data_even, st->w, FFTR4_TWIDDLE_WIDTH, FFTR4_DATA_WIDTH, ifft);
        fftr4(NFFT/2, st->data_odd, st->w, FFTR4_TWIDDLE_WIDTH, FFTR4_DATA_WIDTH, ifft);

        for (jj = 0; jj < NFFT/2; jj++)
        {

        int idx = dibit_reverse_int(jj, NFFT/2);
            st->data_even_d[jj].re = st->data_even[idx].re;
            st->data_even_d[jj].im = st->data_even[idx].im;
            st->data_odd_d[jj].re  = st->data_odd[idx].re;
            st->data_odd_d[jj].im  = st->data_odd[idx].im;
        }
        for (jj=0;jj<NFFT/2;jj++)
        {
          long long mbr,mbi;
          FftData_t ta;
          FftData_t tmp;
          double a;
      mbr = (long long)(st->data_odd_d[jj].re) * st->w128[jj].re - (long long)(st->data_odd_d[jj].im) * st->w128[jj].im;
      mbi = (long long)(st->data_odd_d[jj].im) * st->w128[jj].re + (long long)(st->data_odd_d[jj].re) * st->w128[jj].im;
      ta.re = int(mbr>>(FFTR4_TWIDDLE_WIDTH-2));
      ta.im = int(mbi>>(FFTR4_TWIDDLE_WIDTH-2));
      st->data[jj].re = (st->data_even_d[jj].re + ta.re)/2;
      st->data[jj].im = (st->data_even_d[jj].im + ta.im)/2;
      //data[jj] = sat(data[jj],FFTR4_DATA_WIDTH);
      st->data[jj+NFFT/2].re = (st->data_even_d[jj].re - ta.re)/2;
      st->data[jj+NFFT/2].im = (st->data_even_d[jj].im - ta.im)/2;
      //data[jj+NFFT/2] = sat(data[jj+NFFT/2],FFTR4_DATA_WIDTH);

      a = st->data[jj].re;///double(1 << FFTR4_OUTPUT_FORMAT_Y);// * double(1 << FFTR4_SCALE);
          tmp.re              = (int)a;
          a = st->data[jj].im;///double(1 << FFTR4_OUTPUT_FORMAT_Y);// * double(1 << FFTR4_SCALE);
          tmp.im              = (int)a;
            st->x[ii*NFFT+jj].re = (int) tmp.re;
            st->x[ii*NFFT+jj].im = (int) tmp.im;
      a = st->data[jj+NFFT/2].re;///double(1 << FFTR4_OUTPUT_FORMAT_Y);// * double(1 << FFTR4_SCALE);
          tmp.re              = (int)a;
          a = st->data[jj+NFFT/2].im;///double(1 << FFTR4_OUTPUT_FORMAT_Y);// * double(1 << FFTR4_SCALE);
          tmp.im              = (int)a;
          st->x[ii*NFFT+jj+NFFT/2].re = (int) tmp.re;
            st->x[ii*NFFT+jj+NFFT/2].im = (int) tmp.im;
        }
    }

    for (i=0; i<N; i++){
        st->out[i] = st->x[i].re * st->x[i].re +  st->x[i].im * st->x[i].im;
    }

    return 0;

}

void *app_factorymode_mic_cancellation_init(void* (* alloc_ext)(int))
{
    struct mic_st_t *mic_st;
    mic_st = (struct mic_st_t *)alloc_ext(sizeof(struct mic_st_t));
    return (void *)mic_st;
}

#endif

