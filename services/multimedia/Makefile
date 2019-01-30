
cur_dir := $(dir $(lastword $(MAKEFILE_LIST)))

mmlib_obj :=

mmlib_obj += algorithm/fft/src/

mmlib_obj += audio/process/adp/src/
mmlib_obj += audio/process/anc/src/
mmlib_obj += audio/process/filters/src/
mmlib_obj += audio/process/resample/src/

ifeq ($(FM_ENABLE),1)
mmlib_obj += fm/
endif

ifeq ($(OPUS_CODEC),1)
mmlib_obj += opus121/src/
endif

ifeq ($(A2DP_AAC_ON),1)
mmlib_obj += audio/codec/fdkaac_codec/libAACdec/src/
mmlib_obj += audio/codec/fdkaac_codec/libAACenc/src/
mmlib_obj += audio/codec/fdkaac_codec/libFDK/src/
mmlib_obj += audio/codec/fdkaac_codec/libMpegTPDec/src/
mmlib_obj += audio/codec/fdkaac_codec/libMpegTPEnc/src/
mmlib_obj += audio/codec/fdkaac_codec/libPCMutils/src/
mmlib_obj += audio/codec/fdkaac_codec/libSBRdec/src/
mmlib_obj += audio/codec/fdkaac_codec/libSBRenc/src/
mmlib_obj += audio/codec/fdkaac_codec/libSYS/src/
endif

mmlib_obj += speech/src/

MULTIMEDIA_LIB_NAME := libmultimedia

ifeq ($(CHIP),best2000)
MULTIMEDIA_LIB_NAME := best2000_$(MULTIMEDIA_LIB_NAME)
endif

ifeq ($(CHIP),best2300)
MULTIMEDIA_LIB_NAME := best2300_$(MULTIMEDIA_LIB_NAME)
endif

$(MULTIMEDIA_LIB_NAME)-y := $(mmlib_obj)

rel_src_obj :=
rel_src_obj += audio/process/anc/cfg/
rel_src_obj += audio/process/filters/cfg/
rel_src_obj += audio/process/resample/coef/

obj-y := $(MULTIMEDIA_LIB_NAME).a $(rel_src_obj)

subdir-ccflags-y := \
	-Iutils/heap \

