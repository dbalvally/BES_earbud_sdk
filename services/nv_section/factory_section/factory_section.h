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
#ifndef __FACTORY_SECTIONS_H__
#define __FACTORY_SECTIONS_H__

#define ALIGN4 __attribute__((aligned(4)))
#define nvrec_dev_version   1
#define nvrec_dev_magic      0xba80

typedef struct{
    unsigned short magic;
    unsigned short version;
    unsigned int crc ;
    unsigned int reserved0;
    unsigned int reserved1;
}section_head_t;

typedef struct{
    unsigned char device_name[248+1] ALIGN4;
    unsigned char bt_address[8] ALIGN4;
    unsigned char ble_address[8] ALIGN4;
    unsigned char tester_address[8] ALIGN4;
    unsigned int  xtal_fcap ALIGN4;
}factory_section_data_t;

typedef struct{
    section_head_t head;
    factory_section_data_t data;
}factory_section_t;

#ifdef __cplusplus
extern "C" {
#endif

int factory_section_open(void);
factory_section_data_t *factory_section_data_ptr_get(void);
void factory_section_original_btaddr_get(uint8_t *btAddr);
int factory_section_xtal_fcap_get(unsigned int *xtal_fcap);
int factory_section_xtal_fcap_set(unsigned int xtal_fcap);

#ifdef __cplusplus
}
#endif
#endif
