/**
 ****************************************************************************************
 *
 * @file rwapp_config.h
 *
 * @brief Application configuration definition
 *
 * Copyright (C) RivieraWaves 2009-2016
 *
 ****************************************************************************************
 */


#ifndef _RWAPP_CONFIG_H_
#define _RWAPP_CONFIG_H_

/**
 ****************************************************************************************
 * @addtogroup app
 * @brief Application configuration definition
 *
 * @{
 ****************************************************************************************
 */


/*
 * INCLUDE FILES
 ****************************************************************************************
 */

/*
 * DEFINES
 ****************************************************************************************
 */

/******************************************************************************************/
/* -------------------------   BLE APPLICATION SETTINGS      -----------------------------*/
/******************************************************************************************/
// #if VOICE_DATAPATH
#define CFG_APP_DATAPATH_SERVER
// #endif

#define FAST_PAIR_REV_1_0	0
#define FAST_PAIR_REV_2_0	1
#define BLE_APP_GFPS_VER	FAST_PAIR_REV_2_0

#if _GFPS_
#if BLE_APP_GFPS_VER==FAST_PAIR_REV_2_0
	#define CFG_APP_GFPS
    #ifndef CFG_APP_SEC
    #define CFG_APP_SEC
    #endif
#else
	#undef CFG_APP_GFPS
#endif
#endif
#if VOICE_DATAPATH
#define CFG_APP_VOICEPATH
#endif

#ifdef BES_OTA_ENABLED
#define CFG_APP_OTA
#endif

#if defined  (__AMA_VOICE__)
#ifndef CFG_APP_SEC
#define CFG_APP_SEC
#endif
#ifndef CFG_SEC_CON
#define CFG_SEC_CON
#endif

#define CFG_APP_AMA_VOICE
#endif

#if VOICE_DATAPATH&&GSOUND_ENABLED
#define ANCS_PROXY_ENABLE 1
#else
#define ANCS_PROXY_ENABLE 0
#endif

#if ANCS_PROXY_ENABLE
#define CFG_APP_AMS
#define CFG_APP_ANCC
#endif
#define CFG_APP_SEC
/// Health Thermometer Application
#if defined(CFG_APP_HT)
#define BLE_APP_HT           1
#else // defined(CFG_APP_HT)
#define BLE_APP_HT           0
#endif // defined(CFG_APP_HT)

#if defined(CFG_APP_HR)
#define BLE_APP_HR           1
#else
#define BLE_APP_HR           0
#endif

/// Data Path Server Application
#if defined(CFG_APP_DATAPATH_SERVER)
#define BLE_APP_DATAPATH_SERVER           1
#else // defined(CFG_APP_DATAPATH_SERVER)
#define BLE_APP_DATAPATH_SERVER           0
#endif // defined(CFG_APP_DATAPATH_SERVER)

/// HID Application
#if defined(CFG_APP_HID)
#define BLE_APP_HID          1
#else // defined(CFG_APP_HID)
#define BLE_APP_HID          0
#endif // defined(CFG_APP_HID)

/// DIS Application
#if defined(CFG_APP_DIS)
#define BLE_APP_DIS          1
#else // defined(CFG_APP_DIS)
#define BLE_APP_DIS          0
#endif // defined(CFG_APP_DIS)

/// Time Application
#if defined(CFG_APP_TIME)
#define BLE_APP_TIME         1
#else // defined(CFG_APP_TIME)
#define BLE_APP_TIME         0
#endif // defined(CFG_APP_TIME)

/// Battery Service Application
#if (BLE_APP_HID)
#define BLE_APP_BATT          1
#else
#define BLE_APP_BATT          0
#endif // (BLE_APP_HID)

/// Security Application
#if (defined(CFG_APP_SEC) || BLE_APP_HID || defined(BLE_APP_AM0))
#define BLE_APP_SEC          1
#else // defined(CFG_APP_SEC)
#define BLE_APP_SEC          0
#endif // defined(CFG_APP_SEC)

/// Voice Path Application
#if defined(CFG_APP_VOICEPATH)
#define BLE_APP_VOICEPATH           1
#else // defined(CFG_APP_VOICEPATH)
#define BLE_APP_VOICEPATH           0
#endif // defined(CFG_APP_VOICEPATH)

/// OTA Application
#if defined(CFG_APP_OTA)
#define BLE_APP_OTA           1
#else // defined(CFG_APP_OTA)
#define BLE_APP_OTA           0
#endif // defined(CFG_APP_OTA)

/// ANCC Application
#if defined(CFG_APP_ANCC)
#define BLE_APP_ANCC    1
#else // defined(CFG_APP_ANCC)
#define BLE_APP_ANCC    0
#endif // defined(CFG_APP_ANCC)

/// AMS Application
#if defined(CFG_APP_AMS)
#define BLE_APP_AMS    1
#else // defined(CFG_APP_AMS)
#define BLE_APP_AMS    0
#endif // defined(CFG_APP_AMS)
/// GFPS Application
#if defined(CFG_APP_GFPS)
#define BLE_APP_GFPS          1
#else // defined(CFG_APP_GFPS)
#define BLE_APP_GFPS          0
#endif // defined(CFG_APP_GFPS)


/// AMA Voice Application
#if defined(CFG_APP_AMA_VOICE)
#define BLE_APP_AMA_VOICE           1
#else // defined(CFG_APP_AMA)
#define BLE_APP_AMA_VOICE           0
#endif // defined(CFG_APP_AMA)

/// @} rwapp_config

#endif /* _RWAPP_CONFIG_H_ */
