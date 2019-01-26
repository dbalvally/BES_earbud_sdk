#ifndef __APP_VOICE_DETECTOR_H__
#define __APP_VOICE_DETECTOR_H__
#include "hal_aud.h"
#include "audioflinger.h"
#include "voice_detector.h"

#ifdef __cplusplus
extern "C" {
#endif

enum voice_detector_evt {
	VOICE_DET_EVT_OPEN,
	VOICE_DET_EVT_VAD_START,
	VOICE_DET_EVT_AUD_CAP_START,
	VOICE_DET_EVT_CLOSE,
};

void app_voice_detector_init();

int app_voice_detector_open(enum voice_detector_id id);

int app_voice_detector_setup_vad(enum voice_detector_id id,
                                struct codec_vad_conf *conf);

int app_voice_detector_setup_stream(enum voice_detector_id id,
                                    enum AUD_STREAM_T stream_id,
                                    struct AF_STREAM_CONFIG_T *stream);

int app_voice_detector_setup_callback(enum voice_detector_id id,
                                      enum voice_detector_cb_id func_id,
                                      voice_detector_cb_t func,
                                      void *param);

int app_voice_detector_send_event(enum voice_detector_id id,
                                enum voice_detector_evt evt);

void app_voice_detector_close(enum voice_detector_id id);

#ifdef __cplusplus
}
#endif

#endif /* __APP_VOICE_DETECTOR_H__ */
