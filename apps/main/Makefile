cur_dir := $(dir $(lastword $(MAKEFILE_LIST)))

obj-y := $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)*.c $(cur_dir)*.cpp $(cur_dir)*.S))

ifneq ($(wildcard $(cur_dir)../../config/$(T)/app_status_ind.c),)
obj-y += ../../config/$(T)/app_status_ind.c
else
obj-y += ../../config/$(DEFAULT_CFG_SRC)/app_status_ind.c
endif

obj-y := $(obj-y:.c=.o)
obj-y := $(obj-y:.cpp=.o)
obj-y := $(obj-y:.S=.o)

ccflags-y +=  \
    -Iservices/bt_app \
    -Iservices/audioflinger \
    -Iservices/nvrecord \
    -Iservices/nv_section/factory_section \
    -Iservices/overlay \
    -Iservices/resources \
    -Iservices/bt_if/inc \
    -Iapps/apptester \
    -Iapps/factory \
    -Iutils/crc32 \
    -Iutils/cqueue \
    -Iplatform/drivers/bt \
    -Iplatform/drivers/ana \
    -Iapps/audioplayers/rbplay \
    -Iplatform/hal \
    -Iservices/ble_stack/ble_ip \
    -Iservices/ble_stack/hl/api \
    -Iservices/ble_app/app_datapath \
    -Iservices/ble_app \
    -Iservices/halfduplexuart \
    -Iservices/ble_app/app_tws	\
    -Itests/anc_usb \
    -Iservices/tws/inc \
    -Iapps/anc/inc \
    -Iservices/voicepath/gsound/gsound_service \
    -Iservices/voicepath/gsound/gsound_target \
    -Iservices/voicepath/gsound \
    -Iservices/tota \
    -Iservices/nv_section/log_section
ifeq ($(VOICE_DATAPATH_ENABLED),1)
ccflags-y += -Iservices/voicepath/$(VOICE_DATAPATH_TYPE)
endif

ifeq ($(APP_TEST_AUDIO),1)
CFLAGS_apps_tester.o += -DAPP_TEST_AUDIO
endif

ifeq ($(ANC_APP),1)
CFLAGS_apps_tester.o += -DANC_APP
CFLAGS_apps.o += -DANC_APP
endif

ifeq ($(VOICE_PROMPT),1)
CFLAGS_apps.o += -DMEDIA_PLAYER_SUPPORT
endif

ifeq ($(ENGINEER_MODE),1)
CFLAGS_apps.o += -D__ENGINEER_MODE_SUPPORT__
endif

ifeq ($(MCU_HIGH_PERFORMANCE_MODE),1)
CFLAGS_apps.o += -DMCU_HIGH_PERFORMANCE_MODE
endif

ifeq ($(BT_ONE_BRING_TWO),1)
ccflags-y += -D__BT_ONE_BRING_TWO__
endif
