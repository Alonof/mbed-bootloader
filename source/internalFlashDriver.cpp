#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif

#include "excuteFlashScript.h"
#include "internalFlashDriver.h"
#include "bootloader_common.h"
#include "update-client-paal/arm_uc_paal_update.h"

#include "mbed.h"

#include <inttypes.h>
extern applyFunctionsPtr_st entryFunctions;
extern uint8_t buffer_array[BUFFER_SIZE];
extern uint8_t EraseBuffer[BUFFER_SIZE];
static FlashIAP flash;

bool internalFlashInit(void)
{
    static int rc = 0;
    if(rc != 0)
    {
        memset(EraseBuffer, 0xFF, sizeof(EraseBuffer));
        rc = flash.init();        
    }
    return (rc == 0);
}

void internalFlashDeinit(void)
{
    flash.deinit();
}

bool internalFlashRead(applyScriptEntry_st * entry, size_t Offset, arm_uc_buffer_t * buffer)
{
    memcpy(buffer->ptr, entry->fromAddress, buffer->size_max);
    return true;
}

bool internalFlashEraseSector(uint32_t addr)
{
    tr_info("internalFlashEraseSector");
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

bool internalFlashProgram(applyScriptEntry_st * entry)
{
    tr_info("internalFlashProgram");
    int ret = false;
    verifyErase_e status = ERASE_ERROR;
    const uint32_t pageSize = flash.get_page_size();
    if(!IS_ALIGNED_TO_ADDRESS((uint32_t)entry->toAddress, pageSize))
    {//Write addr is not aligned to page size return Error
        return ret;
    }

    const uint32_t MaxReadSize = ROUND_DOWN_TO_PAGE_SIZE(BUFFER_SIZE , pageSize);
    uint32_t ImageSizeInPageSize = ROUND_UP_TO_PAGE_SIZE(entry->info.bufferLength, pageSize);
    uint32_t SectorSize, SectorOffset = 0, LeftToWriteOnSector, WriteCounter = 0, ReadSize = 0;    
    bool SectorDeleted = false;
    arm_uc_buffer_t Buffer = {
            .size_max = MaxReadSize,
            .size     = 0,
            .ptr      = buffer_array
        };

    while((ImageSizeInPageSize - WriteCounter) > 0) //Writing entire program sector by sector
    {
        memset(Buffer.ptr, -1, MaxReadSize);
        SectorSize = flash.get_sector_size((uint32_t)entry->toAddress + WriteCounter);
        SectorOffset = ((uint32_t)entry->toAddress + WriteCounter - \
                       getSectorStart((uint32_t)entry->toAddress + WriteCounter));
        SectorDeleted = false;
        //Calculate how much left on sector (need to consider that start address is not aligned to sector start)
        LeftToWriteOnSector = BOOT_MIN(SectorSize - SectorOffset, ImageSizeInPageSize - WriteCounter);        
        tr_debug("WriteCounter %" PRIX32 , WriteCounter);
        tr_debug("Write Sector %" PRIX32 " Sector Size %" PRId32 ,\
                (uint32_t)entry->toAddress + WriteCounter , SectorSize);

        while( LeftToWriteOnSector > 0) //writing data inside a sector
        {
            tr_debug("Left on sector  0x%" PRIX32 , LeftToWriteOnSector);
            ReadSize = BOOT_MIN(LeftToWriteOnSector, MaxReadSize);  //Choose the min size between Sector size and maximum buffer, the writing is up to one sector!
            Buffer.size = ReadSize;          
            ret = entryFunctions.ReadFrom(entry, WriteCounter + SectorOffset, &Buffer);
            if (ret)
            {
                if(SectorDeleted == false) //If sector not delete need to check each write
                {
                    status = internalVerifyErase(entry, WriteCounter + SectorOffset, Buffer.ptr, ReadSize);                    
                    if(status == ERASE_DATA)
                    {
                        tr_debug("Delete Sector");
                        // Erase sector and start from the begining of the sector
                        internalFlashEraseSector(getSectorStart((uint32_t)entry->toAddress + WriteCounter)); 
                        //Reset all Sector Offsets
                        SectorOffset = ((uint32_t)entry->toAddress + WriteCounter - \
                                        getSectorStart((uint32_t)entry->toAddress + WriteCounter));
                        LeftToWriteOnSector = BOOT_MIN(SectorSize - SectorOffset, ImageSizeInPageSize - WriteCounter);
                        SectorDeleted = true;
                        continue;
                    }
                    else if (status == EQUAL_DATA)
                    {//Skip erase Flash data is equal to buffer data no need in write nor erase
                        SectorOffset += ReadSize;
                        LeftToWriteOnSector -= ReadSize;
                        continue;
                    }
                    else if (status == ERASE_ERROR)
                    {
                        return false; //Cannot prefrom overwrite
                    }
                    //else Do Nothing OVERWRITE_DATA continue to program the flash
                }
                tr_debug("Sector Offset  0x%" PRIX32 " Read Size %" PRIX32 " Write Address %" PRIX32 ,\
                            SectorOffset, ReadSize, (uint32_t)entry->toAddres + WriteCounter + SectorOffset);                   
                ret = flash.program(Buffer.ptr, (uint32_t)entry->toAddress + WriteCounter + SectorOffset, ReadSize);                                                          
                if(ret)
                {
                    SectorOffset += ReadSize;
                    LeftToWriteOnSector -= ReadSize;
                }
                else
                {
                        tr_error("Error writing to flash");
                        return false;
                }                
            }
            else
            {
                 tr_error("Read Error");
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

verifyErase_e internalVerifyErase(applyScriptEntry_st * entry, size_t Offset, uint8_t  *buffer, size_t length)
{
    tr_info("bufferVerifyErase");
    verifyErase_e ret = ERASE_ERROR;

    switch((applyOpCode_e)entry->info.opCode)
    {
        case OVERWRITE:
            ret = OVERWRITE_DATA;
            for(size_t i = 0; i < length; i++)
            {
                if((*(entry->toAddress + Offset + i) & *(buffer + i)) != *(buffer + i))
                {
                    ret = OVERWRITE_ERROR;
                    break;
                }
            }
        case WRITE://No Break!
            if(!memcmp((void *)(entry->toAddress + Offset), buffer,  length))
            {
                ret = EQUAL_DATA; //data is equal
            }
            else
            {
                if(ret != OVERWRITE_DATA)
                {
                    ret = ERASE_DATA; //Erase Data
                }
            }
            break;

        default:
            break;            
    }
    return ret;
}

uint32_t internalGetSectorSize(uint32_t addr)
{
    return flash.get_sector_size(addr);
}

uint32_t internalGetPageSize(void)
{
    return flash.get_page_size();
}

uint32_t internalGetFlashStart(void)
{
    return flash.get_flash_start();
}

uint32_t internalGetFlashSize(void)
{
    return flash.get_flash_size();
}

