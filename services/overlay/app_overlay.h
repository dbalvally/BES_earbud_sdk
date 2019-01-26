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
#ifndef __APP_OVERLAY_H__
#define __APP_OVERLAY_H__

#include "hal_overlay.h"

#ifdef __cplusplus
extern "C" {
#endif

#define app_overlay_load(id) hal_overlay_load((enum HAL_OVERLAY_ID_T)id)
#define app_overlay_unload(id) hal_overlay_unload((enum HAL_OVERLAY_ID_T)id)

enum APP_OVERLAY_ID_T {
	APP_OVERLAY_HFP = HAL_OVERLAY_ID_0,
	APP_OVERLAY_A2DP = HAL_OVERLAY_ID_1,
	APP_OVERLAY_FM = HAL_OVERLAY_ID_2,
	APP_OVERLAY_AAC = HAL_OVERLAY_ID_3,
	APP_OVERLAY_A2DP_AAC = HAL_OVERLAY_ID_4,
	APP_OVERLAY_MPA = HAL_OVERLAY_ID_5,
#if defined(A2DP_LHDC_ON)
	APP_OVERLAY_A2DP_LHDC= HAL_OVERLAY_ID_6,
#else
	APP_OVERLAY_WAV = HAL_OVERLAY_ID_6,
#endif
	APP_OVERLAY_FLAC = HAL_OVERLAY_ID_7,
	                      
	APP_OVERLAY_ID_QTY = HAL_OVERLAY_ID_QTY,
	APP_OVERLAY_ID_IN_CFG = HAL_OVERLAY_ID_IN_CFG,
};

void app_overlay_select(enum APP_OVERLAY_ID_T id);

enum APP_OVERLAY_ID_T app_overlay_getload(void);


void app_overlay_unloadall(void);

void app_overlay_open(void);

void app_overlay_close(void);

#ifdef __cplusplus
}
#endif

#endif
