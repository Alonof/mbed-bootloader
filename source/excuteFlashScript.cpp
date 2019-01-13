#include "bootloader_common.h"
#include "excuteFlashScript.h"
#include "internalFlashDriver.h"
#include "sdCardBlockDeviceDriver.h"
#include "tlv_stab.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

applyFunctionsPtr_st entryFunctions;

#define APPLY_SCRIPT_ADDRESS    (uint8_t *)0x2000000    /* location of the script located in the RAM memory*/
#define APPLY_TLV_HEADER        2  /*TLV header Type + length */
#define APPLY_SCRIPT_LENGTH     108 + APPLY_TLV_HEADER   /* Max length of the apply script, this memory is reserved only for this purpose*/

static void populateFunctionPtr(applyScriptEntry_st* entry)
{
    switch((storageDevices_e)entry->info.fromDeviceType)
    {
        case SD_CARD:
            entryFunctions.InitializeTo = externalStorageSDInit;            
            entryFunctions.ReadFrom = externalStorageSDRead;
            break;

        case INTERNAL:
            entryFunctions.InitializeTo = internalFlashInit;            
            entryFunctions.ReadFrom = internalFlashRead;
            break;
    }

    switch((storageDevices_e)entry->info.toDeviceType)
    {
        case SD_CARD:
            entryFunctions.InitializeFrom = externalStorageSDInit;            
            entryFunctions.WriteTo = externalStorageSDProgram;
            entryFunctions.EraseTo = externalStorageSDEraseSector;
            break;

        case INTERNAL:
            entryFunctions.InitializeFrom = internalFlashInit;            
            entryFunctions.WriteTo = internalFlashProgram;
            entryFunctions.EraseTo = internalFlashEraseSector;
            entryFunctions.GetSectorSizeTo = internalGetSectorSize;
            entryFunctions.GetPageSizeTo = internalGetPageSize;
            entryFunctions.GetFlashStartTo = internalGetFlashStart;
            entryFunctions.GetFlashSizeTo = internalGetFlashSize;
            break;
    }
}


void newFlashScriptProtocol(void)
{
    tr_info("newFlashScriptProtocol");
    //Write Script to flash
    applyScriptEntry_st entry;
    entry.info.opCode = OVERWRITE;
    entry.info.fromDeviceType = INTERNAL;
    entry.info.toDeviceType = INTERNAL;
    entry.info.bufferLength = ROUND_UP_TO_PAGE_SIZE(APPLY_SCRIPT_LENGTH, internalGetPageSize());
    entry.fromAddress = APPLY_SCRIPT_ADDRESS;
    entry.toAddress = (uint8_t *)journalGetNextFree();
    internalFlashProgram(&entry);
    
    //Erase RAM memory
    memset(APPLY_SCRIPT_ADDRESS, 0, APPLY_SCRIPT_LENGTH);

    //execute script from flash
    executeFlashScript((applyScriptEntry_st *)(entry.toAddress + 2));//Plus 2 = this is TLV -> 1 byte T + 1 byte L = 2 bytes
}

void executeFlashScript(applyScriptEntry_st * flashScriptAddr)
{   
    tr_info("executeFlashScript");
    bool ret = true; 
    applyScriptEntry_st entry;
    while((flashScriptAddr->info.opCode != EMPTY_COMMAND) && (ret == true))
    {       
        populateFunctionPtr(flashScriptAddr);
        entryFunctions.InitializeTo();
        entryFunctions.InitializeFrom();
        switch(flashScriptAddr->info.opCode)
        {
            case WRITE:
            case OVERWRITE:
                    ret = entryFunctions.WriteTo(flashScriptAddr);
                    break;

                case ERASE:
                    ret = bufferErase(flashScriptAddr, false);
                    break;

                case PARTIAL_ERASE:
                    ret = bufferErase(flashScriptAddr, true);
                    break;

                case INIT_JOURNAL:
                    ret = bufferInitJournal(flashScriptAddr);
                    break;

                default:
                    ret  = false;
                    break;
        }
        //log entry in internal flash        
        uint8_t buffer[3];
        buffer[0] = COMMIT_MESSAGE;
        buffer[1] = ROUND_UP_TO_PAGE_SIZE(sizeof(buffer), internalGetPageSize());
        buffer[2] = flashScriptAddr->info.opCode;                
        
        entry.info.opCode = OVERWRITE;
        entry.info.fromDeviceType = INTERNAL;
        entry.info.toDeviceType = INTERNAL;
        entry.info.bufferLength = buffer[1];
        entry.fromAddress = buffer;
        entry.toAddress = (uint8_t *)journalGetNextFree();
        internalFlashProgram(&entry);
        flashScriptAddr++;
    }

    if( ret == false )
    {
        //TODO: error logging T.B.D
    }
}


applyScriptEntry_st* isLastScriptDone(void)
{
    tr_info("isLastScriptDone");
    applyScriptEntry_st* flashScriptEntry = NULL;
    uint8_t * commitMsgEntry = NULL;
    bool status = true;
    journalGetLastEntry((uint8_t**)&flashScriptEntry, FLASH_SCRIPT);
    if(flashScriptEntry != NULL)
    {        
        commitMsgEntry = (uint8_t*)flashScriptEntry;
        flashScriptEntry = (applyScriptEntry_st* )((uint8_t*)flashScriptEntry + 2); //Skip TLV header
        while(flashScriptEntry->info.opCode != EMPTY_COMMAND)
        {
            if((journalingCommands_e)**(journalGetNextEntry(&commitMsgEntry)) != COMMIT_MESSAGE)
            {
                status = false;
                break; 
            }
           flashScriptEntry++; 
        }        
    }
    
    return (status ? NULL : flashScriptEntry);
}

bool bufferErase(applyScriptEntry_st * entry, bool isPartialErase)
{
    tr_info("bufferErase");
     bool ret = true;
    size_t offset = 0, eraseBlockSize = 0;
    int32_t leftToErase = entry->info.bufferLength;

    if(isPartialErase)
    {
        //Verify if start addres == to sector size if not round up to next sector
        uint32_t sectorSize = entryFunctions.GetSectorSizeTo((uint32_t)entry->toAddress);
        uint32_t sectorStart = getSectorStart((uint32_t)entry->toAddress);
        if( sectorStart != (uint32_t)entry->toAddress )
        {//Partial Erase -> skip to next sector
            offset =  sectorSize - ((uint32_t)entry->toAddress - sectorStart);
            leftToErase -= offset;
        }

        //Trim end address if needed
        sectorStart = getSectorStart((uint32_t)entry->toAddress + entry->info.bufferLength);
        if( sectorStart < (uint32_t)entry->toAddress + entry->info.bufferLength)
        {//Trim End to sector start
            leftToErase -=  ((uint32_t)entry->toAddress + entry->info.bufferLength) - sectorStart;
        }
    }
    else
    {//Round down to sector start
        uint32_t sectorStart = getSectorStart((uint32_t)entry->toAddress);
        if( sectorStart != (uint32_t)entry->toAddress )
        {
            offset = -((uint32_t)entry->toAddress - sectorStart);
            leftToErase -= offset;
        }        
    }

    while((leftToErase > 0) && (ret == true))
    {
        eraseBlockSize = entryFunctions.GetSectorSizeTo((uint32_t)entry->toAddress + offset);
        switch((storageDevices_e)entry->info.toDeviceType)
        {
            case SD_CARD:
                ret = entryFunctions.EraseTo((uint32_t)entry->toAddress + offset);
                break;

            case INTERNAL:                       
                ret = entryFunctions.EraseTo((uint32_t)entry->toAddress + offset);
                break;

            default:            
                break;
        }
        leftToErase -= eraseBlockSize;
        offset += eraseBlockSize;   
    }
    return ret;
}

uint32_t getSectorStart(uint32_t addr)
{
    /* Find the exact end sector boundary. Some platforms have different sector
       sizes from sector to sector. Hence we count the sizes 1 sector at a time here */
    uint32_t flashOffset = entryFunctions.GetFlashStartTo();
    uint32_t sectorSize = 0;
    while (flashOffset < (addr))
    {
        sectorSize = entryFunctions.GetSectorSizeTo(flashOffset);
        flashOffset += sectorSize;
    }
    return (flashOffset - sectorSize);
}


uint8_t fillSectorSizeTable(sectorTable_st * table)
{
    uint32_t startAddr = entryFunctions.GetFlashStartTo();
    uint32_t sectorSize = entryFunctions.GetSectorSizeTo(startAddr);
    uint32_t flashSize = entryFunctions.GetFlashStartTo();
    uint32_t nextSectorAddr = startAddr;
    uint8_t nextFreeSlot = 0, i = 0;
    
    while(nextSectorAddr < (flashSize + startAddr))
    {
        for(i = 0; i < nextFreeSlot; i++)
        {
          if(table[i].sectorSize == sectorSize)
          {
              break;
          }
        }

        if(i == nextSectorAddr)
        {//New sector add size to table
            nextSectorAddr++;
            table[i].sectorSize = sectorSize;
        }
        table[i].numberOfSectors++; //add counter
        nextSectorAddr += entryFunctions.GetSectorSizeTo(nextSectorAddr); //jump to next sector
    }
    return nextFreeSlot;
}


bool bufferInitJournal(applyScriptEntry_st * entry)
{
    //Fill MBR buffer
    journalMbrRecord_st buffer = {0};
    buffer.sectorTuples = fillSectorSizeTable(buffer.tableTuples);
    buffer.opCode = entry->fromAddress[0];
    buffer.mbrSize =  ROUND_UP_TO_PAGE_SIZE(7 + buffer.sectorTuples * sizeof(sectorTable_st), internalGetPageSize());
    buffer.journalSize = entry->fromAddress[2];
    buffer.minWriteSize = internalGetPageSize();
    buffer.minEraseSize = 1;

    //edit new entry
    applyScriptEntry_st entryMBR;
    memcpy(&entryMBR ,entry, sizeof(applyScriptEntry_st));
    entryMBR.fromAddress = (uint8_t*)&buffer; //set from pointer to new buffer;
    entryMBR.info.opCode = WRITE;
    entryMBR.info.bufferLength =  buffer.mbrSize;

    //Write to start of MBR record  in internal flash    
    internalFlashProgram(&entryMBR);

    //Write to end of MBR record
    entryMBR.toAddress = entryMBR.fromAddress + entryMBR.fromAddress[1] - entryMBR.info.bufferLength;
    internalFlashProgram(&entryMBR);
    return true;
}