cur_dir := $(dir $(lastword $(MAKEFILE_LIST)))

obj-y := $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)*.c $(cur_dir)*.cpp $(cur_dir)*.S))
obj-y := $(obj-y:.c=.o)
obj-y := $(obj-y:.cpp=.o)
obj-y := $(obj-y:.S=.o)

ccflags-y += -Iutils/hexdump    \
        -Iapps/key  \
        -Iservices/bt_app   \
        -Iservices/bt_if/inc  \
        -Iutils/crc32   \
        -Iplatform/drivers/ana \
        -Iservices/nv_section/customparam_section \
        -Iplatform/drivers/norflash \
		-Iservices/tws/inc \
		-Iutils/cqueue \
		-Iutils/heap \
		-Iservices/bt_app \
		-Iapps/audioplayers

CFLAGS_nvrecord.o += -DAUDIO_OUTPUT_VOLUME_DEFAULT=$(AUDIO_OUTPUT_VOLUME_DEFAULT)

