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
#include <assert.h>
#include "hal_sleep.h"
#include "besbt.h"
#include "nvrecord.h"
#include "nvrecord_env.h"
#include "crc32.h"
#include "hal_timer.h"
#include "hal_norflash.h"
#include "pmu.h"
#include "app_tws.h"
#include "tgt_hardware.h"


extern uint32_t __factory_start[];
extern uint32_t __userdata_start[];
extern uint32_t __userdata_start_bak[];
uint8_t nv_record_dev_rev = NVREC_DEV_VERSION_1;

nv_record_struct nv_record_config;
static bool nvrec_init = false;
static bool nvrec_mempool_init = false;
static bool dev_sector_valid = false;
static uint32_t usrdata_ddblist_pool[1024] __attribute__((section(".userdata_pool")));
static void nv_record_print_dev_record(const btif_device_record_t* record)
{
#ifdef nv_record_debug

    LOG_PRINT_NVREC("nv record bdAddr = ");
    LOG_PRINT_NVREC_DUMP8("%x ",record->bdAddr.address,sizeof(record->bdAddr.address));
    LOG_PRINT_NVREC("record_trusted = ");
    LOG_PRINT_NVREC_DUMP8("%d ",&record->trusted,sizeof((uint8_t)record->trusted));
    LOG_PRINT_NVREC("record_linkKey = ");
    LOG_PRINT_NVREC_DUMP8("%x ",record->linkKey,sizeof(record->linkKey));
    LOG_PRINT_NVREC("record_keyType = ");
    LOG_PRINT_NVREC_DUMP8("%x ",&record->keyType,sizeof(record->keyType));
    LOG_PRINT_NVREC("record_pinLen = ");
    LOG_PRINT_NVREC_DUMP8("%x ",&record->pinLen,sizeof(record->pinLen));
#endif
}

heap_handle_t g_nv_mempool = NULL;

static void nv_record_mempool_init(void)
{
    unsigned char *poolstart = 0;

    poolstart = (unsigned char *)(usrdata_ddblist_pool+mempool_pre_offset);
    if(!nvrec_mempool_init)
    {
        g_nv_mempool = heap_register((unsigned char*)poolstart, (size_t)(sizeof(usrdata_ddblist_pool)-(mempool_pre_offset*sizeof(uint32_t))));
        nvrec_mempool_init = TRUE;
    }
    /*add other memory pool */

}

static bool nv_record_data_is_valid(void * userdata_section)
{
    uint32_t crc;
    uint32_t flsh_crc;
    uint32_t verandmagic;

    memcpy((void *)usrdata_ddblist_pool,(void *)userdata_section,sizeof(usrdata_ddblist_pool));
    verandmagic = usrdata_ddblist_pool[0];
    if(((nvrecord_struct_version<<16)|nvrecord_magic) != verandmagic)
        return false;
    crc = crc32(0,(uint8_t *)(&usrdata_ddblist_pool[pos_heap_contents]),(sizeof(usrdata_ddblist_pool)-(pos_heap_contents*sizeof(uint32_t))));
    LOG_PRINT_NVREC("%s,crc=0x%x",nvrecord_tag,crc);
    flsh_crc = usrdata_ddblist_pool[pos_crc];
    LOG_PRINT_NVREC("%s,read crc from flash is 0x%x",nvrecord_tag,flsh_crc);
    if (flsh_crc == crc)
        return true;

    return false;
}

bt_status_t nv_record_open(SECTIONS_ADP_ENUM section_id)
{
    bt_status_t ret_status = BT_STS_FAILED;

    if(nvrec_init)
        return BT_STS_SUCCESS;
    switch(section_id)
    {
        case section_usrdata_ddbrecord:
#ifndef FPGA
            if(nv_record_data_is_valid(__userdata_start))
            {
                LOG_PRINT_NVREC("%s,normal crc valid.",__func__);
                nv_record_config.config = (nvrec_config_t *)usrdata_ddblist_pool[pos_config_addr];
                LOG_PRINT_NVREC("%s,nv_record_config.config=0x%x,g_nv_mempool=0x%x,ln=%d\n",nvrecord_tag,usrdata_ddblist_pool[pos_config_addr],g_nv_mempool,__LINE__);
                g_nv_mempool = (heap_handle_t)(&usrdata_ddblist_pool[mempool_pre_offset]);
                LOG_PRINT_NVREC("%s,g_nv_mempool=0x%x.",__func__,g_nv_mempool);
                nv_record_config.is_update = false;
                ret_status = BT_STS_SUCCESS;
            }else if(nv_record_data_is_valid(__userdata_start_bak)){
                LOG_PRINT_NVREC("%s,userdate backup crc valid.",__func__);
                nv_record_config.config = (nvrec_config_t *)usrdata_ddblist_pool[pos_config_addr];
                LOG_PRINT_NVREC("%s,nv_record_config.config=0x%x,g_nv_mempool=0x%x,ln=%d\n",nvrecord_tag,usrdata_ddblist_pool[pos_config_addr],g_nv_mempool,__LINE__);
                g_nv_mempool = (heap_handle_t)(&usrdata_ddblist_pool[mempool_pre_offset]);
                LOG_PRINT_NVREC("%s,g_nv_mempool=0x%x.",__func__,g_nv_mempool);
                nv_record_config.is_update = true;
                nv_record_flash_flush();
                ret_status = BT_STS_SUCCESS;
            }
            else
            {
                LOG_PRINT_NVREC("%s,crc invalid. ",__func__);
                memset(usrdata_ddblist_pool,0,sizeof(usrdata_ddblist_pool));

                nv_record_mempool_init();
                nv_record_config.config = nvrec_config_new("PRECIOUS 4K.");
                nv_record_config.is_update = true;
                LOG_PRINT_NVREC("%s,nv_record_config.config=0x%x,g_nv_mempool=0x%x,ln=%d\n",nvrecord_tag,nv_record_config.config,g_nv_mempool,__LINE__);
                nv_record_flash_flush();
                ret_status = BT_STS_SUCCESS;;
            }
 #endif

            break;
        case section_source_ring:

            break;
        default:
            break;
    }
    nvrec_init = true;
#if FPGA==0
    if (ret_status == BT_STS_SUCCESS)
        hal_sleep_set_deep_sleep_hook(HAL_DEEP_SLEEP_HOOK_USER_1, nv_record_flash_flush);
#endif
    LOG_PRINT_NVREC("%s,open,ret_status=%d\n",nvrecord_tag,ret_status);
    return ret_status;
}

static size_t config_entries_get_ddbrec_length(const char *secname)
{
    size_t entries_len = 0;

    if(NULL != nv_record_config.config)
    {
        nvrec_section_t *sec = NULL;
        sec = nvrec_section_find(nv_record_config.config,secname);
        if (NULL != sec)
            entries_len = sec->entries->length;
    }
    return entries_len;
}

//workaround tws record lose issue, this api fixed it from the root. 
static void config_entries_ddbdev_delete_oldest_phone_record(void)
{
    list_node_t *node_p = NULL;
    list_t *entry = NULL;
    nvrec_section_t *sec = NULL;
    char masterRecKey[BTIF_BD_ADDR_SIZE+1];
    char slaveRecKey[BTIF_BD_ADDR_SIZE+1];
    int recaddr = 0;
    btif_device_record_t *rec = NULL;
    nvrec_entry_t *entry_temp;

    memcpy(masterRecKey, tws_mode_ptr()->masterAddr.address, BTIF_BD_ADDR_SIZE);
    memcpy(slaveRecKey, tws_mode_ptr()->slaveAddr.address, BTIF_BD_ADDR_SIZE);

    sec = nvrec_section_find(nv_record_config.config,section_name_ddbrec);
    entry = sec->entries;
    node_p = list_begin_ext(entry);
    if(NULL == node_p)
    {
        LOG_PRINT_NVREC("PDL is NULL!!!");
        return;
    }

    //skip the TWS record
    entry_temp = list_node_ext(node_p);
    while(!memcmp(entry_temp->key, masterRecKey, BTIF_BD_ADDR_SIZE) || !memcmp(entry_temp->key, slaveRecKey, BTIF_BD_ADDR_SIZE))
    {
        node_p = list_next_ext(node_p);
        entry_temp = list_node_ext(node_p);
        if(NULL == node_p)
        {
            LOG_PRINT_NVREC("PDL is NULL, except TWS record!!!");
            return;
        }
    }

#if 0
    //remove the oldest phone record
    recaddr = nvrec_config_get_int(nv_record_config.config,section_name_ddbrec,entry_temp->key,0);
    rec = (btif_device_record_t *)recaddr;
    pool_free(rec);
    
    //remove node from node list
    list_remove_ext(entry, node_p->data); 
#else
    nv_record_ddbrec_delete((bt_bdaddr_t *)entry_temp->key);
#endif
}

static void config_entries_ddbdev_delete_head(void)//delete the oldest record.
{
    list_node_t *head_node = NULL;
    list_t *entry = NULL;
    nvrec_section_t *sec = NULL;

    sec = nvrec_section_find(nv_record_config.config,section_name_ddbrec);
    entry = sec->entries;
    head_node = list_begin_ext(entry);
    if(NULL != head_node)
    {
        btif_device_record_t *rec = NULL;
        unsigned int recaddr = 0;

        nvrec_entry_t *entry_temp = list_node_ext(head_node);
        recaddr = nvrec_config_get_int(nv_record_config.config,section_name_ddbrec,entry_temp->key,0);
        rec = (btif_device_record_t *)recaddr;
        pool_free(rec);
        nvrec_entry_free(entry_temp);
         if(1 == config_entries_get_ddbrec_length(section_name_ddbrec))
            entry->head = entry->tail = NULL;
        else
            entry->head = list_next_ext(head_node);
        pool_free(head_node);
        entry->length -= 1;
    }
}

static void config_entries_ddbdev_delete_tail(void)
{
    list_node_t *node;
    list_node_t *temp_ptr;
    list_node_t *tailnode;
    size_t entrieslen = 0;
    nvrec_entry_t *entry_temp = NULL;
    nvrec_section_t *sec = nvrec_section_find(nv_record_config.config,section_name_ddbrec);
    btif_device_record_t *recaddr = NULL;
    unsigned int addr = 0;

    if (!sec)
        assert(0);
    sec->entries->length -= 1;
    entrieslen = sec->entries->length;
    node = list_begin_ext(sec->entries);
    while(entrieslen > 1)
    {
        node = list_next_ext(node);
        entrieslen--;
    }
    tailnode = list_next_ext(node);
    entry_temp = list_node_ext(tailnode);
    addr = nvrec_config_get_int(nv_record_config.config,section_name_ddbrec,entry_temp->key,0);
    recaddr = (btif_device_record_t *)addr;
    pool_free(recaddr);
    nvrec_entry_free(entry_temp);
    //pool_free(entry_temp);
    temp_ptr = node->next;
    node->next = NULL;
    pool_free(temp_ptr);
    sec->entries->tail = node;
}

static void config_entries_ddbdev_delete(const btif_device_record_t* param_rec)
{
    nvrec_section_t *sec = NULL;
    list_node_t *entry_del = NULL;
    list_node_t *entry_pre = NULL;
    list_node_t *entry_next = NULL;
    list_node_t *node = NULL;
    btif_device_record_t *recaddr = NULL;
    unsigned int addr = 0;
    int pos = 0,pos_pre=0;
    nvrec_entry_t *entry_temp = NULL;

    sec = nvrec_section_find(nv_record_config.config,section_name_ddbrec);
    for(node=list_begin_ext(sec->entries);node!=NULL;node=list_next_ext(node))
    {
        nvrec_entry_t *entries = list_node_ext(node);
        pos++;
        if(0 == memcmp(entries->key,param_rec->bdAddr.address,BTIF_BD_ADDR_SIZE))
        {
            entry_del = node;
            entry_next = entry_del->next;
            break;
        }
    }

    if (entry_del){
        /*get entry_del pre node*/
        pos_pre = (pos-1);
        pos=0;
        node=list_begin_ext(sec->entries);
        pos++;
        while(pos < pos_pre)
        {
            node=list_next_ext(node);
            pos += 1;
        }
        entry_pre = node;

        /*delete entry_del following...*/
        entry_temp = list_node_ext(entry_del);
        addr = nvrec_config_get_int(nv_record_config.config,section_name_ddbrec,(char *)entry_temp->key,0);
        assert(0!=addr);
        recaddr = (btif_device_record_t *)addr;
        pool_free(recaddr);
        nvrec_entry_free(entry_temp);
        //pool_free(entry_temp);
        pool_free(entry_pre->next);
        entry_pre->next = entry_next;
        sec->entries->length -= 1;
    }
}

static bool config_entries_ddbdev_is_head(const btif_device_record_t* param_rec)
{
    list_node_t *head_node = NULL;
    nvrec_section_t *sec = NULL;
    list_t *entry = NULL;
    nvrec_entry_t *entry_temp = NULL;

    sec = nvrec_section_find(nv_record_config.config,section_name_ddbrec);
    entry = sec->entries;
    head_node = list_begin_ext(entry);
    entry_temp = list_node_ext(head_node);
    if(0 == memcmp(entry_temp->key,param_rec->bdAddr.address,BTIF_BD_ADDR_SIZE))
        return true;
    return false;
}

static bool config_entries_ddbdev_is_tail(const btif_device_record_t* param_rec)
{
    list_node_t *node;
    nvrec_entry_t *entry_temp = NULL;

    nvrec_section_t *sec = nvrec_section_find(nv_record_config.config,section_name_ddbrec);
    if (!sec)
        assert(0);
    for (node = list_begin_ext(sec->entries); node != list_end_ext(sec->entries); node = list_next_ext(node))
    {
        entry_temp = list_node_ext(node);
    }
    if(0 == memcmp(entry_temp->key,param_rec->bdAddr.address,BTIF_BD_ADDR_SIZE))
        return true;
    return false;
}

/*
this function should be surrounded by OS_LockStack and OS_UnlockStack when call.
*/
static void config_specific_entry_value_delete(const btif_device_record_t* param_rec)
{
    if(config_entries_ddbdev_is_head(param_rec))
        config_entries_ddbdev_delete_head();
    else if(config_entries_ddbdev_is_tail(param_rec))
        config_entries_ddbdev_delete_tail();
    else
        config_entries_ddbdev_delete(param_rec);
    nv_record_config.is_update = true;
}


/**********************************************
gyt add:list head is the odest,list tail is the latest.
this function should be surrounded by OS_LockStack and OS_UnlockStack when call.
**********************************************/
static bt_status_t nv_record_ddbrec_add(const btif_device_record_t* param_rec)
{
    btdevice_volume device_vol;
    btdevice_profile device_plf;
    char key[BTIF_BD_ADDR_SIZE+1] = {0,};
    nvrec_btdevicerecord *nvrec_pool_record = NULL;
    bool ddbrec_exist = false;
    int getint;

    if(NULL == param_rec)
        return BT_STS_FAILED;
    memcpy(key,param_rec->bdAddr.address,BTIF_BD_ADDR_SIZE);
    getint = nvrec_config_get_int(nv_record_config.config,section_name_ddbrec,(char *)key,0);
    if(getint){
        ddbrec_exist = true;
        nvrec_pool_record = (nvrec_btdevicerecord *)getint;
        device_vol.a2dp_vol = nvrec_pool_record->device_vol.a2dp_vol;
        device_vol.hfp_vol = nvrec_pool_record->device_vol.hfp_vol;
        device_plf.hfp_act = nvrec_pool_record->device_plf.hfp_act;
        device_plf.hsp_act = nvrec_pool_record->device_plf.hsp_act;
        device_plf.a2dp_act = nvrec_pool_record->device_plf.a2dp_act;
        device_plf.a2dp_codectype = nvrec_pool_record->device_plf.a2dp_codectype;
        config_specific_entry_value_delete(param_rec);
    }
    if(DDB_RECORD_NUM == config_entries_get_ddbrec_length(section_name_ddbrec))
    {
        LOG_PRINT_NVREC("%s,ddbrec list is full,delete the oldest phone record and add param_rec to list.",nvrecord_tag);
		config_entries_ddbdev_delete_oldest_phone_record();
        //config_entries_ddbdev_delete_head();
    }
    nvrec_pool_record = (nvrec_btdevicerecord *)pool_malloc(sizeof(nvrec_btdevicerecord));
    if(NULL == nvrec_pool_record)
    {
        LOG_PRINT_NVREC("%s,pool_malloc failure.",nvrecord_tag);
        return BT_STS_FAILED;
    }
    LOG_PRINT_NVREC("%s,pool_malloc addr = 0x%x\n",nvrecord_tag,(unsigned int)nvrec_pool_record);
    memcpy(nvrec_pool_record,param_rec,sizeof(btif_device_record_t));
    memcpy(key,param_rec->bdAddr.address,BTIF_BD_ADDR_SIZE);
    if (ddbrec_exist){
        nvrec_pool_record->device_vol.a2dp_vol = device_vol.a2dp_vol;
        nvrec_pool_record->device_vol.hfp_vol = device_vol.hfp_vol;
        nvrec_pool_record->device_plf.hfp_act = device_plf.hfp_act;
        nvrec_pool_record->device_plf.hsp_act = device_plf.hsp_act;
        nvrec_pool_record->device_plf.a2dp_act = device_plf.a2dp_act;
        nvrec_pool_record->device_plf.a2dp_codectype = device_plf.a2dp_codectype;
    }else{
        nvrec_pool_record->device_vol.a2dp_vol = NVRAM_ENV_STREAM_VOLUME_A2DP_VOL_DEFAULT;
        nvrec_pool_record->device_vol.hfp_vol = NVRAM_ENV_STREAM_VOLUME_HFP_VOL_DEFAULT;
        nvrec_pool_record->device_plf.hfp_act = false;
        nvrec_pool_record->device_plf.hsp_act = false;
        nvrec_pool_record->device_plf.a2dp_act = false;
        nvrec_pool_record->device_plf.a2dp_codectype = BTIF_AVDTP_CODEC_TYPE_NON_A2DP;
    }
    nvrec_config_set_int(nv_record_config.config,section_name_ddbrec,(char *)key,(int)nvrec_pool_record);
    LOG_PRINT_NVREC("Saved key:");
    LOG_PRINT_NVREC_DUMP8("%x ", nvrec_pool_record->record.linkKey, 16);
#ifdef nv_record_debug
    getint = nvrec_config_get_int(nv_record_config.config,section_name_ddbrec,(char *)key,0);
    LOG_PRINT_NVREC("%s,get pool_malloc addr = 0x%x\n",nvrecord_tag,(unsigned int)getint);
#endif
    nv_record_config.is_update = true;
    return BT_STS_SUCCESS;
}

/*
this function should be surrounded by OS_LockStack and OS_UnlockStack when call.
*/
bt_status_t nv_record_add(SECTIONS_ADP_ENUM type,void *record)
{
    bt_status_t retstatus = BT_STS_FAILED;

    if ((NULL == record) || (section_none == type))
        return BT_STS_FAILED;
    switch(type)
    {
        case section_usrdata_ddbrecord:
            retstatus = nv_record_ddbrec_add(record);
            break;

        case section_usrdata_other:
            break;
        default:
            break;
    }

    return retstatus;
}

/*
this function should be surrounded by OS_LockStack and OS_UnlockStack when call.
*/
bt_status_t nv_record_ddbrec_find(const bt_bdaddr_t* bd_ddr,btif_device_record_t *record)
{
    unsigned int getint = 0;
    char key[BTIF_BD_ADDR_SIZE+1] = {0,};
    btif_device_record_t *getrecaddr = NULL;

    if((NULL == nv_record_config.config) || (NULL == bd_ddr) || (NULL == record))
        return BT_STS_FAILED;
    memcpy(key,bd_ddr->address,BTIF_BD_ADDR_SIZE);
    getint = nvrec_config_get_int(nv_record_config.config,section_name_ddbrec,(char *)key,0);
    if(0 == getint)
        return BT_STS_FAILED;
    getrecaddr = (btif_device_record_t *)getint;
    memcpy(record,(void *)getrecaddr,sizeof(btif_device_record_t));
    return BT_STS_SUCCESS;
}

int nv_record_btdevicerecord_find(const bt_bdaddr_t *bd_ddr, nvrec_btdevicerecord **record)
{
    unsigned int getint = 0;
    char key[BTIF_BD_ADDR_SIZE+1] = {0,};

    if((NULL == nv_record_config.config) || (NULL == bd_ddr) || (NULL == record))
        return -1;
    memcpy(key,bd_ddr->address, BTIF_BD_ADDR_SIZE);
    getint = nvrec_config_get_int(nv_record_config.config,section_name_ddbrec,(char *)key,0);
    if(0 == getint)
        return -1;
    *record = (nvrec_btdevicerecord *)getint;
    return 0;
}

/*
this function should be surrounded by OS_LockStack and OS_UnlockStack when call.
*/
bt_status_t nv_record_ddbrec_delete(const bt_bdaddr_t *bdaddr)
{
    unsigned int getint = 0;
    btif_device_record_t *getrecaddr = NULL;
    char key[BTIF_BD_ADDR_SIZE+1] = {0,};

    memcpy(key,bdaddr->address,BTIF_BD_ADDR_SIZE);
    getint = nvrec_config_get_int(nv_record_config.config,section_name_ddbrec,(char *)key,0);
    if(0 == getint)
        return BT_STS_FAILED;
    //found ddb record,del it and return succeed.
    getrecaddr = (btif_device_record_t *)getint;
    config_specific_entry_value_delete((const btif_device_record_t *)getrecaddr);
    return BT_STS_SUCCESS;
}

/*
this function should be surrounded by OS_LockStack and OS_UnlockStack when call.
*/
bt_status_t nv_record_enum_dev_records(unsigned short index,btif_device_record_t* record)
{
    nvrec_section_t *sec = NULL;
    list_node_t *node;
    unsigned short pos = 0;
    nvrec_entry_t *entry_temp = NULL;
    btif_device_record_t *recaddr = NULL;
    unsigned int addr = 0;

    if((index >= DDB_RECORD_NUM) || (NULL == record) || (NULL == nv_record_config.config))
        return BT_STS_FAILED;
    sec = nvrec_section_find(nv_record_config.config,section_name_ddbrec);
    if(NULL == sec)
        return BT_STS_INVALID_PARM;
    if(NULL == sec->entries)
        return BT_STS_INVALID_PARM;
    if(0 == sec->entries->length)
        return BT_STS_FAILED;
    if(index >= sec->entries->length)
        return BT_STS_FAILED;
    node = list_begin_ext(sec->entries);

    while(pos < index)
    {
        node = list_next_ext(node);
        pos++;
    }
    entry_temp = list_node_ext(node);
    addr = nvrec_config_get_int(nv_record_config.config,section_name_ddbrec,(char *)entry_temp->key,0);
    if(0 == addr)
        return BT_STS_FAILED;
    recaddr = (btif_device_record_t *)addr;
    memcpy(record,recaddr,sizeof(btif_device_record_t));
    nv_record_print_dev_record(record);
    return BT_STS_SUCCESS;
}

int nv_record_get_paired_dev_count(void)
{
#ifdef FPGA
    return 0;
#else
    nvrec_section_t *sec = NULL;
    list_t *entry = NULL;

    sec = nvrec_section_find(nv_record_config.config,section_name_ddbrec);
    if(NULL == sec)
        return 0;
    entry = sec->entries;
    if(NULL == entry)
        return 0;

    return entry->length;
#endif
}


/*
return:
    -1:     enum dev failure.
    0:      without paired dev.
    1:      only 1 paired dev,store@record1.
    2:      get 2 paired dev.notice:record1 is the latest record.
*/
int nv_record_enum_latest_two_paired_dev(btif_device_record_t* record1,btif_device_record_t* record2)
{
    bt_status_t getret1 = BT_STS_FAILED;
    bt_status_t getret2 = BT_STS_FAILED;
    nvrec_section_t *sec = NULL;
    size_t entries_len = 0;
    list_t *entry = NULL;

    if((NULL == record1) || (NULL == record2))
        return -1;
    sec = nvrec_section_find(nv_record_config.config,section_name_ddbrec);
    if(NULL == sec)
        return 0;
    entry = sec->entries;
    if(NULL == entry)
        return 0;
    entries_len = entry->length;
    if(0 == entries_len)
        return 0;
    else if(entries_len < 0)
        return -1;
    if(1 == entries_len)
    {
        getret1 = nv_record_enum_dev_records(0,record1);
        if(BT_STS_SUCCESS == getret1)
            return 1;
        return -1;
    }
    getret1 = nv_record_enum_dev_records(entries_len-1,record1);
    getret2 = nv_record_enum_dev_records(entries_len-2,record2);
    if((BT_STS_SUCCESS != getret1) || (BT_STS_SUCCESS != getret2))
    {
        memset(record1,0x0,sizeof(btif_device_record_t));
        memset(record2,0x0,sizeof(btif_device_record_t));
        return -1;
    }
    return 2;
}

int nv_record_touch_cause_flush(void)
{
    nv_record_config.is_update = true;
    return 0;
}

void nv_record_all_ddbrec_print(void)
{
    list_t *entry = NULL;
    int tmp_i = 0;
    nvrec_section_t *sec = NULL;
    size_t entries_len = 0;

    sec = nvrec_section_find(nv_record_config.config,section_name_ddbrec);
    if(NULL == sec)
        return;
    entry = sec->entries;
    if(NULL == entry)
        return;
    entries_len = entry->length;
    if(0 == entries_len)
    {
        nvrec_trace("%s,without btdevicerec.",nvrecord_tag,__LINE__);
        return;
    }
    for(tmp_i=0;tmp_i<entries_len;tmp_i++)
    {
        btif_device_record_t record;
        bt_status_t ret_status;
        ret_status = nv_record_enum_dev_records(tmp_i,&record);
        if(BT_STS_SUCCESS== ret_status)
            nv_record_print_dev_record(&record);
    }
}

void nv_record_sector_clear(void)
{
    uint32_t lock;

    lock = int_lock();
    pmu_flash_write_config();

    hal_norflash_erase(HAL_NORFLASH_ID_0,(uint32_t)(__userdata_start_bak),0x1000);
    hal_norflash_erase(HAL_NORFLASH_ID_0,(uint32_t)(__userdata_start),0x1000);

    pmu_flash_read_config();
    nvrec_init = false;
    nvrec_mempool_init = false;
    int_unlock(lock);
}

#define DISABLE_NV_RECORD_CRC_CHECK_BEFORE_FLUSH    1
void nv_record_update_runtime_userdata(void)
{
    if (NULL == nv_record_config.config)
    {
        return;
    }

    uint32_t lock;
    lock = int_lock();
    nv_record_config.is_update = true;

#if !DISABLE_NV_RECORD_CRC_CHECK_BEFORE_FLUSH
    buffer_alloc_ctx* heapctx = memory_buffer_heap_getaddr();
    memcpy((void *)(&usrdata_ddblist_pool[pos_heap_contents]),heapctx,sizeof(buffer_alloc_ctx));
    uint32_t crc = crc32(0,(uint8_t *)(&usrdata_ddblist_pool[pos_heap_contents]),(sizeof(usrdata_ddblist_pool)-(pos_heap_contents*sizeof(uint32_t))));
    
    usrdata_ddblist_pool[pos_crc] = crc;
#endif

    int_unlock(lock);
}

void nv_record_force_flash_flush(void)
{
#if !DISABLE_NV_RECORD_CRC_CHECK_BEFORE_FLUSH   
    nv_record_update_runtime_userdata();
#endif
    nv_record_flash_flush();
}

int nv_record_flash_flush(void)
{
#ifndef FPGA    
    uint32_t crc;
    uint32_t trycnt = 0;
    uint32_t lock;
    

    if(false == nv_record_config.is_update)
    {
        //LOG_PRINT_NVREC("%s,without update.\n",nvrecord_tag);
        return 0;
    }
    if(NULL == nv_record_config.config)
    {
        LOG_PRINT_NVREC("%s,nv_record_config.config is null.\n",nvrecord_tag);
        return 0;
    }
         
    LOG_PRINT_NVREC("%s,nv_record_flash_flush,nv_record_config.config=0x%x\n",nvrecord_tag,nv_record_config.config);
tryagain:

    lock = int_lock();
    crc = crc32(0,(uint8_t *)(&usrdata_ddblist_pool[pos_heap_contents]),(sizeof(usrdata_ddblist_pool)-(pos_heap_contents*sizeof(uint32_t))));
    LOG_PRINT_NVREC("%s,crc=%x.\n",nvrecord_tag,crc);
    usrdata_ddblist_pool[pos_version_and_magic] = ((nvrecord_struct_version<<16)|nvrecord_magic);
    usrdata_ddblist_pool[pos_crc] = crc;
    usrdata_ddblist_pool[pos_reserv1] = 0;
    usrdata_ddblist_pool[pos_reserv2] = 0;
    usrdata_ddblist_pool[pos_config_addr] = (uint32_t)nv_record_config.config;

    pmu_flash_write_config();

    hal_norflash_erase(HAL_NORFLASH_ID_0,(uint32_t)(__userdata_start),sizeof(usrdata_ddblist_pool));
    hal_norflash_write(HAL_NORFLASH_ID_0,(uint32_t)(__userdata_start),(uint8_t *)usrdata_ddblist_pool,sizeof(usrdata_ddblist_pool));

    pmu_flash_read_config();
    nv_record_config.is_update = false;

    int_unlock(lock);

    hal_sys_timer_delay(MS_TO_TICKS(20));

    lock=int_lock();
    pmu_flash_write_config();
    hal_norflash_erase(HAL_NORFLASH_ID_0,(uint32_t)(__userdata_start_bak),sizeof(usrdata_ddblist_pool));
    hal_norflash_write(HAL_NORFLASH_ID_0,(uint32_t)(__userdata_start_bak),(uint8_t *)usrdata_ddblist_pool,sizeof(usrdata_ddblist_pool));
    pmu_flash_read_config();
    int_unlock(lock);

#if 1//def nv_record_debug
    lock = int_lock();
    bool needretry = false;
    for (uint32_t i=0; i<sizeof(usrdata_ddblist_pool)/sizeof(uint32_t); i++){
        if (__userdata_start[i]!=usrdata_ddblist_pool[i]){
            needretry = true;
            
            LOG_PRINT_NVREC("tryagain offset:%d cnt:%d %08x/%08x", i, trycnt, __userdata_start[i], usrdata_ddblist_pool[i]);
            break;
        }
    }
    int_unlock(lock);

    if (needretry){
        trycnt++;
        goto tryagain;
    }
    LOG_PRINT_NVREC("nv_record_flash_flush cnt:%d", trycnt);
#endif
#endif
    return 0;
}


const char* nvrec_dev_get_ble_name(void)
{
    if ((NVREC_DEV_VERSION_1 == nv_record_dev_rev) || (!dev_sector_valid))
    {
        return BLE_DEFAULT_NAME;
    }
    else
    {
        return (const char *)(&__factory_start[rev2_dev_ble_name]);
    }
}

