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
static uint8_t nv_record_dev_rev = nvrec_current_version;

int factory_section_open(void)
{
    factory_section_p = (factory_section_t *)__factory_start;
    
    if (factory_section_p->head.magic != nvrec_dev_magic){
        factory_section_p = NULL;
        return -1;
    }
    if ((factory_section_p->head.version < nvrec_mini_version) ||
    	(factory_section_p->head.version > nvrec_current_version))
    {
        factory_section_p = NULL;
        return -1;
    }

    nv_record_dev_rev = factory_section_p->head.version;

	if (1 == nv_record_dev_rev)
	{
	    if (factory_section_p->head.crc != 
	    	crc32(0,(unsigned char *)(&(factory_section_p->head.reserved0)),
	    	sizeof(factory_section_t)-2-2-4)){
	        factory_section_p = NULL;
	        return -1;
	    }
	    
		memcpy(bt_addr, factory_section_p->data.bt_address, 6);
		memcpy(ble_addr, factory_section_p->data.ble_address, 6);
		TRACE("%s sucess btname:%s", __func__, factory_section_p->data.device_name);
    }
    else
	{
		// check the data length
		if (((uint32_t)(&((factory_section_t *)0)->data.rev2_reserved0)+ 
			factory_section_p->data.rev2_data_len) > 4096)
		{
			TRACE("nv rec dev data len %d has exceeds the facory sector size!.",
				factory_section_p->data.rev2_data_len);
			return -1;
		}
		
		if (factory_section_p->data.rev2_crc != 
			crc32(0,(unsigned char *)(&(factory_section_p->data.rev2_reserved0)),
			factory_section_p->data.rev2_data_len)){
	        factory_section_p = NULL;
	        return -1;
	    }

	    
		memcpy(bt_addr, factory_section_p->data.rev2_bt_addr, 6);
		memcpy(ble_addr, factory_section_p->data.rev2_ble_addr, 6);
		TRACE("%s sucess btname:%s", __func__, factory_section_p->data.rev2_bt_name);
	}
	
    DUMP8("%02x ", bt_addr, 6);
    DUMP8("%02x ", ble_addr, 6);
    return 0;
}

uint8_t* factory_section_get_bt_address(void)
{
	if (factory_section_p)
	{
		if (1 == nv_record_dev_rev)
		{
			return (uint8_t *)&(factory_section_p->data.bt_address);
		}
		else
		{
			return (uint8_t *)&(factory_section_p->data.rev2_bt_addr);
		}
	}
	else
	{
		return NULL;
	}
}

uint8_t* factory_section_get_bt_name(void)
{
	if (factory_section_p)
	{
		if (1 == nv_record_dev_rev)
		{
			return (uint8_t *)&(factory_section_p->data.device_name);
		}
		else
		{
			return (uint8_t *)&(factory_section_p->data.rev2_bt_name);
		}
	}
	else
	{
		return (uint8_t *)BT_LOCAL_NAME;
	}
}

uint8_t* factory_section_get_ble_name(void)
{
	if (factory_section_p)
	{
		if (1 == nv_record_dev_rev)
		{
			return (uint8_t *)BLE_DEFAULT_NAME;
		}
		else
		{
			return (uint8_t *)&(factory_section_p->data.rev2_ble_name);
		}
	}
	else
	{
		return (uint8_t *)BLE_DEFAULT_NAME;
	}
}

uint32_t factory_section_get_version(void)
{
	if (factory_section_p)
	{
		return nv_record_dev_rev;
	}
	
	return 0;
}


int factory_section_xtal_fcap_get(unsigned int *xtal_fcap)
{
    if (factory_section_p){
    	if (1 == nv_record_dev_rev)
    	{
        	*xtal_fcap = factory_section_p->data.xtal_fcap;
        }
        else
        {
        	*xtal_fcap = factory_section_p->data.rev2_xtal_fcap;
        }
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
        if (1 == nv_record_dev_rev)
        {
        	((factory_section_t *)mempool)->data.xtal_fcap = xtal_fcap;
        	((factory_section_t *)mempool)->head.crc = crc32(0,(unsigned char *)(&(((factory_section_t *)mempool)->head.reserved0)),sizeof(factory_section_t)-2-2-4);
		}
		else
		{
        	((factory_section_t *)mempool)->data.rev2_xtal_fcap = xtal_fcap;
        	((factory_section_t *)mempool)->data.rev2_crc = 
        		crc32(0,(unsigned char *)(&(factory_section_p->data.rev2_reserved0)),
				factory_section_p->data.rev2_data_len);			
		}
        lock = int_lock_global();
        pmu_flash_write_config();
        hal_norflash_erase(HAL_NORFLASH_ID_0,(uint32_t)(__factory_start),0x1000);
        hal_norflash_write(HAL_NORFLASH_ID_0,(uint32_t)(__factory_start),(uint8_t *)mempool,0x1000);
        pmu_flash_read_config();
        int_unlock_global(lock);

        return 0;
    }else{
        return -1;
    }
}

void factory_section_original_btaddr_get(uint8_t *btAddr)
{
    if(factory_section_p){
        TRACE("get factory_section_p");
        if (1 == nv_record_dev_rev)
        {
        	memcpy(btAddr, factory_section_p->data.bt_address, 6);
        }
        else
        {
        	memcpy(btAddr, factory_section_p->data.rev2_bt_addr, 6);
        }
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
        if (1 == nv_record_dev_rev)
        {
        	dev->localname = (const char *)(factory_section_p->data.device_name);
        	TRACE("%s sucess btname:%s", __func__, factory_section_p->data.device_name);
		}
		else
		{
        	dev->localname = (const char *)(factory_section_p->data.rev2_bt_name);
        	TRACE("%s sucess btname:%s", __func__, factory_section_p->data.rev2_bt_name);			
		}
        DUMP8("%02x ", bt_addr, 6);
        DUMP8("%02x ", ble_addr, 6);
        return true;
    }else{
        return false;
    }
}
#endif

