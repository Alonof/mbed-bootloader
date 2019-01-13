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

#ifndef SD_CARD_BLOCK_DEVICE_DRIVER_H
#define SD_CARD_BLOCK_DEVICE_DRIVER_H
#include <inttypes.h>

bool externalStorageSDInit(void);
void externalStorageSDDeinit(void);

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
bool externalStorageSDRead(applyScriptEntry_st * entry, size_t Offset, arm_uc_buffer_t * buffer);

/**
 * @brief Program External flash 
 * 
 * @param entry - flash script entry that contains the necessary information
 * @return true  - succeeded 
 * @return false -failed
 */
bool externalStorageSDProgram(applyScriptEntry_st * entry);

/**
 * @brief  Erase Sectors, skip erase if sector is already erased (-1)
 * 
 * @param addr[in] - address of the begining of the sector
 * @param 
 * @return true - erase complete
 * @return false - failed to erase
 */
bool externalStorageSDEraseSector(uint32_t addr);


uint32_t externalStorageSDGetSectorSize(uint32_t addr);

uint32_t externalStorageSDGetPageSize(void);

uint32_t externalStorageSDGetFlashStart(void);

uint32_t externalStorageSDGetFlashSize(void);




#endif //SD_CARD_BLOCK_DEVICE_DRIVER_H