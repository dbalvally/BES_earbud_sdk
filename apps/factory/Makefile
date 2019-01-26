cur_dir := $(dir $(lastword $(MAKEFILE_LIST)))

obj-y := $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)*.c $(cur_dir)*.cpp $(cur_dir)*.S))
obj-y := $(obj-y:.c=.o)
obj-y := $(obj-y:.cpp=.o)
obj-y := $(obj-y:.S=.o)

ccflags-y +=  \
    -Iutils/boot_struct \
    -Iutils/intersyshci \
    -Iservices/bt_app \
    -Iservices/nvrecord \
    -Iservices/nv_section/factory_section \
    -Iservices/bt_if/inc \
    -Iservices/resources \
    -Iservices/overlay \
    -Iservices/multimedia/algorithm/fft/include \
    -Iapps/key \
    -Iplatform/drivers/bt \
    -Iplatform/drivers/ana \
    -Iutils/cqueue

ifeq ($(AUDIO_RESAMPLE),1)
CFLAGS_app_factory_audio.o += -D__AUDIO_RESAMPLE__
endif
ifeq ($(SW_CAPTURE_RESAMPLE),1)
CFLAGS_app_factory_audio.o += -DSW_CAPTURE_RESAMPLE
endif

ifeq ($(POWERKEY_I2C_SWITCH),1)
CFLAGS_app_factory.o += -DPOWERKEY_I2C_SWITCH
endif

