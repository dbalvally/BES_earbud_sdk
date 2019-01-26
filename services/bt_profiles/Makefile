
cur_dir := $(dir $(lastword $(MAKEFILE_LIST)))

obj_s := $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)*.s))
obj_c := $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)hcitrans/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)hcitrans/modinit/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)besport/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)profiles/a2dp/*.c))

ifeq ($(OPUS_ON),1)
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)profiles/a2dp/opus113/src/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)profiles/a2dp/opus113/celt/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)profiles/a2dp/opus113/celt/fixed/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)profiles/a2dp/opus113/silk/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)profiles/a2dp/opus113/silk/fixed/*.c))
endif

obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)profiles/avctp/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)profiles/avdtp/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)profiles/avrcp/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)profiles/common/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)profiles/hf/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)stack/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)stack/hci/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)stack/l2cap/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)stack/me/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)stack/rfcomm/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)stack/sdp/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)profiles/hid/*.c))

obj_cpp := $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)*.cpp))




BTPROFILE_LIB_NAME := libbt_profiles


ifeq ($(CHIP),best2000)
BTPROFILE_LIB_NAME := best2000_$(BTPROFILE_LIB_NAME)
endif

ifeq ($(CHIP),best2300)
BTPROFILE_LIB_NAME := best2300_$(BTPROFILE_LIB_NAME)
endif

ifeq ($(OPUS_ON),1)
BTPROFILE_LIB_NAME := $(BTPROFILE_LIB_NAME)_opus
endif

ifeq ($(HFP_1_6_ENABLE), 1)
BTPROFILE_LIB_NAME := $(BTPROFILE_LIB_NAME)_sbc_enc
endif
 
ifeq ($(SBC_FUNC_IN_ROM), 1)
BTPROFILE_LIB_NAME := $(BTPROFILE_LIB_NAME)_sbc_in_rom
endif

ifeq ($(A2DP_AAC_ON), 1)
BTPROFILE_LIB_NAME := $(BTPROFILE_LIB_NAME)_aac
endif

ifeq ($(SBC_TRANS_TO_AAC), 1)
BTPROFILE_LIB_NAME := $(BTPROFILE_LIB_NAME)_sbc2aac
endif

ifeq ($(A2DP_LHDC_ON), 1)
BTPROFILE_LIB_NAME := $(BTPROFILE_LIB_NAME)_lhdc
endif

ifeq ($(BT_RF_PREFER), 2M)
BTPROFILE_LIB_NAME := $(BTPROFILE_LIB_NAME)_2m
endif

ifeq ($(BT_RF_PREFER), 3M)
BTPROFILE_LIB_NAME := $(BTPROFILE_LIB_NAME)_3m
endif

ifeq ($(TWS_ROLE_SWITCH), 1)
BTPROFILE_LIB_NAME := $(BTPROFILE_LIB_NAME)_tws_role_switch
endif

ifeq ($(TWS_USE_PLL_SYNC), 1)
BTPROFILE_LIB_NAME := $(BTPROFILE_LIB_NAME)_pllsync
endif

$(BTPROFILE_LIB_NAME)-y := $(obj_c:.c=.o) $(obj_s:.S=.o) $(obj_cpp:.cpp=.o)

obj-y += $(BTPROFILE_LIB_NAME).a

CFLAGS_a2alloc.o += -O3
CFLAGS_a2dp.o += -O3
CFLAGS_a2dp_sdp.o += -O3
CFLAGS_a2dp_sm.o += -O3
CFLAGS_sbc.o += -O3
CFLAGS_sbc_math.o += -O3

subdir-ccflags-y += \
    -Iservices/fs/fat \
    -Iservices/fs/sd \
    -Iservices/fs/fat/ChaN \
    -Iservices/bt_profiles/inc \
    -Iservices/bt_if/inc \
    -Iplatform/drivers/uarthci \
    -Iplatform/drivers/ana \
    -Iplatform/drivers/bt \
    -Itests/rom \
    -Iutils/cqueue \
    -Iutils/heap \
    -Iservices/bt_app \
    -Iservices/tws/inc \
    -Iservices/audioflinger \
    -Iservices/nvrecord \
    -Iservices/osif \
    -Iutils/lockcqueue \
    -Iapps/key \
	$(OPUS_INCLUDES) \
    -Iapps/audioplayers

