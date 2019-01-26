
cur_dir := $(dir $(lastword $(MAKEFILE_LIST)))

obj_s := $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)src/*.s))
obj_c := $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)src/*.c))

obj_cpp := $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)src/*.cpp))

src_obj := $(obj_c:.c=.o) $(obj_s:.S=.o) $(obj_cpp:.cpp=.o)

TWS_LIB_NAME := libtws

ifeq ($(CHIP),best2000)
TWS_LIB_NAME := best2000_$(TWS_LIB_NAME)
endif

ifeq ($(CHIP),best2300)
TWS_LIB_NAME := best2300_$(TWS_LIB_NAME)
endif

ifeq ($(OPUS_ON),1)
TWS_LIB_NAME := $(TWS_LIB_NAME)_opus
endif

ifeq ($(SBC_FUNC_IN_ROM), 1)
TWS_LIB_NAME := $(TWS_LIB_NAME)_sbc_in_rom
endif

ifeq ($(A2DP_AAC_ON), 1)
TWS_LIB_NAME := $(TWS_LIB_NAME)_aac
endif

ifeq ($(SBC_TRANS_TO_AAC), 1)
TWS_LIB_NAME := $(TWS_LIB_NAME)_sbc2aac
endif

ifeq ($(A2DP_LHDC_ON), 1)
TWS_LIB_NAME := $(TWS_LIB_NAME)_lhdc
endif

ifeq ($(BT_RF_PREFER), 2M)
TWS_LIB_NAME := $(TWS_LIB_NAME)_2m
endif

ifeq ($(BT_RF_PREFER), 3M)
TWS_LIB_NAME := $(TWS_LIB_NAME)_3m
endif

ifeq ($(TWS_ALL_SIDE_STEREO), 1)
TWS_LIB_NAME := $(TWS_LIB_NAME)_ass
endif

ifeq ($(TWS_ROLE_SWITCH), 1)
TWS_LIB_NAME := $(TWS_LIB_NAME)_tws_role_switch
endif

ifeq ($(TWS_USE_PLL_SYNC), 1)
TWS_LIB_NAME := $(TWS_LIB_NAME)_pllsync
endif


$(TWS_LIB_NAME)-y := $(src_obj)

obj-y :=  $(TWS_LIB_NAME).a


ifeq ($(OPUS_ON),1)
subdir-ccflags-y += -O3 -DARM_MATH_CM4 -DOPUS_HAVE_CONFIG_H
OPUS_INCLUDES = \
	-Iservices/opus113/celt \
	-Iservices/opus113/celt/fixed \
	-Iservices/opus113/silk \
	-Iservices/opus113/silk/fixed \
	-Iservices/opus113/include \
	-Iservices/opus113/src \
else
OPUS_INCLUDES = 
endif

ifeq ($(A2DP_AAC_ON),1)
AAC_INCLUDES =  \
	-Iservices/multimedia/audio/codec/fdkaac_codec/libAACdec/include \
	-Iservices/multimedia/audio/codec/fdkaac_codec/libAACenc/include \
	-Iservices/multimedia/audio/codec/fdkaac_codec/libFDK/include \
	-Iservices/multimedia/audio/codec/fdkaac_codec/libMpegTPDec/include \
	-Iservices/multimedia/audio/codec/fdkaac_codec/libMpegTPEnc/include \
	-Iservices/multimedia/audio/codec/fdkaac_codec/libPCMutils/include \
	-Iservices/multimedia/audio/codec/fdkaac_codec/libSBRdec/include \
	-Iservices/multimedia/audio/codec/fdkaac_codec/libSBRenc/include \
	-Iservices/multimedia/audio/codec/fdkaac_codec/libSYS/include
else
AAC_INCLUDES =
endif

subdir-ccflags-y += \
	-Iservices/fs/fat \
	-Iservices/fs/sd \
	-Iservices/fs/fat/ChaN \
	-Iservices/overlay \
	-Iservices/nvrecord \
	-Iservices/resources \
	-Iservices/multimedia/audio/process/resample/include \
	-Iservices/bt_if/inc \
	-Iplatform/drivers/uarthci \
	-Iplatform/drivers/ana \
	-Iplatform/drivers/bt \
	-Iutils/cqueue \
	-Iutils/heap \
	-Iservices/audioflinger \
	-Iutils/lockcqueue \
	-Iutils/intersyshci \
	-Iapps/key \
	-Iapps/main \
	-Iapps/common \
	-Iapps/audioplayers \
	-Iapps/common \
	-Iapps/factory \
	-Iservices/resources \
	-Iservices/ble_app \
	-Iservices/bt_app \
	-Iservices/tws/inc \
	$(OPUS_INCLUDES) \
	$(AAC_INCLUDES) \
     	-Iutils/hwtimer_list \
     	-Ithirdparty/audio_codec_lib/liblhdc-dec/inc \
     	-Iservices/bt_profiles/inc/sys \
	-Iservices/ble_stack/common/api \
	-Iservices/ble_app/app_tws	\
	-Iservices/bt_profiles/inc

KBUILD_CPPFLAGS += -DAUDIO_OUTPUT_VOLUME_DEFAULT=$(AUDIO_OUTPUT_VOLUME_DEFAULT)

ifeq ($(AUDIO_RESAMPLE),1)
CFLAGS_app_tws.o += -D__AUDIO_RESAMPLE__
endif
ifeq ($(SW_PLAYBACK_RESAMPLE),1)
CFLAGS_app_tws.o += -DSW_PLAYBACK_RESAMPLE
endif
ifeq ($(SW_CAPTURE_RESAMPLE),1)
CFLAGS_app_tws.o += -DSW_CAPTURE_RESAMPLE
endif
ifeq ($(RESAMPLE_ANY_SAMPLE_RATE),1)
CFLAGS_app_tws.o += -DRESAMPLE_ANY_SAMPLE_RATE
endif
