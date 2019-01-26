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
#include "plat_types.h"
#include "hal_i2c.h"
#include "hal_uart.h"
#include "hal_chipid.h"
#include "bt_drv.h"
#include "bt_drv_interface.h"

extern void btdrv_write_memory(uint8_t wr_type,uint32_t address,const uint8_t *value,uint8_t length);

///enable m4 patch func
#define BTDRV_PATCH_EN_REG   0xe0002000

//set m4 patch remap adress
#define BTDRV_PATCH_REMAP_REG    0xe0002004

////instruction patch compare src address
#define BTDRV_PATCH_INS_COMP_ADDR_START   0xe0002008

#define BTDRV_PATCH_INS_REMAP_ADDR_START   0xc0000100

////data patch compare src address
#define BTDRV_PATCH_DATA_COMP_ADDR_START   0xe00020e8

#define BTDRV_PATCH_DATA_REMAP_ADDR_START   0xc00001e0



#define BTDRV_PATCH_ACT   0x1
#define BTDRV_PATCH_INACT   0x0



typedef struct
{
    uint8_t patch_index;   //patch position
    uint8_t patch_state;   //is patch active
    uint16_t patch_length;     ///patch length 0:one instrution replace  other:jump to ram to run more instruction
    uint32_t patch_remap_address;   //patch occured address
    uint32_t patch_remap_value;      //patch replaced instuction
    uint32_t patch_start_address;    ///ram patch address for lenth>0
    uint8_t *patch_data;                  //ram patch date for length >0

}BTDRV_PATCH_STRUCT;

///////////////////ins  patch ..////////////////////////////////////



const uint32_t bes2300_patch0_ins_data[] =
{
    0x68134a04,
    0x3300f423,
    0x46206013,
    0xfa48f628,
    0xbcbdf618,
    0xd02200a4
};


const BTDRV_PATCH_STRUCT bes2300_ins_patch0 =
{
    0,
    BTDRV_PATCH_ACT,
    sizeof(bes2300_patch0_ins_data),
    0x0001ed88,
    0xbb3af1e7,
    0xc0006400,
    (uint8_t *)bes2300_patch0_ins_data
};/////test mode



const uint32_t bes2300_patch1_ins_data[] =
{
    0x52434313,
    0x03fff003,
    0xd0012b04,
    0x47702000,
    0x47702001,
};


const BTDRV_PATCH_STRUCT bes2300_ins_patch1 =
{
    1,
    BTDRV_PATCH_ACT,
    sizeof(bes2300_patch1_ins_data),
    0x00003044,
    0xb9ecf203,
    0xc0006420,
    (uint8_t *)bes2300_patch1_ins_data
};//inc power



const uint32_t bes2300_patch2_ins_data[] =
{
    0xf0035243,
    0x2b0003ff,
    0x2000d001,
    0x20014770,
    0xbf004770,
};


const BTDRV_PATCH_STRUCT bes2300_ins_patch2 =
{
    2,
    BTDRV_PATCH_ACT,
    sizeof(bes2300_patch2_ins_data),
    0x00003014,
    0xba14f203,
    0xc0006440,
    (uint8_t *)bes2300_patch2_ins_data
};//dec power


const BTDRV_PATCH_STRUCT bes2300_ins_patch3 =
{
    3,
    BTDRV_PATCH_ACT,
    0,
    0x00000494,
    0xbD702000,
    0,
    NULL
};//sleep


const uint32_t bes2300_patch4_ins_data[] =
{
    0x0008f109,
    0xbf0060b0,
    0xba30f626,
};


const BTDRV_PATCH_STRUCT bes2300_ins_patch4 =
{
    4,
    BTDRV_PATCH_ACT,
    sizeof(bes2300_patch4_ins_data),
    0x0002c8c8,
    0xbdcaf1d9,
    0xc0006460,
    (uint8_t *)bes2300_patch4_ins_data
};//ld data tx


const BTDRV_PATCH_STRUCT bes2300_ins_patch5 =
{
    5,
    BTDRV_PATCH_ACT,
    0,
    0x00019804,
    0xe0092b01,
    0,
    NULL
};//max slot req



const uint32_t bes2300_patch6_ins_data[] =
{
    0x0301f043,
    0xd1012b07,
    0xbc8af624,
    0xbc78f624,    
};

const BTDRV_PATCH_STRUCT bes2300_ins_patch6=
{
    6,
    BTDRV_PATCH_ACT,
    sizeof(bes2300_patch6_ins_data),
    0x0002ad88,
    0xbb72f1db,
    0xc0006470,
    (uint8_t *)bes2300_patch6_ins_data
};//ld data tx 3m



const BTDRV_PATCH_STRUCT bes2300_ins_patch7 =
{
    7,    
    BTDRV_PATCH_ACT,  
    0,    
    0x0003520c,  
    0xbf00e006,  
    0,  
    NULL
};///disable for error


const BTDRV_PATCH_STRUCT bes2300_ins_patch8 =
{
    8,    
    BTDRV_PATCH_ACT,  
    0,    
    0x00035cf4,  
    0xbf00e038,  
    0,  
    NULL
};//bt address ack

const BTDRV_PATCH_STRUCT bes2300_ins_patch9 =
{
    9,
    BTDRV_PATCH_ACT,
    0,
    0x000273a0,
    0x789a3100,
    0,
    NULL
};///no sig tx test

static const uint32_t best2300_ins_patch_config[] =
{
    10,
    (uint32_t)&bes2300_ins_patch0,
    (uint32_t)&bes2300_ins_patch1,
    (uint32_t)&bes2300_ins_patch2,
    (uint32_t)&bes2300_ins_patch3,
    (uint32_t)&bes2300_ins_patch4,
    (uint32_t)&bes2300_ins_patch5,
    (uint32_t)&bes2300_ins_patch6,
    (uint32_t)&bes2300_ins_patch7,
    (uint32_t)&bes2300_ins_patch8,
    (uint32_t)&bes2300_ins_patch9,
};


///PATCH RAM POSITION
///PATCH0   0xc0006690--C000669C
///PATCH2   0XC0006920--C0006928
///PATCH3   0XC0006930--C0006944
///PATCH4   0XC0006950--C0006958
///PATCH5   0XC0006980--C00069E4
///PATCH6   0XC00069F0--C0006A0C
///PATCH7   0XC0006A28--C0006A4C
///PATCH9   0xc0006960 --0xc0006978
///PATCH11  0xc0006ca0 --0xc0006cb4
///PATCH12  0xc0006cc0 --0xc0006cd4
///PATCH14  0XC0006800--C0006810
///PATCH15  0XC0006810--C0006820
///PATCH16  0XC0006a60--C0006a74 //(0XC00067f0--C0006800)empty
///PATCH17  0XC00068f0--C0006914
///PATCH19  0xc0006704--c00067ec
///PATCH22  0xc0006848--0xc00068e0
///PATCH25  0xc0006a18--0xc0006a20
///PATCH26  0xc0006670--0xc0006680
///PATCH28  0xc00066b0--0xc00066F8
///PATCH35  0XC0006A54--0xc0006a60
///PATCH42  0XC0006848--0xc0006864
///patch36  0xc0006a80-0xc0006a94
///patch37  0xc0006aa0 --0xc0006ab0
///patch38  0xc0006ac0--0xc0006aec
///patch40  0xc0006af0--0xc0006b00
///patch43  0xc0006be0--0xc0006bf8
///patch44  0xc0006c00--0xc0006c44
///patch46  0xc0006c50--0xc0006c8c
///patch47  0xc0006c98--0xc0006db0

#define BT_FORCE_3M
#define BT_VERSION_50
#define BT_SNIFF_SCO
#define BT_MS_SWITCH
#define BT_DEBUG_PATCH
////
const uint32_t bes2300_patch0_ins_data_2[] =
{
    0x0008f109,
    0xbf0060b0,
    0xba12f627,
};


const BTDRV_PATCH_STRUCT bes2300_ins_patch0_2 =
{
    0,
    BTDRV_PATCH_ACT,
    sizeof(bes2300_patch0_ins_data_2),
    0x0002dabc,
    0xbde8f1d8,
    0xc0006690,
    (uint8_t *)bes2300_patch0_ins_data_2
};//ld data tx


const BTDRV_PATCH_STRUCT bes2300_ins_patch1_2 =
{
    1,
    BTDRV_PATCH_ACT,
    0,
    0x0000e40c,
    0xbf00e0ca,
    0,
    NULL
};///fix sniff re enter 

const uint32_t bes2300_patch2_ins_data_2[] =
{
    0xbf00b2c8,
    0xba02f60e,
};

const BTDRV_PATCH_STRUCT bes2300_ins_patch2_2 =
{
    2, 
    BTDRV_PATCH_ACT,
    sizeof(bes2300_patch2_ins_data_2),
    0x00014d24,
    0xbdfcf1f1,
    0xc0006920,
    (uint8_t *)bes2300_patch2_ins_data_2
};/*lc_unsniff() lmp_unsniff anchor point*/


const uint32_t bes2300_patch3_ins_data_2[] =
{
    0x0080f105, 
    0xf60f2101, 
    0x8918fe59,
    0xfe8ef600,
    0xb90ef610,
};


const BTDRV_PATCH_STRUCT bes2300_ins_patch3_2 =
{
    3,
    BTDRV_PATCH_ACT,
    sizeof(bes2300_patch3_ins_data_2),
    0x00016b28,
    0xbf02f1ef,
    0xc0006930,
    (uint8_t *)bes2300_patch3_ins_data_2
};//free buff 

const uint32_t bes2300_patch4_ins_data_2[] =
{
    0x091ff04f,
    0xbd93f628,
};


const BTDRV_PATCH_STRUCT bes2300_ins_patch4_2 =
{
    4,
    BTDRV_PATCH_ACT,
    sizeof(bes2300_patch4_ins_data_2),
    0x0002f474,
    0xba6cf1d7,
    0xc0006950,
    (uint8_t *)bes2300_patch4_ins_data_2
};//curr prio assert




const uint32_t bes2300_patch5_ins_data_2[] =
{
    0x3002f898,
    0x03c3eb07,
    0x306cf893,
    0x4630b333,
    0xf60b2100,
    0xf640fedd,
    0x21000003,
    0x230e2223,
    0xf918f5fa,
    0x70042400,
    0x80453580,
    0x2002f898,
    0xeb07320d,
    0xf85707c2,
    0x687a1f04,
    0x1006f8c0,
    0x200af8c0,
    0x2002f898,
    0xf8987102,
    0x71422002,
    0xff28f63c,
    0xe8bd2000,
    0xbf0081f0,
    0xe8bd2002,
    0xbf0081f0,
};

const BTDRV_PATCH_STRUCT bes2300_ins_patch5_2 =
{
    5,
    BTDRV_PATCH_ACT,
    sizeof(bes2300_patch5_ins_data_2),
    0x00020b00,
    0xbf3ef1e5,
    0xc0006980,
    (uint8_t *)bes2300_patch5_ins_data_2
};//slave feature rsp


const uint32_t bes2300_patch6_ins_data_2[] =
{
    0xf4038873,
    0xf5b34370,
    0xd0055f80,
    0xf1058835,
    0xbf004550,
    0xbe1bf61a,
    0xbe42f61a,
};


const BTDRV_PATCH_STRUCT bes2300_ins_patch6_2 =
{
    6,
    BTDRV_PATCH_ACT,
    sizeof(bes2300_patch6_ins_data_2),
    0x00021638,
    0xb9daf1e5,
    0xc00069f0,
    (uint8_t *)bes2300_patch6_ins_data_2
};///continue packet error


const uint32_t bes2300_patch7_ins_data_2[] =
{   
    0xf003783b,
    0x2b000301,
    0x4630d108,
    0x22242117,
    0xfbe4f61b,
    0xbf002000,
    0xbf36f613,
    0xf86cf5fa,
    0xbec6f613,
};

const BTDRV_PATCH_STRUCT bes2300_ins_patch7_2 =
{
    7,
    BTDRV_PATCH_ACT,
    sizeof(bes2300_patch7_ins_data_2),    
    0x0001a7d4,
    0xb928f1ec,
    0xc0006a28,
    (uint8_t *)bes2300_patch7_ins_data_2
};//sniff req collision


const BTDRV_PATCH_STRUCT bes2300_ins_patch8_2 =
{
    8,
    BTDRV_PATCH_ACT,
    0,
    0x00035948,
    0x124cf640,
    0,
    NULL
};//acl evt dur 4 slot


#if 0

const uint32_t bes2300_patch9_ins_data_2[] =
{
    0x2b00b29b,
    0xf896d006,
    0x330130bb,
    0x30bbf886,
    0xb9c0f626,
    0xb8c8f626,
};


const BTDRV_PATCH_STRUCT bes2300_ins_patch9_2 =
{
    9,
    BTDRV_PATCH_ACT,
    sizeof(bes2300_patch9_ins_data_2),
    0x0002cb00,
    0xbf2ef1d9,
    0xc0006960,
    (uint8_t *)bes2300_patch9_ins_data_2
};//seq error 
#else

const uint32_t bes2300_patch9_ins_data_2[] =
{
    0xbf00d106,
    0xfdb0f609,
    0xe0012801,
    0xbe30f621,
    0xbee6f621,
};


const BTDRV_PATCH_STRUCT bes2300_ins_patch9_2 =
{
    9,
    BTDRV_PATCH_ACT,
    sizeof(bes2300_patch9_ins_data_2),
    0x000285cc,
    0xb9c8f1de,
    0xc0006960,
    (uint8_t *)bes2300_patch9_ins_data_2
};
#endif

const BTDRV_PATCH_STRUCT bes2300_ins_patch10_2 =
{
    10,
    BTDRV_PATCH_ACT,
    0,
    0x0004488c,
    0x0212bf00,
    0,
    NULL
};///acl data rx error when disconnect


#ifdef BT_VERSION_50

#if 1
const BTDRV_PATCH_STRUCT bes2300_ins_patch11_2 =
{
    11,
    BTDRV_PATCH_ACT,
    0,
    0x000227ec,
    0xf88d2309,
    0,
    NULL
};//version



const BTDRV_PATCH_STRUCT bes2300_ins_patch12_2 =
{
    12,
    BTDRV_PATCH_ACT,
    0,
    0x000170a0,
    0x23093004,
    0,
    NULL
};//version
#else

const uint32_t bes2300_patch11_ins_data_2[] =
{
    0xf88d2309,
    0x4b023001,
    0xbf00681b,
    0xbda2f61b,
    0xc0006cb4,
    0x000002b0
};


const BTDRV_PATCH_STRUCT bes2300_ins_patch11_2 =
{
    11,
    BTDRV_PATCH_ACT,
    sizeof(bes2300_patch11_ins_data_2),
    0x000227ec,
    0xba58f1e4,
    0xc0006ca0,
    (uint8_t *)bes2300_patch11_ins_data_2
};

const uint32_t bes2300_patch12_ins_data_2[] =
{
    0xf88d2309,
    0x4b023001,
    0xbf00681b,
    0xb9edf610,
    0xc0006cd4,
    0x000002b0
};


const BTDRV_PATCH_STRUCT bes2300_ins_patch12_2 =
{
    12,
    BTDRV_PATCH_ACT,
    sizeof(bes2300_patch12_ins_data_2),
    0x000170a4,
    0xbe0cf1ef,
    0xc0006cc0,
    (uint8_t *)bes2300_patch12_ins_data_2
};

#endif
#else

const BTDRV_PATCH_STRUCT bes2300_ins_patch11_2 =
{
    11,
    BTDRV_PATCH_INACT,
    0,
    0,
    0,
    0,
    NULL
};

const BTDRV_PATCH_STRUCT bes2300_ins_patch12_2 =
{
    12,
    BTDRV_PATCH_INACT,
    0,
    0,
    0,
    0,
    NULL
};
#endif


#ifdef BLE_POWER_LEVEL_0
const BTDRV_PATCH_STRUCT bes2300_ins_patch13_2 =
{
    13,
    BTDRV_PATCH_ACT,
    0,
    0x0003e4b8,
    0x7f032600,
    0,
    NULL
};//adv power level


const uint32_t bes2300_patch14_ins_data_2[] =
{
    0x32082100, ///00 means used the power level 0
    0x250052a1, //00 means used the power level 0
    0xb812f639,
};


const BTDRV_PATCH_STRUCT bes2300_ins_patch14_2 =
{
    14,
    BTDRV_PATCH_ACT,
    sizeof(bes2300_patch14_ins_data_2),
    0x0003f824,
    0xbfecf1c6,
    0xc0006800,
    (uint8_t *)bes2300_patch14_ins_data_2
};//slave power level

const uint32_t bes2300_patch15_ins_data_2[] =
{
    0xf8282100, ///00 means used the power level 0
    0xf04f1002, 
    0xbf000a00,//00 means used the power level 0
    0xbbdcf638,
};


const BTDRV_PATCH_STRUCT bes2300_ins_patch15_2 =
{
    15,
    BTDRV_PATCH_ACT,
    sizeof(bes2300_patch15_ins_data_2),
    0x0003efd0,
    0xbc1ef1c7,
    0xc0006810,
    (uint8_t *)bes2300_patch15_ins_data_2
};//master

#else
const BTDRV_PATCH_STRUCT bes2300_ins_patch13_2 =
{
    13,
    BTDRV_PATCH_INACT,
    0,
    0,
    0,
    0,
    NULL
};

const BTDRV_PATCH_STRUCT bes2300_ins_patch14_2 =
{
    14,
    BTDRV_PATCH_INACT,
    0,
    0,
    0,
    0,
    NULL
};


const BTDRV_PATCH_STRUCT bes2300_ins_patch15_2 =
{
    15,
    BTDRV_PATCH_INACT,
    0,
    0,
    0,
    0,
    NULL
};

#endif


#ifdef BT_SNIFF_SCO
const uint32_t bes2300_patch16_ins_data_2[] =
{
    0xf62fb508,	
    0xf62ff9cb,	
    0x4b01f9e5,	
    0xba00f62f,
    0xc00061c8,
};

const BTDRV_PATCH_STRUCT bes2300_ins_patch16_2 =
{
    16,
    BTDRV_PATCH_ACT,
    sizeof(bes2300_patch16_ins_data_2),
    0x00035e6c,
    0xbdf8f1d0,
    0xc0006a60,
    (uint8_t *)bes2300_patch16_ins_data_2
};//send bitoff when start sniffer sco


const uint32_t bes2300_patch17_ins_data_2[] =
{
    0x4b07b955,
    0x2042f893,
    0xbf004b04,
    0x3022f853,
    0x4630b113,
    0xbc7cf62f,
    0xbc83f62f,
    0xc0005b48,
    0xc00061c8,
};

const BTDRV_PATCH_STRUCT bes2300_ins_patch17_2 =
{
    17,       
    BTDRV_PATCH_ACT,
    sizeof(bes2300_patch17_ins_data_2),
    0x000361fc,
    0xbb78f1d0,
    0xc00068f0,
    (uint8_t *)bes2300_patch17_ins_data_2
};//stop sco Assert

const BTDRV_PATCH_STRUCT bes2300_ins_patch18_2 =
{
    18,
    BTDRV_PATCH_ACT,
    0,
    0x00030420,
    0xbd702001,
    0,
    NULL
};//sco param assert


#ifdef __3RETX_SNIFF__        
const uint32_t bes2300_patch19_ins_data_2[] =
{
    0x47f0e92d,
    0x4614460f,
    0xd9492a02,
    0x25004682,
    0xf04f46a8,
    0xb2ee0901,
    0x42b37fbb,
    0x4630d03d,
    0xf6054641,
    0xb920fbd7,
    0x46494630,
    0xfbd2f605,
    0x4b26b398,
    0x0026f853,
    0x2b036883,
    0x2100bf94,
    0xebca2101,
    0xf0220201,
    0xf1b54578,
    0xbf886f80,
    0x020aebc1,
    0x4278f022,
    0xbf183200,
    0xb1fa2201,
    0x0203ebca,
    0x4178f022,
    0x6f80f1b1,
    0xebc3bf83,
    0xf023030a,
    0x425b4378,
    0x4378f022,
    0x42937eba,
    0xf890dc0e,
    0x2b0330b5,
    0x2c04d104,
    0x2c03d019,
    0xe010d106,
    0xd1032c04,
    0x3501e00f,
    0xd1ba2d03,
    0x04247ffb,
    0x490a015b,
    0xf422585a,
    0x4314027f,
    0xe8bd505c,
    0x240387f0,
    0x2403e000,
    0x04247ffb,
    0x7ffbe7f0,
    0x3400f44f,
    0xbf00e7ec,
    0xc00008fc,
    0xd0220150,
    0x68a04602,
    0x0128f104,
    0xff90f7ff,
    0x30b3f895,
    0xbe8af628,
};

const BTDRV_PATCH_STRUCT bes2300_ins_patch19_2 =
{
    19,
    BTDRV_PATCH_ACT,
    sizeof(bes2300_patch19_ins_data_2),
    0x0002f4f0,
    0xb972f1d7,
    0xc0006704,
    (uint8_t *)bes2300_patch19_ins_data_2
};//esco retx 3 dec 2
#else

const uint32_t bes2300_patch19_ins_data_2[] =
{
    0x3026f853,
	0xf8842000,
	0xbf0002fa,
	0xba2af60f,
};

const BTDRV_PATCH_STRUCT bes2300_ins_patch19_2 =
{
    19,
    BTDRV_PATCH_ACT,
    sizeof(bes2300_patch19_ins_data_2),
    0x00015b64,
    0xbdcef1f0,
    0xc0006704,
    (uint8_t *)bes2300_patch19_ins_data_2
};//bt role switch clear rswerror at lc_epr_cmp

#endif


const BTDRV_PATCH_STRUCT bes2300_ins_patch20_2 =
{
   20,
#ifdef __3RETX_SNIFF__        
    BTDRV_PATCH_ACT,
#else
    BTDRV_PATCH_INACT,
#endif
    0,
    0x00010be8,
    0xb81ef000,
    0,
    NULL
};//support 2/3 esco retx 

const BTDRV_PATCH_STRUCT bes2300_ins_patch21_2 =
{
    21,
#ifdef __3RETX_SNIFF__        
    BTDRV_PATCH_ACT,
#else
    BTDRV_PATCH_INACT,
#endif
    0,
    0x0000e4dc,
    0xb808f000,
    0,
    NULL
};//in eSCO support LM_AddSniff()


#if 0
const uint32_t bes2300_patch22_ins_data_2[] =
{
    0xf8534b1e,
    0x4b1e2020,
    0xb913681b,
    0x685b4b1c,
    0xf893b393,
    0x42811046,
    0xb430d02e,
    0x00d4f8d2,
    0x0401f000,
    0x42886b59,
    0xf893d30a,
    0x1d0d3042,
    0xfbb11a41,
    0xfb03f1f3,
    0xf0235301,
    0xe0094378,
    0x3042f893,
    0x1a091f0d,
    0xf1f3fbb1,
    0x5311fb03,
    0x4378f023,
    0xf013b934,
    0xd00a0f01,
    0xf0233b01,
    0xe0064378,
    0x0f01f013,
    0xf103bf04,
    0xf02333ff,
    0xf8c24378,
    0xbc3030d4,
    0xbf004770,
    0xc00008fc,
    0xc00008ec,
    0xbf004638,
    0xffbaf7ff,
    0x236ebf00,
    0xf707fb03,
    0xb9f4f621,
};

const BTDRV_PATCH_STRUCT bes2300_ins_patch22_2 =
{
    22,
#ifdef __3RETX_SNIFF__        
    BTDRV_PATCH_INACT,
#else
    BTDRV_PATCH_INACT,
#endif
    sizeof(bes2300_patch22_ins_data_2),
    0x00027cc4,
    0xbe02f1de,
    0xc0006848,
    (uint8_t *)bes2300_patch22_ins_data_2
};//In eSCO sniff anchor point calculate
#else
const BTDRV_PATCH_STRUCT bes2300_ins_patch22_2 =
{
    22,
    BTDRV_PATCH_ACT,
    0,
    0x0002b3f8,
    0xbf00bf00,
    0,
    NULL
};///remove PCM EN in controller
#endif
#else
const BTDRV_PATCH_STRUCT bes2300_ins_patch16_2 =
{
    16,
    BTDRV_PATCH_INACT,
    0,
    0,
    0,
    0,
    NULL
};

const BTDRV_PATCH_STRUCT bes2300_ins_patch17_2 =
{
    17,
    BTDRV_PATCH_INACT,
    0,
    0,
    0,
    0,
    NULL
};

const BTDRV_PATCH_STRUCT bes2300_ins_patch18_2 =
{
    18,
    BTDRV_PATCH_INACT,
    0,
    0,
    0,
    0,
    NULL
};

const BTDRV_PATCH_STRUCT bes2300_ins_patch19_2 =
{
    19,
    BTDRV_PATCH_INACT,
    0,
    0,
    0,
    0,
    NULL
};

const BTDRV_PATCH_STRUCT bes2300_ins_patch20_2 =
{
    20,
    BTDRV_PATCH_INACT,
    0,
    0,
    0,
    0,
    NULL
};


const BTDRV_PATCH_STRUCT bes2300_ins_patch21_2 =
{
    21,
    BTDRV_PATCH_INACT,
    0,
    0,
    0,
    0,
    NULL
};

const BTDRV_PATCH_STRUCT bes2300_ins_patch22_2 =
{
    22,
    BTDRV_PATCH_INACT,
    0,
    0,
    0,
    0,
    NULL
};

#endif

#ifdef BT_MS_SWITCH

const BTDRV_PATCH_STRUCT bes2300_ins_patch23_2 =
{
    23,
    BTDRV_PATCH_ACT,
    0,
    0x000362c0,
    0x93012300,
    0,
    NULL
};//wipe acl lt addr

const BTDRV_PATCH_STRUCT bes2300_ins_patch24_2 =
{
    24,
    BTDRV_PATCH_ACT,
    0,
    0x0002f1bc,
    0xb804f000,
    0,
    NULL
};//clear fnak bit


const uint32_t bes2300_patch25_ins_data_2[] =
{   
    0x30fff04f,   
    0xbf00bd70,
};

const BTDRV_PATCH_STRUCT bes2300_ins_patch25_2 =
{    
    25,    
    BTDRV_PATCH_ACT,    
    sizeof(bes2300_patch25_ins_data_2),    
    0x00030cc4,    
    0xbea8f1d5,    
    0xc0006a18,    
    (uint8_t *)bes2300_patch25_ins_data_2
};// acl switch



#else

const BTDRV_PATCH_STRUCT bes2300_ins_patch23_2 =
{
    23,
    BTDRV_PATCH_INACT,
    0,
    0,
    0,
    0,
    NULL
};

const BTDRV_PATCH_STRUCT bes2300_ins_patch24_2 =
{
    24,
    BTDRV_PATCH_INACT,
    0,
    0,
    0,
    0,
    NULL
};

const BTDRV_PATCH_STRUCT bes2300_ins_patch25_2 =
{
    25,
    BTDRV_PATCH_INACT,
    0,
    0,
    0,
    0,
    NULL
};


#endif


const uint32_t bes2300_patch26_ins_data_2[] =
{
    0x0301f043,
    0xd1012b07,
    0xbc00f625,
    0xbbeef625,    
};



const BTDRV_PATCH_STRUCT bes2300_ins_patch26_2=
{
    26,
    BTDRV_PATCH_INACT,
    sizeof(bes2300_patch26_ins_data_2),
    0x0002be74,
    0xbbfcf1da,
    0xc0006670,
    (uint8_t *)bes2300_patch26_ins_data_2
};//ld data tx 3m


const BTDRV_PATCH_STRUCT bes2300_ins_patch27_2 =
{
    27,
#ifdef __CLK_GATE_DISABLE__        
    BTDRV_PATCH_ACT,
#else
    BTDRV_PATCH_INACT,
#endif
    0,
    0x000061a8,
    0xbf30bf00,
    0,
    NULL
};///disable clock gate

#ifdef BT_DEBUG_PATCH
const uint32_t bes2300_patch28_ins_data_2[] =
{
    0x2b0b7b23,
    0xbf00d118,
    0x3b017b63,
    0x2b01b2db,
    0xbf00d812,
    0x010ef104,
    0xb1437ba3,
    0x33012200,
    0xb2d2441a,
    0x780b4419,
    0xd1f82b00,
    0x2200e000,
    0xf8ad3202,
    0xbf002006,
    0x72a323ff,
    0x3006f8bd,
    0xbf0072e3,
    0xbe10f63d,
};

const BTDRV_PATCH_STRUCT bes2300_ins_patch28_2 =
{
    28,
    BTDRV_PATCH_ACT,
    sizeof(bes2300_patch28_ins_data_2),
    0x00044310,
    0xb9cef1c2,
    0xc00066b0,
    (uint8_t *)bes2300_patch28_ins_data_2
};//hci_build_dbg_evt for lmp record   ///can close when release sw 

const BTDRV_PATCH_STRUCT bes2300_ins_patch29_2 =
{
    29,
    BTDRV_PATCH_ACT,
    0,
    0x00000650,
    0xbdfcf005,
    0,
    NULL
};//enter default handler when assert

const BTDRV_PATCH_STRUCT bes2300_ins_patch30_2 =
{
    30,
    BTDRV_PATCH_ACT,
    0,
    0x000062c4,
    0xfffef3ff,
    0,
    NULL
};//bus fault handler while(1)

const BTDRV_PATCH_STRUCT bes2300_ins_patch31_2 =
{
    31,
    BTDRV_PATCH_INACT,
    0,
    0x0002f4d4,
    0xbf002301,
    0,
    NULL
};//force retx to 1


const BTDRV_PATCH_STRUCT bes2300_ins_patch32_2 =
{
    32,
    BTDRV_PATCH_INACT,
    0,
    0x00021de0,
    0xdd212b05,
    0,
    NULL
}; ///lmp record 


const BTDRV_PATCH_STRUCT bes2300_ins_patch33_2 =
{
    33,
    BTDRV_PATCH_INACT,
    0,
    0x00021e78,
    0xdd212b05,
    0,
    NULL
};//lmp record

#else
const BTDRV_PATCH_STRUCT bes2300_ins_patch28_2 =
{
    28,
    BTDRV_PATCH_INACT,
    0,
    0,
    0,
    0,
    NULL
};


const BTDRV_PATCH_STRUCT bes2300_ins_patch29_2 =
{
    29,
    BTDRV_PATCH_INACT,
    0,
    0,
    0,
    0,
    NULL
};


const BTDRV_PATCH_STRUCT bes2300_ins_patch30_2 =
{
    30,
    BTDRV_PATCH_INACT,
    0,
    0,
    0,
    0,
    NULL
};


const BTDRV_PATCH_STRUCT bes2300_ins_patch31_2 =
{
    31,
    BTDRV_PATCH_INACT,
    0,
    0,
    0,
    0,
    NULL
};


const BTDRV_PATCH_STRUCT bes2300_ins_patch32_2 =
{
    32,
    BTDRV_PATCH_INACT,
    0,
    0,
    0,
    0,
    NULL
};


const BTDRV_PATCH_STRUCT bes2300_ins_patch33_2 =
{
    33,
    BTDRV_PATCH_INACT,
    0,
    0,
    0,
    0,
    NULL
};




#endif


///can close when there is no patch position
const BTDRV_PATCH_STRUCT bes2300_ins_patch34_2 =
{
    34,
    BTDRV_PATCH_ACT,
    0,
    0x00017708,
    0x700ef247,
    0,
    NULL
};//set afh timeout to 20s

const uint32_t bes2300_patch35_ins_data_2[] =
{
    0x4778f027,
    0x708cf8c4,
    0xbc86f621,
};

const BTDRV_PATCH_STRUCT bes2300_ins_patch35_2 =
{
    35,
    BTDRV_PATCH_ACT,
    sizeof(bes2300_patch35_ins_data_2),    
    0x00028368,
    0xbb74f1de,
    0xc0006a54,
    (uint8_t *)bes2300_patch35_ins_data_2
};//ld_acl_rx_sync

const uint32_t bes2300_patch36_ins_data_2[] =
{
    0xfbb4f61d,
    0x60184b01,
    0x81f0e8bd,
    0xc0006a90,
    0x00000000,
};

const BTDRV_PATCH_STRUCT bes2300_ins_patch36_2 =
{
    36,
    BTDRV_PATCH_ACT,
    sizeof(bes2300_patch36_ins_data_2),    
    0x0004438c,
    0xbb78f1c2,
    0xc0006a80,
    (uint8_t *)bes2300_patch36_ins_data_2
};//hci timout 1:h4tl_write record clock


const uint32_t bes2300_patch37_ins_data_2[] =
{
    0xfba4f61d,
    0x60184b01,
    0xbf00bd70,
    0xc0006a90,
};

const BTDRV_PATCH_STRUCT bes2300_ins_patch37_2 =
{
    37,
    BTDRV_PATCH_ACT,
    sizeof(bes2300_patch37_ins_data_2),    
    0x00005e10,
    0xbe46f200,
    0xc0006aa0,
    (uint8_t *)bes2300_patch37_ins_data_2
};//hci timout 2:h4tl_rx_done recored clock

const uint32_t bes2300_patch38_ins_data_2[] =
{
    0x2b007b1b,
    0xbf00d00c,
    0xfb90f61d,
    0x681b4b05,
    0xf0201ac0,
    0xf5b04078,
    0xd9016f80,
    0xbbb6f5f9,
    0xbcdaf5f9,
    0xc0006a90,
};

const BTDRV_PATCH_STRUCT bes2300_ins_patch38_2 =
{
    38,
    BTDRV_PATCH_ACT,
    sizeof(bes2300_patch38_ins_data_2),    
    0x00000244,
    0xbc3cf206,
    0xc0006ac0,
    (uint8_t *)bes2300_patch38_ins_data_2
};//hci timout 3,sleep check

#if 0
const uint32_t bes2300_patch39_ins_data_2[] =
{
    0xf8534b22,
    0x4b222020,
    0xb913681b,
    0x685b4b20,
    0xf893b3d3,
    0x42811046,
    0xb410d036,
    0x00d4f8d2,
    0x42886b59,
    0x1a41d317,
    0x4042f893,
    0xf3f4fbb1,
    0x1113fb04,
    0xd8042904,
    0xf0231c83,
    0xf8c24378,
    0x290730d4,
    0xf8d2d91e,
    0x3b0230d4,
    0x4378f023,
    0x30d4f8c2,
    0x1a09e016,
    0x4042f893,
    0xf3f4fbb1,
    0x1113fb04,
    0xd8042904,
    0xf0231e83,
    0xf8c24378,
    0x290730d4,
    0xf8d2d906,
    0x330230d4,
    0x4378f023,
    0x30d4f8c2,
    0x4b04f85d,
    0xbf004770,
    0xc00008fc,
    0xc00008ec,
    0xbf009801,
    0xffb2f7ff,
    0x30d4f8d4,
    0xbf1af622,
};

const BTDRV_PATCH_STRUCT bes2300_ins_patch39_2 =
{
    39,
 #ifdef __3RETX_SNIFF__        
    BTDRV_PATCH_INACT,
#else
    BTDRV_PATCH_INACT,
#endif
    sizeof(bes2300_patch39_ins_data_2),    
    0x000299e0,
    0xb8def1dd,
    0xc0006b0c,
    (uint8_t *)bes2300_patch39_ins_data_2
};//ld_acl_sniff_sched
#else
const BTDRV_PATCH_STRUCT bes2300_ins_patch39_2 =
{
    39,
    BTDRV_PATCH_ACT,
    0,
    0x0002b0dc,
    0x491d0300,
    0,
    NULL
};///remove PCM EN in controller
#endif

//////////////////////////////////////////////////
///meybe no used patch
#if 0
const BTDRV_PATCH_STRUCT bes2300_ins_patch16_2 =
{
    16,
#ifdef APB_PCM     
    BTDRV_PATCH_ACT,
#else
    BTDRV_PATCH_INACT,
#endif
    0,
    0x0002b3f8,
    0xbf00bf00,
    0,
    NULL
};///remove PCM EN in controller

const BTDRV_PATCH_STRUCT bes2300_ins_patch17_2 =
{
    17,
#ifdef APB_PCM     
    BTDRV_PATCH_ACT,
#else
    BTDRV_PATCH_INACT,
#endif
    0,
    0x0002b0dc,
    0x491d0300,
    0,
    NULL
};///remove PCM EN in controller



const uint32_t bes2300_patch18_ins_data_2[] =
{
    0x134cf640,
    0xf8b48263, 
    0xbf0020b8,
    0xbe62f622,
};


const BTDRV_PATCH_STRUCT bes2300_ins_patch18_2 =
{
    18,
    BTDRV_PATCH_INACT,
    sizeof(bes2300_patch18_ins_data_2),
    0x00029500,
    0xb996f1dd,
    0xc0006830,
    (uint8_t *)bes2300_patch18_ins_data_2
};///set acl in esco to 2 frame

#endif



const uint32_t bes2300_patch40_ins_data_2[] =
{
    0x134cf640,
    0xf8b48263, 
    0x68a320b8,
    0xbd03f622,
};

const BTDRV_PATCH_STRUCT bes2300_ins_patch40_2 =
{
    40,
    BTDRV_PATCH_ACT,
    sizeof(bes2300_patch40_ins_data_2),
    0x00029500,
    0xbaf6f1dd,
    0xc0006af0,
    (uint8_t *)bes2300_patch40_ins_data_2
};


const BTDRV_PATCH_STRUCT bes2300_ins_patch41_2 =
{
    41,
    BTDRV_PATCH_ACT,
    0,
    0x00027884,
    0xbf00e012,
    0,
    NULL
};//disable sscan

const uint32_t bes2300_patch42_ins_data_2[] =
{
    0x44137da3,
    0xbf0075a3,
    0xfa06f62f,
    0x4620b920,
    0xf0002101,
    0xb908fa1d,
    0xbb5bf623,
    0xbb68f623,
};

const BTDRV_PATCH_STRUCT bes2300_ins_patch42_2 =
{
    42,
    BTDRV_PATCH_ACT,
    sizeof(bes2300_patch42_ins_data_2),
    0x00029f14,
    0xbc98f1dc,
    0xc0006848,
    (uint8_t *)bes2300_patch42_ins_data_2
};

#ifdef __NEW_LARGE_SYNC_WIN__ 
const uint32_t bes2300_patch43_ins_data_2[] =
{
    0x789b4b07,
    0xd0032b00,
    0x7a9b4b05,
    0xd00242bb,
    0x66a3231a,
    0x2354e002,
    0xbf0066a3,
    0xbdcef620,
    0xc00061c8

};

const BTDRV_PATCH_STRUCT bes2300_ins_patch43_2 =
{
    43,
#ifdef __NEW_LARGE_SYNC_WIN__        
    BTDRV_PATCH_ACT,
#else
    BTDRV_PATCH_INACT,
#endif    
    sizeof(bes2300_patch43_ins_data_2),
    0x00027794,
    0xba22f1df,
    0xc0006bdc,
    (uint8_t *)bes2300_patch43_ins_data_2
};//ld_acl_rx_sync2

const uint32_t bes2300_patch44_ins_data_2[] =
{
    0x78924a12,
    0xd0032a00,
    0x7a924a10,
    0xd003454a,
    0x310d0c89,
    0xbe56f620,
    0xf1a12178,
    0xeba60354,
    0xb29b0353,
    0x4f00f413,
    0x2600d00c,
    0x2371f203,
    0x1c3ab29b,
    0x4778f022,
    0xb2f63601,
    0x4f00f413,
    0xe001d1f4,
    0xbe54f620,
    0xbe53f620,
    0xc00061c8

};

const BTDRV_PATCH_STRUCT bes2300_ins_patch44_2 =
{
    44,
#ifdef __NEW_LARGE_SYNC_WIN__        
    BTDRV_PATCH_ACT,
#else
    BTDRV_PATCH_INACT,
#endif    
    sizeof(bes2300_patch44_ins_data_2),
    0x000278c0,
    0xb99ef1df,
    0xc0006c00,
    (uint8_t *)bes2300_patch44_ins_data_2
};//ld_acl_rx_no_sync

#else
const uint32_t bes2300_patch43_ins_data_2[] =
{
    0x781b4b05,
    0xd00342bb,
    0x66a3231a,
    0xbf00e002,
    0x66a32354,
    0xbdd0f620,
    0xc0006bfc,
    0x000000ff,
};

const BTDRV_PATCH_STRUCT bes2300_ins_patch43_2 =
{
    43,
#ifdef __LARGE_SYNC_WIN__        
    BTDRV_PATCH_ACT,
#else
    BTDRV_PATCH_INACT,
#endif    
    sizeof(bes2300_patch43_ins_data_2),
    0x00027794,
    0xba24f1df,
    0xc0006be0,
    (uint8_t *)bes2300_patch43_ins_data_2
};

const uint32_t bes2300_patch44_ins_data_2[] =
{
    0x78124a10,
    0xd003454a,
    0x310d0c89,
    0xbe5af620,
    0xf1a12178,
    0xeba60354,
    0xb29b0353,
    0x4f00f413,
    0x2600d00c,
    0x2371f203,
    0x1c3ab29b,
    0x4778f022,
    0xb2f63601,
    0x4f00f413,
    0xe001d1f4,
    0xbe58f620,
    0xbe57f620,
    0xc0006bfc,
};

const BTDRV_PATCH_STRUCT bes2300_ins_patch44_2 =
{
    44,
#ifdef __LARGE_SYNC_WIN__        
    BTDRV_PATCH_ACT,
#else
    BTDRV_PATCH_INACT,
#endif    
    sizeof(bes2300_patch44_ins_data_2),
    0x000278c0,
    0xb99ef1df,
    0xc0006c00,
    (uint8_t *)bes2300_patch44_ins_data_2
};
#endif

#ifndef __SW_SEQ_FILTER__
const BTDRV_PATCH_STRUCT bes2300_ins_patch45_2 =
{
    45,
    BTDRV_PATCH_ACT,
    0,
    0x0001e3d0,
    0xbf00bf00,
    0,
    NULL,
};//lmp_accept_ext_handle assert
#else
const uint32_t bes2300_patch45_ins_data_2[] =
{
    0xf8934b0b,
    0xebc331cb,
    0x005b03c3,
    0x5a9b4a09,
    0xf3c3b29b,
    0x4a082340,
    0x5c509903,
    0xd00328ff,
    0xd0034298,
    0xbf005453,
    0xbf92f625,
    0xb83af626,
    0xc0005c0c,
    0xd021159a,
    0xc0006c8c,
    0x02020202
};

const BTDRV_PATCH_STRUCT bes2300_ins_patch45_2 =
{
    45,
    BTDRV_PATCH_ACT,
    sizeof(bes2300_patch45_ins_data_2),
    0x0002cb7c,
    0xb868f1da,
    0xc0006c50,
    (uint8_t *)bes2300_patch45_ins_data_2
};


#endif

const BTDRV_PATCH_STRUCT bes2300_ins_patch46_2 =
{
    46,
    BTDRV_PATCH_ACT,
    0,
    0x0002b300,
    0xb2ad0540,
    0,
    NULL
};//sco duration

const uint32_t bes2300_patch47_ins_data_2[] =
{
    0x4605b5f8,//ld_calculate_acl_timestamp
    0xf609460e,
    0x2800fc13,
    0x4b2fd056,
    0x2c00681c,
    0xf894d054,
    0xf61d7044,
    0xf894fa9b,
    0x6b621042,
    0xf0221a52,
    0x42904278,
    0x1853d904,
    0x4298461a,
    0xe000d8fb,
    0x2e004613,
    0xf895d135,
    0x2a0120b3,
    0xf640d119,
    0x826a124c,
    0x4a203701,
    0xeb026812,
    0x44130247,
    0x4a1e60ab,
    0x42936812,
    0x1ad2bf34,
    0xf0221a9a,
    0x2a044278,
    0xf894d82a,
    0x44132042,
    0x200160ab,
    0xf640bdf8,
    0x826a124c,
    0x2042f894,
    0xbf282f02,
    0x37012702,
    0x10b8f8b5,
    0xf1f2fb91,
    0x2202fb01,
    0x0747eb02,
    0x68124a0e,
    0x443b4417,
    0x200160ab,
    0x2e01bdf8,
    0x68abd10c,
    0x60ab330c,
    0x75ab2306,
    0xbdf82001,
    0xbdf82000,
    0xbdf82000,
    0xbdf82001,
    0xbdf82001,
    0xc00008ec,
    0xc0006d74,
    0xc0006d78,// master timestamp addr
    0xc0006d7c,
    0x00000000,// extra_slave
    0x00000000,// master timestamp 
    0x00000000,// extra_master	
    0xff6ef62e,// jump function
    0x4620b118,
    0xf7ff2100,
    0x4620ff85,
    0xfbe0f63f,
    0x2300b950,
    0x30b4f884,
    0x30b3f894,
    0x68a2b913,
    0x601a4b02,
    0xbf00bdf8,
    0xbbe1f622,
    0xc0006d78,//master timestamp addr
};

const BTDRV_PATCH_STRUCT bes2300_ins_patch47_2 =
{
    47,
    BTDRV_PATCH_ACT,
    sizeof(bes2300_patch47_ins_data_2),
    0x00029564,
    0xbc0cf1dd,
    0xc0006c98,
    (uint8_t *)bes2300_patch47_ins_data_2
};//ld_calculate_acl_timestamp


const uint32_t bes2300_patch48_ins_data_2[] =
{
    0x30abf894, 
    0x490fb9d3, 
    0x4a10880b, 
    0xf4037810, 
    0x4303437f, 
    0x490d800b, 
    0x02006808, 
    0x88134a0a, 
    0xf423b29b, 
    0x430363e0, 
    0x68088013, 
    0x3001bf00,
    0xd8012802, 
    0xe0016008, 
    0x600a2200, 
    0x1090f8b4, 
    0xb9c2f63a, 
    0xd02101d6, 
    0xd02101d0, 
    0xc0006e10, 
    0x00000000, 


};

const BTDRV_PATCH_STRUCT bes2300_ins_patch48_2 =
{
    48,
#ifdef __DYNAMIC_ADV_POWER__        
    BTDRV_PATCH_ACT,
#else
    BTDRV_PATCH_INACT,
#endif
    sizeof(bes2300_patch48_ins_data_2),
    0x00041184,
    0xbe1cf1c5,
    0xc0006dc0,
    (uint8_t *)bes2300_patch48_ins_data_2
};


const uint32_t bes2300_patch49_ins_data_2[] =
{

    0xb083b500, 
    0x0101f001, 
    0x0148f041, 
    0x1004f88d, 
    0x2005f88d, 
    0xf61ba901, 
    0xb003f959, 
    0xfb04f85d, 
    0xbf004d0f, 
    0x28ff6828, 
    0x0200d016, 
    0x407ff400, 
    0x0001f040, 
    0xfafaf5fa, 
    0xd10d2801, 
    0x4b076828, 
    0x3020f853, 
    0xf893b2c0, 
    0x4b071040, 
    0xf7ff781a, 
    0x23ffffd7, 
    0xbf00602b, 
    0xb986f5ff, 
    0xc0005b48, 
    0xc0006e84, 
    0x000000ff, 
    0xc0006e8c, 
    0x00000000, 

};

const BTDRV_PATCH_STRUCT bes2300_ins_patch49_2 =
{
    49,
#ifdef __SEND_PREFERRE_RATE__        
    BTDRV_PATCH_ACT,
#else
    BTDRV_PATCH_INACT,
#endif
    sizeof(bes2300_patch49_ins_data_2),
    0x00006174,
    0xbe64f200,
    0xc0006E20,
    (uint8_t *)bes2300_patch49_ins_data_2
};


const uint32_t bes2300_patch50_ins_data_2[] =
{
    0xf5fa4628,
    0x4903fad3,
    0x46286008,
    0xbf002101,
    0xbd4af613,
    0xc0006eb8,
    0x00000000

};

const BTDRV_PATCH_STRUCT bes2300_ins_patch50_2 =
{
    50,
    BTDRV_PATCH_ACT,
    sizeof(bes2300_patch50_ins_data_2),
    0x0001a944,
    0xbaacf1ec,
    0xc0006ea0,
    (uint8_t *)bes2300_patch50_ins_data_2
};



const uint32_t bes2300_patch51_ins_data_2[] =
{
    0x0f00f1b8,
    0x4628d004,
    0x68094904,
    0xfa3af5fa,
    0x21134630,
    0x3044f894,
    0xbd6bf613,
    0xc0006eb8


};

const BTDRV_PATCH_STRUCT bes2300_ins_patch51_2 =
{
    51,
    BTDRV_PATCH_ACT,
    sizeof(bes2300_patch51_ins_data_2),
    0x0001a9dc,
    0xba88f1ec,
    0xc0006ef0,
    (uint8_t *)bes2300_patch51_ins_data_2
};


const uint32_t bes2300_patch52_ins_data_2[] =
{
    0xbf00251f,
    0xbe88f61e,
};


const BTDRV_PATCH_STRUCT bes2300_ins_patch52_2 =
{
    52,
    BTDRV_PATCH_ACT,
    sizeof(bes2300_patch52_ins_data_2),
    0x00025c20,
    0xb976f1e1,
    0xc0006f10,
    (uint8_t *)bes2300_patch52_ins_data_2
};

#ifdef __POWER_CONTROL_TYPE_1__
const uint32_t bes2300_patch53_ins_data_2[] =
{
    0xf0135243,
    0x2b00030f,
    0x2000bf14,
    0x47702001,
};


const BTDRV_PATCH_STRUCT bes2300_ins_patch53_2 =
{
    53,
    BTDRV_PATCH_ACT,
    sizeof(bes2300_patch53_ins_data_2),
    0x000033bc,
    0xbdb0f203,
    0xc0006f20,
    (uint8_t *)bes2300_patch53_ins_data_2
};//pwr controll
#else
    
const BTDRV_PATCH_STRUCT bes2300_ins_patch53_2 =
{
    53,
    BTDRV_PATCH_ACT,
    0,
    0x000033f4,
    0xe0072200,
    0,
    NULL
};
#endif

const uint32_t bes2300_patch54_ins_data_2[] =
{
    0x30b5f894,
    0xb2b2b973,     
    0x3098f8b4,
    0xbf8c429a,     
    0x1a9b1ad3,     
    0x7f00f5b3,
    0xf5c3bf84,     
    0x3301731c,
    0xd8052b0d,     
    0x3098f8b4,
    0x5088f8d4,
    0xbbeaf620,
    0xbf002000,     
    0x87f0e8bd,
};


const BTDRV_PATCH_STRUCT bes2300_ins_patch54_2 =
{
    54,
#ifdef BT_RF_OLD_CORR_MODE
    BTDRV_PATCH_INACT,
#else
    BTDRV_PATCH_INACT,
#endif
    sizeof(bes2300_patch54_ins_data_2),
    0x00027730,
    0xbbfef1df,
    0xc0006f30,
    (uint8_t *)bes2300_patch54_ins_data_2
};//bitoff check

static const uint32_t best2300_ins_patch_config_2[] =
{
   55,
    (uint32_t)&bes2300_ins_patch0_2,
    (uint32_t)&bes2300_ins_patch1_2,
    (uint32_t)&bes2300_ins_patch2_2,
    (uint32_t)&bes2300_ins_patch3_2,
    (uint32_t)&bes2300_ins_patch4_2,
    (uint32_t)&bes2300_ins_patch5_2,
    (uint32_t)&bes2300_ins_patch6_2,
    (uint32_t)&bes2300_ins_patch7_2,
    (uint32_t)&bes2300_ins_patch8_2,
    (uint32_t)&bes2300_ins_patch9_2,
    (uint32_t)&bes2300_ins_patch10_2,
    (uint32_t)&bes2300_ins_patch11_2,
    (uint32_t)&bes2300_ins_patch12_2,
    (uint32_t)&bes2300_ins_patch13_2,
    (uint32_t)&bes2300_ins_patch14_2,
    (uint32_t)&bes2300_ins_patch15_2,
    (uint32_t)&bes2300_ins_patch16_2,
    (uint32_t)&bes2300_ins_patch17_2,
    (uint32_t)&bes2300_ins_patch18_2,
    (uint32_t)&bes2300_ins_patch19_2,
    (uint32_t)&bes2300_ins_patch20_2,
    (uint32_t)&bes2300_ins_patch21_2,
    (uint32_t)&bes2300_ins_patch22_2,
    (uint32_t)&bes2300_ins_patch23_2,
    (uint32_t)&bes2300_ins_patch24_2,
    (uint32_t)&bes2300_ins_patch25_2,
    (uint32_t)&bes2300_ins_patch26_2,
    (uint32_t)&bes2300_ins_patch27_2,
    (uint32_t)&bes2300_ins_patch28_2,
    (uint32_t)&bes2300_ins_patch29_2,
    (uint32_t)&bes2300_ins_patch30_2,
    (uint32_t)&bes2300_ins_patch31_2,
    (uint32_t)&bes2300_ins_patch32_2,
    (uint32_t)&bes2300_ins_patch33_2,
    (uint32_t)&bes2300_ins_patch34_2,
    (uint32_t)&bes2300_ins_patch35_2,
    (uint32_t)&bes2300_ins_patch36_2,
    (uint32_t)&bes2300_ins_patch37_2,
    (uint32_t)&bes2300_ins_patch38_2,
    (uint32_t)&bes2300_ins_patch39_2,
    (uint32_t)&bes2300_ins_patch40_2,
    (uint32_t)&bes2300_ins_patch41_2,
    (uint32_t)&bes2300_ins_patch42_2,
    (uint32_t)&bes2300_ins_patch43_2,
    (uint32_t)&bes2300_ins_patch44_2,
    (uint32_t)&bes2300_ins_patch45_2,
    (uint32_t)&bes2300_ins_patch46_2,
    (uint32_t)&bes2300_ins_patch47_2,
    (uint32_t)&bes2300_ins_patch48_2,
    (uint32_t)&bes2300_ins_patch49_2,
    (uint32_t)&bes2300_ins_patch50_2,
    (uint32_t)&bes2300_ins_patch51_2,
    (uint32_t)&bes2300_ins_patch52_2,
    (uint32_t)&bes2300_ins_patch53_2,
    (uint32_t)&bes2300_ins_patch54_2,
    
};



const uint32_t bes2300_patch0_ins_data_3[] =
{
    0x30abf894, 
    0x490fb9d3, 
    0x4a10880b, 
    0xf4037810, 
    0x4303437f, 
    0x490d800b, 
    0x02006808, 
    0x88134a0a, 
    0xf423b29b, 
    0x430363e0, 
    0x68088013, 
    0x3001bf00,
    0xd8012802, 
    0xe0016008, 
    0x600a2200, 
    0x90a4f8b4, 
    0xbadcf63b, 
    0xd02101d6, 
    0xd02101d0, 
    0xc0006e10, 
    0x00000000, 
};

const BTDRV_PATCH_STRUCT bes2300_ins_patch0_3 =
{
    0,
#ifdef __DYNAMIC_ADV_POWER__        
    BTDRV_PATCH_ACT,
#else
    BTDRV_PATCH_INACT,
#endif
    sizeof(bes2300_patch0_ins_data_3),
    0x00041df8,
    0xbd02f1c4,
    0xc0006800,
    (uint8_t *)bes2300_patch0_ins_data_3
};


const uint32_t bes2300_patch1_ins_data_3[] =
{

    0xb083b500, 
    0x0101f001, 
    0x0148f041, 
    0x1004f88d, 
    0x2005f88d, 
    0xf61ca901, 
    0xb003fad5, 
    0xfb04f85d, 
    0xbf004d0f, 
    0x28ff6828, 
    0x0200d016, 
    0x407ff400, 
    0x0001f040, 
    0xfd80f5fa, 
    0xd10d2801, 
    0x4b076828, 
    0x3020f853, 
    0xf893b2c0, 
    0x4b071040, 
    0xf7ff781a, 
    0x23ffffd7, 
    0xbf00602b, 
    0xba1ef600, 
    0xc0005bcc, 
    0xc00068c4, 
    0x000000ff, 
    0xc00068cc, 
    0x00000000, 

};

const BTDRV_PATCH_STRUCT bes2300_ins_patch1_3 =
{
    1,
#ifdef __SEND_PREFERRE_RATE__        
    BTDRV_PATCH_ACT,
#else
    BTDRV_PATCH_INACT,
#endif
    sizeof(bes2300_patch1_ins_data_3),
    0x00006ce4,
    0xbdccf1ff,
    0xc0006860,
    (uint8_t *)bes2300_patch1_ins_data_3
};


const uint32_t bes2300_patch2_ins_data_3[] =
{
    0xf5fa4628,
    0x4903fd61,
    0x46286008,
    0xbf002101,
    0xbe20f614,
    0xc00068e8,
    0x00000000

};

const BTDRV_PATCH_STRUCT bes2300_ins_patch2_3=
{
    2,
    BTDRV_PATCH_ACT,
    sizeof(bes2300_patch2_ins_data_3),
    0x0001b520,
    0xb9d6f1eb,
    0xc00068d0,
    (uint8_t *)bes2300_patch2_ins_data_3
};



const uint32_t bes2300_patch3_ins_data_3[] =
{
    0x0f00f1b8,
    0x4628d004,
    0x68094904,
    0xfce0f5fa,
    0x21134630,
    0x3044f894,
    0xbe59f614,
    0xc00068e8


};

const BTDRV_PATCH_STRUCT bes2300_ins_patch3_3 =
{
    3,
    BTDRV_PATCH_ACT,
    sizeof(bes2300_patch3_ins_data_3),
    0x0001b5b8,
    0xb99af1eb,
    0xc00068f0,
    (uint8_t *)bes2300_patch3_ins_data_3
};

const uint32_t bes2300_patch4_ins_data_3[] =
{
    0xf2400992,
    0x401333ff,
    0xbdeef626 

};

const BTDRV_PATCH_STRUCT bes2300_ins_patch4_3 =
{
    4,
    BTDRV_PATCH_ACT,
    sizeof(bes2300_patch4_ins_data_3),
    0x0002d4f4,
    0xba0ef1d9,
    0xc0006914,
    (uint8_t *)bes2300_patch4_ins_data_3
};//ld_acl_rx rssi

const uint32_t bes2300_patch5_ins_data_3[] =
{
    0xf2400992,
    0x401333ff,
    0xb818f61f

};

const BTDRV_PATCH_STRUCT bes2300_ins_patch5_3 =
{
    5,
    BTDRV_PATCH_ACT,
    sizeof(bes2300_patch5_ins_data_3),
    0x00025954,
    0xbfe4f1e0,
    0xc0006920,
    (uint8_t *)bes2300_patch5_ins_data_3
};//ld_inq_rx rssi

const uint32_t bes2300_patch6_ins_data_3[] =
{
    0xf2400992,
    0x401333ff,
    0xbf56f63a 

};

const BTDRV_PATCH_STRUCT bes2300_ins_patch6_3 =
{
    6,
    BTDRV_PATCH_ACT,
    sizeof(bes2300_patch6_ins_data_3),
    0x000417dc,
    0xb8a6f1c5,
    0xc000692c,
    (uint8_t *)bes2300_patch6_ins_data_3
};//lld_pdu_rx_handler rssi


#ifdef __POWER_CONTROL_TYPE_1__

const uint32_t bes2300_patch7_ins_data_3[] =
{
    0xf0135243,
    0x2b00030f,
    0x2000bf14,
    0x47702001,
};


const BTDRV_PATCH_STRUCT bes2300_ins_patch7_3 =
{
    7,
    BTDRV_PATCH_ACT,
    sizeof(bes2300_patch7_ins_data_3),
    0x00003498,
    0xba50f203,
    0xc000693c,
    (uint8_t *)bes2300_patch7_ins_data_3
};//pwr controll
#else
const BTDRV_PATCH_STRUCT bes2300_ins_patch7_3 =
{
    7,
    BTDRV_PATCH_ACT,
    0,
    0x000034d0,
    0xe0072200,
    0,
    NULL
};

#endif


const uint32_t bes2300_patch8_ins_data_3[] =
{
    0x68109a05,
    0x68926851,
    0x0006f8c3,
    0xbbf4f61b 
};

const BTDRV_PATCH_STRUCT bes2300_ins_patch8_3 =
{
    8,
    BTDRV_PATCH_ACT,
    sizeof(bes2300_patch8_ins_data_3),
    0x00022140,
    0xbc04f1e4, 
    0xc000694c,
    (uint8_t *)bes2300_patch8_ins_data_3
};//ld_send_host_get_mobile_rssi_evt


const uint32_t bes2300_patch9_ins_data_3[] =
{
    0xf894b408,     			
    0xb97330b5,			
    0xf8b4b2b2,     
    0x429a3098,
    0x1ad3bf8c,     
    0xf5b31a9b,     
    0xbf847f00,
    0x731cf5c3,
    0x2b0d3301,     
    0xbc08d808, 	 
    0xf5b2b21a,     
    0xdd017f9c,
    0xb9a4f622,
    0xb9a6f622,
    0xbf002000,     
    0x87f0e8bd,
};


const BTDRV_PATCH_STRUCT bes2300_ins_patch9_3 =
{
    9,
#ifdef BT_RF_OLD_CORR_MODE
    BTDRV_PATCH_INACT,
#else
    BTDRV_PATCH_INACT,
#endif
    sizeof(bes2300_patch9_ins_data_3),
    0x00028cd4,
    0xbe44f1dd,
    0xc0006960,
    (uint8_t *)bes2300_patch9_ins_data_3
};//bitoff check


/////2300 t6 patch
static const uint32_t best2300_ins_patch_config_3[] =
{
    10,
    (uint32_t)&bes2300_ins_patch0_3,
    (uint32_t)&bes2300_ins_patch1_3,
    (uint32_t)&bes2300_ins_patch2_3,
    (uint32_t)&bes2300_ins_patch3_3,
    (uint32_t)&bes2300_ins_patch4_3,
    (uint32_t)&bes2300_ins_patch5_3,
    (uint32_t)&bes2300_ins_patch6_3,
    (uint32_t)&bes2300_ins_patch7_3,
    (uint32_t)&bes2300_ins_patch8_3,
    (uint32_t)&bes2300_ins_patch9_3,
    
};

void btdrv_ins_patch_write(BTDRV_PATCH_STRUCT *ins_patch_p)
{
    uint32_t remap_addr;
    uint8_t i=0;
    remap_addr =   ins_patch_p->patch_remap_address | 1;
    btdrv_write_memory(_32_Bit,(BTDRV_PATCH_INS_REMAP_ADDR_START + ins_patch_p->patch_index*4),
                                            (uint8_t *)&ins_patch_p->patch_remap_value,4);
    if(ins_patch_p->patch_length != 0)  //have ram patch data
    {
        for( ;i<(ins_patch_p->patch_length)/128;i++)
        {
                    btdrv_write_memory(_32_Bit,ins_patch_p->patch_start_address+i*128,
                                                            (ins_patch_p->patch_data+i*128),128);
        }

        btdrv_write_memory(_32_Bit,ins_patch_p->patch_start_address+i*128,ins_patch_p->patch_data+i*128,
                                                ins_patch_p->patch_length%128);
    }

    btdrv_write_memory(_32_Bit,(BTDRV_PATCH_INS_COMP_ADDR_START + ins_patch_p->patch_index*4),
                                            (uint8_t *)&remap_addr,4);

}

void btdrv_ins_patch_init(void)
{
    const BTDRV_PATCH_STRUCT *ins_patch_p;
    if(hal_get_chip_metal_id() == HAL_CHIP_METAL_ID_0 || hal_get_chip_metal_id() == HAL_CHIP_METAL_ID_1)
    {
        for(uint8_t i=0;i<best2300_ins_patch_config[0];i++)
        {
            ins_patch_p = (BTDRV_PATCH_STRUCT *)best2300_ins_patch_config[i+1];
            if(ins_patch_p->patch_state ==BTDRV_PATCH_ACT)
                btdrv_ins_patch_write((BTDRV_PATCH_STRUCT *)best2300_ins_patch_config[i+1]);
        }
    }
    else if(hal_get_chip_metal_id() <= HAL_CHIP_METAL_ID_4)
    {
        //HAL_CHIP_METAL_ID_2
        for(uint8_t i=0;i<best2300_ins_patch_config_2[0];i++)
        {
            ins_patch_p = (BTDRV_PATCH_STRUCT *)best2300_ins_patch_config_2[i+1];
            if(ins_patch_p->patch_state ==BTDRV_PATCH_ACT)
                btdrv_ins_patch_write((BTDRV_PATCH_STRUCT *)best2300_ins_patch_config_2[i+1]);
        }    
    }
    else if(hal_get_chip_metal_id() >= HAL_CHIP_METAL_ID_5)
    {
        for(uint8_t i=0;i<best2300_ins_patch_config_3[0];i++)
        {
            ins_patch_p = (BTDRV_PATCH_STRUCT *)best2300_ins_patch_config_3[i+1];
            if(ins_patch_p->patch_state ==BTDRV_PATCH_ACT)
                btdrv_ins_patch_write((BTDRV_PATCH_STRUCT *)best2300_ins_patch_config_3[i+1]);
        }           
    }
}

///////////////////data  patch ..////////////////////////////////////
#if 0
0106 0109 010a 0114 0114 010a 0212 0105     ................
010a 0119 0119 0105 0105 0108 0214 0114     ................
0108 0114 010a 010a 0105 0105 0114 010a     ................
0b0f 0105 010a 0000
#endif

static const uint32_t bes2300_dpatch0_data[] =
{
    0x01090106,
    0x0114010a,
    0x010a0114,
    0x01050212,
    0x0119010a,
    0x01050119,
    0x01080105,
    0x01140214,
    0x01140108,
    0x010a010a,
    0x01050105,
    0x0b0f0114,
    0x01050b0f,
    0x0000010a,
};

static const BTDRV_PATCH_STRUCT bes2300_data_patch0 =
{
    0,
    BTDRV_PATCH_ACT,
    sizeof(bes2300_dpatch0_data),
    0x00043318,
    0xc0000058,
    0xc0000058,
    (uint8_t *)&bes2300_dpatch0_data
};

static const uint32_t best2300_data_patch_config[] =
{
    1,
    (uint32_t)&bes2300_data_patch0,
        
};






void btdrv_data_patch_write(const BTDRV_PATCH_STRUCT *d_patch_p)
{

    uint32_t remap_addr;
    uint8_t i=0;

    remap_addr = d_patch_p->patch_remap_address |1;
    btdrv_write_memory(_32_Bit,(BTDRV_PATCH_DATA_COMP_ADDR_START + d_patch_p->patch_index*4),
                                        (uint8_t *)&remap_addr,4);
    btdrv_write_memory(_32_Bit,(BTDRV_PATCH_DATA_REMAP_ADDR_START + d_patch_p->patch_index*4),
                                        (uint8_t *)&d_patch_p->patch_remap_value,4);

    if(d_patch_p->patch_length != 0)  //have ram patch data
    {
        for( ;i<(d_patch_p->patch_length-1)/128;i++)
        {
                    btdrv_write_memory(_32_Bit,d_patch_p->patch_start_address+i*128,
                        (d_patch_p->patch_data+i*128),128);

        }

        btdrv_write_memory(_32_Bit,d_patch_p->patch_start_address+i*128,d_patch_p->patch_data+i*128,
            d_patch_p->patch_length%128);
    }

}



void btdrv_data_patch_init(void)
{
    const BTDRV_PATCH_STRUCT *data_patch_p;
    if(hal_get_chip_metal_id() > HAL_CHIP_METAL_ID_1 && hal_get_chip_metal_id() <= HAL_CHIP_METAL_ID_4)
    {
        for(uint8_t i=0;i<best2300_data_patch_config[0];i++)
        {
            data_patch_p = (BTDRV_PATCH_STRUCT *)best2300_data_patch_config[i+1];
            if(data_patch_p->patch_state == BTDRV_PATCH_ACT)
                btdrv_data_patch_write((BTDRV_PATCH_STRUCT *)best2300_data_patch_config[i+1]);
        }
    }
}


//////////////////////////////patch enable////////////////////////


void btdrv_patch_en(uint8_t en)
{
    uint32_t value[2];

    //set patch enable
    value[0] = 0x2f02 | en;
    //set patch remap address  to 0xc0000100
    value[1] = 0x20000100;
    btdrv_write_memory(_32_Bit,BTDRV_PATCH_EN_REG,(uint8_t *)&value,8);
}


const BTDRV_PATCH_STRUCT bes2300_ins_patch0_testmode_2 =
{
    0,
    BTDRV_PATCH_ACT,
    0,
    0x0002816c,
    0x789a3100,
    0,
    NULL
};///no sig tx test


const BTDRV_PATCH_STRUCT bes2300_ins_patch1_testmode_2 =
{
    1,
    BTDRV_PATCH_ACT,
    0,
    0x0000b7ec,
    0xbf00e007,
    0,
    NULL
};///lm init

const BTDRV_PATCH_STRUCT bes2300_ins_patch2_testmode_2 =
{
    2,
    BTDRV_PATCH_ACT,
    0,
    0x0003faa4,
    0xbf002202,
    0,
    NULL
};//ble test power

const BTDRV_PATCH_STRUCT bes2300_ins_patch3_testmode_2 =
{
    3,
    BTDRV_PATCH_ACT,
    0,
    0x00023d64,
    0xbf00bd08,
    0,
    NULL
};//ld_channel_assess


const BTDRV_PATCH_STRUCT bes2300_ins_patch4_testmode_2 =
{
    4,
    BTDRV_PATCH_ACT,
    0,
    0x0003E280,
#ifdef BLE_POWER_LEVEL_0
    0x23008011,
#else
    0x23028011,
#endif    
    0,
    NULL
};

const BTDRV_PATCH_STRUCT bes2300_ins_patch5_testmode_2 =
{
    5,
    BTDRV_PATCH_ACT,
    0,
    0x0003E284,
    0x021CbF00,
    0,
    NULL
};

static const uint32_t ins_patch_2300_config_testmode[] =
{
    6,
    (uint32_t)&bes2300_ins_patch0_testmode_2,
    (uint32_t)&bes2300_ins_patch1_testmode_2,
    (uint32_t)&bes2300_ins_patch2_testmode_2,
    (uint32_t)&bes2300_ins_patch3_testmode_2,
    (uint32_t)&bes2300_ins_patch4_testmode_2,
    (uint32_t)&bes2300_ins_patch5_testmode_2,    
    
};


const BTDRV_PATCH_STRUCT bes2300_ins_patch0_testmode_3 =
{
    0,
    BTDRV_PATCH_ACT,
    0,
    0x00024bcc,
    0xbf00bd10,
    0,
    NULL
};//ld_channel_assess


const BTDRV_PATCH_STRUCT bes2300_ins_patch1_testmode_3 =
{
    1,
    BTDRV_PATCH_ACT,
    0,
    0x0003ed7c,
#ifdef BLE_POWER_LEVEL_0
    0xbf002300,
#else
    0xbf002302,
#endif
    0,
    NULL
};//ble power level

static const uint32_t ins_patch_2300_config_testmode_3[] =
{
    2,
    (uint32_t)&bes2300_ins_patch0_testmode_3,
    (uint32_t)&bes2300_ins_patch1_testmode_3,
    
};


void btdrv_ins_patch_test_init(void)
{
    
    const BTDRV_PATCH_STRUCT *ins_patch_p;

    btdrv_patch_en(0);
    if(hal_get_chip_metal_id() > HAL_CHIP_METAL_ID_1 && hal_get_chip_metal_id() <= HAL_CHIP_METAL_ID_4)
    {
        for(uint8_t i=0;i<ins_patch_2300_config_testmode[0];i++)
        {
            ins_patch_p = (BTDRV_PATCH_STRUCT *)ins_patch_2300_config_testmode[i+1];
            if(ins_patch_p->patch_state ==BTDRV_PATCH_ACT)
                btdrv_ins_patch_write((BTDRV_PATCH_STRUCT *)ins_patch_2300_config_testmode[i+1]);
        }    
#ifdef BLE_POWER_LEVEL_0
        *(uint32_t *)0xd02101d0 = 0xf855;
#endif

        
    }
    else if(hal_get_chip_metal_id()>=HAL_CHIP_METAL_ID_5)
    {
        for(uint8_t i=0;i<ins_patch_2300_config_testmode_3[0];i++)
        {
            ins_patch_p = (BTDRV_PATCH_STRUCT *)ins_patch_2300_config_testmode_3[i+1];
            if(ins_patch_p->patch_state ==BTDRV_PATCH_ACT)
                btdrv_ins_patch_write((BTDRV_PATCH_STRUCT *)ins_patch_2300_config_testmode_3[i+1]);
        }       
    }
    btdrv_patch_en(1);
}

void btdrv_dynamic_patch_moble_disconnect_reason_hacker(uint16_t hciHandle)
{
    uint32_t lc_ptr=0;
    uint32_t disconnect_reason_address = 0;
    if(hal_get_chip_metal_id() > HAL_CHIP_METAL_ID_1 && hal_get_chip_metal_id() <= HAL_CHIP_METAL_ID_4)
    {
        lc_ptr = *(uint32_t *)(0xc0005b48+(hciHandle-0x80)*4);
    }
    else if(hal_get_chip_metal_id() >= HAL_CHIP_METAL_ID_5)
    {
        lc_ptr = *(uint32_t *)(0xc0005bcc+(hciHandle-0x80)*4);
    }
    //TRACE("lc_ptr %x, disconnect_reason_addr %x",lc_ptr,lc_ptr+66);
    if(lc_ptr == 0){
        return;
    }else{
        disconnect_reason_address = (uint32_t)(lc_ptr+66);
        TRACE("%s:hdl=%x reason=%d",__func__,hciHandle,*(uint8_t *)(disconnect_reason_address));
        *(uint8_t *)(disconnect_reason_address) = 0x0;
        TRACE("After op,reason=%d",*(uint8_t *)(disconnect_reason_address));
        return;
    }
}

void btdrv_current_packet_type_hacker(uint16_t hciHandle,uint8_t rate)
{
    uint32_t lc_ptr=0;
    uint32_t currentPacketTypeAddr = 0;
    uint32_t acl_par_ptr = 0;
    uint32_t packet_type_addr = 0;
    if(hal_get_chip_metal_id() > HAL_CHIP_METAL_ID_1 && hal_get_chip_metal_id() <= HAL_CHIP_METAL_ID_4)
    {
        lc_ptr = *(uint32_t *)(0xc0005b48+(hciHandle-0x80)*4);
        acl_par_ptr = *(uint32_t *)(0xc00008fc+(hciHandle-0x80)*4);
    }
    else if(hal_get_chip_metal_id() >= HAL_CHIP_METAL_ID_5)
    {
        lc_ptr = *(uint32_t *)(0xc0005bcc+(hciHandle-0x80)*4);
        acl_par_ptr = *(uint32_t *)(0xc000095c+(hciHandle-0x80)*4);    
    }
    if(lc_ptr == 0){
        return;
    }else{
        currentPacketTypeAddr = (uint32_t)(lc_ptr+40);
        packet_type_addr = (uint32_t)(acl_par_ptr+176);
        TRACE("lc_ptr %x, acl_par_ptr %x",lc_ptr,acl_par_ptr);
        TRACE("%s:hdl=%x currentPacketType %x packet_types %x",__func__,hciHandle, *(uint16_t *)currentPacketTypeAddr,*(uint16_t *)(packet_type_addr));
        if(rate == BT_TX_3M)
        {
            *(uint16_t *)currentPacketTypeAddr = 0xdd1a;
            *(uint16_t *)(packet_type_addr) = 0x553f;
        }
        else if(rate == BT_TX_2M)
        {
            *(uint16_t *)currentPacketTypeAddr = 0xee1c;
            *(uint16_t *)(packet_type_addr) = 0x2a3f;            
        }
        else if(rate == (BT_TX_2M | BT_TX_3M))
        {
            *(uint16_t *)currentPacketTypeAddr = 0xcc18;
            *(uint16_t *)(packet_type_addr) = 0x7f3f;                     
        }
        TRACE("After op,currentPacketType=%x packet_types %x",*(uint16_t *)currentPacketTypeAddr,*(uint16_t *)(packet_type_addr));
    }
}


void btdrv_preferre_rate_send(uint16_t conhdl,uint8_t rate)
{
#ifdef __SEND_PREFERRE_RATE__
    if(hal_get_chip_metal_id() > HAL_CHIP_METAL_ID_1 && hal_get_chip_metal_id() <= HAL_CHIP_METAL_ID_4)
    {
        ASSERT(*(uint32_t *)0xc0006e80 ==0xc0006e84,"ERROR PATCH FOR PREFERRE RATE!");
        ASSERT(conhdl>=0x80,"ERROR CONNECT HANDLE");
        ASSERT(conhdl<=0x83,"ERROR CONNECT HANDLE");    
        *(uint32_t *)0xc0006e8c = rate;
        *(uint32_t *)0xc0006e84 = conhdl-0x80;
    }
    else if(hal_get_chip_metal_id() >= HAL_CHIP_METAL_ID_5)
    {
        ASSERT(*(uint32_t *)0xc00068c0 ==0xc00068c4,"ERROR PATCH FOR PREFERRE RATE!");
        ASSERT(conhdl>=0x80,"ERROR CONNECT HANDLE");
        ASSERT(conhdl<=0x83,"ERROR CONNECT HANDLE");    
        *(uint32_t *)0xc00068cc = rate;
        *(uint32_t *)0xc00068c4 = conhdl-0x80;        
    }
    
#endif
}

void btdrv_preferre_rate_clear(void)
{
#ifdef __SEND_PREFERRE_RATE__
    if(hal_get_chip_metal_id() > HAL_CHIP_METAL_ID_1 && hal_get_chip_metal_id() <= HAL_CHIP_METAL_ID_4)
    {
        ASSERT(*(uint32_t *)0xc0006e80 ==0xc0006e84,"ERROR PATCH FOR PREFERRE RATE!");
        *(uint32_t *)0xc0006e84 = 0xff;
    }
    else if(hal_get_chip_metal_id() >= HAL_CHIP_METAL_ID_5)
    {
        ASSERT(*(uint32_t *)0xc00068c0 ==0xc00068c4,"ERROR PATCH FOR PREFERRE RATE!");
        *(uint32_t *)0xc00068c0 = 0xff;        
    }
#endif
}
void btdrv_dynamic_patch_sco_status_clear(void)
{
    return;
}


void btdrv_seq_bak_mode(uint8_t mode,uint8_t linkid)
{
#ifdef __SW_SEQ_FILTER__
    if(hal_get_chip_metal_id() > HAL_CHIP_METAL_ID_1 && hal_get_chip_metal_id() <= HAL_CHIP_METAL_ID_4)
    {
        uint32_t seq_bak_ptr = 0xc0006c8c;
        TRACE("btdrv_seq_bak_mode mode=%x,linkid = %x",mode,linkid);  
        uint32_t val = *(uint32_t *)seq_bak_ptr;
        val &= ~(0xff<<(linkid*8));
        if(mode ==1)//en
        {
            val |=(0x2<<linkid*8);
        }
        else if(mode ==0)
        {
            val =0xffffffff;
        }
        *(uint32_t *)seq_bak_ptr = val;
        
        TRACE("val=%x",val);
    }
#endif    
}


////////////////////////////////bt 5.0 /////////////////////////////////

const BTDRV_PATCH_STRUCT bes2300_50_ins_patch0 =
{
    0,
    BTDRV_PATCH_INACT,
    0,
    0x00056138,
    0xe0042300,
    0,
    NULL
};//disable reslove to



const uint32_t bes2300_50_patch1_ins_data[] =
{
    0x0f02f013,
    0xf008bf12,
    0x79330303,
    0x0303ea08,
    0x300bf889,
    0xf01378b3,
    0xbf120f01,
    0x0303f008,
    0xea0878f3,
    0xf8890303,
    0xbf00300a,
    0xbf6ff659,
};


const BTDRV_PATCH_STRUCT bes2300_50_ins_patch1 =
{
    1,
    BTDRV_PATCH_INACT,
    sizeof(bes2300_50_patch1_ins_data),
    0x0005f6e8,
    0xb88af1a6,
    0xc0005800,
    (uint8_t *)bes2300_50_patch1_ins_data
};


const uint32_t bes2300_50_patch2_ins_data[] =
{
    0xf01378cb,
    0xd1080f04,
    0xf013790b,
    0xd1040f04,
    0xf003788b,
    0xf6590202,
    0x2211bf16,
    0xbf99f659,
};


const BTDRV_PATCH_STRUCT bes2300_50_ins_patch2 =
{
    2,
    BTDRV_PATCH_INACT,
    sizeof(bes2300_50_patch2_ins_data),
    0x0005f680,
    0xb8def1a6,
    0xc0005840,
    (uint8_t *)bes2300_50_patch2_ins_data
};///SET PHY CMD



const uint32_t bes2300_50_patch3_ins_data[] =
{
    0x4b034902,
    0x4b036019,
    0xb8c5f64b,
    0x00800190,
    0xc0004af4,
    0xc00002d4,
};


const BTDRV_PATCH_STRUCT bes2300_50_ins_patch3 =
{
    3,
    BTDRV_PATCH_ACT,
    sizeof(bes2300_50_patch3_ins_data),
    0x00050a70,
    0xbf36f1b4,
    0xc00058e0,
    (uint8_t *)bes2300_50_patch3_ins_data
};


const BTDRV_PATCH_STRUCT bes2300_50_ins_patch4 =
{
    4,
    BTDRV_PATCH_ACT,
    0,
    0x000676c4,
    0x0301f043,
    0,
    NULL
};



static const uint32_t best2300_50_ins_patch_config[] =
{
    5,
    (uint32_t)&bes2300_50_ins_patch0,
    (uint32_t)&bes2300_50_ins_patch1,
    (uint32_t)&bes2300_50_ins_patch2,
    (uint32_t)&bes2300_50_ins_patch3,
    (uint32_t)&bes2300_50_ins_patch4,
    
};


const uint32_t bes2300_50_patch0_ins_data_2[] =
{
    0x4b034902,
    0x4b036019,
    0xbe25f64b,
    0x00800190,
    0xc0004bc0,
    0xc0000390,
};


const BTDRV_PATCH_STRUCT bes2300_50_ins_patch0_2 =
{
    0,
    BTDRV_PATCH_ACT,
    sizeof(bes2300_50_patch0_ins_data_2),
    0x00050f30,
    0xb9d6f1b4,
    0xc00052e0,
    (uint8_t *)bes2300_50_patch0_ins_data_2
};//sleep patch


const uint32_t bes2300_50_patch1_ins_data_2[] =
{
0xf8934b1e,
0x2b0030dc,
0xb470d132,
0x4b1cb082,
0x4d1c8818,
0x4e1c882b,
0x2303f3c3,
0x69f2b2c0,
0x1000ea43,
0x882c4790,
0x9600b2a4,
0x21006a36,
0x2301460a,
0x882b47b0,
0x2403f3c4,
0x2303f3c3,
0xd013429c,
0x68194b11,
0x4b112200,
0x3bd0601a,
0xf422681a,
0x601a4280,
0xf042681a,
0x601a6280,
0x4b0c2201,
0x2302601a,
0x302af881,
0xbc70b002,
0xbf00bf00,
0xff26f65a,
0xbc61f662,
0xc00050d0,
0xd061074c,
0xd0610160,
0xc00048b8,
0xc00008c4,
0xd06200d0,
0xc00053a4,
0x00000000,






};


const BTDRV_PATCH_STRUCT bes2300_50_ins_patch1_2 =
{
    1,
    BTDRV_PATCH_ACT,
    sizeof(bes2300_50_patch1_ins_data_2),
    0x00067c44,
    0xbb62f19d,
    0xc000530c,
    (uint8_t *)bes2300_50_patch1_ins_data_2
};

const uint32_t bes2300_50_patch2_ins_data_2[] =
{

0xf6684620,
0x4b09fd2d,
0x2b01681b,
0xf894d10c,
0x2b02302a,
0xf662d108,
0x4805fbc5,
0xfc58f662,
0x4b022200,
0xbd38601a,
0xbbd1f662,
0xc00053a4,
0xc00053e4,
0x00000000,
0x00000000,


};


const BTDRV_PATCH_STRUCT bes2300_50_ins_patch2_2 =
{
    2,
    BTDRV_PATCH_ACT,
    sizeof(bes2300_50_patch2_ins_data_2),
    0x00067b78,
    0xbc1af19d,
    0xc00053b0,
    (uint8_t *)bes2300_50_patch2_ins_data_2
};



const uint32_t bes2300_50_patch3_ins_data_2[] =
{

0x4b046013,  
0x601a6822,  
0x711a7922,  
0xbf002000,  
0xbc7cf662,  
0xc00053e4,  


};


const BTDRV_PATCH_STRUCT bes2300_50_ins_patch3_2 =
{
    3,
    BTDRV_PATCH_ACT,
    sizeof(bes2300_50_patch3_ins_data_2),
    0x00067d08,
    0xbb7af19d,
    0xc0005400,
    (uint8_t *)bes2300_50_patch3_ins_data_2
};


static const uint32_t best2300_50_ins_patch_config_2[] =
{
    4,
    (uint32_t)&bes2300_50_ins_patch0_2,
    (uint32_t)&bes2300_50_ins_patch1_2,    
    (uint32_t)&bes2300_50_ins_patch2_2,        
    (uint32_t)&bes2300_50_ins_patch3_2,            
};

void btdrv_ins_patch_init_50(void)
{
    const BTDRV_PATCH_STRUCT *ins_patch_p;
    if(hal_get_chip_metal_id() == HAL_CHIP_METAL_ID_0 || hal_get_chip_metal_id() == HAL_CHIP_METAL_ID_1)
    {
        for(uint8_t i=0;i<best2300_50_ins_patch_config[0];i++)
        {
            ins_patch_p = (BTDRV_PATCH_STRUCT *)best2300_50_ins_patch_config[i+1];
            if(ins_patch_p->patch_state ==BTDRV_PATCH_ACT)
                btdrv_ins_patch_write((BTDRV_PATCH_STRUCT *)best2300_50_ins_patch_config[i+1]);
        }
    }
    else if(hal_get_chip_metal_id() < HAL_CHIP_METAL_ID_5)
    {
        for(uint8_t i=0;i<best2300_50_ins_patch_config_2[0];i++)
        {
            ins_patch_p = (BTDRV_PATCH_STRUCT *)best2300_50_ins_patch_config_2[i+1];
            if(ins_patch_p->patch_state ==BTDRV_PATCH_ACT)
                btdrv_ins_patch_write((BTDRV_PATCH_STRUCT *)best2300_50_ins_patch_config_2[i+1]);
        }    
    }
}

const uint32_t dpatch0_data_2300_50[] =
{
    0x29B00033,
    0x0020B000,
    0xB00015B0,
    0x05B0000B,
    0x00F9B000,
    0x7F7F7FB0,
    0x7F7F7F7F,
    0x7F7F7F7F,
    0x7F7F7F7F,
    0x7F7F7F7F,
    0x7F7F7F7F,
    0x0000007F

};

const BTDRV_PATCH_STRUCT data_patch0_2300_50 =
{
    0,
    BTDRV_PATCH_ACT,
    sizeof(dpatch0_data_2300_50),
    0x00051930,
    0xc0005870,
    0xc0005870,
    (uint8_t *)&dpatch0_data_2300_50
};



const uint32_t dpatch1_data_2300_50[] =
{
    0xC1C1BCBC,
    0xDcDcd3d3,
    0xE7E7e2e2,
    0x7f7fEeEe,
    0x7F7F7F7F,
    0x7F7F7F7F,
    0x7F7F7F7F,
    0x00007F7F,

};

const BTDRV_PATCH_STRUCT data_patch1_2300_50 =
{
    1,
    BTDRV_PATCH_ACT,
    sizeof(dpatch1_data_2300_50),
    0x00051aa8,
    0xc00058b0,
    0xc00058b0,
    (uint8_t *)&dpatch1_data_2300_50
};


uint32_t data_patch_config_2300_50[] =
{
    2,
    (uint32_t)&data_patch0_2300_50,
    (uint32_t)&data_patch1_2300_50,

};





void btdrv_data_patch_init_50(void)
{
    const BTDRV_PATCH_STRUCT *data_patch_p;
    if(hal_get_chip_metal_id() == HAL_CHIP_METAL_ID_0 || hal_get_chip_metal_id() == HAL_CHIP_METAL_ID_1)
    {

        for(uint8_t i=0;i<data_patch_config_2300_50[0];i++)
        {
            data_patch_p = (BTDRV_PATCH_STRUCT *)data_patch_config_2300_50[i+1];
            if(data_patch_p->patch_state == BTDRV_PATCH_ACT)
                btdrv_data_patch_write((BTDRV_PATCH_STRUCT *)data_patch_config_2300_50[i+1]);
        }
    }

}


