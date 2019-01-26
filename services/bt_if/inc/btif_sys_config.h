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
#ifndef __SYS_BT_CFG_H__
#define __SYS_BT_CFG_H__

#define BTIF_DISABLED  0
#define BTIF_ENABLED 1

#ifndef FPGA
#define  __BTIF_EARPHONE__
#define  __BTIF_AUTOPOWEROFF__
#endif

#define BTIF_AV_WORKER  BTIF_ENABLED


#if defined(A2DP_LHDC_ON)
         #define BTIF_A2DP_LHDC_ON 1
#else
         #define BTIF_A2DP_LHDC_ON 0
#endif

#if defined(MASTER_USE_OPUS) || defined(ALL_USE_OPUS)
         #define BTIF_A2DP_OPUS_ON 1
#else
         #define BTIF_A2DP_OPUS_ON 0
#endif

#if defined(A2DP_AAC_ON)
         #define BTIF_A2DP_AAC_ON  1
#else
         #define BTIF_A2DP_AAC_ON  0
#endif


#define BTIF_A2DP_SBC_ON  1


#ifdef __TWS__
         #define BTIF_A2DP_TRANS_ON  1
#else
         #define BTIF_A2DP_TRANS_ON  0
#endif


#define SYS_MAX_A2DP_STREAMS    (BTIF_A2DP_AAC_ON + BTIF_A2DP_OPUS_ON + BTIF_A2DP_LHDC_ON  + BTIF_A2DP_TRANS_ON + BTIF_A2DP_SBC_ON +1 )

#define BTIF_SBC_ENCODER   BTIF_ENABLED
#define BTIF_SBC_DECODER   BTIF_ENABLED

#define SYS_MAX_AVRCP_CHNS  2

#define BTIF_AVRCP_NUM_PLAYER_SETTINGS 4

#define BTIF_AVRCP_NUM_MEDIA_ATTRIBUTES        7

#define  BTIF_AVRCP_VERSION_1_3_ONLY   BTIF_DISABLED

#define  BTIF_L2CAP_PRIORITY BTIF_DISABLED


#define  BTIF_BT_SECURITY  BTIF_ENABLED


//#define  BTIF_XA_STATISTICS

#define  BTIF_L2CAP_NUM_ENHANCED_CHANNELS 0

#define  BTIF_BT_BEST_SYNC_CONFIG   BTIF_ENABLED

#define  BTIF_HCI_HOST_FLOW_CONTROL

#define  BTIF_DEFAULT_ACCESS_MODE_PAIR   BTIF_BAM_GENERAL_ACCESSIBLE

//#define  BTIF_HID_DEVICE

#define BTIF_BT_DEFAULT_PAGE_SCAN_WINDOW 0x80

/*---------------------------------------------------------------------------
 * BT_DEFAULT_PAGE_SCAN_INTERVAL constant
 *
 *     See BT_DEFAULT_PAGE_SCAN_WINDOW.
 */
#define BTIF_BT_DEFAULT_PAGE_SCAN_INTERVAL 0x800

/*---------------------------------------------------------------------------
 * BT_DEFAULT_INQ_SCAN_WINDOW constant
 *
 *     See BT_DEFAULT_PAGE_SCAN_WINDOW.
 */
#define BTIF_BT_DEFAULT_INQ_SCAN_WINDOW 0x80

/*---------------------------------------------------------------------------
 * BT_DEFAULT_INQ_SCAN_INTERVAL constant
 *
 *     See BT_DEFAULT_PAGE_SCAN_WINDOW.
 */
#define BTIF_BT_DEFAULT_INQ_SCAN_INTERVAL 0x800

#define BTIF_SPP_CLIENT BTIF_DISABLED

#define BTIF_SPP_SERVER BTIF_ENABLED

#define BTIF_RF_SEND_CONTROL  BTIF_DISABLED

#define BTIF_MULTITASKING

#define BTIF_SECURITY

#define BTIF_BLE_APP_DATAPATH_SERVER

#define BTIF_AVRCP_ADVANCED_CONTROLLER



#if defined(__3M_PACK__)
#define BTIF_L2CAP_MTU                           980
#define BTIF_L2CAP_MPS                           980
#else
#define BTIF_L2CAP_MTU                           672
#define BTIF_L2CAP_MPS                           672
#endif

#ifndef BTIF_L2CAP_NUM_ENHANCED_CHANNELS
#define BTIF_L2CAP_NUM_ENHANCED_CHANNELS 0
#endif

#define BTIF_L2CAP_ERTM_RX_WIN_SIZE      5

#ifndef BTIF_HCI_NUM_ACL_BUFFERS
#if BTIF_L2CAP_NUM_ENHANCED_CHANNELS > 0
#define BTIF_HCI_NUM_ACL_BUFFERS      (BTIF_L2CAP_ERTM_RX_WIN_SIZE * BTIF_L2CAP_NUM_ENHANCED_CHANNELS)
#else
#define BTIF_HCI_NUM_ACL_BUFFERS       6
#endif
#endif


#if BTIF_L2CAP_NUM_ENHANCED_CHANNELS > 0
/* Maximum: MTU(4) + Flow Control Modes(11) Option + FCS Option (3) + FlushTimeout(4) */
#define BTIF_L2C_WORKSPACE_LEN       22
#else
/* Maximum: MTU(4) and/or FlushTimeout(4) Options */
#define BTIF_L2C_WORKSPACE_LEN       8
#endif


#define BTIF_NUM_BT_DEVICES  2

#define BTIF_NUM_KNOWN_DEVICES       10

#define BTIF_NUM_SCO_CONNS 2


#define BTIF_HCI_NUM_HANDLES (BTIF_NUM_BT_DEVICES + BTIF_NUM_SCO_CONNS + 2)


#define __BTIF_SNIFF__

#define __BTIF_BT_RECONNECT__


#define BTIF_MAX_AAC_BITRATE (264630)




#endif /*__SYS_BT_CFG_H__*/

