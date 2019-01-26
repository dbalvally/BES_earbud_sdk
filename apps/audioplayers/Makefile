
cur_dir := $(dir $(lastword $(MAKEFILE_LIST)))

obj_s := $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)*.s))
obj_c := $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)*.c))
obj_cpp := $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)*.cpp))

ifeq ($(RB_CODEC),1)
obj_cpp += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)rbplay/*.cpp))
endif

obj-y := $(obj_c:.c=.o) $(obj_s:.S=.o) $(obj_cpp:.cpp=.o)

CFLAGS_a2dpplay.o += -O3
CFLAGS_voicebtpcmplay.o += -O3

ccflags-y := \
	-Iservices/audio_eq \
	-Iservices/fs/fat \
	-Iservices/fs/sd \
	-Iservices/resources \
	-Iservices/fs/fat/ChaN \
	-Iservices/bt_app \
	-Iservices/tws/inc \
	-Iservices/bt_if/inc \
	-Iplatform/drivers/uarthci \
	-Iutils/cqueue \
	-Iservices/audio_dump/include \
	-Iservices/multimedia/speech/inc \
	-Iservices/multimedia/rbcodec/inc \
	-Iservices/multimedia/audio/process/eq/include \
	-Iservices/multimedia/audio/process/resample/include \
	-Iservices/multimedia/audio/process/filters/include \
	-Iservices/multimedia/fm/inc \
	-Iservices/overlay \
	-Iservices/nvrecord \
	-Iapps/main \
	-Iapps/audioplayers/rbplay/ \
	-Iutils/list \
	-Iutils/heap \
	-Iplatform/drivers/ana \
	-Iplatform/drivers/codec/best2000/ \
	-Iservices/multimedia/rbcodec/codecs/libhelixaac/ \
	-Iapps/apptester \
	-Ithirdparty/audio_codec_lib/liblhdc-dec/inc \
	-Iapps/key
ifeq ($(APP_TEST_AUDIO),1)
CFLAGS_app_audio.o += -DAPP_TEST_AUDIO
endif

ifeq ($(AUDIO_RESAMPLE),1)
CFLAGS_a2dpplay.o += -D__AUDIO_RESAMPLE__
CFLAGS_voicebtpcmplay.o += -D__AUDIO_RESAMPLE__
endif
ifeq ($(SW_PLAYBACK_RESAMPLE),1)
CFLAGS_a2dpplay.o += -DSW_PLAYBACK_RESAMPLE
endif
ifeq ($(RESAMPLE_ANY_SAMPLE_RATE),1)
CFLAGS_a2dpplay.o += -DRESAMPLE_ANY_SAMPLE_RATE
endif
ifeq ($(SW_SCO_RESAMPLE),1)
CFLAGS_voicebtpcmplay.o += -DSW_SCO_RESAMPLE
endif

ifeq ($(SPEECH_TX_AEC),1)
CFLAGS_voicebtpcmplay.o += -DSPEECH_TX_AEC
endif

ifeq ($(SPEECH_TX_NS),1)
CFLAGS_voicebtpcmplay.o += -DSPEECH_TX_NS
endif

ifeq ($(SPEECH_TX_NS2),1)
CFLAGS_voicebtpcmplay.o += -DSPEECH_TX_NS2
endif

ifeq ($(SPEECH_TX_2MIC_NS2),1)
CFLAGS_voicebtpcmplay.o += -DSPEECH_TX_2MIC_NS2
endif

ifeq ($(SPEECH_RX_PLC),1)
CFLAGS_voicebtpcmplay.o += -DSPEECH_RX_PLC
endif

ifeq ($(SPEECH_TX_EQ),1)
CFLAGS_voicebtpcmplay.o += -DSPEECH_TX_EQ
endif

ifeq ($(SPEECH_RX_NS),1)
CFLAGS_voicebtpcmplay.o += -DSPEECH_RX_NS
CFLAGS_app_audio.o += -DSPEECH_RX_NS
endif

ifeq ($(SPEECH_RX_NS2),1)
CFLAGS_voicebtpcmplay.o += -DSPEECH_RX_NS2
endif

ifeq ($(SPEECH_RX_EQ),1)
CFLAGS_voicebtpcmplay.o += -DSPEECH_RX_EQ
endif

ifeq ($(VOICE_PROMPT),1)
CFLAGS_app_audio.o += -DMEDIA_PLAYER_SUPPORT
endif

ifeq ($(AUDIO_QUEUE_SUPPORT),1)
CFLAGS_app_audio.o += -D__AUDIO_QUEUE_SUPPORT__
endif

ifeq ($(SPEECH_TX_AEC2),1)
ccflags-y += -DVOS_ARM -D_MED_C89_ -DUSE_DSPFNS -DFIXED_POINT -DUSE_KISS_FFT
endif

ifeq ($(ANC_APP),1)
CFLAGS_app_audio.o += -DANC_APP
endif

