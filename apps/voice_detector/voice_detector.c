#include "hal_codec.h"
#include "hal_trace.h"
#include "hal_cmu.h"
#include "hal_aud.h"
#include "hal_timer.h"
#include "hal_sleep.h"
#include "audioflinger.h"
#include "app_audio.h"
#include "app_utils.h"
#include "voice_detector.h"
#include "app_voice_detector.h"
#include <string.h>

#define VD_DEBUG

#ifdef VD_DEBUG
#define VD_LOG TRACE
#else
#define VD_LOG(...) do{}while(0)
#endif

#define CMD_QUEUE_DATA_SIZE 10

struct command_queue {
    int idx;
    int out;
    int data[CMD_QUEUE_DATA_SIZE];
};

struct voice_detector_dev {
    uint8_t init;
    uint8_t dfl;
    uint8_t run;
    enum voice_detector_state state;
    uint32_t wakeup_cnt;
    uint32_t arguments[VOICE_DET_CB_QTY];
    voice_detector_cb_t callback[VOICE_DET_CB_QTY];
    struct command_queue cmd_queue;
    struct codec_vad_conf conf;
    struct AF_STREAM_CONFIG_T cap_conf;
    struct AF_STREAM_CONFIG_T ply_conf;
};

static struct voice_detector_dev voice_det_devs[VOICE_DETECTOR_QTY];

static void voice_detector_vad_callback(void *data);

#define to_voice_dev(id) (&voice_det_devs[(id)])

static int cmd_queue_enqueue(struct command_queue *q, int c)
{
    if (!q)
        return -1;
    if (q->idx >= CMD_QUEUE_DATA_SIZE) {
        VD_LOG("%s, overflow cmd=%d", __func__, c);
        return -2;
    }
    if (q->idx < 0) {
        q->idx = 0;
    }
    q->data[q->idx++] = c;
    VD_LOG("%s, cmd=%d", __func__, c);
    return 0;
}

static int cmd_queue_dequeue(struct command_queue *q)
{
    int cmd;

    if (!q)
        return -1;
    if (q->idx < 0) {
//        VD_LOG("%s, empty", __func__);
        return -2;
    }
    if (q->out < 0) {
        q->out = 0;
    }
    cmd = q->data[q->out++];
    if (q->out >= q->idx) {
        q->out = -1;
        q->idx = -1;
    }
    VD_LOG("%s, cmd=%d", __func__, cmd);
    return cmd;
}

static int cmd_queue_is_empty(struct command_queue *q)
{
    return (q->idx < 0) ? 1 : 0;
}

static void voice_detector_init_cmd_queue(struct command_queue *q)
{
    if (q) {
        q->idx = -1;
        q->out = -1;
    }
}

static void voice_detector_init_vad(struct codec_vad_conf *c, int id)
{
    if (c) {
        c->udc         = VAD_DEFAULT_UDC;
        c->upre        = VAD_DEFAULT_UPRE;
        c->frame_len   = VAD_DEFAULT_FRAME_LEN;
        c->mvad        = VAD_DEFAULT_MVAD;
        c->dig_mode    = VAD_DEFAULT_DIG_MODE;
        c->pre_gain    = VAD_DEFAULT_PRE_GAIN;
        c->sth         = VAD_DEFAULT_STH;
        c->dc_bypass   = VAD_DEFAULT_DC_BYPASS;
        c->frame_th[0] = VAD_DEFAULT_FRAME_TH0;
        c->frame_th[1] = VAD_DEFAULT_FRAME_TH1;
        c->frame_th[2] = VAD_DEFAULT_FRAME_TH2;
        c->range[0]    = VAD_DEFAULT_RANGE0;
        c->range[1]    = VAD_DEFAULT_RANGE1;
        c->range[2]    = VAD_DEFAULT_RANGE2;
        c->range[3]    = VAD_DEFAULT_RANGE3;
        c->psd_th[0]   = VAD_DEFAULT_PSD_TH0;
        c->psd_th[1]   = VAD_DEFAULT_PSD_TH1;
        c->callback = voice_detector_vad_callback;
        c->cbdata = (void *)id;
    }
}

int voice_detector_open(enum voice_detector_id id)
{
    struct voice_detector_dev *pdev;

    if (id >= VOICE_DETECTOR_QTY) {
        VD_LOG("%s, invalid id=%d", __func__, id);
        return -1;
    }

    pdev = to_voice_dev(id);
    if (pdev->init) {
        VD_LOG("%s, dev already open", __func__);
        return -2;
    }
    if (pdev->run) {
        VD_LOG("%s, dev not stoped", __func__);
        return -3;
    }

    memset(pdev, 0x0, sizeof(struct voice_detector_dev));
    memset(&pdev->cap_conf, 0x0, sizeof(struct AF_STREAM_CONFIG_T));
    memset(&pdev->ply_conf, 0x0, sizeof(struct AF_STREAM_CONFIG_T));
    voice_detector_init_cmd_queue(&pdev->cmd_queue);
    voice_detector_init_vad(&pdev->conf, id);
    pdev->state = VOICE_DET_STATE_IDLE;
    pdev->run  = 0;
    pdev->dfl  = 1;
    pdev->init = 1;

    return 0;
}

void voice_detector_close(enum voice_detector_id id)
{
    struct voice_detector_dev *pdev;

    if (id >= VOICE_DETECTOR_QTY) {
        VD_LOG("%s, invalid id=%d", __func__, id);
        return;
    }

    pdev = to_voice_dev(id);
    if (pdev->init) {
        if (pdev->run) {
            cmd_queue_enqueue(&pdev->cmd_queue, VOICE_DET_CMD_EXIT);
        }
        pdev->init = 0;
    }
}

int voice_detector_setup_vad(enum voice_detector_id id,
        struct codec_vad_conf *conf)
{
    struct voice_detector_dev *pdev;

    if (id >= VOICE_DETECTOR_QTY) {
        VD_LOG("%s, invalid id=%d", __func__, id);
        return -1;
    }

    pdev = to_voice_dev(id);
    if (!pdev->init) {
        VD_LOG("%s, dev not open", __func__);
        return -2;
    }

    if (conf) {
        memcpy(&pdev->conf, conf, sizeof(struct codec_vad_conf));
        pdev->conf.callback = voice_detector_vad_callback;
        pdev->conf.cbdata = (void *)id;
        pdev->dfl = 0;
    }
    return 0;
}

int voice_detector_setup_stream(enum voice_detector_id id,
        enum AUD_STREAM_T stream_id, struct AF_STREAM_CONFIG_T *stream)
{
    struct voice_detector_dev *pdev;

    if (id >= VOICE_DETECTOR_QTY) {
        VD_LOG("%s, invalid id=%d", __func__, id);
        return -1;
    }

    pdev = to_voice_dev(id);
    if (!pdev->init) {
        VD_LOG("%s, dev not open", __func__);
        return -2;
    }

    if (stream_id == AUD_STREAM_CAPTURE)
        memcpy(&pdev->cap_conf, stream, sizeof(struct AF_STREAM_CONFIG_T));
    else
        memcpy(&pdev->ply_conf, stream, sizeof(struct AF_STREAM_CONFIG_T));

    return 0;
}

int voice_detector_setup_callback(enum voice_detector_id id,
        enum voice_detector_cb_id func_id, voice_detector_cb_t func, void *param)
{
    struct voice_detector_dev *pdev;

    if (id >= VOICE_DETECTOR_QTY) {
        VD_LOG("%s, invalid id=%d", __func__, id);
        return -1;
    }

    pdev = to_voice_dev(id);
    if (!pdev->init) {
        VD_LOG("%s, dev not open", __func__);
        return -2;
    }

    if (func_id >= VOICE_DET_CB_QTY) {
        VD_LOG("%s, invalid func_id=%d", __func__, func_id);
        return -3;
    }
    pdev->arguments[func_id] = (uint32_t)param;
    pdev->callback[func_id]  = func;
    return 0;
}

int voice_detector_send_cmd(enum voice_detector_id id, enum voice_detector_cmd cmd)
{
    int r;
    struct voice_detector_dev *pdev;

    if (id >= VOICE_DETECTOR_QTY) {
        VD_LOG("%s, invalid id=%d", __func__, id);
        return -1;
    }
    pdev = to_voice_dev(id);

    r = cmd_queue_enqueue(&pdev->cmd_queue, (int)cmd);
    return r;
}

enum voice_detector_state voice_detector_query_status(enum voice_detector_id id)
{
    struct voice_detector_dev *pdev;

    if (id >= VOICE_DETECTOR_QTY) {
        return -1;
    }
    pdev = to_voice_dev(id);

    VD_LOG("%s, state=%d", __func__, pdev->state);
    return pdev->state;
}

static void voice_detector_vad_callback(void *data)
{
    int id = (int)(data);
    struct voice_detector_dev *pdev;

    if (id >= VOICE_DETECTOR_QTY) {
        VD_LOG("%s invalid ID %d", __func__, id);
        return;
    }
    pdev = to_voice_dev(id);

    pdev->wakeup_cnt++;
    VD_LOG("%s, voice detector[%d], wakeup_cnt=%d",
            __func__, id, pdev->wakeup_cnt);

    if (voice_detector_query_status(id) == VOICE_DET_STATE_VAD_START) {
        //TODO: copy data from VAD buffer
        app_voice_detector_send_event(id, VOICE_DET_EVT_AUD_CAP_START);
    }
}

static int voice_detector_vad_open(struct voice_detector_dev *pdev)
{
    struct codec_vad_conf *c = &pdev->conf;

    if (pdev->dfl) {
        VD_LOG("%s, use dfl vad", __func__);
    }
    hal_codec_vad_open();
    hal_codec_vad_config(c);

    return 0;
}

static int voice_detector_vad_start(struct voice_detector_dev *pdev)
{
    hal_codec_vad_start();
    return 0;
}

static int voice_detector_vad_stop(struct voice_detector_dev *pdev)
{
    hal_codec_vad_stop();
    return 0;
}

static int voice_detector_vad_close(struct voice_detector_dev *pdev)
{
    hal_codec_vad_close();
    return 0;
}

static int voice_detector_aud_cap_open(struct voice_detector_dev *pdev)
{
    struct AF_STREAM_CONFIG_T *conf = &pdev->cap_conf;

    if ((!conf->handler) || (!conf->data_ptr)) {
        VD_LOG("%s, capture stream is null", __func__);
        return -1;
    }
    app_sysfreq_req(APP_SYSFREQ_USER_APP_CYB, APP_SYSFREQ_104M);
    af_stream_open(AUD_STREAM_ID_0, AUD_STREAM_CAPTURE, conf);
    return 0;
}

static int voice_detector_aud_cap_start(struct voice_detector_dev *pdev)
{
    app_sysfreq_req(APP_SYSFREQ_USER_APP_CYB, APP_SYSFREQ_104M);
    af_stream_start(AUD_STREAM_ID_0, AUD_STREAM_CAPTURE);
    return 0;
}

static int voice_detector_aud_cap_stop(struct voice_detector_dev *pdev)
{
    af_stream_stop(AUD_STREAM_ID_0, AUD_STREAM_CAPTURE);
    app_sysfreq_req(APP_SYSFREQ_USER_APP_CYB, APP_SYSFREQ_32K);
    return 0;
}

static int voice_detector_aud_cap_close(struct voice_detector_dev *pdev)
{
    af_stream_close(AUD_STREAM_ID_0, AUD_STREAM_CAPTURE);
    return 0;
}

static int voice_detector_exit(struct voice_detector_dev *pdev)
{
    //TODO: exit process
    return 0;
}

static int voice_detector_process_cmd(struct voice_detector_dev *pdev, int cmd)
{
    int err = 0;

    switch(cmd) {
    case VOICE_DET_CMD_VAD_OPEN:
        VD_LOG("VOICE_DET_CMD_VAD_OPEN");
        err = voice_detector_vad_open(pdev);
        break;
    case VOICE_DET_CMD_VAD_START:
        VD_LOG("VOICE_DET_CMD_VAD_START");
        err = voice_detector_vad_start(pdev);
        break;
    case VOICE_DET_CMD_VAD_STOP:
        VD_LOG("VOICE_DET_CMD_VAD_STOP");
        err = voice_detector_vad_stop(pdev);
        break;
    case VOICE_DET_CMD_VAD_CLOSE:
        VD_LOG("VOICE_DET_CMD_VAD_CLOSE");
        err = voice_detector_vad_close(pdev);
        break;
    case VOICE_DET_CMD_AUD_CAP_OPEN:
        VD_LOG("VOICE_DET_CMD_AUD_CAP_OPEN");
        err = voice_detector_aud_cap_open(pdev);
        break;
    case VOICE_DET_CMD_AUD_CAP_START:
        VD_LOG("VOICE_DET_CMD_AUD_CAP_START");
        err = voice_detector_aud_cap_start(pdev);
        break;
    case VOICE_DET_CMD_AUD_CAP_STOP:
        VD_LOG("VOICE_DET_CMD_AUD_CAP_STOP");
        err = voice_detector_aud_cap_stop(pdev);
        break;
    case VOICE_DET_CMD_AUD_CAP_CLOSE:
        VD_LOG("VOICE_DET_CMD_AUD_CAP_CLOSE");
        err = voice_detector_aud_cap_close(pdev);
        break;
    case VOICE_DET_CMD_EXIT:
        VD_LOG("VOICE_DET_CMD_EXIT");
        err = voice_detector_exit(pdev);
        break;
    case VOICE_DET_CMD_IDLE:
        VD_LOG("VOICE_DET_CMD_IDLE");
        break;
    default:
        break;
    }
    return err;
}

static void voice_detector_set_status(struct voice_detector_dev *pdev, enum voice_detector_state s)
{
    pdev->state = s;
    VD_LOG("%s, state=%d", __func__, pdev->state);
}

static void voice_detector_exec_callback(struct voice_detector_dev *pdev,
                enum voice_detector_cb_id id)
{
    voice_detector_cb_t func;

    func = pdev->callback[id];
    if (func) {
        void *argv = (void *)(pdev->arguments[id]);

        func(pdev->state, argv);
    }
}

int voice_detector_run(enum voice_detector_id id, int continous)
{
    int exit = 0;
    int exit_code = 0;
    struct voice_detector_dev *pdev;

    if (id >= VOICE_DETECTOR_QTY) {
        return -1;
    }

    pdev = to_voice_dev(id);
    if (!pdev->init)
        return -2;

    voice_detector_exec_callback(pdev, VOICE_DET_CB_PREVIOUS);
    pdev->run = 1;
    while (1) {
        int err = 0;
        int cmd = cmd_queue_dequeue(&pdev->cmd_queue);

        if (cmd < 0) {
            // cmd is invalid, invoke waitting callback
            voice_detector_exec_callback(pdev, VOICE_DET_CB_RUN_WAIT);
        } else {
            // cmd is okay, process it
            err = voice_detector_process_cmd(pdev, cmd);
            if (!err) {
                voice_detector_set_status(pdev, (enum voice_detector_state)cmd);
            } else {
                voice_detector_exec_callback(pdev, VOICE_DET_CB_ERROR);
                VD_LOG("%s, process cmd %d error, %d", __func__, cmd, err);
            }
            voice_detector_exec_callback(pdev, VOICE_DET_CB_RUN_DONE);
        }
        switch(continous) {
        case 0: // not continous, run only once
            exit_code = err;
            exit = 1;
            break;
        case 1: // continous run,exit until cmd queue is empty
            if (cmd_queue_is_empty(&pdev->cmd_queue)) {
                exit = 1;
                exit_code = err;
            }
            break;
        case 2: //continous run forever, exit until receive VOICE_DET_CMD_EXIT
            if (cmd == VOICE_DET_CMD_EXIT) {
                exit = 1;
                exit_code = err;
            }
            break;
        default:
            break;
        }
        if (exit) {
            break;
        }
    }
    pdev->run = 0;
    voice_detector_exec_callback(pdev, VOICE_DET_CB_POST);

    return exit_code;
}
