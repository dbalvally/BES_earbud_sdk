#include <stdio.h>
#include <assert.h>
#include "cmsis_os.h"
#include "tgt_hardware.h"
#include "audioflinger.h"
#include "hal_trace.h"
#include "app_thread.h"
#include "voice_detector.h"
#include "app_voice_detector.h"

osMutexId vd_mutex_id = NULL;
osMutexDef(vd_mutex);

static int app_voice_detector_process(APP_MESSAGE_BODY *msg_body)
{
    enum voice_detector_id id   = (enum voice_detector_id)msg_body->message_id;
    enum voice_detector_evt evt = (enum voice_detector_evt)msg_body->message_ptr;
    int ret = 0;

    osMutexWait(vd_mutex_id, osWaitForever);
    switch(evt) {
    case VOICE_DET_EVT_VAD_START:
        voice_detector_send_cmd(id, VOICE_DET_CMD_AUD_CAP_STOP);
        voice_detector_send_cmd(id, VOICE_DET_CMD_VAD_OPEN);
        voice_detector_send_cmd(id, VOICE_DET_CMD_VAD_START);
        break;
    case VOICE_DET_EVT_AUD_CAP_START:
        voice_detector_send_cmd(id, VOICE_DET_CMD_VAD_STOP);
        voice_detector_send_cmd(id, VOICE_DET_CMD_VAD_CLOSE);
        voice_detector_send_cmd(id, VOICE_DET_CMD_AUD_CAP_START);
        break;
    case VOICE_DET_EVT_CLOSE:
        voice_detector_send_cmd(id, VOICE_DET_CMD_VAD_STOP);
        voice_detector_send_cmd(id, VOICE_DET_CMD_VAD_CLOSE);
        voice_detector_send_cmd(id, VOICE_DET_CMD_AUD_CAP_STOP);
        voice_detector_send_cmd(id, VOICE_DET_CMD_AUD_CAP_CLOSE);
        voice_detector_close(id);
        break;
    default:
        break;
    }
    voice_detector_run(VOICE_DETECTOR_ID_0, 1);
    osMutexRelease(vd_mutex_id);
    return ret;
}

static void voice_detector_send_msg(uint32_t id, uint32_t evt)
{
   APP_MESSAGE_BLOCK msg;

    msg.mod_id = APP_MODUAL_VOICE_DETECTOR;
    msg.msg_body.message_id  = id;
    msg.msg_body.message_ptr = evt;

    app_mailbox_put(&msg);
}

void app_voice_detector_init(void)
{
    TRACE("%s", __func__);
    vd_mutex_id = osMutexCreate((osMutex(vd_mutex)));
    app_set_threadhandle(APP_MODUAL_VOICE_DETECTOR, app_voice_detector_process);
}

int app_voice_detector_open(enum voice_detector_id id)
{
    int r;

    TRACE("%s", __func__);
    osMutexWait(vd_mutex_id, osWaitForever);
    r = voice_detector_open(id);
    osMutexRelease(vd_mutex_id);
    return r;
}

int app_voice_detector_setup_vad(enum voice_detector_id id,
                                struct codec_vad_conf *conf)
{
    int r;

    TRACE("%s", __func__);
    osMutexWait(vd_mutex_id, osWaitForever);
    r = voice_detector_setup_vad(id, conf);
    osMutexRelease(vd_mutex_id);
    return r;
}

int app_voice_detector_setup_stream(enum voice_detector_id id,
                                    enum AUD_STREAM_T stream_id,
                                    struct AF_STREAM_CONFIG_T *stream)
{
    int r;

    TRACE("%s", __func__);
    osMutexWait(vd_mutex_id, osWaitForever);
    r = voice_detector_setup_stream(id, stream_id, stream);
    osMutexRelease(vd_mutex_id);
    return r;
}

int app_voice_detector_setup_callback(enum voice_detector_id id,
                                      enum voice_detector_cb_id func_id,
                                      voice_detector_cb_t func,
                                      void *param)
{
    int r;

    TRACE("%s", __func__);
    osMutexWait(vd_mutex_id, osWaitForever);
    r = voice_detector_setup_callback(id, func_id, func, param);
    osMutexRelease(vd_mutex_id);
    return r;
}

int app_voice_detector_send_event(enum voice_detector_id id,
                                enum voice_detector_evt evt)
{
    TRACE("%s, id=%d, evt=%d", __func__, id, evt);
    voice_detector_send_msg(id, evt);
    return 0;
}

void app_voice_detector_close(enum voice_detector_id id)
{
    TRACE("%s", __func__);
    voice_detector_send_msg(id, VOICE_DET_EVT_CLOSE);
}
