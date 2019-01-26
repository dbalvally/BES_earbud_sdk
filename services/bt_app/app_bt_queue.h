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
#ifndef __APP_BT_QUEUE_H__
#define __APP_BT_QUEUE_H__

void app_bt_queue_init(CQueue* ptrQueue, uint8_t* ptrBuf, uint32_t bufLen);
void app_bt_queue_push(CQueue* ptrQueue, uint8_t* ptrData, uint32_t length);
uint16_t app_bt_queue_get_next_entry_length(CQueue* ptrQueue);
void app_bt_queue_pop(CQueue* ptrQueue, uint8_t *ptrBuf, uint32_t len);


#endif	// __APP_BT_QUEUE_H__

