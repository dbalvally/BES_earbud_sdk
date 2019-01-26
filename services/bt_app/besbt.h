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
/***
 * besbt.h
 */

#ifndef BESBT_H
#define BESBT_H
#ifdef __cplusplus
extern "C" {
#endif

enum BESBT_HOOK_USER_T {
    BESBT_HOOK_USER_0 = 0,
    BESBT_HOOK_USER_1,
    BESBT_HOOK_USER_2,    
    BESBT_HOOK_USER_3,
    BESBT_HOOK_USER_QTY
};

typedef void (*BESBT_HOOK_HANDLER)(void);


void BesbtInit(void);
void BesbtThread(void const *argument);
int Besbt_hook_handler_set(enum BESBT_HOOK_USER_T user, BESBT_HOOK_HANDLER handler);

unsigned char *randaddrgen_get_bt_addr(void);
unsigned char *randaddrgen_get_ble_addr(void);
const char *randaddrgen_get_btd_localname(void);
#ifdef __cplusplus
}
#endif
#endif /* BESBT_H */
