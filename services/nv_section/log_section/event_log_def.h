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

#ifndef __EVENT_LOG_DEF_H__
#define __EVENT_LOG_DEF_H__

/* Event macro definition */
#define EVENT_START_BT_CONNECTING                 1
#define EVENT_START_WAITING_FOR_BT_CONNECTING     2
#define EVENT_BT_CONNECTION_CREATED               3
#define EVENT_DISCONNECT_BT_CONNECTION            4
#define EVENT_BT_CONNECTION_DOWN                  5
#define EVENT_START_RECONNECT_BLE_ADV             16
#define EVENT_TARGET_BLE_ADV_SCANNED              17
#define EVENT_BLE_CONNECTION_CREATED              18
#define EVENT_INITIATIVE_BLE_DISCONNECTING        19
#define EVENT_BLE_CONNECTION_DOWN                 20
#define EVENT_START_TWS_ROLE_SWITCH               32
#define EVENT_TWS_ROLE_SWITCH_DONE                33
#define EVENT_START_CONNECTING_A2DP               40
#define EVENT_A2DP_CONNECTED                      41
#define EVENT_DISCONNECT_A2DP                     42
#define EVENT_A2DP_DISCONNECTED                   43
#define EVENT_SUSPEND_A2DP_STREAMING              44
#define EVENT_A2DP_STREAMING_SUSPENDED            45
#define EVENT_START_A2DP_STREAMING                46
#define EVENT_A2DP_STREAMING_STARTED              47
#define EVENT_A2DP_CONFIG_UPDATED                 48
#define EVENT_START_CONNECTING_HFP                56
#define EVENT_HFP_CONNECTED                       57
#define EVENT_START_CREATING_HFP_SCO_LINK         58
#define EVENT_HFP_SCO_LINK_CREATED                59
#define EVENT_DISCONNECT_HFP                      60
#define EVENT_HFP_DISCONNECTED                    61
#define EVENT_DISCONNECT_HFP_SCO_LINK             62
#define EVENT_HFP_SCO_LINK_DISCONNECTED           63
#define EVENT_HFP_INCOMING_CALL                   64
#define EVENT_HFP_WAKEUP_SIRI                     65
#define EVENT_START_CONNECTING_AVRCP              80
#define EVENT_AVRCP_CONNECTED                     81
#define EVENT_DISCONNECT_AVRCP                    82
#define EVENT_AVRCP_DISCONNECTED                  83
#define EVENT_AVRCP_VOLUME_CHANGE_REQ_RECEIVED    84
#define EVENT_PUT_IN_BOX                          96
#define EVENT_OUT_OF_BOX                          97
#define EVENT_BOX_OPENED                          98
#define EVENT_BOX_CLOSED                          99
#define EVENT_PAIRING_STARTED                     112
#define EVENT_TWS_PAIRED                          113
#define EVENT_MOBILE_CONNECTED_DURING_PAIRING     114
#define EVENT_PAIRING_TIMEOUT                     115
#define EVENT_TRIGGER_BT_KEY                      128
#define EVENT_WEARED                              129
#define EVENT_TAKEN_OFF                           130
#define EVENT_BATTERY_LEVEL_CHANGED               136
#define EVENT_BATTERY_FULL                        137
#define EVENT_LOW_BATTERY_LEVEL_WARNING           138
#define EVENT_LOW_BATTERY_POWER_OFF               139
#define EVENT_START_MASTER_OTA                    144
#define EVENT_START_SLAVE_OTA                     145
#define EVENT_MASTER_OTA_DONE                     146
#define EVENT_SLAVE_OTA_DONE                      147
#define EVENT_MASTER_TO_SLAVE_INFO_EXCHANGE       160
#define EVENT_SLAVE_TO_MASTER_INFO_EXCHANGE       161
#define EVENT_MIC_SWITCHED                        176

/* Event struct definition */
typedef struct
{
    uint8_t roleToConnect :2;
    uint8_t reserve       :6;
    uint8_t DstBtAddr[6];
} __attribute__((packed)) LOG_START_BT_CONNECTING_T;

typedef struct
{
    uint8_t waitedRole :2;
    uint8_t reserve    :6;
} __attribute__((packed)) LOG_START_WAITING_FOR_BT_CONNECTING_T;

typedef struct
{
    uint8_t connectedRole :2;
    uint8_t connIndex     :2;
    uint8_t reserve       :4;
    uint8_t peerBtAddr[6];
} __attribute__((packed)) LOG_BT_CONNECTION_CREATED_T;

typedef struct
{
    uint8_t roleToDisconnect :2;
    uint8_t connIndex        :2;
    uint8_t reserve          :4;
} __attribute__((packed)) LOG_DISCONNECT_BT_CONNECTION_T;

typedef struct
{
    uint8_t disconnectedRole :2;
    uint8_t connIndex        :2;
    uint8_t reserve          :4;
    uint8_t errorCode;
} __attribute__((packed)) LOG_BT_CONNECTION_DOWN_T;

typedef struct
{
    uint8_t bleAddr[6];
} __attribute__((packed)) LOG_START_RECONNECT_BLE_ADV_T;

typedef struct
{
    uint8_t bleAddr[6];
} __attribute__((packed)) LOG_TARGET_BLE_ADV_SCANNED_T;

typedef struct
{
    uint8_t connIndex;
} __attribute__((packed)) LOG_BLE_CONNECTION_CREATED_T;

typedef struct
{
    uint8_t connIndex;
} __attribute__((packed)) LOG_INITIATIVE_BLE_DISCONNECTING_T;

typedef struct
{
    uint8_t connIndex;
    uint8_t errorCode;
} __attribute__((packed)) LOG_BLE_CONNECTION_DOWN_T;

typedef struct
{
    uint8_t retStatus;
} __attribute__((packed)) LOG_TWS_ROLE_SWITCH_DONE_T;

typedef struct
{
    uint8_t roleToConnect :2;
    uint8_t reserve       :6;
} __attribute__((packed)) LOG_START_CONNECTING_A2DP_T;

typedef struct
{
    uint8_t connectedRole :2;
    uint8_t connIndex     :2;
    uint8_t reserve       :4;
} __attribute__((packed)) LOG_A2DP_CONNECTED_T;

typedef struct
{
    uint8_t roleToDisconnect :2;
    uint8_t connIndex        :2;
    uint8_t reserve          :4;
} __attribute__((packed)) LOG_DISCONNECT_A2DP_T;

typedef struct
{
    uint8_t disconnectedRole :2;
    uint8_t connIndex        :2;
    uint8_t reserve          :4;
    uint8_t errorCode;
} __attribute__((packed)) LOG_A2DP_DISCONNECTED_T;

typedef struct
{
    uint8_t roleToSuspendA2dp :2;
    uint8_t connIndex         :2;
    uint8_t reserve           :4;
} __attribute__((packed)) LOG_SUSPEND_A2DP_STREAMING_T;

typedef struct
{
    uint8_t a2dpSuspendedRole :2;
    uint8_t connIndex         :2;
    uint8_t reserve           :4;
} __attribute__((packed)) LOG_A2DP_STREAMING_SUSPENDED_T;

typedef struct
{
    uint8_t roleToStartA2dp :2;
    uint8_t connIndex       :2;
    uint8_t reserve         :4;
} __attribute__((packed)) LOG_START_A2DP_STREAMING_T;

typedef struct
{
    uint8_t a2dpStartedRole :2;
    uint8_t connIndex       :2;
    uint8_t reserve         :4;
} __attribute__((packed)) LOG_A2DP_STREAMING_STARTED_T;

typedef struct
{
    uint8_t codecUpdatedRole :2;
    uint8_t connIndex        :2;
    uint8_t reserve          :4;
    uint8_t newCodec;
    uint32_t newSampleRate;
} __attribute__((packed)) LOG_A2DP_CONFIG_UPDATED_T;

typedef struct
{
    uint8_t connIndex :2;
    uint8_t reserve   :6;
} __attribute__((packed)) LOG_START_CONNECTING_HFP_T;

typedef struct
{
    uint8_t connIndex :2;
    uint8_t reserve   :6;
} __attribute__((packed)) LOG_HFP_CONNECTED_T;

typedef struct
{
    uint8_t connIndex :2;
    uint8_t reserve   :6;
} __attribute__((packed)) LOG_START_CREATING_HFP_SCO_LINK_T;

typedef struct
{
    uint8_t connIndex :2;
    uint8_t reserve   :6;
} __attribute__((packed)) LOG_HFP_SCO_LINK_CREATED_T;

typedef struct
{
    uint8_t connIndex :2;
    uint8_t reserve   :6;
} __attribute__((packed)) LOG_DISCONNECT_HFP_T;

typedef struct
{
    uint8_t connIndex :2;
    uint8_t reserve   :6;
    uint8_t errorCode;
} __attribute__((packed)) LOG_HFP_DISCONNECTED_T;

typedef struct
{
    uint8_t connIndex :2;
    uint8_t reserve   :6;
} __attribute__((packed)) LOG_DISCONNECT_HFP_SCO_LINK_T;

typedef struct
{
    uint8_t connIndex :2;
    uint8_t reserve   :6;
    uint8_t errorCode;
} __attribute__((packed)) LOG_HFP_SCO_LINK_DISCONNECTED_T;

typedef struct
{
    uint8_t connIndex :2;
    uint8_t reserve   :6;
} __attribute__((packed)) LOG_HFP_INCOMING_CALL_T;

typedef struct
{
    uint8_t connIndex :2;
    uint8_t isEnable  :1;
    uint8_t reserve   :5;
} __attribute__((packed)) LOG_HFP_WAKEUP_SIRI_T;

typedef struct
{
    uint8_t connIndex :2;
    uint8_t reserve   :6;
} __attribute__((packed)) LOG_START_CONNECTING_AVRCP_T;

typedef struct
{
    uint8_t connIndex :2;
    uint8_t reserve   :6;
} __attribute__((packed)) LOG_AVRCP_CONNECTED_T;

typedef struct
{
    uint8_t connIndex :2;
    uint8_t reserve   :6;
} __attribute__((packed)) LOG_DISCONNECT_AVRCP_T;

typedef struct
{
    uint8_t connIndex :2;
    uint8_t reserve   :6;
    uint8_t errorCode;
} __attribute__((packed)) LOG_AVRCP_DISCONNECTED_T;

typedef struct
{
    uint8_t connIndex :2;
    uint8_t reserve   :6;
    uint8_t newVolume;
} __attribute__((packed)) LOG_AVRCP_VOLUME_CHANGE_REQ_RECEIVED_T;

typedef struct
{
    uint8_t assignedRole :2;
    uint8_t reserve      :6;
    uint8_t peerBtAddr[6];
} __attribute__((packed)) LOG_PAIRING_STARTED_T;

typedef struct
{
    uint8_t assignedRole :2;
    uint8_t reserve      :6;
    uint8_t peerBtAddr[6];
} __attribute__((packed)) LOG_TWS_PAIRED_T;

typedef struct
{
    uint8_t mobileBtAddr[6];
} __attribute__((packed)) LOG_MOBILE_CONNECTED_DURING_PAIRING_T;

typedef struct
{
    uint8_t keyType;
} __attribute__((packed)) LOG_TRIGGER_BT_KEY_T;

typedef struct
{
    uint8_t newLevel;
} __attribute__((packed)) LOG_BATTERY_LEVEL_CHANGED_T;

typedef struct
{
    uint16_t len;
    uint8_t value[1];
} __attribute__((packed)) LOG_MASTER_TO_SLAVE_INFO_EXCHANGE_T;

typedef struct
{
    uint16_t len;
    uint8_t value[1];
} __attribute__((packed)) LOG_SLAVE_TO_MASTER_INFO_EXCHANGE_T;

typedef struct
{
    uint8_t isToEnable :1;
    uint8_t reserve    :7;
} __attribute__((packed)) LOG_MIC_SWITCHED_T;


#if DEBUG_LOG_ENABLED
#define log_event(event) \
    do { \
        log_fill_event_entry(event); \
    } while(0)

#define log_event_1(event, arg1) \
    do { \
        log_fill_##event##_event(arg1); \
    } while(0)

#define log_event_2(event, arg1, arg2) \
    do { \
        log_fill_##event##_event(arg1, arg2); \
    } while(0)

#define log_event_3(event, arg1, arg2, arg3) \
    do { \
        log_fill_##event##_event(arg1, arg2, arg3); \
    } while(0)

#define log_event_4(event, arg1, arg2, arg3, arg4) \
    do { \
        log_fill_##event##_event(arg1, arg2, arg3, arg4); \
    } while(0)
#else /*!DEBUG_LOG_ENABLED*/
#define log_event(event)
#define log_event_1(event, arg1)
#define log_event_2(event, arg1, arg2)
#define log_event_3(event, arg1, arg2, arg3)
#define log_event_4(event, arg1, arg2, arg3, arg4)
#endif/*DEBUG_LOG_ENABLED*/

/* Event function declaration */
#if DEBUG_LOG_ENABLED
void log_fill_event_entry(int event);
void log_fill_EVENT_START_BT_CONNECTING_event(uint8_t roleToConnect, uint8_t* DstBtAddr);
void log_fill_EVENT_START_WAITING_FOR_BT_CONNECTING_event(uint8_t waitedRole);
void log_fill_EVENT_BT_CONNECTION_CREATED_event(uint8_t connectedRole, uint8_t connIndex, uint8_t* peerBtAddr);
void log_fill_EVENT_DISCONNECT_BT_CONNECTION_event(uint8_t roleToDisconnect, uint8_t connIndex);
void log_fill_EVENT_BT_CONNECTION_DOWN_event(uint8_t disconnectedRole, uint8_t connIndex, uint8_t errorCode);
void log_fill_EVENT_START_RECONNECT_BLE_ADV_event(uint8_t* bleAddr);
void log_fill_EVENT_TARGET_BLE_ADV_SCANNED_event(uint8_t* bleAddr);
void log_fill_EVENT_BLE_CONNECTION_CREATED_event(uint8_t connIndex);
void log_fill_EVENT_INITIATIVE_BLE_DISCONNECTING_event(uint8_t connIndex);
void log_fill_EVENT_BLE_CONNECTION_DOWN_event(uint8_t connIndex, uint8_t errorCode);
void log_fill_EVENT_TWS_ROLE_SWITCH_DONE_event(uint8_t retStatus);
void log_fill_EVENT_START_CONNECTING_A2DP_event(uint8_t roleToConnect);
void log_fill_EVENT_A2DP_CONNECTED_event(uint8_t connectedRole, uint8_t connIndex);
void log_fill_EVENT_DISCONNECT_A2DP_event(uint8_t roleToDisconnect, uint8_t connIndex);
void log_fill_EVENT_A2DP_DISCONNECTED_event(uint8_t disconnectedRole, uint8_t connIndex, uint8_t errorCode);
void log_fill_EVENT_SUSPEND_A2DP_STREAMING_event(uint8_t roleToSuspendA2dp, uint8_t connIndex);
void log_fill_EVENT_A2DP_STREAMING_SUSPENDED_event(uint8_t a2dpSuspendedRole, uint8_t connIndex);
void log_fill_EVENT_START_A2DP_STREAMING_event(uint8_t roleToStartA2dp, uint8_t connIndex);
void log_fill_EVENT_A2DP_STREAMING_STARTED_event(uint8_t a2dpStartedRole, uint8_t connIndex);
void log_fill_EVENT_A2DP_CONFIG_UPDATED_event(uint8_t codecUpdatedRole, uint8_t connIndex, uint8_t newCodec, uint32_t newSampleRate);
void log_fill_EVENT_START_CONNECTING_HFP_event(uint8_t connIndex);
void log_fill_EVENT_HFP_CONNECTED_event(uint8_t connIndex);
void log_fill_EVENT_START_CREATING_HFP_SCO_LINK_event(uint8_t connIndex);
void log_fill_EVENT_HFP_SCO_LINK_CREATED_event(uint8_t connIndex);
void log_fill_EVENT_DISCONNECT_HFP_event(uint8_t connIndex);
void log_fill_EVENT_HFP_DISCONNECTED_event(uint8_t connIndex, uint8_t errorCode);
void log_fill_EVENT_DISCONNECT_HFP_SCO_LINK_event(uint8_t connIndex);
void log_fill_EVENT_HFP_SCO_LINK_DISCONNECTED_event(uint8_t connIndex, uint8_t errorCode);
void log_fill_EVENT_HFP_INCOMING_CALL_event(uint8_t connIndex);
void log_fill_EVENT_HFP_WAKEUP_SIRI_event(uint8_t connIndex, uint8_t isEnable);
void log_fill_EVENT_START_CONNECTING_AVRCP_event(uint8_t connIndex);
void log_fill_EVENT_AVRCP_CONNECTED_event(uint8_t connIndex);
void log_fill_EVENT_DISCONNECT_AVRCP_event(uint8_t connIndex);
void log_fill_EVENT_AVRCP_DISCONNECTED_event(uint8_t connIndex, uint8_t errorCode);
void log_fill_EVENT_AVRCP_VOLUME_CHANGE_REQ_RECEIVED_event(uint8_t connIndex, uint8_t newVolume);
void log_fill_EVENT_PAIRING_STARTED_event(uint8_t assignedRole, uint8_t* peerBtAddr);
void log_fill_EVENT_TWS_PAIRED_event(uint8_t assignedRole, uint8_t* peerBtAddr);
void log_fill_EVENT_MOBILE_CONNECTED_DURING_PAIRING_event(uint8_t* mobileBtAddr);
void log_fill_EVENT_TRIGGER_BT_KEY_event(uint8_t keyType);
void log_fill_EVENT_BATTERY_LEVEL_CHANGED_event(uint8_t newLevel);
void log_fill_EVENT_MASTER_TO_SLAVE_INFO_EXCHANGE_event(uint16_t len, uint8_t* value);
void log_fill_EVENT_SLAVE_TO_MASTER_INFO_EXCHANGE_event(uint16_t len, uint8_t* value);
void log_fill_EVENT_MIC_SWITCHED_event(uint8_t isToEnable);
#else  // !DEBUG_LOG_ENABLED
#define log_fill_event_entry(...)
#define log_fill_EVENT_START_BT_CONNECTING_event(...)
#define log_fill_EVENT_START_WAITING_FOR_BT_CONNECTING_event(...)
#define log_fill_EVENT_BT_CONNECTION_CREATED_event(...)
#define log_fill_EVENT_DISCONNECT_BT_CONNECTION_event(...)
#define log_fill_EVENT_BT_CONNECTION_DOWN_event(...)
#define log_fill_EVENT_START_RECONNECT_BLE_ADV_event(...)
#define log_fill_EVENT_TARGET_BLE_ADV_SCANNED_event(...)
#define log_fill_EVENT_BLE_CONNECTION_CREATED_event(...)
#define log_fill_EVENT_INITIATIVE_BLE_DISCONNECTING_event(...)
#define log_fill_EVENT_BLE_CONNECTION_DOWN_event(...)
#define log_fill_START_TWS_ROLE_SWITCH_event(...)
#define log_fill_TWS_ROLE_SWITCH_DONE_event(...)
#define log_fill_EVENT_START_CONNECTING_A2DP_event(...)
#define log_fill_EVENT_A2DP_CONNECTED_event(...)
#define log_fill_EVENT_DISCONNECT_A2DP_event(...)
#define log_fill_EVENT_A2DP_DISCONNECTED_event(...)
#define log_fill_EVENT_SUSPEND_A2DP_STREAMING_event(...)
#define log_fill_EVENT_A2DP_STREAMING_SUSPENDED_event(...)
#define log_fill_EVENT_START_A2DP_STREAMING_event(...)
#define log_fill_EVENT_A2DP_STREAMING_STARTED_event(...)
#define log_fill_EVENT_A2DP_CONFIG_UPDATED_event(...)
#define log_fill_EVENT_START_CONNECTING_HFP_event(...)
#define log_fill_EVENT_HFP_CONNECTED_event(...)
#define log_fill_EVENT_START_CREATING_HFP_SCO_LINK_event(...)
#define log_fill_EVENT_HFP_SCO_LINK_CREATED_event(...)
#define log_fill_EVENT_DISCONNECT_HFP_event(...)
#define log_fill_EVENT_HFP_DISCONNECTED_event(...)
#define log_fill_EVENT_DISCONNECT_HFP_SCO_LINK_event(...)
#define log_fill_EVENT_HFP_SCO_LINK_DISCONNECTED_event(...)
#define log_fill_EVENT_HFP_INCOMING_CALL_event(...)
#define log_fill_EVENT_HFP_WAKEUP_SIRI_event(...)
#define log_fill_EVENT_START_CONNECTING_AVRCP_event(...)
#define log_fill_EVENT_AVRCP_CONNECTED_event(...)
#define log_fill_EVENT_DISCONNECT_AVRCP_event(...)
#define log_fill_EVENT_AVRCP_DISCONNECTED_event(...)
#define log_fill_EVENT_AVRCP_VOLUME_CHANGE_REQ_RECEIVED_event(...)
#define log_fill_EVENT_PAIRING_STARTED_event(...)
#define log_fill_EVENT_TWS_PAIRED_event(...)
#define log_fill_EVENT_MOBILE_CONNECTED_DURING_PAIRING_event(...)
#define log_fill_EVENT_TRIGGER_BT_KEY_event(...)
#define log_fill_EVENT_BATTERY_LEVEL_CHANGED_event(...)
#define log_fill_EVENT_MASTER_TO_SLAVE_INFO_EXCHANGE_event(...)
#define log_fill_EVENT_SLAVE_TO_MASTER_INFO_EXCHANGE_event(...)
#define log_fill_EVENT_MIC_SWITCHED_event(...)

#endif  // DEBUG_LOG_ENABLED

#endif  // __EVENT_LOG_DEF_H__
