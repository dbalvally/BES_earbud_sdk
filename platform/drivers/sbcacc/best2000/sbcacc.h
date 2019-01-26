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
#ifndef __SBCACC_HEADER__
#define __SBCACC_HEADER__

#ifdef __cplusplus
extern "C" {
#endif

int Synthesis1_init(const char *coef, int coef_len, int channel_num);
int Synthesis2_init(const char *coef, int coef_len, int channel_num);
int Synthesis_init(const char *coef1, int coef_len1, const char *coef2, int coef_len2, int channel_num);

#ifdef __cplusplus
}
#endif

#endif /* __SBCACC_HEADER__ */
