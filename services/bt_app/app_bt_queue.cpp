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
#include <stdio.h>
#include "cmsis_os.h"
#include "hal_trace.h"
#include "rtos.h"
#include "besbt.h"
#include "cqueue.h"
#include "app_bt_queue.h"
#include "os_api.h"

void app_bt_queue_init(CQueue* ptrQueue, uint8_t* ptrBuf, uint32_t bufLen)
{
    memset(ptrBuf, 0, bufLen);
	InitCQueue(ptrQueue, bufLen, (CQItemType *)ptrBuf);
}

void app_bt_queue_push(CQueue* ptrQueue, uint8_t* ptrData, uint32_t length)
{
    if (length > 0)
    {
        uint16_t dataLen = (uint16_t)length;
        EnCQueue(ptrQueue, (uint8_t *)&dataLen, 2);
        EnCQueue(ptrQueue, ptrData, length);
        osapi_notify_evm();
    }
}

uint16_t app_bt_queue_get_next_entry_length(CQueue* ptrQueue)
{
    uint8_t *e1 = NULL, *e2 = NULL;
    unsigned int len1 = 0, len2 = 0;
    uint16_t length = 0;
    uint8_t* ptr = (uint8_t *)&length;
    
    // get the length of the fake message
    PeekCQueue(ptrQueue, 2, &e1, &len1, &e2, &len2);

    memcpy(ptr,e1,len1);
    memcpy(ptr+len1,e2,len2);

    return length;
}

void app_bt_queue_pop(CQueue* ptrQueue, uint8_t *buff, uint32_t len)
{
    uint8_t *e1 = NULL, *e2 = NULL;
    unsigned int len1 = 0, len2 = 0;

    // overcome the two bytes of msg length
    DeCQueue(ptrQueue, 0, 2);
    
    PeekCQueue(ptrQueue, len, &e1, &len1, &e2, &len2);
    if (len==(len1+len2)){
        memcpy(buff,e1,len1);
        memcpy(buff+len1,e2,len2);
        DeCQueue(ptrQueue, 0, len);

        // reset the poped data to ZERO
        memset(e1, 0, len1);
        memset(e2, 0, len2);
    }else{
        memset(buff, 0x00, len);
    }
}

