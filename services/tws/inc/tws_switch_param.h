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
#ifndef __TWS_SWITCH_PARAM_H__
#define __TWS_SWITCH_PARAM_H__

uint32_t tws_switch_send_bt_me_data(uint8_t *send_data_buf);

uint32_t tws_switch_reset_bt_me_data(uint8_t *send_data_buf);

uint32_t tws_switch_send_bt_hci_data(uint8_t *send_data_buf);

uint32_t tws_switch_reset_bt_hci_data(uint8_t *send_data_buf);

uint32_t tws_switch_send_bt_l2cap_data(uint8_t *send_data_buf);

uint32_t tws_switch_reset_bt_l2cap_data(uint8_t *send_data_buf);

uint32_t tws_switch_send_bt_rfc_data(uint8_t *send_data_buf);

uint32_t tws_switch_reset_bt_rfc_data(uint8_t *send_data_buf);

uint32_t tws_switch_send_avdev_data(uint8_t *send_data_buf);

uint32_t tws_switch_reset_avdev_data(uint8_t *send_data_buf);

uint32_t tws_switch_send_app_bt_device_data(uint8_t *send_data_buf);

uint32_t tws_switch_reset_app_bt_device_data(uint8_t *send_data_buf);

void tws_reset_app_bt_device_streaming(void);

uint32_t tws_switch_send_cmgrcontext_data(uint8_t *send_data_buf);

void tws_switch_reset_cmgrcontext_data(uint8_t *send_data_buf);

#endif /*__TWS_SWITCH_PARAM_H__*/
