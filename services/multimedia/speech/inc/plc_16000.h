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
/////////////////////////////////////////////////////////////////////////
//Packets Loss Concealment                                              /
/////////////////////////////////////////////////////////////////////////
#ifndef _PLC_16000_H_
#define _PLC_16000_H_

typedef struct PlcSt_16000_ PlcSt_16000;

void speech_plc_16000_Dofe(PlcSt_16000 *lc, short *out, short *encbuf, int num);
void speech_plc_16000_AddToHistory(PlcSt_16000 *lc, short *s, int num);
PlcSt_16000 *speech_plc_16000_init(void* (* speex_alloc_ext)(int));

#endif












