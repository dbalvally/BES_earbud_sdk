obj-y := audio_dump/ audioflinger/ bt_profiles/ fs/ bt_app/ overlay/ resources/ ../utils/crc32/ \
		../utils/heap/ bt_if/ osif/
ifeq ($(VOICE_DATAPATH_ENABLED),1)
obj-y += voicepath/
obj-y += voicepath/$(VOICE_DATAPATH_TYPE)/
endif
ifeq ($(BLE),1)
obj-y += ble_app/ bridge/ ble_stack/ ble_profiles/
endif


obj-y += nv_section/

ifeq ($(OPUS_ON),1)
obj-y += opus113/
endif
ifeq ($(AUTO_TEST),1)
obj-y += auto_test/
endif
ifneq ($(FPGA),1)
obj-y += nvrecord/
endif

ifneq ($(filter 1, $(HW_FIR_EQ_PROCESS) $(SW_IIR_EQ_PROCESS) $(HW_CODEC_IIR_EQ_PROCESS) $(AUDIO_DRC)),)
obj-y += audio_eq/
endif

ifeq ($(VOICE_RECOGNITION),1)
obj-y += voice_recognition/
endif


ifeq ($(TWS),1)
obj-y += tws/
endif

ifeq ($(APP_ANC_TEST),1)
obj-y += anc_test/
endif

ifeq ($(TEST_OVER_THE_AIR),1)
obj-y += tota/
endif

ifeq ($(HALFDUPLEXUART),1)
obj-y += halfduplexuart/
endif
