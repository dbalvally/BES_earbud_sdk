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
#include <stdio.h>
#include "hal_trace.h"
#include "log_section.h"
#include <string.h>
#include "hal_timer.h"
#include "hal_norflash.h"
#include "cmsis.h"
#include "hal_cache.h"
#include "pmu.h"
#include "cmsis_os.h"

extern uint32_t __debug_log_start[];
extern uint32_t __debug_log_end[];
extern uint32_t __user_statistics_start[];
extern uint32_t __user_statistics_end[];

static uint32_t __debug_trace_start = 0x3c200000;
static uint32_t section_size_debug_trace_log;

static uint32_t section_size_debug_log;
static uint32_t section_size_user_statistics_data;

extern int hal_trace_printf_ex(uint32_t lvl, const char *fmt, ...);

#define TRACE_EX(str, ...)             hal_trace_printf_ex(HAL_TRACE_LEVEL_0, str, ##__VA_ARGS__)

#define LOG_COMPRESS_RATION             1

#define DUMP_LOG_MAGIC_CODE             0x44554D50

#define FLASH_SECTOR_SIZE_IN_BYTES      0x1000
#define BRIDGE_EVENT_LOG_BUFFER_SIZE    0x1000
#define HISTORY_EVENT_LOG_BUFFER_SIZE   0x5000
#define MARGIN_HISTORY_EVENT_LOG_BUFFER_SIZE    (BRIDGE_EVENT_LOG_BUFFER_SIZE*2)
#define SIZE_TO_PUSH_HISTORY_EVENT_LOG  (BRIDGE_EVENT_LOG_BUFFER_SIZE/LOG_COMPRESS_RATION)

#if HISTORY_EVENT_LOG_BUFFER_SIZE%BRIDGE_EVENT_LOG_BUFFER_SIZE
#error hisotry event log buffer size must be integral multiple of the bridge event log buffer!
#endif

static uint16_t event_log_size_in_bridge_buffer = 0;
static uint8_t bridge_event_log_buffer[BRIDGE_EVENT_LOG_BUFFER_SIZE];

static uint16_t event_log_size_in_history_buffer = 0;
static uint32_t offset_to_fill_event_log_history_buffer = 0;
static uint8_t history_event_log_buffer[HISTORY_EVENT_LOG_BUFFER_SIZE];

#define BRIDGE_TRACE_LOG_BUFFER_SIZE    0x1000
#define HISTORY_TRACE_LOG_BUFFER_SIZE   0x14000
#define MARGIN_HISTORY_TRACE_LOG_BUFFER_SIZE    (BRIDGE_TRACE_LOG_BUFFER_SIZE*3)

#define SIZE_TO_PUSH_HISTORY_TRACE_LOG      (BRIDGE_TRACE_LOG_BUFFER_SIZE/LOG_COMPRESS_RATION)

#if HISTORY_TRACE_LOG_BUFFER_SIZE%BRIDGE_TRACE_LOG_BUFFER_SIZE
#error hisotry trace log buffer size must be integral multiple of the bridge trace log buffer!
#endif

static uint32_t trace_log_size_in_bridge_buffer = 0;
static uint8_t bridge_trace_log_buffer[BRIDGE_TRACE_LOG_BUFFER_SIZE];

static uint32_t trace_log_size_in_history_buffer = 0;
static uint32_t offset_to_fill_trace_log_history_buffer = 0;
static uint8_t history_trace_log_buffer[HISTORY_EVENT_LOG_BUFFER_SIZE];

LOG_NV_INFO_T log_nv_info;
static uint32_t offsetInUserStatisticsSectionToWrite = 0;
uint16_t logTwsSyncMark;

static uint32_t lastLoggingTimerMs = 0;

osMutexDef(log_dump_mutex);

static uint8_t isDumpLogPendingForFlush = false;

extern bool bt_media_is_media_active(void);

static void compress_log(uint8_t* input, uint8_t* output, uint32_t len)
{
    // TODO:
    memcpy(output, input, len);
}

static void dump_data_into_debug_log_section(uint8_t* ptrSource, uint32_t lengthToBurn, 
    uint32_t offsetInFlashToProgram)
{
    TRACE_EX("Write %d bytes log into 0x%x", 
        lengthToBurn, offsetInFlashToProgram);
    
    uint32_t lock;

    lock = int_lock();
    pmu_flash_write_config();

    uint32_t preBytes = (FLASH_SECTOR_SIZE_IN_BYTES - (offsetInFlashToProgram%FLASH_SECTOR_SIZE_IN_BYTES))%FLASH_SECTOR_SIZE_IN_BYTES;
    if (lengthToBurn < preBytes)
    {
        preBytes = lengthToBurn;
    }
    
    uint32_t middleBytes = 0;
    if (lengthToBurn > preBytes)
    {
       middleBytes = ((lengthToBurn - preBytes)/FLASH_SECTOR_SIZE_IN_BYTES*FLASH_SECTOR_SIZE_IN_BYTES);
    }
    uint32_t postBytes = 0;
    if (lengthToBurn > (preBytes + middleBytes))
    {
        postBytes = (offsetInFlashToProgram + lengthToBurn)%FLASH_SECTOR_SIZE_IN_BYTES;
    }

    if (preBytes > 0)
    {
        hal_norflash_write(HAL_NORFLASH_ID_0, offsetInFlashToProgram, 
            ptrSource, preBytes);

        ptrSource += preBytes; 
        offsetInFlashToProgram += preBytes;
    }

    uint32_t sectorIndexInFlash = offsetInFlashToProgram/FLASH_SECTOR_SIZE_IN_BYTES;

    if (middleBytes > 0)
    {
        uint32_t sectorCntToProgram = middleBytes/FLASH_SECTOR_SIZE_IN_BYTES;
        for (uint32_t sector = 0;sector < sectorCntToProgram;sector++)
        {
            hal_norflash_erase(HAL_NORFLASH_ID_0, sectorIndexInFlash*FLASH_SECTOR_SIZE_IN_BYTES, FLASH_SECTOR_SIZE_IN_BYTES);

            hal_norflash_write(HAL_NORFLASH_ID_0, sectorIndexInFlash*FLASH_SECTOR_SIZE_IN_BYTES, 
                ptrSource + sector*FLASH_SECTOR_SIZE_IN_BYTES, FLASH_SECTOR_SIZE_IN_BYTES);

            sectorIndexInFlash++;
        }    

        ptrSource += middleBytes;
    }

    if (postBytes > 0)
    {
        hal_norflash_erase(HAL_NORFLASH_ID_0, sectorIndexInFlash*FLASH_SECTOR_SIZE_IN_BYTES, FLASH_SECTOR_SIZE_IN_BYTES);
        hal_norflash_write(HAL_NORFLASH_ID_0, sectorIndexInFlash*FLASH_SECTOR_SIZE_IN_BYTES, 
                ptrSource, postBytes);        
    }        

    pmu_flash_read_config();
    int_unlock(lock);
}

void flush_pending_event_log(void)
{   
    uint32_t lock;
    TRACE_EX("%s\n",__func__);
    if (event_log_size_in_history_buffer ==0)
        return;
    lock = int_lock();

    compress_log(bridge_event_log_buffer, 
            &history_event_log_buffer[offset_to_fill_event_log_history_buffer], 
            event_log_size_in_bridge_buffer);
    offset_to_fill_event_log_history_buffer += 
        (event_log_size_in_bridge_buffer/LOG_COMPRESS_RATION);

    event_log_size_in_history_buffer += (event_log_size_in_bridge_buffer/LOG_COMPRESS_RATION);
    if (event_log_size_in_history_buffer > HISTORY_EVENT_LOG_BUFFER_SIZE)
    {
        event_log_size_in_history_buffer = HISTORY_EVENT_LOG_BUFFER_SIZE;
    }

    uint32_t historyEventLogSize = event_log_size_in_history_buffer;

    uint32_t leftSpaceInDebugLogSection = section_size_debug_log - 
        log_nv_info.offsetInDebugLogSectionToWrite;
    uint32_t index = 0;

    if (leftSpaceInDebugLogSection < historyEventLogSize)
    {

        TRACE_EX("leftSpaceInDebugLogSection %d historyEventLogSize: %d,offsetInDebugLogSectionToWrite=%d", 
                leftSpaceInDebugLogSection,historyEventLogSize,log_nv_info.offsetInDebugLogSectionToWrite);
        dump_data_into_debug_log_section(&history_event_log_buffer[index],
                leftSpaceInDebugLogSection, 
                (uint32_t)(__debug_log_start) + 
                log_nv_info.offsetInDebugLogSectionToWrite);

        log_nv_info.offsetInDebugLogSectionToWrite = 0;

        TRACE_EX("offset in log section is %d at line %d", 
                log_nv_info.offsetInDebugLogSectionToWrite, __LINE__);

        index += leftSpaceInDebugLogSection;

        historyEventLogSize -= leftSpaceInDebugLogSection;
    }
    TRACE_EX("2 leftSpaceInDebugLogSection %d historyEventLogSize: %d,offsetInDebugLogSectionToWrite=%d", 
            leftSpaceInDebugLogSection,historyEventLogSize,log_nv_info.offsetInDebugLogSectionToWrite);

    dump_data_into_debug_log_section(&history_event_log_buffer[index],
            historyEventLogSize, 
            (uint32_t)(__debug_log_start) + 
            log_nv_info.offsetInDebugLogSectionToWrite);

    log_nv_info.offsetInDebugLogSectionToWrite += historyEventLogSize;
    TRACE_EX("offset in log section is %d at line %d", 
            log_nv_info.offsetInDebugLogSectionToWrite, __LINE__);

    event_log_size_in_bridge_buffer = 0;    
    offset_to_fill_event_log_history_buffer = 0;
    event_log_size_in_history_buffer = 0;

    int_unlock(lock);
}

void push_event_log_into_bridge_buffer(uint8_t* ptr, uint32_t len)
{
    uint32_t lock;
    bool isToFlushPendingEventLog = false;
    uint32_t index = 0, leftBytes;
    uint32_t sizeToPushBridgeBuffer = 0;
    TRACE_EX("%s\n",__func__);
    lock = int_lock();
    
    leftBytes = len;
    
    while ((event_log_size_in_bridge_buffer + leftBytes) >= BRIDGE_EVENT_LOG_BUFFER_SIZE)
    {
        sizeToPushBridgeBuffer = BRIDGE_EVENT_LOG_BUFFER_SIZE - event_log_size_in_bridge_buffer;
        
        memcpy(((uint8_t *)bridge_event_log_buffer) + event_log_size_in_bridge_buffer, 
            ptr + index, sizeToPushBridgeBuffer);

        compress_log(bridge_event_log_buffer, 
            &history_event_log_buffer[offset_to_fill_event_log_history_buffer], 
            BRIDGE_EVENT_LOG_BUFFER_SIZE);
        
        event_log_size_in_history_buffer += SIZE_TO_PUSH_HISTORY_EVENT_LOG;
        if (event_log_size_in_history_buffer > HISTORY_EVENT_LOG_BUFFER_SIZE)
        {
            event_log_size_in_history_buffer = HISTORY_EVENT_LOG_BUFFER_SIZE;
        }
        
        offset_to_fill_event_log_history_buffer += SIZE_TO_PUSH_HISTORY_EVENT_LOG;
        if ((HISTORY_EVENT_LOG_BUFFER_SIZE-MARGIN_HISTORY_EVENT_LOG_BUFFER_SIZE) <= 
            offset_to_fill_event_log_history_buffer)
        {
            // write the event log into flash
            if (bt_media_is_media_active())
            {
                set_dump_log_flush(true);
            }
            else
            {
                isToFlushPendingEventLog = true;                
            }
        }

        leftBytes -= sizeToPushBridgeBuffer;
        index += sizeToPushBridgeBuffer;
        event_log_size_in_bridge_buffer = 0;
    }

    sizeToPushBridgeBuffer = leftBytes;
    memcpy(((uint8_t *)bridge_event_log_buffer) + event_log_size_in_bridge_buffer, 
            ptr + index, sizeToPushBridgeBuffer);
    event_log_size_in_bridge_buffer += sizeToPushBridgeBuffer;

    int_unlock(lock);

    if (isToFlushPendingEventLog)
    {
        flush_pending_event_log();
    }
}

void log_fill_ASSERT_event(uint32_t traceLen)
{
    LOG_ASSERT_EVENT_T content;
    content.len= traceLen;
    push_event_log_into_bridge_buffer((uint8_t *)&content, sizeof(content));
}

#if DEBUG_LOG_ENABLED
static void flush_pending_trace_log(void)
{
    uint32_t lock;
    TRACE_EX("%s\n",__func__);
    TRACE_EX("offset_to_fill_trace_log_history_buffer=%d,trace_log_size_in_bridge_buffer=%d\n",offset_to_fill_trace_log_history_buffer,trace_log_size_in_bridge_buffer);
    lock = int_lock();
    
    compress_log(bridge_trace_log_buffer, 
        &history_trace_log_buffer[offset_to_fill_trace_log_history_buffer], 
        trace_log_size_in_bridge_buffer);

    trace_log_size_in_history_buffer += (trace_log_size_in_bridge_buffer/LOG_COMPRESS_RATION);
    if (trace_log_size_in_history_buffer > HISTORY_EVENT_LOG_BUFFER_SIZE)
    {
        trace_log_size_in_history_buffer = HISTORY_EVENT_LOG_BUFFER_SIZE;
    }

    uint32_t pendingHistoryTraceLog = trace_log_size_in_history_buffer;
    
    offset_to_fill_trace_log_history_buffer += 
        (trace_log_size_in_bridge_buffer/LOG_COMPRESS_RATION);

    uint32_t historyTraceLogSize = trace_log_size_in_history_buffer;
    uint32_t leftSpaceInDebugLogSection = section_size_debug_trace_log - 
        log_nv_info.offsetInTraceSectionToWrite;
    uint32_t index = 0;
    TRACE_EX("leftSpaceInDebugLogSection= %d, historyTraceLogSize= %d", 
        leftSpaceInDebugLogSection, historyTraceLogSize);

    if (leftSpaceInDebugLogSection < historyTraceLogSize)
    {
        dump_data_into_debug_log_section(&history_trace_log_buffer[index],
            leftSpaceInDebugLogSection, 
            (uint32_t)(__debug_trace_start) + 
            log_nv_info.offsetInTraceSectionToWrite);
        
        TRACE_EX("offset in trace log section is %d at line %d", 
            log_nv_info.offsetInTraceSectionToWrite, __LINE__);

        log_nv_info.offsetInTraceSectionToWrite = 0;

        index += leftSpaceInDebugLogSection;

        historyTraceLogSize -= leftSpaceInDebugLogSection;
    }

    dump_data_into_debug_log_section(&history_trace_log_buffer[index],
            historyTraceLogSize, 
            (uint32_t)(__debug_trace_start) + 
            log_nv_info.offsetInTraceSectionToWrite);

    log_nv_info.offsetInTraceSectionToWrite += historyTraceLogSize;
    
    TRACE_EX("offset in trace log section is %d at line %d", 
        log_nv_info.offsetInTraceSectionToWrite, __LINE__);
    
    trace_log_size_in_bridge_buffer = 0;    
    trace_log_size_in_history_buffer = 0;
    offset_to_fill_trace_log_history_buffer = 0;

    int_unlock(lock);

}
#endif

void dump_whole_logs_at_assert(void)
{   
#if DEBUG_LOG_ENABLED       
    flush_pending_event_log();
    
    flush_pending_trace_log();
#endif

#if USER_STATISTIC_DATA_LOG_ENABLED
    flush_user_statistics_data();
#endif
}

void dump_whole_logs_in_normal_case(void)
{   
#if DEBUG_LOG_ENABLED   
    flush_pending_event_log();
    flush_pending_trace_log();
#endif

#if USER_STATISTIC_DATA_LOG_ENABLED
    flush_user_statistics_data();
#endif
}

void log_section_init(void)
{   
    section_size_debug_log = (uint32_t)__debug_log_end - (uint32_t)__debug_log_start;
    section_size_user_statistics_data = (uint32_t)__user_statistics_end - (uint32_t)__user_statistics_start;
    section_size_debug_trace_log = (uint32_t)__debug_log_start - (uint32_t)__debug_trace_start;
    
    uint32_t user_statistics_nv_addr = (uint32_t)__user_statistics_start;
    TRACE_EX("dump log magic num 0x%x", ((LOG_NV_INFO_T *)(user_statistics_nv_addr))->dump_log_magic_code);
    TRACE_EX("log_section_init __debug_log_start=0X%x,__debug_trace_start=0x%x",__debug_log_start,__debug_trace_start);
    
    TRACE_EX("log_section_init section_size_debug_log=0X%x,section_size_debug_trace_log=0x%x",section_size_debug_log,section_size_debug_trace_log);
    DUMP8("%02x ", (uint8_t *)__debug_log_start, 32);
    uint32_t index = 0;
    if (DUMP_LOG_MAGIC_CODE == 
            ((LOG_NV_INFO_T *)(user_statistics_nv_addr))->dump_log_magic_code)
    {
        uint32_t nvEntryMaximumCount = section_size_user_statistics_data/sizeof(LOG_NV_INFO_T);
        int32_t formerLogIndex = -1;
        for (index = 0;index < nvEntryMaximumCount;index++)
        {
            LOG_NV_INFO_T* pLogNvInfo = (LOG_NV_INFO_T *)(user_statistics_nv_addr + index*sizeof(LOG_NV_INFO_T));
            if (DUMP_LOG_MAGIC_CODE == pLogNvInfo->dump_log_magic_code)
            {
                TRACE_EX("eventLogIndex %d formerLogIndex %d", (int32_t)pLogNvInfo->eventLogIndex,
                    formerLogIndex);
                if (((int32_t)pLogNvInfo->eventLogIndex) < formerLogIndex)
                {
                    break;
                }
                else
                {
                    formerLogIndex = pLogNvInfo->eventLogIndex;
                }
            }
            else
            {
                break;
            }
        }
        offsetInUserStatisticsSectionToWrite = index*sizeof(LOG_NV_INFO_T);
    }   
    else
    {
        memset((uint8_t *)&log_nv_info, 0, sizeof(log_nv_info));
        log_nv_info.dump_log_magic_code = DUMP_LOG_MAGIC_CODE;
    }

    TRACE_EX("offsetInUserStatisticsSectionToWrite %d", offsetInUserStatisticsSectionToWrite);

    if (0 != offsetInUserStatisticsSectionToWrite)
    {
        log_nv_info = *(LOG_NV_INFO_T *)(user_statistics_nv_addr + offsetInUserStatisticsSectionToWrite - 
            sizeof(LOG_NV_INFO_T));
    }

    TRACE_EX("eventLogIndex %d lifeTimeUntilNowInMin %d",
        log_nv_info.eventLogIndex, log_nv_info.lifeTimeUntilNowInMin);

    TRACE_EX("offsetInDebugLogSectionToWrite %d",
        log_nv_info.offsetInDebugLogSectionToWrite);
    
    TRACE_EX("offsetInTraceSectionToWrite %d",
        log_nv_info.offsetInTraceSectionToWrite);
}

void flush_user_statistics_data(void)
{
    uint32_t lock = int_lock();

    pmu_flash_write_config();
    if ((offsetInUserStatisticsSectionToWrite == 0) ||
        ((offsetInUserStatisticsSectionToWrite + sizeof(LOG_NV_INFO_T)) > 
        section_size_user_statistics_data))
    {
        hal_norflash_erase(HAL_NORFLASH_ID_0, (uint32_t)(__user_statistics_start), 
            section_size_user_statistics_data);

        offsetInUserStatisticsSectionToWrite = 0;
    }

    hal_norflash_write(HAL_NORFLASH_ID_0, 
        (uint32_t)((uint32_t)__user_statistics_start + offsetInUserStatisticsSectionToWrite), 
        (uint8_t *)&log_nv_info, sizeof(LOG_NV_INFO_T));
    pmu_flash_read_config();
    TRACE_EX("Flush user statistics data to location 0x%x", 
        (uint32_t)((uint32_t)__user_statistics_start + offsetInUserStatisticsSectionToWrite));
    offsetInUserStatisticsSectionToWrite += sizeof(LOG_NV_INFO_T);  

    int_unlock(lock);
}

void log_update_time_stamp(void)
{
    uint32_t passedTimerMs;
    uint32_t currentTimerMs = GET_CURRENT_MS();
    
    if (0 == lastLoggingTimerMs)
    {
        passedTimerMs = currentTimerMs;
    }
    else
    {
        if (currentTimerMs > lastLoggingTimerMs)
        {
            passedTimerMs = currentTimerMs - lastLoggingTimerMs;
        }
        else
        {
            passedTimerMs = 0xFFFFFFFF - lastLoggingTimerMs + currentTimerMs + 1;
        }
    }

    uint32_t minToAdd;
    if (passedTimerMs > 1000*60)
    {
        minToAdd = passedTimerMs/(60*1000);
        lastLoggingTimerMs += (minToAdd*60*1000);
        log_nv_info.lifeTimeUntilNowInMin += minToAdd;
    }   
}

static bool isCantDump = false;
void logsection_set_dont_dump(bool cantdump)
{
    isCantDump = cantdump;
}

void push_trace_log_into_bridge_buffer(uint8_t* ptr, uint32_t len)
{
    uint32_t lock;
    uint32_t index = 0, leftBytes;
    uint32_t sizeToPushBridgeBuffer = 0;
    bool isToFlushPendingTraceLog = false;
    if (isCantDump || ((int)section_size_debug_trace_log) <= 0)
        return;
    lock = int_lock();

    leftBytes = len;

    while ((trace_log_size_in_bridge_buffer + leftBytes) >= BRIDGE_TRACE_LOG_BUFFER_SIZE)
    {
        sizeToPushBridgeBuffer = BRIDGE_TRACE_LOG_BUFFER_SIZE - trace_log_size_in_bridge_buffer;

        memcpy(((uint8_t *)bridge_trace_log_buffer) + trace_log_size_in_bridge_buffer, 
                ptr + index, sizeToPushBridgeBuffer);
        if (offset_to_fill_trace_log_history_buffer+BRIDGE_TRACE_LOG_BUFFER_SIZE < HISTORY_EVENT_LOG_BUFFER_SIZE)
        {
            compress_log(bridge_trace_log_buffer, 
                    &history_trace_log_buffer[offset_to_fill_trace_log_history_buffer], 
                    BRIDGE_TRACE_LOG_BUFFER_SIZE);

            trace_log_size_in_history_buffer += SIZE_TO_PUSH_HISTORY_TRACE_LOG;             
            offset_to_fill_trace_log_history_buffer += SIZE_TO_PUSH_HISTORY_TRACE_LOG;
        }
        else
        {
            TRACE_EX("Losing trace");
        }
        if ((HISTORY_EVENT_LOG_BUFFER_SIZE-MARGIN_HISTORY_TRACE_LOG_BUFFER_SIZE) <= 
                offset_to_fill_trace_log_history_buffer)
        {
            // write the event log into flash
            if (bt_media_is_media_active())
            {
                set_dump_log_flush(true);
            }
            else
            {
                isToFlushPendingTraceLog = true;                
            }
        }

        leftBytes -= sizeToPushBridgeBuffer;
        index += sizeToPushBridgeBuffer;
        trace_log_size_in_bridge_buffer = 0;
    }

    sizeToPushBridgeBuffer = leftBytes;
    memcpy(((uint8_t *)bridge_trace_log_buffer) + trace_log_size_in_bridge_buffer, 
            ptr + index, sizeToPushBridgeBuffer);
    trace_log_size_in_bridge_buffer += sizeToPushBridgeBuffer;

    int_unlock(lock);

    if (isToFlushPendingTraceLog)
    {
#if DEBUG_LOG_ENABLED   
        flush_pending_trace_log();
#endif
    }
}

void set_dump_log_flush(bool isEnable)
{
    TRACE_EX("Set dump log flush flag %d", isEnable);
    isDumpLogPendingForFlush = isEnable;
}

bool is_dump_log_flush_pending(void)
{
    return isDumpLogPendingForFlush;
}

void flush_dump_log_handler(void)
{
#if DEBUG_LOG_ENABLED   
    set_dump_log_flush(false);
    flush_pending_event_log();
    flush_pending_trace_log();
#endif    
}

void clear_dump_log(void)
{
    uint32_t lock = int_lock();
    pmu_flash_write_config();

    hal_norflash_erase(HAL_NORFLASH_ID_0, (uint32_t)(__user_statistics_start), 
        section_size_user_statistics_data);     
    pmu_flash_read_config();

    offsetInUserStatisticsSectionToWrite = 0;
    memset((uint8_t *)&log_nv_info, 0, sizeof(log_nv_info));
    log_nv_info.dump_log_magic_code = DUMP_LOG_MAGIC_CODE;
    int_unlock(lock);
}

