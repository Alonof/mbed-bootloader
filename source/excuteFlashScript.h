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

#ifndef EXECUTE_FLASH_SCRIPT_H
#define EXECUTE_FLASH_SCRIPT_H
#include "update-client-paal/arm_uc_paal_update.h"

#define ROUND_UP_TO_PAGE_SIZE(size, pageSize)      (((size + pageSize - 1) / pageSize) * pageSize)
#define ROUND_DOWN_TO_PAGE_SIZE(size, pageSize)    ((size / pageSize) * pageSize)
#define IS_ALIGNED_TO_ADDRESS(addr, alignment)     ((addr % alignment) != 0 ? false : true)
                                                    

/**
 * @brief  flash script commands max 4 bits
 */
typedef enum{
    WRITE           = 0x00,     /*Write the data use erase if needed*/
    OVERWRITE       = 0x01,     /*Overwrite the data no erase is needed*/ 
    ERASE           = 0x02,     /*Erase Full Sector*/
    PARTIAL_ERASE   = 0x03,     /*Round up start address and round down end address and erase all sectors between*/
    INIT_JOURNAL    = 0x04,     /*Initialize the MBR for the flash journal*/
    EMPTY_COMMAND   = 0x0F //Last command
}applyOpCode_e;

/**
 * @brief  Diffrent Storage devices (max 2 bits - 4 options)
 */
typedef enum{
    INTERNAL           = 0, /*internal memory (flash, ram)*/
    SD_CARD            = 1, /*External storage SD card */
}storageDevices_e;

typedef struct {
   unsigned  opCode         : 4;        /*applyOpCode_e*/
   unsigned  fromDeviceType : 2;        /*storageDevices_e*/
   unsigned  toDeviceType   : 2;        /*storageDevices_e*/
   unsigned  bufferLength   : 24;
} applyFlashCommand_b;

typedef struct {
    applyFlashCommand_b info; // Write address in IAP
    uint8_t * fromAddress;
    uint8_t * toAddress; 
}__attribute__((packed))applyScriptEntry_st;

typedef struct
{
    uint16_t    numberOfSectors;
    uint32_t    sectorSize;
}sectorTable_st;


typedef struct
{
    uint8_t opCode;
    uint8_t mbrSize;
    uint16_t journalSize;
    uint8_t minWriteSize; //2^n
    uint8_t sectorTuples;
    sectorTable_st tableTuples[10];
}__attribute__((packed))journalMbrRecord_st;

/**
 * @brief return value for verify erase function 
 */
typedef enum{
    ERASE_DATA, /*Need to erase data */
    EQUAL_DATA, /*Data is equal no erase is needed*/
    OVERWRITE_DATA, /*Can preform Overwrite on sector*/
    OVERWRITE_ERROR, /*Overwrite is not allowed */
    ERASE_ERROR    /* Overwriting is impossible return error*/
}verifyErase_e;


/**
 * @brief Structure definition holding API function pointers.
 */
typedef struct {

    /**
     * @brief Initialize the underlying storage for the To device
     *
     * @param void
     * @return Returns True on success
     *         Returns False on failure
     */
    bool (*InitializeTo)(void);

    /**
     * @brief Initialize the underlying storage for the From device
     *
     * @param void
     * @return Returns True on success
     *         Returns False on failure
     */
    bool (*InitializeFrom)(void);

    /**
     * @brief Read data from flash into buffer with the given length
     * @details The function will read until the buffer is full or the end of
     *          the storage location has been reached. The actual amount of
     *          bytes read is set in the buffer struct.
     * 
     * @param entry - flash script entry that contains the necessary information
     *                FromAddress - Start address from where to start reading
     *                Length -      Length to read
     * @param Offset - Offset buffer from begging of the FromAddress
     * @param buffer[out] - output buffer 
     * @return true  - Pass
     * @return false  - Fail
     */
    bool (*ReadFrom)(applyScriptEntry_st * entry, size_t offset, arm_uc_buffer_t * buffer);


    /**
     * @brief write buffer into flash memory.
     *        The function verify if write is possible before erasing 
     *        the destintion address must be page aligned
     *        The length doesn't need to be aligned to page size.
     * 
     * @param entry - flash script entry that contains the necessary information
     *                FromAddress - The address of the data being read
     *                ToAddress -  Destintion Start address where the data should be written to. must be page allign 
     *                Length -      Length of the input data
     *                fromDeviceType - the device from which to read
     *                toDeviceType - the device from which to write
     * @return true - Pass
     * @return false - ToAddress not page aligned,
     *                 Error writing to flash
     */
    bool (*WriteTo)(applyScriptEntry_st * entry);


    /**
     * @brief  Erase Sectors, skip erase if sector is already erased (-1)
     * 
     * @param addr[in] - address of the begining of the sector
     * @return true - erase complete
     * @return false - failed to erase
     */
    bool (*EraseTo)(uint32_t addr);

    /**
     * @brief This functions get the page size for the device being written to
     *
     * @param entry - flash script entry that contains the necessary information 
     *                FromAddress - Start address from where to erase
     *                Length -      Length to erase
     * @return page size
     * 
     */
    uint32_t (*GetPageSizeTo)(void);

    /**
    * @brief This functions get the sector size for the device being written to
     *
     * @param address - the address of the sector which its size is requested
     * @return sector size
     * 
     */
    uint32_t (*GetSectorSizeTo)(uint32_t addr);

    /**
     * @brief Get the start address of the flash, from the "TO" device
     * 
     * @return Flash start address
     */
    uint32_t (*GetFlashStartTo)(void);

        /**
     * @brief Return Flash Size from the "TO" device
     * 
     * @return uint32 
     */
    uint32_t (*GetFlashSizeTo)(void);

} applyFunctionsPtr_st;


/**
 * \brief        Write and Execute the scrip commands
 *      1. Write Script to flash
 *      2. Erase  script from RAM
 *      3. calls executeFlashScript
 * 
 * \input 	     void
 * \return       void
 */ 
void newFlashScriptProtocol(void);

/**
 * \brief        Write and Execute the scrip commands
 *      LOOP # commands
 *      {
 *          1. Execute # Script Command
 *          2. Write # command to Journal
 *      }
 *      3. RESET
 * 
 * \input       flashScriptAddr - script address
 * \return      void
 */ 
void executeFlashScript(applyScriptEntry_st * flashScriptAddr);

/**
 * @brief search for unfinished scripts 
 * 
 * @return applyScriptEntry_st * Null if not found else address
 * @return false 
 */
applyScriptEntry_st* isLastScriptDone(void);

/**
 * @brief Prepare an MBR record,
 *        The record shall hold the Journal size & table of flash sector table
 * 
  * @param entry - flash script entry that contains the necessary information
 * @return true - pass
 * @return false - fail
 * 
 */
bool bufferInitJournal(applyScriptEntry_st * entry);

/**
 * @brief Get the start of the sector for a given address (round down to sector start)
 * 
 * @param addr - addr of the sector 
 * @return uint32_t 
 */
uint32_t getSectorStart(uint32_t addr);

/**
     * @brief Erase the flash Memory. 
     *        Before erasing the sector verify if sector is not already erased
     *        Start Address must be aligned to start of sector
     *
     * @param entry - flash script entry that contains the necessary information 
     *                FromAddress - Start address from where to erase
     *                Length -      Length to erase
     *        isPartialErase - Erase partial flash, the erase length is define as so:
     *        [start address(aligned to start of sector), 
     *         end address round down to sector start unless the end address is sector end] 
     *         all the sectors between shall be erased.
     *         before erasing verify if sector is not already erased
     * @return true - pass
     * @return false - fail
     * 
     * @Note The end addres is round up to the next sector. 
     *       Meaning when the end address is in the middle a sector then all 
     *       the sector shall be erased
     *          
     */
bool bufferErase(applyScriptEntry_st * entry, bool isPartialErase);


/**
 * @brief This function fill the table with tuples (count, sector size) 
 *        this information is stored in the MBR and used for calculation in the main boot
 *        
 * 
 * @param table[out] - pointer to an empty table
 * @return uint8_t the number of tuples found
 */
uint8_t fillSectorSizeTable(sectorTable_st * table);
#endif //EXECUTE_FLASH_SCRIPT_H