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
#ifndef __SPEECH_DRC_H__
#define __SPEECH_DRC_H__

struct speech_drc_cfg
{
    int ch_num;             //channel number
    int fs;                 //frequency
    int depth;              //audio depth
    int frame_size;         //if change this only, do not need call  drc_setup
    int threshold;
    int makeup_gain;
    int ratio;
    int knee;
    int release_time;
    int attack_time;
};

#ifdef __cplusplus
extern "C" {
#endif

void speech_drc_init(struct speech_drc_cfg *cfg);
void speech_drc_process_fix(short *buf_p_l, short *buf_p_r);


#ifdef __cplusplus
}
#endif

#endif
