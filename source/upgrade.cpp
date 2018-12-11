// ----------------------------------------------------------------------------
// Copyright 2016-2017 ARM Ltd.
//
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// ----------------------------------------------------------------------------

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif

#include "upgrade.h"
#include "bootloader_common.h"
#include "update-client-paal/arm_uc_paal_update.h"
#include "mbed.h"

#include <inttypes.h>

static FlashIAP flash;

/* buffer used in storage operations */
uint8_t buffer_array[BUFFER_SIZE];
uint8_t EraseBuffer[BUFFER_SIZE];
#define ROUND_UP_TO_PAGE_SIZE(size, page_size)  ((size + pageSize - 1) / pageSize) * pageSize;
#define ROUND_DOWN_TO_PAGE_SIZE(size, page_size)    ((size / page_size) * page_size)


#ifndef DOWNLOADED_FIRMWARE_SLOT_ID
#define DOWNLOADED_FIRMWARE_SLOT_ID             0
#endif

#ifndef APPLICATION_HEADER_MAGIC_WORD
#define APPLICATION_HEADER_MAGIC_WORD            0xAAAAAAAA
#endif

static bool writeActiveFirmware(image_manifest_st * p_Header);
static int eraseSector(uint32_t addr);
static void activeStorageDeinit(void);
static bool activeStorageInit(void);
static void PrintHeaderDetails( image_manifest_st * p_header);

/**
 * Find suitable update candidate and copy firmware into active region
 * @return true if the active firmware region is valid.
 */
bool upgradeApplicationFromStorage(void)
{
    bool ret = false;
    image_manifest_st *p_Header = NULL;
    uint8_t HeaderBuffer[MBED_CONF_APP_APPLICATION_APPLICATION_DETAILS_SIZE];
    arm_uc_buffer_t Buffer = {
            .size_max = MBED_CONF_APP_APPLICATION_APPLICATION_DETAILS_SIZE,
            .size     = 0,
            .ptr      = HeaderBuffer
        };

    /*************************************************************************/
    /* Step 1.  Fetch Header and calculate CRC 32 or Magic Word              */
    /*************************************************************************/
    /* Read header of active application */
    Buffer.size = MBED_CONF_APP_APPLICATION_APPLICATION_DETAILS_SIZE;
    arm_uc_error_t ucp_status = ARM_UCP_Read(DOWNLOADED_FIRMWARE_SLOT_ID, 0, &Buffer);
    /* wait for event if the call is accepted */
    if (ucp_status.error == ERR_NONE) {
        while (event_callback == CLEAR_EVENT) {
            __WFI();
        }
    }     

     if ((event_callback == ARM_UC_PAAL_EVENT_READ_DONE) && (Buffer.size > 0))
    {
        tr_info("Comparing Magic Word");
        p_Header = (image_manifest_st*)&Buffer.ptr[MBED_CONF_APP_APPLICATION_APPLICATION_DETAILS_SIZE / 2]; //point to header
        tr_info("Header Addr - %" PRIX32 ", Full Buffer Addr - %" PRIX32, p_Header, Buffer.ptr);
        PrintHeaderDetails(p_Header);
        /* Check Magic WORD */        
        if(p_Header->MagicWord == APPLICATION_HEADER_MAGIC_WORD)//TODO change to CRC32
        {
            tr_info("Magic Word Valid (-: Start upgrading");            
        }
        else
        {   
            tr_info("Magic word failed )-:");
            return ret;        
        }
    }
    else
    {
        tr_error("Error Reading Header");
        return ret;
    }  
    /*************************************************************************/
    /* Step 1. Start upgrading                                                */
    /*************************************************************************/
    ret = activeStorageInit();
    if (ret) {
        ret = writeActiveFirmware(p_Header);
    }
    activeStorageDeinit();

    // return the integrity of the active image
    return ret;
}

static void PrintHeaderDetails( image_manifest_st * p_header)
{
    tr_info("Header Raw Data\n\r:");
    printBuffer((uint8_t *)p_header, sizeof(image_manifest_st));

    tr_info("Read Header Data ------------\n\r");
    tr_info("VERIFY: Write Address - %" PRIX32 " ," \
            "Read Address - %" PRIX32 ,\
            p_header->WriteAddress ,\
            p_header->ReadAddress);

    tr_info("Hashed Offset - %" PRIX32  \
            " ,Hashed Length - %" PRIX32  \
            " ,Sig Offset - %" PRIX32  \
            " ,Sig Length - %" PRIX32 , \
            p_header->HashedDataOffset, \
            p_header->HashedDataLength,\
            p_header->SigOffset,\
            p_header->SigLength);

    tr_info("Image Details:"\
            "Hash Offset - %" PRIX32 \
            " ,Image Size- %" PRIX64 \
            " ,Jump Address- %" PRIX32 ,\
            p_header->applicationHashOffset, \
            p_header->ImageSize,\
            p_header->JumpAddress);

    tr_info("Manifest Data:");
    printBuffer(&p_header->ManifestBuffer[0],\
            p_header->ManifestSize);
    tr_info("\n\rEND Manifest\n\r\n\r");
}

static bool activeStorageInit(void)
{
    memset(EraseBuffer, 0xFF, sizeof(EraseBuffer));
    int rc = flash.init();
    return (rc == 0);
}

static  void activeStorageDeinit(void)
{
    flash.deinit();
}

static int eraseSector(uint32_t addr)
{
    int result = -1;
    const uint32_t pageSize = flash.get_page_size();
    uint32_t MaxReadSize = ROUND_DOWN_TO_PAGE_SIZE(BUFFER_SIZE , pageSize);
    uint32_t sector_size = flash.get_sector_size(addr);
    uint32_t LeftOnSector = sector_size, CompareLength = 0;
    bool DeleteSector = false;


    while(LeftOnSector > 0)
    {
        CompareLength = BOOT_MIN(LeftOnSector, MaxReadSize);
        if(memcmp((void *)addr, (void *)EraseBuffer, CompareLength))
        {
            DeleteSector = true;
            break;
        }
        LeftOnSector -= CompareLength;
    }

    if(DeleteSector)
    {
        result = flash.erase(addr, sector_size);
        tr_debug("Erasing from 0x%08" PRIX32 " to 0x%08" PRIX32 ,\
                addr, addr + sector_size);
        if (result != 0) {
            tr_debug("Erasing from 0x%08" PRIX32 " to 0x%08" PRIX32 " failed with retval %i",
                        addr, addr + sector_size, result);
        }
    }
    else
    {
        tr_debug("Skip Earsing");
    }
    return result;
}

static  bool writeActiveFirmware(image_manifest_st * p_Header)
{
    tr_info("writeActiveFirmwareHeader");
    int ret;
    /* round down the read size to a multiple of the page size
    that still fits inside the main buffer.
      */
    const uint32_t pageSize = flash.get_page_size();
    uint32_t MaxReadSize = ROUND_DOWN_TO_PAGE_SIZE(BUFFER_SIZE , pageSize);
    uint32_t ImageSizeInPageSize = ROUND_UP_TO_PAGE_SIZE(p_Header->ImageSize +\
                                    MBED_CONF_APP_APPLICATION_APPLICATION_DETAILS_SIZE, pageSize);
    uint32_t SectorSize, SectorOffset = 0, LeftToWriteOnSector, WriteCounter = 0, ReadSize = 0;    
    bool SectorDeleted = false;
    arm_uc_buffer_t Buffer = {
            .size_max = MaxReadSize,
            .size     = 0,
            .ptr      = buffer_array
        };
    
    while((ImageSizeInPageSize - WriteCounter) > 0) //Writing entire program  Sector by sector
    {
        SectorOffset = 0;
        SectorDeleted = false;
        SectorSize = flash.get_sector_size(p_Header->WriteAddress + WriteCounter);
        LeftToWriteOnSector = BOOT_MIN(SectorSize, ImageSizeInPageSize - WriteCounter);
        tr_debug("\n\r\n\WriteCounter %" PRIX32 , WriteCounter);
        tr_debug("Write Secotr %" PRIX32 " Sector Size %" PRId32 ,\
                p_Header->WriteAddress + WriteCounter , SectorSize);

        while( LeftToWriteOnSector > 0) //writing data inside a sector
        {             
            tr_debug("Write Sector: Left on sector  0x%" PRIX32 , LeftToWriteOnSector);
            ReadSize = BOOT_MIN(LeftToWriteOnSector, MaxReadSize);  //Choose the min size between Sectore size and maximum buffer, the writing is up to one sector!
            Buffer.size = ReadSize;         
            arm_uc_error_t ucp_status = ARM_UCP_Read(p_Header->ReadAddress, WriteCounter + SectorOffset, &Buffer);            
            /* wait for event if the call is accepted */
            if (ucp_status.error == ERR_NONE) {
                while (event_callback == CLEAR_EVENT) {
                    __WFI();
                }
            }
            if ((event_callback == ARM_UC_PAAL_EVENT_READ_DONE) && (Buffer.size > 0))
            {              
                if(SectorDeleted == false) //If sector not delete need to check each write
                {
                    ret = memcmp((void *)(p_Header->WriteAddress + WriteCounter + SectorOffset), Buffer.ptr,  ReadSize);
                    if(ret) //Need to delete
                    {
                        tr_debug("Delete Sector");                
                        eraseSector(p_Header->WriteAddress + WriteCounter); // Erase sector and start from the begining of the sector
                        //Reset all Sector Offsets
                        SectorOffset = 0;
                        LeftToWriteOnSector = BOOT_MIN(SectorSize, ImageSizeInPageSize - WriteCounter);
                        SectorDeleted = true;
                    }
                    else
                    {
                        SectorOffset += ReadSize;
                        LeftToWriteOnSector -= ReadSize;
                    }
                }
                else
                {
                    tr_debug("Sector Offset  0x%" PRIX32 " Read Size %" PRIX32 " Write Address %" PRIX32 , SectorOffset, ReadSize, p_Header->WriteAddress + WriteCounter + SectorOffset);                    
                    ret = flash.program(Buffer.ptr, p_Header->WriteAddress + WriteCounter + SectorOffset, ReadSize);
                    SectorOffset += ReadSize;
                    LeftToWriteOnSector -= ReadSize;
                }
            }
            else
            {
                 tr_error("OOPSSSSS ----- ARM_UCP_Read returned 0 bytes");
                 return false;
            }
        }//While inside sector 
        WriteCounter += SectorOffset;
#if defined(SHOW_PROGRESS_BAR) && SHOW_PROGRESS_BAR == 1
                printProgress(WriteCounter, ImageSizeInPageSize);
#endif
    }//While sector by sector
    return true;
}
