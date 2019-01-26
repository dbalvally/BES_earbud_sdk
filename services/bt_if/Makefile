cur_dir := $(dir $(lastword $(MAKEFILE_LIST)))

src_obj := $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)*.c $(cur_dir)*.S))
src_obj += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)*.cpp))
src_obj += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)a2dp/*.cpp ))
src_obj += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)avdtp/*.cpp ))
src_obj += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)avctp/*.cpp ))
src_obj += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)avrcp/*.cpp ))
src_obj += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)conmgr/*.cpp ))
src_obj += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)hci/*.cpp ))
src_obj += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)l2cap/*.cpp ))
src_obj += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)me/*.cpp ))
src_obj += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)rfcom/*.cpp ))
src_obj += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)sco/*.cpp ))
src_obj += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)sdp/*.cpp ))
src_obj += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)spp/*.c ))
src_obj += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)hfp/*.cpp ))
src_obj += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)os/*.cpp ))
src_obj += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)rfcomm/*.cpp ))
src_obj += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)sync/*.c ))
src_obj += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)at/*.c ))

src_obj := $(src_obj:.c=.o)
src_obj := $(src_obj:.cpp=.o)
src_obj := $(src_obj:.S=.o)

BTAPI_LIB_NAME := libbt_api


ifeq ($(CHIP),best2000)
BTAPI_LIB_NAME := best2000_$(BTAPI_LIB_NAME)
endif

ifeq ($(CHIP),best2300)
BTAPI_LIB_NAME := best2300_$(BTAPI_LIB_NAME)
endif

ifeq ($(OPUS_ON),1)
BTAPI_LIB_NAME := $(BTAPI_LIB_NAME)_opus
endif

ifeq ($(HFP_1_6_ENABLE), 1)
BTAPI_LIB_NAME := $(BTAPI_LIB_NAME)_sbc_enc
endif

ifeq ($(SBC_FUNC_IN_ROM), 1)
BTAPI_LIB_NAME := $(BTAPI_LIB_NAME)_sbc_in_rom
endif

ifeq ($(A2DP_AAC_ON), 1)
BTAPI_LIB_NAME := $(BTAPI_LIB_NAME)_aac
endif

ifeq ($(SBC_TRANS_TO_AAC), 1)
BTAPI_LIB_NAME := $(BTAPI_LIB_NAME)_sbc2aac
endif

ifeq ($(A2DP_LHDC_ON), 1)
BTAPI_LIB_NAME := $(BTAPI_LIB_NAME)_lhdc
endif

ifeq ($(BT_RF_PREFER), 2M)
BTAPI_LIB_NAME := $(BTAPI_LIB_NAME)_2m
endif

ifeq ($(BT_RF_PREFER), 3M)
BTAPI_LIB_NAME := $(BTAPI_LIB_NAME)_3m
endif

ifeq ($(TWS_ROLE_SWITCH), 1)
BTAPI_LIB_NAME := $(BTAPI_LIB_NAME)_tws_role_switch
endif

ifeq ($(TWS_USE_PLL_SYNC), 1)
BTAPI_LIB_NAME := $(BTAPI_LIB_NAME)_pllsync
endif

$(BTAPI_LIB_NAME)-y := $(src_obj)

obj-y += $(BTAPI_LIB_NAME).a


ccflags-y += \
	-Iservices/bt_if/inc \
	-Iservices/audio_eq \
	-Iservices/bt_profiles/inc \
	-Iservices/bt_profiles/inc/sys \
	-Iservices/overlay \
	-Iservices/nvrecord \
	-Iutils/cqueue \
	-Iutils/heap \
	-Iservices/audioflinger \
	-Iutils/lockcqueue \
	-Iservices/ble_app \
	-Iservices/ble_stack/common/api/ \
	-Iservices/tws/inc \
    -Iservices/bt_app \
    -Iservices/tws/inc \
    -Iservices/resources/ \
	-Iapps/audioplayers \
	-Iapps/main/ \
	-Iplatform/drivers/ana/ \
	-Iplatform/drivers/bt/ \
	-Iutils/intersyshci

