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
#include "cmsis.h"
#include "plat_types.h"
#include "tgt_hardware.h"
#include "string.h"
#include "export_fn_rom.h"
#include "factory_section.h"
#include "pmu.h"
#include "hal_trace.h"
#include "hal_norflash.h"
#include "heap_api.h"

#define crc32(crc, buf, len) __export_fn_rom.crc32(crc, buf, len)

extern uint32_t __factory_start[];

static factory_section_t *factory_section_p = NULL;

int factory_section_open(void)
{
    factory_section_p = (factory_section_t *)__factory_start;
    
    if (factory_section_p->head.magic != nvrec_dev_magic){
        factory_section_p = NULL;
        return -1;
    }
    if (factory_section_p->head.version != nvrec_dev_version){
        factory_section_p = NULL;
        return -1;
    }
    if (factory_section_p->head.crc != crc32(0,(unsigned char *)(&(factory_section_p->head.reserved0)),sizeof(factory_section_t)-2-2-4)){
        factory_section_p = NULL;
        return -1;
    }
    memcpy(bt_addr, factory_section_p->data.bt_address, 6);
    memcpy(ble_addr, factory_section_p->data.ble_address, 6);
    TRACE("%s sucess btname:%s", __func__, factory_section_p->data.device_name);
    DUMP8("%02x ", bt_addr, 6);
    DUMP8("%02x ", ble_addr, 6);
    return 0;
}

factory_section_data_t *factory_section_data_ptr_get(void)
{
    if (factory_section_p)
        return &(factory_section_p->data);
    else
        return NULL;
}

int factory_section_xtal_fcap_get(unsigned int *xtal_fcap)
{
    if (factory_section_p){
        *xtal_fcap = factory_section_p->data.xtal_fcap;
        return 0;
    }else{
        return -1;
    }
}

int factory_section_xtal_fcap_set(unsigned int xtal_fcap)
{
    uint8_t *mempool = NULL;
    uint32_t lock;

    if (factory_section_p){        
        TRACE("factory_section_xtal_fcap_set:%d", xtal_fcap);
        syspool_init();
        syspool_get_buff((uint8_t **)&mempool, 0x1000);
        memcpy(mempool, factory_section_p, 0x1000);
        ((factory_section_t *)mempool)->data.xtal_fcap = xtal_fcap;
        ((factory_section_t *)mempool)->head.crc = crc32(0,(unsigned char *)(&(((factory_section_t *)mempool)->head.reserved0)),sizeof(factory_section_t)-2-2-4);

        lock = int_lock();
        pmu_flash_write_config();
        hal_norflash_erase(HAL_NORFLASH_ID_0,(uint32_t)(__factory_start),0x1000);
        hal_norflash_write(HAL_NORFLASH_ID_0,(uint32_t)(__factory_start),(uint8_t *)mempool,0x1000);
        pmu_flash_read_config();
        int_unlock(lock);

        return 0;
    }else{
        return -1;
    }
}

void factory_section_original_btaddr_get(uint8_t *btAddr)
{
    if(factory_section_p){
        TRACE("get factory_section_p");
        memcpy(btAddr, factory_section_p->data.bt_address, 6);
    }else{
        TRACE("get bt_addr");
        memcpy(btAddr, bt_addr, 6);
    }
}

#if defined(A2DP_LHDC_ON)
typedef struct
{
    unsigned char *btd_addr;
    unsigned char *ble_addr;
    const char *localname;
}dev_addr_name;

bool nvrec_dev_localname_addr_init(dev_addr_name *dev)
{
    if(factory_section_p){
        TRACE("%s valid",__func__);
        memcpy(dev->btd_addr, bt_addr, 6);
        memcpy(dev->ble_addr, ble_addr, 6);
        dev->localname = factory_section_p->data.device_name;
        TRACE("%s sucess btname:%s", __func__, factory_section_p->data.device_name);
        DUMP8("%02x ", bt_addr, 6);
        DUMP8("%02x ", ble_addr, 6);
        return true;
    }else{
        return false;
    }
}
#endif

