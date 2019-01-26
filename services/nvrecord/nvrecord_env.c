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
#include "nvrecord.h"
#include "nvrecord_env.h"
#include "hal_trace.h"
#include "bluetooth.h"
#include "customparam_section.h"

#define NV_RECORD_ENV_KEY  "env_key"

extern nv_record_struct nv_record_config;
struct nvrecord_env_t *nvrecord_env_p = NULL;

#ifdef FPGA
struct nvrecord_env_t nv_fpga;
#endif

static int nv_record_env_new(void)
{
#ifdef FPGA
    nvrecord_env_p = (struct nvrecord_env_t *)&nv_fpga;
#else
    nvrecord_env_p = (struct nvrecord_env_t *)pool_malloc(sizeof(struct nvrecord_env_t));
#endif

    if (!nvrecord_env_p){
        TRACE("%s pool_malloc failed ");
        return -1;
    }
    nvrec_config_set_int(nv_record_config.config,section_name_other,(const char *)NV_RECORD_ENV_KEY,(int)nvrecord_env_p);

    nvrecord_env_p->media_language.language = NVRAM_ENV_MEDIA_LANGUAGE_DEFAULT;
#ifdef __TWS__      
    nvrecord_env_p->tws_mode.originalMode = NVRAM_ENV_TWS_MODE_DEFAULT;
    nvrecord_env_p->tws_mode.mode = NVRAM_ENV_TWS_MODE_DEFAULT;
#endif
    nvrecord_env_p->factory_tester_status.status = NVRAM_ENV_FACTORY_TESTER_STATUS_DEFAULT;
    nv_record_config.is_update = true;

    TRACE("%s nvrecord_env_p:%x",__func__, nvrecord_env_p);

    return 0;
}

int nv_record_env_init(void)
{
    int nRet = 0; 
#ifdef FPGA
    if (nv_record_env_new())
        nRet = -1;
#else
    nv_record_open(section_usrdata_ddbrecord);

    nv_custom_parameter_section_init();

    nvrecord_env_p = (struct nvrecord_env_t *)nvrec_config_get_int(nv_record_config.config, (const char *)section_name_other,(const char *)NV_RECORD_ENV_KEY, NVRAM_ENV_INVALID);

    if (nvrecord_env_p == (struct nvrecord_env_t *)NVRAM_ENV_INVALID){
        LOG_PRINT_NVREC("NVRAM_ENV_INVALID !!");
        if (nv_record_env_new())
            nRet = -1;
    }
#endif
    LOG_PRINT_NVREC("%s nvrecord_env_p:%x",__func__, nvrecord_env_p);
#ifdef __TWS__          
    LOG_PRINT_ENV("tws_mode: %d",nvrecord_env_p->tws_mode.originalMode);
    LOG_PRINT_ENV("TWS Master:");
    LOG_PRINT_ENV_DUMP8("0x%02x ", nvrecord_env_p->tws_mode.masterAddr.address, BTIF_BD_ADDR_SIZE);
    LOG_PRINT_ENV("\nTWS Slave:");
    LOG_PRINT_ENV_DUMP8("0x%02x ", nvrecord_env_p->tws_mode.slaveAddr.address, BTIF_BD_ADDR_SIZE);
#endif
    return nRet;
}

int nv_record_env_get(struct nvrecord_env_t **nvrecord_env)
{
    if (!nvrecord_env)
        return -1;

    if (!nvrecord_env_p)
        return -1;

    *nvrecord_env = nvrecord_env_p;

    return 0;
}

int nv_record_env_set(struct nvrecord_env_t *nvrecord_env)
{
    if (!nvrecord_env)
        return -1;

    if (!nvrecord_env_p)
        return -1;

    nv_record_config.is_update = true;

    return 0;
}

uint32_t nv_record_get_tws_mode(void)
{
    return nvrecord_env_p->tws_mode.mode;
}

void nv_record_update_tws_mode(uint32_t newMode)
{
    if (nvrecord_env_p)
    {
        LOG_PRINT_NVREC("##%s,%d",__func__,newMode);
        nvrecord_env_p->tws_mode.originalMode = newMode;
        nvrecord_env_p->tws_mode.mode = newMode;
        nv_record_config.is_update = true;
    }
}

int nv_record_env_reset(void)
{
    struct nvrecord_env_t *nvrecord_env = NULL;
    nv_record_env_get(&nvrecord_env);

    nv_record_sector_clear();
    nv_record_open(section_usrdata_ddbrecord);

    if(nvrecord_env) {
#ifdef __TWS__      
        nvrecord_env->tws_mode.originalMode = NVRAM_ENV_TWS_MODE_DEFAULT;
        nvrecord_env->tws_mode.mode = NVRAM_ENV_TWS_MODE_DEFAULT;
#endif
        
        nvrecord_env->factory_tester_status.status = NVRAM_ENV_FACTORY_TESTER_STATUS_DEFAULT;
    }
    nv_record_env_set(nvrecord_env);
#if FPGA==0 
    nv_record_flash_flush();
#endif
    return 0;
}

uint16_t current_tws_mode(void)
{
    return (uint16_t)tws_mode_ptr()->mode;
}

