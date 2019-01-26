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
#ifndef __APP_HFP_H__
#define __APP_HFP_H__

#ifdef __cplusplus
extern "C" {
#endif

#if defined(SUPPORT_BATTERY_REPORT) || defined(SUPPORT_HF_INDICATORS)
int app_hfp_battery_report(uint8_t level);
#else
static inline int app_hfp_battery_report(uint8_t level) {return 0;}
#endif

hfp_sco_codec_t bt_sco_get_current_codecid(void);
void bt_sco_set_current_codecid(hfp_sco_codec_t id);
void btapp_hfp_report_speak_gain(void);
bt_status_t app_bt_disconnect_hfp(void);
bool btapp_hfp_call_is_active(void);
bool btapp_hfp_call_is_present(void);
bool btapp_hfp_is_voice_ongoing(void);
int hfp_handle_add_to_earphone_1_6(hf_chan_handle_t chan);

#ifdef __cplusplus
}
#endif

#endif /*__APP_HFP_H__*/

