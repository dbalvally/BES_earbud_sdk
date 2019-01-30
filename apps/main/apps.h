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
#ifndef __APPS_H__
#define __APPS_H__

#include "app_status_ind.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "plat_types.h"
#include "bluetooth.h"

enum APP_SYSTEM_STATUS_T {
    APP_SYSTEM_STATUS_INVALID = 0,
    APP_SYSTEM_STATUS_NORMAL_OP,
    APP_SYSTEM_STATUS_POWERON_PROC,
    APP_SYSTEM_STATUS_POWEROFF_PROC,
};

int app_init(void);

int app_deinit(int deinit_case);

int app_shutdown(void);

int app_reset(void);

uint8_t app_system_status_get(void);

uint8_t app_system_status_set(int flag);

int app_status_battery_report(uint8_t level);

int app_voice_report( APP_STATUS_INDICATION_T status,uint8_t device_id);

/*FixME*/
void app_status_set_num(const char* p);

////////////10 second tiemr///////////////
enum APP_10_SECOND_TIMER_T{
	APP_PAIR_TIMER_ID,
	APP_POWEROFF_TIMER_ID,
	APP_RECONNECT_TIMER_ID,
	APP_10_SECOND_TIMER_TOTAL_NUM,
};
void app_stop_10_second_timer(uint8_t timer_id);
void app_start_10_second_timer(uint8_t timer_id);

void app_start_auto_power_off_supervising(void);
void app_stop_auto_power_off_supervising(void);
void app_refresh_random_seed(void);

void app_start_postponed_reset(void);

////////////////////


#ifdef __cplusplus
}
#endif
#endif//__FMDEC_H__
