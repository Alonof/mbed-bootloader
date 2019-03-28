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

#include <inttypes.h>

#include "mbed.h"

#include "update-client-paal/arm_uc_paal_update.h"

#include "bootloader_common.h"
#include "excuteFlashScript.h"

//#include "OEM_functions.h"
//stub function
static void set_application_hint(int a)
{
    return;
}

#ifdef MBED_CLOUD_CLIENT_UPDATE_STORAGE
extern ARM_UC_PAAL_UPDATE MBED_CLOUD_CLIENT_UPDATE_STORAGE;
#else
#error Update client storage must be defined in user configuration file
#endif

#if defined(ARM_UC_USE_PAL_BLOCKDEVICE) && (ARM_UC_USE_PAL_BLOCKDEVICE==1)
#include "SDBlockDevice.h"

/* initialise sd card blockdevice */
#if defined(MBED_CONF_APP_SPI_MOSI) && defined(MBED_CONF_APP_SPI_MISO) && \
    defined(MBED_CONF_APP_SPI_CLK)  && defined(MBED_CONF_APP_SPI_CS)
SDBlockDevice sd(MBED_CONF_APP_SPI_MOSI, MBED_CONF_APP_SPI_MISO,
                 MBED_CONF_APP_SPI_CLK,  MBED_CONF_APP_SPI_CS);
#else
SDBlockDevice sd(MBED_CONF_SD_SPI_MOSI, MBED_CONF_SD_SPI_MISO,
                 MBED_CONF_SD_SPI_CLK,  MBED_CONF_SD_SPI_CS);
#endif

BlockDevice *arm_uc_blockdevice = &sd;
#endif

#ifndef MBED_CONF_APP_APPLICATION_START_ADDRESS
#error Application start address must be defined
#endif

/* If jump address is not set then default to start address. */
#ifndef MBED_CONF_APP_APPLICATION_JUMP_ADDRESS
#define MBED_CONF_APP_APPLICATION_JUMP_ADDRESS MBED_CONF_APP_APPLICATION_START_ADDRESS
#endif


__attribute__((used)) void boot_reset( void )
{
    tr_info("\n\r--------------RESET----------------\n\r");
#ifdef TEST
    for (;;) {
        __WFI();
    }
#endif
	#define RESET_MASK_FOR_CORTEX_M_SERIES	0x5fa0004

	volatile unsigned int * AIRCR_REG = (volatile unsigned int *)(0xE000ED0C);  //This register address is true for the Cortex M family
	*AIRCR_REG = RESET_MASK_FOR_CORTEX_M_SERIES;
        for (;;) {
            __WFI();
        }
}

/**
 * @brief   Main Entry function for apply image
 *          1. Search if there are unfinished flash script if yes complete the script
 *          2. If no Program New Flash Script
 *          3. Reset
 * @return int 
 */
int main(void)
{

    tr_info("\n\r--------------Apply Image----------------\n\r");

    //Search for unfinished script
    applyScriptEntry_st* lastEntry= isLastScriptDone();
    if(lastEntry != NULL)
    {
        executeFlashScript(lastEntry);
    }
    else
    {//Execute new script
        newFlashScriptProtocol();
    }

    //Reset device -> jump to ROM boot
    boot_reset();
    
    /* coverity[no_escape] */
    for (;;) {
        __WFI();
    }
    return 1;
}
