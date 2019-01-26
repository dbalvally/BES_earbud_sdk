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
#ifndef __PLAYER_MODE_MACHINE_H__
#define __PLAYER_MODE_MACHINE_H__

enum _MUSIC_BOX_PLAYER_ROLE {
    PLAYER_ROLE_SD = 0x00,
    PLAYER_ROLE_LINEIN = 0x01,
    PLAYER_ROLE_USB = 0x02,
    PLAYER_ROLE_TWS = 0xff,
};

void app_set_play_role(int role);
int    app_get_play_role(void) ;
void app_switch_to_tws(void);
void app_switch_player_role_key(APP_KEY_STATUS *status, void *param);
#endif

