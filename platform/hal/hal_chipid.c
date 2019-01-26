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
#include "hal_chipid.h"
#include "hal_location.h"

enum HAL_CHIP_METAL_ID_T BOOT_BSS_LOC metal_id;

uint32_t WEAK BOOT_TEXT_FLASH_LOC read_hw_metal_id(void)
{
    return HAL_CHIP_METAL_ID_0;
}

void BOOT_TEXT_FLASH_LOC hal_chipid_init(void)
{
    metal_id = read_hw_metal_id();
}

enum HAL_CHIP_METAL_ID_T BOOT_TEXT_SRAM_LOC hal_get_chip_metal_id(void)
{
#ifdef FPGA
    return HAL_CHIP_METAL_ID_15;
#else
    return metal_id;
#endif
}


enum HAL_CHIP_T  hal_get_chip_id(void)
{
#ifdef CHIP_BEST1000
    return HAL_CHIP_2000;
#elif defined(CHIP_BEST2000)
    return HAL_CHIP_2200;
#elif defined(CHIP_BEST2300)
    return HAL_CHIP_2300;
#endif
}
