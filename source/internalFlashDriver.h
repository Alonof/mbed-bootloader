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

#ifndef INTERNAL_FLASH_DRIVER_H
#define INTERNAL_FLASH_DRIVER_H
#include <inttypes.h>


bool internalFlashInit(void);
void internalFlasheDeinit(void);

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
bool internalFlashRead(applyScriptEntry_st * entry, size_t Offset, arm_uc_buffer_t * buffer);

/**
 * @brief  Erase Sectors, skip erase if sector is already erased (-1)
 * 
 * @param addr[in] - address of the begining of the sector
 * @return true - erase complete
 * @return false - failed to erase
 */
bool internalFlashEraseSector(uint32_t addr);

/**
 * @brief Program internal flash 
 * 
 * @param entry - flash script entry that contains the necessary information
 * @return true  - succeeded 
 * @return false -failed
 */
bool internalFlashProgram(applyScriptEntry_st * entry);

/**
 * @brief return the page size for the toDevice
 * 
 * @param storageDevices_e - device
 * @return uint32_t - page size
 */
uint32_t bufferGetPageSize(storageDevices_e device);


/**
 * @brief return the sector size for a given sector address
 * 
  * @param addr - address of a given sector 
 * @return uint32_t  - size of the sector
 */
uint32_t internalGetSectorSize(uint32_t addr);

/**
 * @brief return the page size for the flash driver
 * 
 * @return uint32_t page size
 */
uint32_t internalGetPageSize(void);

/**
 * @brief Get the start address of the flash
 * 
 * @return Flash start address
 */
uint32_t internalGetFlashStart(void);

/**
 * @brief return the size of the flash
 * 
 * @return uint32_t 
 */
uint32_t internalGetFlashSize(void);

/**
 * @brief Verify if overwriting is possible and if erase is needed
 *        - If overwrite is requested then the function verifies that overwriting the flash is possible
 *          if not possible then Error is return
 *        - If Writing is requested then the function verifies that the flash is not already equal to the data being written
 * 
 * @param entry  - flash script entry that contains the necessary information
 * @param Offset - offset of the data from the starting address provided by the entry
 * @param buffer - buffer to compare to
 * @param length - length of the data being compared
 * @return verifyErase_e -    ERASE_DATA Need to erase data 
 *                            EQUAL_DATA Data is equal no erase is needed
 *                            OVERWRITE_DATA Can preform Overwrite on sector
 *                            ERASE_ERROR     Overwriting is impossible return error
 */
verifyErase_e internalVerifyErase(applyOpCode_e command, uint8_t *sectorStart, size_t Offset, uint8_t  *buffer, size_t length);
#endif //INTERNAL_FLASH_DRIVER_H
