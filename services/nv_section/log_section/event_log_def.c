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
#include "event_log_def.h"
#include "log_section.h"
#include "hal_timer.h"

#if DEBUG_LOG_ENABLED

void log_fill_event_entry(int event)
{
    EVENT_LOG_ENTRY_T entry =
    {
        log_nv_info.eventLogIndex++,
        GET_CURRENT_TICKS(),
        current_tws_mode(),
        logTwsSyncMark,
        event,
        0,
    };
    push_event_log_into_bridge_buffer((uint8_t *)&entry, sizeof(entry));
}

void log_fill_EVENT_START_BT_CONNECTING_event(uint8_t roleToConnect, uint8_t* DstBtAddr)
{
    int event = EVENT_START_BT_CONNECTING;

    LOG_START_BT_CONNECTING_T content;
    content.roleToConnect = roleToConnect;
    memcpy(content.DstBtAddr, DstBtAddr, 6);

    push_event_log_into_bridge_buffer((uint8_t *)&content, sizeof(content));
    log_fill_event_entry(event);
}

void log_fill_EVENT_START_WAITING_FOR_BT_CONNECTING_event(uint8_t waitedRole)
{
    int event = EVENT_START_WAITING_FOR_BT_CONNECTING;

    LOG_START_WAITING_FOR_BT_CONNECTING_T content;
    content.waitedRole = waitedRole;

    push_event_log_into_bridge_buffer((uint8_t *)&content, sizeof(content));
    log_fill_event_entry(event);
}

void log_fill_EVENT_BT_CONNECTION_CREATED_event(uint8_t connectedRole, uint8_t connIndex, uint8_t* peerBtAddr)
{
    int event = EVENT_BT_CONNECTION_CREATED;

    LOG_BT_CONNECTION_CREATED_T content;
    content.connectedRole = connectedRole;
    content.connIndex = connIndex;
    memcpy(content.peerBtAddr, peerBtAddr, 6);

    push_event_log_into_bridge_buffer((uint8_t *)&content, sizeof(content));
    log_fill_event_entry(event);
}

void log_fill_EVENT_DISCONNECT_BT_CONNECTION_event(uint8_t roleToDisconnect, uint8_t connIndex)
{
    int event = EVENT_DISCONNECT_BT_CONNECTION;

    LOG_DISCONNECT_BT_CONNECTION_T content;
    content.roleToDisconnect = roleToDisconnect;
    content.connIndex = connIndex;

    push_event_log_into_bridge_buffer((uint8_t *)&content, sizeof(content));
    log_fill_event_entry(event);
}

void log_fill_EVENT_BT_CONNECTION_DOWN_event(uint8_t disconnectedRole, uint8_t connIndex, uint8_t errorCode)
{
    int event = EVENT_BT_CONNECTION_DOWN;

    LOG_BT_CONNECTION_DOWN_T content;
    content.disconnectedRole = disconnectedRole;
    content.connIndex = connIndex;
    content.errorCode = errorCode;

    push_event_log_into_bridge_buffer((uint8_t *)&content, sizeof(content));
    log_fill_event_entry(event);
}

void log_fill_EVENT_START_RECONNECT_BLE_ADV_event(uint8_t* bleAddr)
{
    int event  = EVENT_START_RECONNECT_BLE_ADV;

    LOG_START_RECONNECT_BLE_ADV_T content;
    memcpy(content.bleAddr, bleAddr, 6);

    push_event_log_into_bridge_buffer((uint8_t *)&content, sizeof(content));
    log_fill_event_entry(event);
}

void log_fill_EVENT_TARGET_BLE_ADV_SCANNED_event(uint8_t* bleAddr)
{
    int event = EVENT_TARGET_BLE_ADV_SCANNED;

    LOG_TARGET_BLE_ADV_SCANNED_T content;
    memcpy(content.bleAddr, bleAddr, 6);

    push_event_log_into_bridge_buffer((uint8_t *)&content, sizeof(content));
    log_fill_event_entry(event);
}

void log_fill_EVENT_BLE_CONNECTION_CREATED_event(uint8_t connIndex)
{
    int event = EVENT_BLE_CONNECTION_CREATED;

    LOG_BLE_CONNECTION_CREATED_T content;
    content.connIndex = connIndex;

    push_event_log_into_bridge_buffer((uint8_t *)&content, sizeof(content));
    log_fill_event_entry(event);
}

void log_fill_EVENT_INITIATIVE_BLE_DISCONNECTING_event(uint8_t connIndex)
{
    int event = EVENT_INITIATIVE_BLE_DISCONNECTING;

    LOG_INITIATIVE_BLE_DISCONNECTING_T content;
    content.connIndex = connIndex;

    push_event_log_into_bridge_buffer((uint8_t *)&content, sizeof(content));
    log_fill_event_entry(event);
}

void log_fill_EVENT_BLE_CONNECTION_DOWN_event(uint8_t connIndex, uint8_t errorCode)
{
    int event = EVENT_BLE_CONNECTION_DOWN;

    LOG_BLE_CONNECTION_DOWN_T content;
    content.connIndex = connIndex;
    content.errorCode = errorCode;

    push_event_log_into_bridge_buffer((uint8_t *)&content, sizeof(content));
    log_fill_event_entry(event);
}

void log_fill_EVENT_TWS_ROLE_SWITCH_DONE_event(uint8_t retStatus)
{
    int event = EVENT_TWS_ROLE_SWITCH_DONE;

    LOG_TWS_ROLE_SWITCH_DONE_T content;
    content.retStatus = retStatus;

    push_event_log_into_bridge_buffer((uint8_t *)&content, sizeof(content));
    log_fill_event_entry(event);
}

void log_fill_EVENT_START_CONNECTING_A2DP_event(uint8_t roleToConnect)
{
    int event = EVENT_START_CONNECTING_A2DP;

    LOG_START_CONNECTING_A2DP_T content;
    content.roleToConnect = roleToConnect;

    push_event_log_into_bridge_buffer((uint8_t *)&content, sizeof(content));
    log_fill_event_entry(event);
}

void log_fill_EVENT_A2DP_CONNECTED_event(uint8_t connectedRole, uint8_t connIndex)
{
    int event = EVENT_A2DP_CONNECTED;

    LOG_A2DP_CONNECTED_T content;
    content.connectedRole = connectedRole;
    content.connIndex = connIndex;

    push_event_log_into_bridge_buffer((uint8_t *)&content, sizeof(content));
    log_fill_event_entry(event);
}

void log_fill_EVENT_DISCONNECT_A2DP_event(uint8_t roleToDisconnect, uint8_t connIndex)
{
    int event = EVENT_DISCONNECT_A2DP;

    LOG_DISCONNECT_A2DP_T content;
    content.roleToDisconnect = roleToDisconnect;
    content.connIndex = connIndex;

    push_event_log_into_bridge_buffer((uint8_t *)&content, sizeof(content));
    log_fill_event_entry(event);
}

void log_fill_EVENT_A2DP_DISCONNECTED_event(uint8_t disconnectedRole, uint8_t connIndex, uint8_t errorCode)
{
    int event = EVENT_A2DP_DISCONNECTED;

    LOG_A2DP_DISCONNECTED_T content;
    content.disconnectedRole = disconnectedRole;
    content.connIndex = connIndex;
    content.errorCode = errorCode;

    push_event_log_into_bridge_buffer((uint8_t *)&content, sizeof(content));
    log_fill_event_entry(event);
}

void log_fill_EVENT_SUSPEND_A2DP_STREAMING_event(uint8_t roleToSuspendA2dp, uint8_t connIndex)
{
    int event = EVENT_SUSPEND_A2DP_STREAMING;

    LOG_SUSPEND_A2DP_STREAMING_T content;
    content.roleToSuspendA2dp = roleToSuspendA2dp;
    content.connIndex = connIndex;

    push_event_log_into_bridge_buffer((uint8_t *)&content, sizeof(content));
    log_fill_event_entry(event);
}

void log_fill_EVENT_A2DP_STREAMING_SUSPENDED_event(uint8_t a2dpSuspendedRole, uint8_t connIndex)
{
    int event = EVENT_A2DP_STREAMING_SUSPENDED;

    LOG_A2DP_STREAMING_SUSPENDED_T content;
    content.a2dpSuspendedRole = a2dpSuspendedRole;
    content.connIndex = connIndex;

    push_event_log_into_bridge_buffer((uint8_t *)&content, sizeof(content));
    log_fill_event_entry(event);
}

void log_fill_EVENT_START_A2DP_STREAMING_event(uint8_t roleToStartA2dp, uint8_t connIndex)
{
    int event = EVENT_START_A2DP_STREAMING;

    LOG_START_A2DP_STREAMING_T content;
    content.roleToStartA2dp = roleToStartA2dp;
    content.connIndex = connIndex;

    push_event_log_into_bridge_buffer((uint8_t *)&content, sizeof(content));
    log_fill_event_entry(event);
}

void log_fill_EVENT_A2DP_STREAMING_STARTED_event(uint8_t a2dpStartedRole, uint8_t connIndex)
{
    int event = EVENT_A2DP_STREAMING_STARTED;

    LOG_A2DP_STREAMING_STARTED_T content;
    content.a2dpStartedRole = a2dpStartedRole;
    content.connIndex = connIndex;

    push_event_log_into_bridge_buffer((uint8_t *)&content, sizeof(content));
    log_fill_event_entry(event);
}

void log_fill_EVENT_A2DP_CONFIG_UPDATED_event(uint8_t codecUpdatedRole, uint8_t connIndex, uint8_t newCodec, uint32_t newSampleRate)
{
    int event = EVENT_A2DP_CONFIG_UPDATED;

    LOG_A2DP_CONFIG_UPDATED_T content;
    content.codecUpdatedRole = codecUpdatedRole;
    content.connIndex = connIndex;
    content.newCodec = newCodec;
    content.newSampleRate = newSampleRate;

    push_event_log_into_bridge_buffer((uint8_t *)&content, sizeof(content));
    log_fill_event_entry(event);
}

void log_fill_EVENT_START_CONNECTING_HFP_event(uint8_t connIndex)
{
    int event = EVENT_START_CONNECTING_HFP;

    LOG_START_CONNECTING_HFP_T content;
    content.connIndex = connIndex;

    push_event_log_into_bridge_buffer((uint8_t *)&content, sizeof(content));
    log_fill_event_entry(event);
}

void log_fill_EVENT_HFP_CONNECTED_event(uint8_t connIndex)
{
    int event = EVENT_HFP_CONNECTED;

    LOG_HFP_CONNECTED_T content;
    content.connIndex = connIndex;

    push_event_log_into_bridge_buffer((uint8_t *)&content, sizeof(content));
    log_fill_event_entry(event);
}

void log_fill_EVENT_START_CREATING_HFP_SCO_LINK_event(uint8_t connIndex)
{
    int event = EVENT_START_CREATING_HFP_SCO_LINK;

    LOG_START_CREATING_HFP_SCO_LINK_T content;
    content.connIndex = connIndex;

    push_event_log_into_bridge_buffer((uint8_t *)&content, sizeof(content));
    log_fill_event_entry(event);
}

void log_fill_EVENT_HFP_SCO_LINK_CREATED_event(uint8_t connIndex)
{
    int event = EVENT_HFP_SCO_LINK_CREATED;

    LOG_HFP_SCO_LINK_CREATED_T content;
    content.connIndex = connIndex;

    push_event_log_into_bridge_buffer((uint8_t *)&content, sizeof(content));
    log_fill_event_entry(event);
}

void log_fill_EVENT_DISCONNECT_HFP_event(uint8_t connIndex)
{
    int event = EVENT_DISCONNECT_HFP;

    LOG_DISCONNECT_HFP_T content;
    content.connIndex = connIndex;

    push_event_log_into_bridge_buffer((uint8_t *)&content, sizeof(content));
    log_fill_event_entry(event);
}

void log_fill_EVENT_HFP_DISCONNECTED_event(uint8_t connIndex, uint8_t errorCode)
{
    int event = EVENT_HFP_DISCONNECTED;

    LOG_HFP_DISCONNECTED_T content;
    content.connIndex = connIndex;
    content.errorCode = errorCode;

    push_event_log_into_bridge_buffer((uint8_t *)&content, sizeof(content));
    log_fill_event_entry(event);
}

void log_fill_EVENT_DISCONNECT_HFP_SCO_LINK_event(uint8_t connIndex)
{
    int event = EVENT_DISCONNECT_HFP_SCO_LINK;

    LOG_DISCONNECT_HFP_SCO_LINK_T content;
    content.connIndex = connIndex;

    push_event_log_into_bridge_buffer((uint8_t *)&content, sizeof(content));
    log_fill_event_entry(event);
}

void log_fill_EVENT_HFP_SCO_LINK_DISCONNECTED_event(uint8_t connIndex, uint8_t errorCode)
{
    int event = EVENT_HFP_SCO_LINK_DISCONNECTED;

    LOG_HFP_SCO_LINK_DISCONNECTED_T content;
    content.connIndex = connIndex;
    content.errorCode = errorCode;

    push_event_log_into_bridge_buffer((uint8_t *)&content, sizeof(content));
    log_fill_event_entry(event);
}

void log_fill_EVENT_HFP_INCOMING_CALL_event(uint8_t connIndex)
{
    int event = EVENT_HFP_INCOMING_CALL;

    LOG_HFP_INCOMING_CALL_T content;
    content.connIndex = connIndex;

    push_event_log_into_bridge_buffer((uint8_t *)&content, sizeof(content));
    log_fill_event_entry(event);
}

void log_fill_EVENT_HFP_WAKEUP_SIRI_event(uint8_t connIndex, uint8_t isEnable)
{
    int event = EVENT_HFP_WAKEUP_SIRI;

    LOG_HFP_WAKEUP_SIRI_T content;
    content.connIndex = connIndex;
    content.isEnable = isEnable;

    push_event_log_into_bridge_buffer((uint8_t *)&content, sizeof(content));
    log_fill_event_entry(event);
}

void log_fill_EVENT_START_CONNECTING_AVRCP_event(uint8_t connIndex)
{
    int event = EVENT_START_CONNECTING_AVRCP;

    LOG_START_CONNECTING_AVRCP_T content;
    content.connIndex = connIndex;

    push_event_log_into_bridge_buffer((uint8_t *)&content, sizeof(content));
    log_fill_event_entry(event);
}

void log_fill_EVENT_AVRCP_CONNECTED_event(uint8_t connIndex)
{
    int event = EVENT_AVRCP_CONNECTED;

    LOG_AVRCP_CONNECTED_T content;
    content.connIndex = connIndex;

    push_event_log_into_bridge_buffer((uint8_t *)&content, sizeof(content));
    log_fill_event_entry(event);
}

void log_fill_EVENT_DISCONNECT_AVRCP_event(uint8_t connIndex)
{
    int event = EVENT_DISCONNECT_AVRCP;

    LOG_DISCONNECT_AVRCP_T content;
    content.connIndex = connIndex;

    push_event_log_into_bridge_buffer((uint8_t *)&content, sizeof(content));
    log_fill_event_entry(event);
}

void log_fill_EVENT_AVRCP_DISCONNECTED_event(uint8_t connIndex, uint8_t errorCode)
{
    int event = EVENT_AVRCP_DISCONNECTED;

    LOG_AVRCP_DISCONNECTED_T content;
    content.connIndex = connIndex;
    content.errorCode = errorCode;

    push_event_log_into_bridge_buffer((uint8_t *)&content, sizeof(content));
    log_fill_event_entry(event);
}

void log_fill_EVENT_AVRCP_VOLUME_CHANGE_REQ_RECEIVED_event(uint8_t connIndex, uint8_t newVolume)
{
    int event = EVENT_AVRCP_VOLUME_CHANGE_REQ_RECEIVED;

    LOG_AVRCP_VOLUME_CHANGE_REQ_RECEIVED_T content;
    content.connIndex = connIndex;
    content.newVolume = newVolume;

    push_event_log_into_bridge_buffer((uint8_t *)&content, sizeof(content));
    log_fill_event_entry(event);
}

void log_fill_EVENT_PAIRING_STARTED_event(uint8_t assignedRole, uint8_t* peerBtAddr)
{
    int event = EVENT_PAIRING_STARTED;

    LOG_PAIRING_STARTED_T content;
    content.assignedRole = assignedRole;
    memcpy(content.peerBtAddr, peerBtAddr, 6);

    push_event_log_into_bridge_buffer((uint8_t *)&content, sizeof(content));
    log_fill_event_entry(event);
}

void log_fill_EVENT_TWS_PAIRED_event(uint8_t assignedRole, uint8_t* peerBtAddr)
{
    int event = EVENT_TWS_PAIRED;

    LOG_TWS_PAIRED_T content;
    content.assignedRole = assignedRole;
    memcpy(content.peerBtAddr, peerBtAddr, 6);

    push_event_log_into_bridge_buffer((uint8_t *)&content, sizeof(content));
    log_fill_event_entry(event);
}

void log_fill_EVENT_MOBILE_CONNECTED_DURING_PAIRING_event(uint8_t* mobileBtAddr)
{
    int event = EVENT_MOBILE_CONNECTED_DURING_PAIRING;

    LOG_MOBILE_CONNECTED_DURING_PAIRING_T content;
    memcpy(content.mobileBtAddr, mobileBtAddr, 6);

    push_event_log_into_bridge_buffer((uint8_t *)&content, sizeof(content));
    log_fill_event_entry(event);
}

void log_fill_EVENT_TRIGGER_BT_KEY_event(uint8_t keyType)
{
    int event = EVENT_TRIGGER_BT_KEY;

    LOG_TRIGGER_BT_KEY_T content;
    content.keyType = keyType;

    push_event_log_into_bridge_buffer((uint8_t *)&content, sizeof(content));
    log_fill_event_entry(event);
}

void log_fill_EVENT_BATTERY_LEVEL_CHANGED_event(uint8_t newLevel)
{
    int event = EVENT_BATTERY_LEVEL_CHANGED;

    LOG_BATTERY_LEVEL_CHANGED_T content;
    content.newLevel = newLevel;

    push_event_log_into_bridge_buffer((uint8_t *)&content, sizeof(content));
    log_fill_event_entry(event);
}

void log_fill_MASTER_TO_SLAVE_INFO_EXCHANGE_event(uint16_t len, uint8_t* value)
{
    int event = EVENT_MASTER_TO_SLAVE_INFO_EXCHANGE;

    uint8_t tmpBuf[128];
    LOG_MASTER_TO_SLAVE_INFO_EXCHANGE_T* ptrLog = (LOG_MASTER_TO_SLAVE_INFO_EXCHANGE_T *)tmpBuf;
    memcpy(ptrLog->value, value, len);
    ptrLog->len = len;

    push_event_log_into_bridge_buffer((uint8_t *)ptrLog, sizeof(LOG_MASTER_TO_SLAVE_INFO_EXCHANGE_T) + len - 1);

    log_fill_event_entry(event);
}

void log_fill_EVENT_SLAVE_TO_MASTER_INFO_EXCHANGE_event(uint16_t len, uint8_t* value)
{
    int event = EVENT_SLAVE_TO_MASTER_INFO_EXCHANGE;

    uint8_t tmpBuf[128];
    LOG_SLAVE_TO_MASTER_INFO_EXCHANGE_T* ptrLog = (LOG_SLAVE_TO_MASTER_INFO_EXCHANGE_T *)tmpBuf;
    memcpy(ptrLog->value, value, len);
    ptrLog->len = len;

    push_event_log_into_bridge_buffer((uint8_t *)ptrLog, sizeof(LOG_SLAVE_TO_MASTER_INFO_EXCHANGE_T) + len - 1);

    log_fill_event_entry(event);
}

void log_fill_EVENT_MIC_SWITCHED_event(uint8_t isToEnable)
{
    int event = EVENT_MIC_SWITCHED;

    LOG_MIC_SWITCHED_T content;
    content.isToEnable = isToEnable;

    push_event_log_into_bridge_buffer((uint8_t *)&content, sizeof(content));
    log_fill_event_entry(event);
}

#endif  // DEBUG_LOG_ENABLED
