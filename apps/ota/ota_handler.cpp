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
#ifdef BES_OTA_ENABLED

#include "cmsis.h"
#include "cmsis_os.h"
#include "hal_timer.h"
#include "hal_gpio.h"
#include "hal_trace.h"
#include "string.h"
#include "crc32.h"
#include "pmu.h"
#include "hal_norflash.h"
#include "ota_handler.h"
#ifndef FPGA
#include "nvrecord.h"
#endif
#include "hal_cache.h"
#include "cmsis_nvic.h"
#include "hal_bootmode.h"

extern uint32_t __factory_start[];
static OTA_ENV_T    ota_env;
static uint32_t     user_data_nv_flash_offset;
#define OTA_BOOT_INFO_FLASH_OFFSET  0x1000

static void ota_send_configuration_response(bool isDone);
static void ota_flush_data_to_flash(uint8_t* ptrSource, uint32_t lengthToBurn, uint32_t offsetInFlashToProgram);
static void ota_send_segment_verification_response(bool isPass);
static bool ota_check_whole_image_crc(void);
static void ota_send_result_response(bool isSuccessful);
#define LEN_OF_IMAGE_TAIL_TO_FIND_SANITY_CRC    512
static const char* image_info_sanity_crc_key_word = "CRC32_OF_IMAGE=0x";
static const char* old_image_info_sanity_crc_key_word = "CRC32_OF_IMAGE=";

static void ota_update_nv_data(void)
{
    uint32_t lock;
    if (ota_env.configuration.isToClearUserData)
    {
        lock = int_lock_global();
        pmu_flash_write_config();
        hal_norflash_erase(HAL_NORFLASH_ID_0, OTA_FLASH_LOGIC_ADDR + ota_env.flasehOffsetOfUserDataPool,
                FLASH_SECTOR_SIZE_IN_BYTES);
        pmu_flash_read_config();
        int_unlock_global(lock);
    }

    if (ota_env.configuration.isToRenameBT || ota_env.configuration.isToRenameBLE ||
        ota_env.configuration.isToUpdateBTAddr || ota_env.configuration.isToUpdateBLEAddr)
    {
        factory_section_t* pOrgFactoryData, *pUpdatedFactoryData;
        pOrgFactoryData = (uint32_t *)(OTA_FLASH_LOGIC_ADDR + ota_env.flasehOffsetOfFactoryDataPool);
        memcpy(ota_env.dataBufferForBurning, (uint8_t *)pOrgFactoryData,
            FLASH_SECTOR_SIZE_IN_BYTES);
        pUpdatedFactoryData = (uint32_t *)&(ota_env.dataBufferForBurning);

        if (1 == factory_section_get_version())
        {
            if (ota_env.configuration.isToRenameBT)
            {
                memset(pUpdatedFactoryData->data.device_name, 0, 
                	sizeof(pUpdatedFactoryData->data.device_name));
                memcpy(pUpdatedFactoryData->data.device_name, (uint8_t *)(ota_env.configuration.newBTName),
                    NAME_LENGTH);
            }

            if (ota_env.configuration.isToUpdateBTAddr)
            {            
                memcpy(pUpdatedFactoryData->data.bt_address, (uint8_t *)(ota_env.configuration.newBTAddr),
                    BD_ADDR_LENGTH);
            }

            if (ota_env.configuration.isToUpdateBLEAddr)
            {
                memcpy(pUpdatedFactoryData->data.ble_address, (uint8_t *)(ota_env.configuration.newBLEAddr),
                    BD_ADDR_LENGTH);
            }

			pUpdatedFactoryData->data.
            pUpdatedFactoryData->head.crc = 
            	crc32(0,(unsigned char *)(&(pUpdatedFactoryData->head.reserved0)),
	    			sizeof(factory_section_t)-2-2-4);

        }
        else
        {
            if (ota_env.configuration.isToRenameBT)
            {
                memset(pUpdatedFactoryData->data.rev2_bt_name, 0, 
                	sizeof(pUpdatedFactoryData->data.rev2_bt_name));
                memcpy(pUpdatedFactoryData->data.rev2_bt_name, (uint8_t *)(ota_env.configuration.newBTName),
                    NAME_LENGTH);
            }

            if (ota_env.configuration.isToRenameBLE)
            {
                memset(pUpdatedFactoryData->data.rev2_ble_name, 0, 
                	sizeof(pUpdatedFactoryData->data.rev2_ble_name));
                memcpy(pUpdatedFactoryData->data.rev2_ble_name, (uint8_t *)(ota_env.configuration.newBLEName),
                    NAME_LENGTH);
            }

            if (ota_env.configuration.isToUpdateBTAddr)
            {            
                memcpy(pUpdatedFactoryData->data.rev2_bt_addr, (uint8_t *)(ota_env.configuration.newBTAddr),
                    BD_ADDR_LENGTH);
            }

            if (ota_env.configuration.isToUpdateBLEAddr)
            {
                memcpy(pUpdatedFactoryData->data.rev2_ble_addr, (uint8_t *)(ota_env.configuration.newBLEAddr),
                    BD_ADDR_LENGTH);
            }

            pUpdatedFactoryData->data.rev2_crc = 
            	crc32(0,(unsigned char *)(&(pUpdatedFactoryData->data.rev2_reserved0)),
				pUpdatedFactoryData->data.rev2_data_len);

        }
        lock = int_lock_global();
        pmu_flash_write_config();
        hal_norflash_erase(HAL_NORFLASH_ID_0, OTA_FLASH_LOGIC_ADDR + ota_env.flasehOffsetOfFactoryDataPool,
                FLASH_SECTOR_SIZE_IN_BYTES);

        hal_norflash_write(HAL_NORFLASH_ID_0, OTA_FLASH_LOGIC_ADDR + ota_env.flasehOffsetOfFactoryDataPool,
            (uint8_t *)pUpdatedFactoryData, FLASH_SECTOR_SIZE_IN_BYTES);
        pmu_flash_read_config();
        int_unlock_global(lock);        
    }
}

void ota_handler_init(void)
{
#ifdef __APP_USER_DATA_NV_FLASH_OFFSET__
    user_data_nv_flash_offset = __APP_USER_DATA_NV_FLASH_OFFSET__;
#else
    user_data_nv_flash_offset = hal_norflash_get_flash_total_size(HAL_NORFLASH_ID_0) - 2*4096;
#endif

    memset(&ota_env, 0, sizeof(ota_env));
    ota_reset_env();
}

void ota_update_MTU(uint16_t mtu)
{
    // remove the 3 bytes of overhead
    ota_env.dataPacketSize = mtu - 3;
    TRACE("updated data packet size is %d", ota_env.dataPacketSize);
}

void ota_reboot_to_use_new_image(void)
{
    TRACE("OTA data receiving is done successfully, systerm will reboot ");
    hal_sw_bootmode_set(HAL_SW_BOOTMODE_ENTER_HIDE_BOOT);
    hal_cmu_sys_reboot();
}

void ota_disconnection_handler(void)
{
    if (ota_env.isPendingForReboot)
    {
        ota_reboot_to_use_new_image();
    }
    else
    {
        ota_reset_env();
    }      
}

void ota_reset_env(void)
{
    ota_env.configuration.startLocationToWriteImage = NEW_IMAGE_FLASH_OFFSET;
    ota_env.offsetInFlashToProgram = ota_env.configuration.startLocationToWriteImage;;
    ota_env.offsetInFlashOfCurrentSegment = ota_env.offsetInFlashToProgram;

    ota_env.configuration.isToClearUserData = true;
    ota_env.configuration.isToRenameBLE = false;
    ota_env.configuration.isToRenameBT = false;
    ota_env.configuration.isToUpdateBLEAddr = false;
    ota_env.configuration.isToUpdateBTAddr = false;
    ota_env.configuration.lengthOfFollowingData = 0;
    ota_env.AlreadyReceivedConfigurationLength = 0;
    ota_env.flasehOffsetOfUserDataPool = user_data_nv_flash_offset;
    ota_env.flasehOffsetOfFactoryDataPool = user_data_nv_flash_offset + FLASH_SECTOR_SIZE_IN_BYTES;
    ota_env.crc32OfSegment = 0;
    ota_env.crc32OfImage = 0;
    ota_env.offsetInDataBufferForBurning = 0;
    ota_env.alreadyReceivedDataSizeOfImage = 0;
    ota_env.offsetOfImageOfCurrentSegment = 0;
    ota_env.isOTAInProgress = false;
    ota_env.isPendingForReboot = false;

    ota_env.leftSizeOfFlashContentToRead = 0;
}

void ota_register_transmitter(ota_transmit_data_t transmit_handle)
{
    ota_env.transmitHander = transmit_handle;
}

void ota_set_datapath_type(OTA_DATAPATH_TYPE_E datapathType)
{
    ota_env.dataPathType = datapathType;
}

/** Program the data in the data buffer to flash
  *
  * @param[in] ptrSource  Pointer of the source data buffer to program.
  * @param[in] lengthToBurn  Length of the data to program.
  * @param[in] offsetInFlashToProgram  Offset in bytes in flash to program
  *
  * @return
  *
  * @note
  */
static void ota_flush_data_to_flash(uint8_t* ptrSource, uint32_t lengthToBurn, uint32_t offsetInFlashToProgram)
{
    uint32_t lock;

    TRACE("flush %d bytes to flash offset 0x%x", lengthToBurn, offsetInFlashToProgram);

    lock = int_lock_global();
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

    TRACE("Prebytes is %d middlebytes is %d postbytes is %d", preBytes, middleBytes, postBytes);

    if (preBytes > 0)
    {
        hal_norflash_write(HAL_NORFLASH_ID_0, offsetInFlashToProgram, ptrSource, preBytes);

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
	int_unlock_global(lock);
}

static void ota_send_start_response(bool isViaBle)
{
    if (isViaBle)
    {
        ota_env.dataPacketSize = 
            (ota_env.dataPacketSize >= OTA_BLE_DATA_PACKET_MAX_SIZE)?OTA_BLE_DATA_PACKET_MAX_SIZE:ota_env.dataPacketSize;
    }
    else
    {
        ota_env.dataPacketSize = 
            (ota_env.dataPacketSize >= OTA_BT_DATA_PACKET_MAX_SIZE)?OTA_BT_DATA_PACKET_MAX_SIZE:ota_env.dataPacketSize;        
    }

    OTA_START_RSP_T tRsp = 
        {OTA_RSP_START, OTA_START_MAGIC_CODE, OTA_SW_VERSION, OTA_HW_VERSION, 
        ota_env.dataPacketSize};                
    
    ota_env.transmitHander((uint8_t *)&tRsp, sizeof(tRsp));    
}

static void ota_send_configuration_response(bool isDone)
{
    OTA_RSP_CONFIG_T tRsp = {OTA_RSP_CONFIG, isDone};
    ota_env.transmitHander((uint8_t *)&tRsp, sizeof(tRsp));
}

#if DATA_ACK_FOR_SPP_DATAPATH_ENABLED
static void ota_send_data_ack_response(void)
{
    uint8_t packeType = OTA_DATA_ACK;
    ota_env.transmitHander((uint8_t *)&packeType, sizeof(packeType));    
}
#endif

static void ota_send_segment_verification_response(bool isPass)
{
    TRACE("Segment of image's verification pass status is %d (1:pass 0:failed)", isPass);

    OTA_RSP_SEGMENT_VERIFY_T tRsp = {OTA_RSP_SEGMENT_VERIFY, isPass};
    ota_env.transmitHander((uint8_t *)&tRsp, sizeof(tRsp));
}

static void ota_send_result_response(bool isSuccessful)
{
    OTA_RSP_OTA_RESULT_T tRsp = {OTA_RSP_RESULT, isSuccessful};
    ota_env.transmitHander((uint8_t *)&tRsp, sizeof(tRsp));
}

static bool ota_check_whole_image_crc(void)
{
    uint32_t verifiedDataSize = 0;
    uint32_t crc32Value = 0;
    uint32_t verifiedBytes = 0;
    uint32_t lock;

    while (verifiedDataSize < ota_env.totalImageSize)
    {
        if (ota_env.totalImageSize - verifiedDataSize > OTA_DATA_BUFFER_SIZE_FOR_BURNING)
        {
            verifiedBytes = OTA_DATA_BUFFER_SIZE_FOR_BURNING;
        }
        else
        {
            verifiedBytes = ota_env.totalImageSize - verifiedDataSize;
        }

        lock = int_lock();

        hal_norflash_read(HAL_NORFLASH_ID_0, NEW_IMAGE_FLASH_OFFSET + verifiedDataSize,
            ota_env.dataBufferForBurning, OTA_DATA_BUFFER_SIZE_FOR_BURNING);

        int_unlock(lock);

        if (0 == verifiedDataSize)
        {
            if (*(uint32_t *)ota_env.dataBufferForBurning != NORMAL_BOOT)
            {
                return false;
            }
            else
            {
                *(uint32_t *)ota_env.dataBufferForBurning = 0xFFFFFFFF;
            }
        }

        crc32Value = crc32(crc32Value, (uint8_t *)ota_env.dataBufferForBurning, verifiedBytes);

        verifiedDataSize += verifiedBytes;
    }

    TRACE("Original CRC32 is 0x%x Confirmed CRC32 is 0x%x.", ota_env.crc32OfImage, crc32Value);
    return (crc32Value == ota_env.crc32OfImage);
}

static int32_t find_key_word(uint8_t* targetArray, uint32_t targetArrayLen, 
    uint8_t* keyWordArray, 
    uint32_t keyWordArrayLen)
{
    if ((keyWordArrayLen > 0) && (targetArrayLen >= keyWordArrayLen))
    {
        uint32_t index = 0, targetIndex = 0;
        for (targetIndex = 0;targetIndex < targetArrayLen;targetIndex++)
        {
            for (index = 0;index < keyWordArrayLen;index++)
            {
                if (targetArray[targetIndex + index] != keyWordArray[index])
                {
                    break;
                }
            }

            if (index == keyWordArrayLen)
            {
                return targetIndex;
            }
        }

        return -1;
    }
    else
    {
        return -1;
    } 
}

static uint8_t asciiToHex(uint8_t asciiCode)
{
	if ((asciiCode >= '0') && (asciiCode <= '9'))
	{
		return asciiCode - '0';
	}
	else if ((asciiCode >= 'a') && (asciiCode <= 'f')) 
	{
		return asciiCode - 'a' + 10;
	}
	else if ((asciiCode >= 'A') && (asciiCode <= 'F')) 
	{
		return asciiCode - 'A' + 10;
	}
	else 
	{
		return 0xff;
	}
}

static bool ota_check_image_data_sanity_crc(void) {
  // find the location of the CRC key word string
  uint8_t* ptrOfTheLast4KImage = (uint8_t *)(OTA_FLASH_LOGIC_ADDR+NEW_IMAGE_FLASH_OFFSET+
    ota_env.totalImageSize-LEN_OF_IMAGE_TAIL_TO_FIND_SANITY_CRC);

  uint32_t sanityCrc32;
  uint32_t crc32ImageOffset;
  int32_t sanity_crc_location = find_key_word(ptrOfTheLast4KImage, 
    LEN_OF_IMAGE_TAIL_TO_FIND_SANITY_CRC, 
    (uint8_t *)image_info_sanity_crc_key_word, 
    strlen(image_info_sanity_crc_key_word));
  if (-1 == sanity_crc_location)
  {
  	sanity_crc_location = find_key_word(ptrOfTheLast4KImage, 
    	LEN_OF_IMAGE_TAIL_TO_FIND_SANITY_CRC, 
    	(uint8_t *)old_image_info_sanity_crc_key_word, 
    	strlen(old_image_info_sanity_crc_key_word));
    if (-1 == sanity_crc_location)
    {
      // if no sanity crc, fail the check
      return false;
    }
    else
    {
      crc32ImageOffset = sanity_crc_location+ota_env.totalImageSize-
    	LEN_OF_IMAGE_TAIL_TO_FIND_SANITY_CRC+strlen(old_image_info_sanity_crc_key_word);
      sanityCrc32 = *(uint32_t *)(OTA_FLASH_LOGIC_ADDR+NEW_IMAGE_FLASH_OFFSET+crc32ImageOffset);
    }
  }
  else
  {
  	crc32ImageOffset = sanity_crc_location+ota_env.totalImageSize-
    	LEN_OF_IMAGE_TAIL_TO_FIND_SANITY_CRC+strlen(image_info_sanity_crc_key_word);

    sanityCrc32 = 0;
    uint8_t* crcString = (uint8_t *)(OTA_FLASH_LOGIC_ADDR+NEW_IMAGE_FLASH_OFFSET+crc32ImageOffset);

    for (uint8_t index = 0;index < 8;index++)
    {
  	  sanityCrc32 |= (asciiToHex(crcString[index]) << (28-4*index));
    }
  }

  TRACE("sanity_crc_location is %d", sanity_crc_location);

  TRACE("sanityCrc32 is 0x%x", sanityCrc32);

  // generate the CRC from image data
  uint32_t calculatedCrc32 = 0;
  calculatedCrc32 = crc32(calculatedCrc32, (uint8_t *)(OTA_FLASH_LOGIC_ADDR+NEW_IMAGE_FLASH_OFFSET), 
    crc32ImageOffset);

  TRACE("calculatedCrc32 is 0x%x", calculatedCrc32);

  return (sanityCrc32 == calculatedCrc32);
}

bool ota_is_in_progress(void)
{
    return ota_env.isOTAInProgress;
}

void ota_data_transmission_done_callback(void)
{
    if (ota_env.leftSizeOfFlashContentToRead > 0)
    {
        uint32_t sizeToRead = (128 > 
            ota_env.leftSizeOfFlashContentToRead)?
            ota_env.leftSizeOfFlashContentToRead:
            128;
        ota_env.bufForFlashReading[0] = OTA_FLASH_CONTENT_DATA;
        memcpy(&(ota_env.bufForFlashReading[1]), 
            (uint8_t *)(OTA_FLASH_LOGIC_ADDR+ota_env.offsetInFlashToRead),
            sizeToRead);

        TRACE("Send %d bytes at addr 0x%x", sizeToRead,
            ota_env.offsetInFlashToRead);
        ota_env.transmitHander(ota_env.bufForFlashReading, 
            sizeToRead + 1);

        ota_env.leftSizeOfFlashContentToRead -= sizeToRead;
        ota_env.offsetInFlashToRead += sizeToRead;
    }
}

static void ota_sending_flash_content(OTA_READ_FLASH_CONTENT_REQ_T* pReq)
{
    OTA_READ_FLASH_CONTENT_RSP_T rsp = {OTA_READ_FLASH_CONTENT, true};

    if (pReq->isToStart)
    {
        TRACE("Getreading flash content request start addr 0x%x size %d",
                    pReq->startAddr, pReq->lengthToRead);
        
        // check the sanity of the request
        if ((pReq->startAddr >= hal_norflash_get_flash_total_size(HAL_NORFLASH_ID_0)) ||
            ((pReq->startAddr + pReq->lengthToRead) > 
            hal_norflash_get_flash_total_size(HAL_NORFLASH_ID_0)))
        {
            rsp.isReadingReqHandledSuccessfully = false;
        }
        else
        {
            ota_env.offsetInFlashToRead = pReq->startAddr;
            ota_env.leftSizeOfFlashContentToRead = pReq->lengthToRead;
            TRACE("Start sending flash content start addr 0x%x size %d",
                pReq->startAddr, pReq->lengthToRead);
        }
    }
    else
    {
        TRACE("Get stop reading flash content request.");
        ota_env.leftSizeOfFlashContentToRead = 0;
    }
    
    ota_env.transmitHander((uint8_t *)&rsp, sizeof(rsp)); 
}

static void app_update_magic_number_of_app_image(uint32_t newMagicNumber)
{
    uint32_t lock;

    lock = int_lock_global();
    hal_norflash_read(HAL_NORFLASH_ID_0, ota_env.dstFlashOffsetForNewImage, 
        ota_env.dataBufferForBurning, FLASH_SECTOR_SIZE_IN_BYTES);
    int_unlock_global(lock); 
    
    *(uint32_t *)ota_env.dataBufferForBurning = newMagicNumber;
    
    lock = int_lock_global();
    pmu_flash_write_config();
    hal_norflash_erase(HAL_NORFLASH_ID_0, ota_env.dstFlashOffsetForNewImage,
        FLASH_SECTOR_SIZE_IN_BYTES);
    hal_norflash_write(HAL_NORFLASH_ID_0, ota_env.dstFlashOffsetForNewImage,
        ota_env.dataBufferForBurning, FLASH_SECTOR_SIZE_IN_BYTES);
    pmu_flash_read_config();
    int_unlock_global(lock);
}

static void ota_update_boot_info(FLASH_OTA_BOOT_INFO_T* otaBootInfo)
{
    uint32_t lock;

    lock = int_lock_global();
    pmu_flash_write_config();
    hal_norflash_erase(HAL_NORFLASH_ID_0, OTA_BOOT_INFO_FLASH_OFFSET, FLASH_SECTOR_SIZE_IN_BYTES);
    hal_norflash_write(HAL_NORFLASH_ID_0, OTA_BOOT_INFO_FLASH_OFFSET, (uint8_t*)otaBootInfo, sizeof(FLASH_OTA_BOOT_INFO_T));
    pmu_flash_read_config();

    int_unlock_global(lock);
}

void ota_handle_received_data(uint8_t *data, uint32_t len, bool isViaBle)
{
    uint8_t typeCode = data[0];
   
    switch (typeCode)
    {
        case OTA_DATA_PACKET:
        {
            if (!ota_env.isOTAInProgress)
            {
                return;
            }

            uint8_t* rawDataPtr = &data[1];
            
            uint32_t rawDataSize = len - 1; 
            TRACE("Received image data size %d", rawDataSize);
            uint32_t leftDataSize = rawDataSize;
            uint32_t offsetInReceivedRawData = 0;
            do
            {
                uint32_t bytesToCopy;
                // copy to data buffer
                if ((ota_env.offsetInDataBufferForBurning + leftDataSize) > 
                    OTA_DATA_BUFFER_SIZE_FOR_BURNING)
                {
                    bytesToCopy = OTA_DATA_BUFFER_SIZE_FOR_BURNING - ota_env.offsetInDataBufferForBurning;
                }
                else
                {
                    bytesToCopy = leftDataSize;
                }

                leftDataSize -= bytesToCopy;
            
                memcpy(&ota_env.dataBufferForBurning[ota_env.offsetInDataBufferForBurning],
                        &rawDataPtr[offsetInReceivedRawData], bytesToCopy);
                offsetInReceivedRawData += bytesToCopy;
                ota_env.offsetInDataBufferForBurning += bytesToCopy;
                TRACE("offsetInDataBufferForBurning is %d", ota_env.offsetInDataBufferForBurning);
                if (OTA_DATA_BUFFER_SIZE_FOR_BURNING == ota_env.offsetInDataBufferForBurning)
                {
                    TRACE("Program the image to flash.");
                    
                    ota_flush_data_to_flash(ota_env.dataBufferForBurning, OTA_DATA_BUFFER_SIZE_FOR_BURNING, 
                        ota_env.offsetInFlashToProgram);            
                    ota_env.offsetInFlashToProgram += OTA_DATA_BUFFER_SIZE_FOR_BURNING;
                    ota_env.offsetInDataBufferForBurning = 0;
                }
            } while (offsetInReceivedRawData < rawDataSize);

            ota_env.alreadyReceivedDataSizeOfImage += rawDataSize;
            TRACE("Image already received %d", ota_env.alreadyReceivedDataSizeOfImage);
#if DATA_ACK_FOR_SPP_DATAPATH_ENABLED
            if (OTA_DATAPATH_SPP == ota_env.dataPathType)
            {
                ota_send_data_ack_response();
            }
#endif
            break;
        }
        case OTA_COMMAND_SEGMENT_VERIFY:
        {
            if (!ota_env.isOTAInProgress)
            {
                return;
            }
            
            OTA_SEGMENT_VERIFY_T* ptVerifyCmd = (OTA_SEGMENT_VERIFY_T *)(data);

            ota_flush_data_to_flash(ota_env.dataBufferForBurning, ota_env.offsetInDataBufferForBurning,
                    ota_env.offsetInFlashToProgram);
            ota_env.offsetInFlashToProgram += ota_env.offsetInDataBufferForBurning;
            ota_env.offsetInDataBufferForBurning = 0;

            TRACE("Calculate the crc32 of the segment.");

            uint32_t startFlashAddr = OTA_FLASH_LOGIC_ADDR + 
            ota_env.dstFlashOffsetForNewImage + ota_env.offsetOfImageOfCurrentSegment;
            uint32_t lengthToDoCrcCheck = ota_env.alreadyReceivedDataSizeOfImage-ota_env.offsetOfImageOfCurrentSegment;

            ota_env.crc32OfSegment = crc32(0, (uint8_t *)(startFlashAddr), lengthToDoCrcCheck);
            TRACE("CRC32 of the segement is 0x%x", ota_env.crc32OfSegment);

            if ((OTA_START_MAGIC_CODE == ptVerifyCmd->magicCode) && 
                (ptVerifyCmd->crc32OfSegment == ota_env.crc32OfSegment))
            {
                ota_send_segment_verification_response(true);

                // backup of the information in case the verification of current segment failed
                ota_env.offsetInFlashOfCurrentSegment = ota_env.offsetInFlashToProgram;
                ota_env.offsetOfImageOfCurrentSegment = ota_env.alreadyReceivedDataSizeOfImage;
            }
            else
            {
                // restore the offset
                ota_env.offsetInFlashToProgram = ota_env.offsetInFlashOfCurrentSegment;
                ota_env.alreadyReceivedDataSizeOfImage = ota_env.offsetOfImageOfCurrentSegment;                
                ota_send_segment_verification_response(false);
            }

            // reset the CRC32 value of the segment
            ota_env.crc32OfSegment = 0;

            // reset the data buffer
            TRACE("total size is %d already received %d", ota_env.totalImageSize,
                ota_env.alreadyReceivedDataSizeOfImage);

            break;
        }
        case OTA_COMMAND_START:
        {
            OTA_START_T* ptStart = (OTA_START_T *)(data);
            if (OTA_START_MAGIC_CODE == ptStart->magicCode)
            {
                TRACE("Receive command start request:");
                ota_reset_env();
                ota_env.totalImageSize = ptStart->imageSize;
                ota_env.crc32OfImage = ptStart->crc32OfImage; 
                ota_env.isOTAInProgress = true;

                ota_env.AlreadyReceivedConfigurationLength = 0;

                TRACE("Image size is 0x%x, crc32 of image is 0x%x",
                    ota_env.totalImageSize, ota_env.crc32OfImage);

                // send response
                ota_send_start_response(isViaBle);
            }
            break;
        }
        case OTA_COMMAND_CONFIG_OTA:
        {
            OTA_FLOW_CONFIGURATION_T* ptConfig = (OTA_FLOW_CONFIGURATION_T *)&(ota_env.configuration);
            memcpy((uint8_t *)ptConfig + ota_env.AlreadyReceivedConfigurationLength, 
                &(data[1]), len - 1);            

            ota_env.AlreadyReceivedConfigurationLength += (len - 1);
            if ((ptConfig->lengthOfFollowingData + 4) <= ota_env.AlreadyReceivedConfigurationLength)
            {
                TRACE("lengthOfFollowingData 0x%x", ptConfig->lengthOfFollowingData);
                TRACE("startLocationToWriteImage 0x%x", ptConfig->startLocationToWriteImage);
                TRACE("isToClearUserData %d", ptConfig->isToClearUserData);
                TRACE("isToRenameBT %d", ptConfig->isToRenameBT);
                TRACE("isToRenameBLE %d", ptConfig->isToRenameBLE);
                TRACE("isToUpdateBTAddr %d", ptConfig->isToUpdateBTAddr);
                TRACE("isToUpdateBLEAddr %d", ptConfig->isToUpdateBLEAddr);
                TRACE("New BT name:");
                DUMP8("0x%02x ", ptConfig->newBTName, NAME_LENGTH);
                TRACE("New BLE name:");
                DUMP8("0x%02x ", ptConfig->newBLEName, NAME_LENGTH);
                TRACE("New BT addr:");
                DUMP8("0x%02x ", ptConfig->newBTAddr, BD_ADDR_LENGTH);
                TRACE("New BLE addr:");
                DUMP8("0x%02x ", ptConfig->newBLEAddr, BD_ADDR_LENGTH);
                TRACE("crcOfConfiguration 0x%x", ptConfig->crcOfConfiguration);   
                
                // check CRC
                if (ptConfig->crcOfConfiguration == crc32(0, (uint8_t *)ptConfig, sizeof(OTA_FLOW_CONFIGURATION_T) - sizeof(uint32_t)))
                {
                    ota_env.offsetInFlashToProgram = NEW_IMAGE_FLASH_OFFSET;
                    ota_env.offsetInFlashOfCurrentSegment = ota_env.offsetInFlashToProgram;
                    ota_env.dstFlashOffsetForNewImage = ota_env.offsetInFlashOfCurrentSegment;

                    TRACE("OTA config pass.");

                    TRACE("Start writing the received image to flash offset 0x%x", ota_env.offsetInFlashToProgram);
    
                    ota_send_configuration_response(true);
                }
                else
                {
                    TRACE("OTA config failed.");
                    ota_send_configuration_response(false);
                }
            }
            break;
        }
        case OTA_COMMON_GET_OTA_RESULT:
        {
            // check whether all image data have been received
            if (ota_env.alreadyReceivedDataSizeOfImage == ota_env.totalImageSize)
            {
                TRACE("The final image programming and crc32 check.");
                
                // flush the remaining data to flash
                ota_flush_data_to_flash(ota_env.dataBufferForBurning, ota_env.offsetInDataBufferForBurning,
                    ota_env.offsetInFlashToProgram);

                // sanity check on the CRC32 of the image content
                bool ret = ota_check_image_data_sanity_crc();

                if (ret)
                {
                    // update the magic code of the application image
                    app_update_magic_number_of_app_image(NORMAL_BOOT);
                
                    // check the crc32 of the image xfer data
                    ret = ota_check_whole_image_crc();
                }
                
                ota_send_result_response(ret);
                if (ret)
                {

                    FLASH_OTA_BOOT_INFO_T otaBootInfo = {COPY_NEW_IMAGE, ota_env.totalImageSize, ota_env.crc32OfImage};                    
                    ota_update_boot_info(&otaBootInfo);                 
                    ota_update_nv_data();
                    TRACE("Whole image verification pass.");
                    ota_env.isPendingForReboot = true;
                }
                else
                {
                    ota_send_result_response(ret);
                    TRACE("Whole image verification failed.");
                    ota_reset_env();
                }
            }  
            else
            {
                ota_send_result_response(false);
            }
            break;
        }
        case OTA_READ_FLASH_CONTENT:
        {
            ota_sending_flash_content((OTA_READ_FLASH_CONTENT_REQ_T*)data);
            break;
        }
        default:
            break;
    }         
}
#endif // BES_OTA_ENABLED

