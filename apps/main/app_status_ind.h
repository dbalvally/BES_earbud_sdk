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
#ifndef __APP_STATUS_IND_H__
#define __APP_STATUS_IND_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef enum APP_STATUS_INDICATION_T {
    APP_STATUS_INDICATION_POWERON = 0,
    APP_STATUS_INDICATION_INITIAL,
    APP_STATUS_INDICATION_PAGESCAN,
    APP_STATUS_INDICATION_POWEROFF,
    APP_STATUS_INDICATION_CHARGENEED,
    APP_STATUS_INDICATION_CHARGING,
    APP_STATUS_INDICATION_FULLCHARGE,
    APP_STATUS_INDICATION_NO_REPEAT_NUM,
    /* repeatable status: */
    APP_STATUS_INDICATION_BOTHSCAN = APP_STATUS_INDICATION_NO_REPEAT_NUM,
    APP_STATUS_INDICATION_CONNECTING,
    APP_STATUS_INDICATION_CONNECTED,
    APP_STATUS_INDICATION_DISCONNECTED,
    APP_STATUS_INDICATION_CALLNUMBER,
    APP_STATUS_INDICATION_INCOMINGCALL,
    APP_STATUS_INDICATION_PAIRSUCCEED,
    APP_STATUS_INDICATION_PAIRFAIL,
    APP_STATUS_INDICATION_HANGUPCALL,
    APP_STATUS_INDICATION_REFUSECALL,
    APP_STATUS_INDICATION_ANSWERCALL,
    APP_STATUS_INDICATION_CLEARSUCCEED,
    APP_STATUS_INDICATION_CLEARFAIL,
    APP_STATUS_INDICATION_WARNING,
    APP_STATUS_INDICATION_INVALID,
#ifdef __TWS__
	APP_STATUS_INDICATION_TWS_ISMASTER,
	APP_STATUS_INDICATION_TWS_ISSLAVE,
	APP_STATUS_INDICATION_TWS_SEARCH,
	APP_STATUS_INDICATION_TWS_STOPSEARCH,
	APP_STATUS_INDICATION_TWS_DISCOVERABLE ,	
	APP_STATUS_INDICATION_TWS_LEFTCHNL,
	APP_STATUS_INDICATION_TWS_RIGHTCHNL,
#endif    

    APP_STATUS_INDICATION_TESTMODE,
    APP_STATUS_INDICATION_TESTMODE1,
#if defined(TWS_RBCODEC_PLAYER)
    APP_STATUS_INDICATION_PLAYER_MODE_TWS,
    APP_STATUS_INDICATION_PLAYER_MODE_SD,
    APP_STATUS_INDICATION_PLAYER_MODE_LINEIN,
#endif
    APP_STATUS_INDICATION_GSOUND_MIC_OPEN,
    APP_STATUS_INDICATION_GSOUND_MIC_CLOSE,
    APP_STATUS_INDICATION_GSOUND_NC,
    
    APP_STATUS_INDICATION_NUM
}APP_STATUS_INDICATION_T;

int app_status_indication_filter_set(APP_STATUS_INDICATION_T status);
APP_STATUS_INDICATION_T app_status_indication_get(void);
int app_status_indication_set(APP_STATUS_INDICATION_T status);


#ifdef __cplusplus
}
#endif

#endif

