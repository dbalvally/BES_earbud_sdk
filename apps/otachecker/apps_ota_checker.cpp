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
#include "stdio.h"
#include "cmsis.h"
#include "cmsis_os.h"
#include "plat_addr_map.h"
#include "string.h"
#include "hal_norflash.h"
#include "hal_location.h"
#include "hal_trace.h"
#include "apps_ota_checker.h"
#include "tool_msg.h"


#define BOOT_UPDATE_SECTION_LOC __attribute__((section(".boot_update_section")))
#define BOOT_MAGIC_NUMBER           0xBE57EC1C
#define FLASH_CACHE_SIZE (1024*4)
#define CACHE_2_UNCACHE(addr) \
            ((unsigned char *)((unsigned int)(addr) & ~(0x04000000)))

/*
The size must identify with OTA BIN SIZE
*/
#define FLASH_OTA_SIZE (1024*128)
    
extern "C" int app_audio_mempool_use_mempoolsection_init(void);
extern "C" int app_audio_mempool_get_buff(uint8_t **buff, uint32_t size);
uint8_t *flash_cache = NULL;


BOOT_UPDATE_SECTION_LOC const uint8_t boot_bin[] = {
#include "ota_boot.txt"
};

struct boot_struct_t *ota_boot = (boot_struct_t *)FLASH_BASE;

int memfind(bool grow, uint8_t *haystack, int haystack_len, uint8_t *needle, int needle_len)
{
    bool find = false;
    int offset;

    if (grow){      
        offset = 0;
        do {
            if (!memcmp(haystack+offset, needle, needle_len)){
                find = true;
                break;
            }
            offset++;
        }while(offset<(haystack_len-needle_len));
    }else{
        offset = haystack_len-needle_len;
        do {
            if (!memcmp(haystack+offset, needle, needle_len)){
                find = true;
                break;
            }
            offset--;
        }while(offset);
    }
    if (!find)
        return -1;
    else
        return offset;
}

int apps_ota_find_rev_info(bool grow, uint8_t *buf, uint32_t len, uint8_t **rev_info_str)
{
    const char *REV_INFO = "REV_INFO=";
    int nRet = -1;
    
    TRACE("%s %08x,%d", __func__, buf, len);
    *rev_info_str = NULL;
    nRet = memfind(grow, buf, len, (uint8_t *)REV_INFO, strlen(REV_INFO));
    if (nRet>=0){
        *rev_info_str =  buf+nRet+strlen(REV_INFO);
        nRet = 0;
    }

    return nRet;
}

int apps_ota_update(void)
{   
    uint32_t write_offset = 0;
    uint32_t write_page_size = 0;
    uint32_t need_write_size = 0;
    uint32_t write_buf = BOOT_MAGIC_NUMBER;

    uint32_t lock = int_lock();
    need_write_size = sizeof(boot_bin);

    app_audio_mempool_use_mempoolsection_init();
    app_audio_mempool_get_buff((uint8_t **)&flash_cache, FLASH_CACHE_SIZE);

    hal_norflash_erase(HAL_NORFLASH_ID_0, (uint32_t)CACHE_2_UNCACHE(FLASH_BASE), FLASH_OTA_SIZE);
    
    do{
        write_page_size = (need_write_size-write_offset) < (FLASH_CACHE_SIZE) ?
            need_write_size-write_offset:
            FLASH_CACHE_SIZE;
            
        memcpy(flash_cache, boot_bin+write_offset, write_page_size);
        hal_norflash_erase(HAL_NORFLASH_ID_0,(uint32_t)CACHE_2_UNCACHE(FLASH_BASE+write_offset), write_page_size);
        hal_norflash_write(HAL_NORFLASH_ID_0,(uint32_t)CACHE_2_UNCACHE(FLASH_BASE+write_offset),(uint8_t *)(flash_cache), write_page_size);
        write_offset += write_page_size;
    }while(write_offset < need_write_size);

    hal_norflash_write(HAL_NORFLASH_ID_0,(uint32_t)CACHE_2_UNCACHE(FLASH_BASE),(uint8_t *)(&write_buf), 4);

    int_unlock(lock);
    return 0;
}

int apps_ota_checker(void)
{
    uint8_t *rev_info_str_curr;
    uint8_t *rev_info_str_boot;

    TRACE("%s %08x,%08x", __func__, FLASH_REGION_BASE, FLASH_BASE);
    
    if (FLASH_REGION_BASE==FLASH_BASE){
        return 0;
    }
    
    apps_ota_find_rev_info(false,(uint8_t *)boot_bin, sizeof(boot_bin), &rev_info_str_curr);
    if (rev_info_str_curr){
        TRACE("%s", rev_info_str_curr);
    }

    
    TRACE("ota boot info addr:%08x", ota_boot->hdr.build_info_start);

    apps_ota_find_rev_info(true, (uint8_t *)(ota_boot->hdr.build_info_start), (FLASH_REGION_BASE-ota_boot->hdr.build_info_start), &rev_info_str_boot);
    if (rev_info_str_curr){
        TRACE("%s", rev_info_str_boot);
    }
    
    if (strcmp((char *)rev_info_str_curr, (char *)rev_info_str_boot)){
        TRACE("Rev not match");
        apps_ota_update();
    }
    
    TRACE("%s end", __func__);
    return 0;
}


