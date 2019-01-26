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
#ifndef __LOG_SECTION_H__
#define __LOG_SECTION_H__

#ifdef __cplusplus
extern "C" {
#endif
#include <string.h>

#define USER_HABIT_DATA_TYPE_ASSERT_COUNT                       0   // ASSERT count
#define USER_HABIT_DATA_TYPE_MUSIC_PLAYING_STUTTERING_COUNT     1   // Music playing stuttering count
#define USER_HABIT_DATA_TYPE_LASTING_TIME_AS_MASTER_IN_SECOND   2   // Lasting time as master
#define USER_HABIT_DATA_TYPE_LASTING_TIME_AS_SLAVE_IN_SECOND    3   // Lasting time as slave
#define USER_HABIT_DATA_TYPE_LASTING_TIME_AS_FREEMAN_IN_SECOND  4   // Lasting time as freeman
#define USER_HABIT_DATA_TYPE_MUSIC_PLAYING_LASTING_TIME_IN_SECOND   5   // Music playing lasting time
#define USER_HABIT_DATA_TYPE_PHONE_CALL_LASTING_TIME_IN_SECOND      6   // Phone call lasting time
#define USER_HABIT_DATA_TYPE_WEARING_COUNT                      7   // Wearing count
#define USER_HABIT_DATA_TYPE_PAIRING_COUNT                      8   // Pairing count
#define USER_HABIT_DATA_TYPE_ROLE_SWITCH_COUNT                  9   // TWS role switch count
#define USER_HABIT_DATA_TYPE_DOUBLE_CLICK_COUNT                 10  // Double click count
#define USER_HABIT_DATA_TYPE_SINGLE_CLICK_COUNT                 11  // Single click count
#define USER_HABIT_DATA_TYPE_IN_OUT_BOX_COUNT                   12  // In/out box count
#define USER_HABIT_DATA_TYPE_OPEN_CLOSE_BOX_COUNT               13  // Close/open box count
#define USER_HABIT_DATA_TYPE_MOST_FREQUENTLY_USED_VOLUME        14  // Most frequently used volume
#define USER_HABIT_DATA_TYPE_LOW_BATTERY_LEVEL_POWER_OFF_COUNT  15  // Low battery level power-off count
#define USER_HABIT_DATA_TYPE_NUMBER                             16

#define EVENT_ASSERT_EVENT  0

typedef struct
{
    uint32_t logIndex;  // Index of the log entry, used to locate the oldest 
                        // log entry during the system power-on time
    uint32_t btTicks;   // Time stamp in unit of 312.5us 

    uint32_t earbudMode     : 2;    // 0: freeman, 1:master, 2:slave
    uint32_t twsSyncMark    : 10;   // Used to sync the logs between TWS. 
                                    // If device is not connected with another twin device, 
                                    // the value is 0. 
                                    // When the master is connected with slave, 
                                    // it will generate a random number as the sync mark and 
                                    // send it to the slave.
    uint32_t eventType      : 10;   // event type
    uint32_t reserve        : 10;   // reserve

    // content, depends on the definition of the event log
} EVENT_LOG_ENTRY_T;

#include "event_log_def.h"

/* ASSERT_EVENT log content */
typedef struct
{
    uint32_t len;
    // following are log trace data
} __attribute__((packed)) LOG_ASSERT_EVENT_T;

typedef struct
{
    uint32_t dump_log_magic_code;   // must be DUMP_LOG_MAGIC_CODE
    uint32_t eventLogIndex;
    uint32_t offsetInDebugLogSectionToWrite;
    uint32_t offsetInTraceSectionToWrite;
    uint32_t lifeTimeUntilNowInMin;     // Lasting time since open-box, in the unit of minutes.
    uint32_t statisticsData[USER_HABIT_DATA_TYPE_NUMBER];
} LOG_NV_INFO_T;

extern LOG_NV_INFO_T    log_nv_info;
extern uint16_t         logTwsSyncMark;

#if USER_STATISTIC_DATA_LOG_ENABLED

#define UPDATE_USER_HABIT_DATA(dataType, value) \
    log_nv_info.statisticsData[(dataType)] = (value)

#define INCREASE_USER_HABIT_DATA(dataType)  \
    log_nv_info.statisticsData[(dataType)]++

#else

#define UPDATE_USER_HABIT_DATA(dataType, value)
#define INCREASE_USER_HABIT_DATA(dataType)


#endif

extern uint32_t btdrv_syn_get_curr_ticks(void);
extern uint16_t current_tws_mode(void);

void push_event_log_into_bridge_buffer(uint8_t* ptr, uint32_t len);
void dump_whole_logs_at_assert(void);
void dump_whole_logs_in_normal_case(void);
void log_section_init(void);
void flush_user_statistics_data(void);
void flush_pending_event_log(void);
void push_trace_log_into_bridge_buffer(uint8_t* ptr, uint32_t len);
void log_update_time_stamp(void);
void set_dump_log_flush(bool isEnable);
bool is_dump_log_flush_pending(void);
void flush_dump_log_handler(void);
void clear_dump_log(void);

#ifdef __cplusplus
}
#endif
#endif

