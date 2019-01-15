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
static arm_uc_buffer_t mainBuffer;
static uint16_t pageSize_g;
static FlashIAP flash;
static bool internalWriteOnSingalSector(applyScriptEntry_st * entry, 
                                        arm_uc_buffer_t* Buffer,
                                        uint32_t WriteCounter,
                                        uint32_t* counterWriteOnSector);

bool internalFlashInit(void)
{
    static int rc = 1;
    if(rc == 1)
    {        
        memset(EraseBuffer, 0xFF, sizeof(EraseBuffer));          
        rc = flash.init(); 
        pageSize_g = flash.get_page_size();    
        mainBuffer.size_max = ROUND_DOWN_TO_PAGE_SIZE(BUFFER_SIZE, pageSize_g);
        mainBuffer.size = 0;
        mainBuffer.ptr = buffer_array;
    }
    return (rc == 0);
}

void internalFlashDeinit(void)
{
    flash.deinit();
}

bool internalFlashRead(applyScriptEntry_st * entry, size_t Offset, arm_uc_buffer_t * buffer)
{
    memcpy(buffer->ptr, entry->fromAddress, buffer->size);
    return true;
}

bool internalFlashEraseSector(uint32_t addr)
{
    tr_info("internalFlashEraseSector");
    int result = -1;    
    uint32_t MaxReadSize = ROUND_DOWN_TO_PAGE_SIZE(BUFFER_SIZE , pageSize_g);
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
        tr_debug("Skip Erasing");
    }
    return (result == 0 ? 1 : 0);
}


bool internalFlashProgram(applyScriptEntry_st * entry)
{
    tr_info("internalFlashProgram");    
    if(!IS_ALIGNED_TO_ADDRESS((uint32_t)entry->toAddress, pageSize_g))
    {//Write address is not aligned to page size return Error
    	tr_info("No align to page address");
        return false;
    }

    uint32_t ImageSizeInPageSize = ROUND_UP_TO_PAGE_SIZE(entry->info.bufferLength, pageSize_g);   
    uint32_t WriteCounter = 0, counterWriteOnSector = 0;
    while((ImageSizeInPageSize - WriteCounter) > 0) //Writing entire program sector by sector
    {        
        tr_debug("WriteCounter %" PRIX32 ", Write Sector %" PRIX32 ", Sector Size %" PRId32 ,\
        		WriteCounter, (uint32_t)entry->toAddress + WriteCounter);

        if(internalWriteOnSingalSector(entry, &mainBuffer, WriteCounter, &counterWriteOnSector))
        {
            WriteCounter += counterWriteOnSector;
        }
        else
        {
            return false;
        } 
#if defined(SHOW_PROGRESS_BAR) && SHOW_PROGRESS_BAR == 1
                printProgress(WriteCounter, ImageSizeInPageSize);
#endif
    }//While sector by sector
    return true;
}

static bool internalWriteOnSingalSector(applyScriptEntry_st * entry, 
                                        arm_uc_buffer_t* Buffer,
                                        uint32_t WriteCounter,
                                        uint32_t* counterWriteOnSector)
{
    bool ret = false;
    uint32_t ReadSize = 0;
    uint32_t SectorSize = flash.get_sector_size((uint32_t)entry->toAddress + WriteCounter);
    uint32_t sectorStart = getSectorStart((uint32_t)entry->toAddress + WriteCounter);
    uint32_t SectorOffset = ((uint32_t)entry->toAddress + WriteCounter - sectorStart);
    uint32_t ImageSizeInPageSize = ROUND_UP_TO_PAGE_SIZE(entry->info.bufferLength, pageSize_g);
    uint32_t reminder = ImageSizeInPageSize - entry->info.bufferLength;
    //Calculate how much left on sector (need to consider that start address is not aligned to sector start)
    uint32_t LeftToWriteOnSector = BOOT_MIN(SectorSize - SectorOffset, ImageSizeInPageSize - WriteCounter); 
    bool SectorDeleted = false;
    
    *counterWriteOnSector = LeftToWriteOnSector; //Keep how much shall be write on the sector
    while( LeftToWriteOnSector > 0) //writing data inside a sector
    {
        tr_debug("Left on sector  0x%" PRIX32 , LeftToWriteOnSector);
        ReadSize = BOOT_MIN(LeftToWriteOnSector, Buffer->size_max);  //Choose the min size between Sector size and maximum buffer, the writing is up to one sector!
        if((LeftToWriteOnSector < Buffer->size_max))
        {
            memset(Buffer->ptr, -1, Buffer->size_max);
            Buffer->size = ReadSize - reminder;
            ret = entryFunctions.ReadFrom(entry, WriteCounter + SectorOffset, Buffer);
        }
        else
        {
            Buffer->size = ReadSize;
            ret = entryFunctions.ReadFrom(entry, WriteCounter + SectorOffset, Buffer);
        }

        if (ret)
        {
            if(SectorDeleted == false) //If sector not delete need to check each write
            {
                verifyErase_e status = ERASE_ERROR;
                status = internalVerifyErase((applyOpCode_e)entry->info.opCode, (uint8_t*)sectorStart,\
                         WriteCounter + SectorOffset, Buffer->ptr, ReadSize);
                if(status == ERASE_DATA)
                {
                    tr_debug("ERASE_DATA");
                    // Erase sector and start from the beginning of the sector
                    internalFlashEraseSector(sectorStart);
                    //Reset all Sector Offsets
                    SectorOffset = ((uint32_t)entry->toAddress + WriteCounter - sectorStart);
                    LeftToWriteOnSector = BOOT_MIN(SectorSize - SectorOffset, ImageSizeInPageSize - WriteCounter);
                    SectorDeleted = true;
                    continue;
                }
                else if (status == EQUAL_DATA)
                {//Skip erase Flash data is equal to buffer data no need in write nor erase
                    tr_debug("EQUAL_DATA");
                    SectorOffset += ReadSize;
                    LeftToWriteOnSector -= ReadSize;
                    continue;
                }
                else if ((status == ERASE_ERROR) || (status == OVERWRITE_ERROR))
                {
                    tr_debug("ERASE_ERROR");
                    return false; //Cannot perform overwrite
                }
                tr_debug("OVERWRITE_DATA");
                //else Do Nothing OVERWRITE_DATA continue to program the flash
            }
            tr_debug("Sector Offset  0x%" PRIX32 " Read Size %" PRIX32 " Write Address %" PRIX32 ,\
                        SectorOffset, ReadSize, sectorStart + WriteCounter + SectorOffset);
            ret = flash.program(Buffer->ptr, sectorStart + WriteCounter + SectorOffset, ReadSize);
            if(ret == 0)
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
    return true;
}

verifyErase_e internalVerifyErase(applyOpCode_e command, uint8_t *sectorStart, size_t Offset, uint8_t  *buffer, size_t length)
{
    tr_info("bufferVerifyErase");
    verifyErase_e ret = ERASE_ERROR;

    switch(command)
    {
        case OVERWRITE:
            ret = OVERWRITE_DATA;
            for(size_t i = 0; i < length; i++)
            {
                if((*(sectorStart + Offset + i) & *(buffer + i)) != *(buffer + i))
                {
                    return OVERWRITE_ERROR;
                }
            }
        case WRITE://No Break!
            if(!memcmp((void *)(sectorStart + Offset), buffer,  length))
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

