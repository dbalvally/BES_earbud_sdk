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
#ifndef __PLAT_ADDR_MAP_BEST1200_H__
#define __PLAT_ADDR_MAP_BEST1200_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ROM_SIZE
#define ROM_SIZE                                0x00009000
#endif

#ifndef RAM_SIZE
#define RAM_SIZE                                0x00020000
#endif

#include "best1000/plat_addr_map_best1000.h"

#undef ISPI_BASE
#define ISPI_BASE                               SPI_BASE

#ifdef __cplusplus
}
#endif

#endif
