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
#ifndef __FMDEC_H__
#define __FMDEC_H__

#ifdef __cplusplus
extern "C" {
#endif

#define ATAN2_HARDWARE 1
#define FM_NEWMODE 1
//#undef FM_NEWMODE

#define NUMOFSAMPLE 576





#define FM_FLOAT 1
#define FM_FIXED 2
#define FM_FIXED16BIT 3

#define FM_MATH  FM_FIXED16BIT

int FmDemodulate(short *Input, short *output, int NumSample);

#ifdef __cplusplus
}
#endif

#endif//__FMDEC_H__
