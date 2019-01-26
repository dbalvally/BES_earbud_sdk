/***************************************************************************
 *
 * Copyright 2015-2019 BES.
 * All rights reserved. All unpublished rights reserved.
 *
 * No part of this work may be used or reproduced in any form or by any
 * means, or stored in a database or retrieval system, without prior written
 * permission of BES.
 *
 * Use of this work is governed by a license granted by BES.
 * This work contains confidential and proprietary information of
 * BES. which is protected by copyright, trade secret,
 * trademark and other intellectual property rights.
 *
 ****************************************************************************/
#ifndef NVRECORD_ENV_H
#define NVRECORD_ENV_H
#include "bluetooth.h"
#ifdef __cplusplus
extern "C" {
#endif

#define NVRAM_ENV_INVALID (0xdead0000)
#define NVRAM_ENV_MEDIA_LANGUAGE_DEFAULT (0)
#define NVRAM_ENV_STREAM_VOLUME_A2DP_VOL_DEFAULT (AUDIO_OUTPUT_VOLUME_DEFAULT)
#define NVRAM_ENV_STREAM_VOLUME_HFP_VOL_DEFAULT (AUDIO_OUTPUT_VOLUME_DEFAULT)
#define NVRAM_ENV_TWS_MODE_DEFAULT (0)
#define NVRAM_ENV_FACTORY_TESTER_STATUS_DEFAULT (0xaabbccdd)

struct media_language_t{
	int8_t language;
};

#ifdef __TWS__
struct tws_mode_t{
	uint16_t	originalMode;	// the TWS mode updated during pairing, 
								//	the value will be loaded to tws_mode_t.mode right after power-on
    uint16_t 	mode;			// the run-time TWS mode
	bt_bdaddr_t	masterAddr;
	bt_bdaddr_t	slaveAddr;
	bt_bdaddr_t	peerBleAddr;
};
#endif

struct factory_tester_status_t{
    uint32_t status;
};

struct nvrecord_env_t{
	struct media_language_t media_language;
#ifdef __TWS__	
	struct tws_mode_t tws_mode;
#endif
	struct factory_tester_status_t factory_tester_status;
};

extern struct nvrecord_env_t *nvrecord_env_p;

int nv_record_env_init(void);

int nv_record_env_get(struct nvrecord_env_t **nvrecord_env);

int nv_record_env_set(struct nvrecord_env_t *nvrecord_env);

uint32_t nv_record_get_tws_mode(void);

void nv_record_update_tws_mode(uint32_t newMode);

int nv_record_env_reset(void);
inline struct tws_mode_t* tws_mode_ptr(void)
{
    return &(nvrecord_env_p->tws_mode);
}

uint16_t current_tws_mode(void);

#ifdef __cplusplus
}
#endif
#endif
