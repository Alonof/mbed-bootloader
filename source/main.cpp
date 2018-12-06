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
#include "upgrade.h"

/* use a cut down version of ARM_UCP_FLASHIAP_BLOCKDEVICE to reduce
   binary size if ARM_UC_USE_PAL_BLOCKDEVICE is set and not running tests */
#if defined(ARM_UC_USE_PAL_BLOCKDEVICE) && (ARM_UC_USE_PAL_BLOCKDEVICE==1) && \
    (!defined(BOOTLOADER_POWER_CUT_TEST) || (BOOTLOADER_POWER_CUT_TEST != 1))
#define MBED_CLOUD_CLIENT_UPDATE_STORAGE ARM_UCP_FLASHIAP_BLOCKDEVICE_READ_ONLY
#endif

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
	#define RESET_MASK_FOR_CORTEX_M_SERIES	0x5fa0004

	volatile unsigned int * AIRCR_REG = (volatile unsigned int *)(0xE000ED0C);  //This register address is true for the Cortex M family
	*AIRCR_REG = RESET_MASK_FOR_CORTEX_M_SERIES;
        for (;;) {
            __WFI();
        }
}

int main(void)
{
    bool ret = false;
    tr_info("\n\r--------------Apply Image----------------\n\r");

    /* Set PAAL Update implementation before initializing Firmware Manager */
    ARM_UCP_SetPAALUpdate(&MBED_CLOUD_CLIENT_UPDATE_STORAGE);

    /* Initialize PAL */
    arm_uc_error_t ucp_result = ARM_UCP_Initialize(arm_ucp_event_handler);
    if(ucp_result.code != ERR_NONE)
    {
         tr_error("ARM_UCP_Initialize faild to init"); 
    }
    else
    {
        /*************************************************************************/
        /* Update                                                                */
        /*************************************************************************/
        
        /* Try to update firmware from journal */
        ret = upgradeApplicationFromStorage();
        if(ret)
        {
            uint32_t app_start_addr = MBED_CONF_APP_APPLICATION_START_ADDRESS;
            uint32_t app_stack_ptr = *((uint32_t *)(MBED_CONF_APP_APPLICATION_JUMP_ADDRESS + 0));
            uint32_t app_jump_addr = *((uint32_t *)(MBED_CONF_APP_APPLICATION_JUMP_ADDRESS + 4));

            tr_info("Application's start address: 0x%" PRIX32, app_start_addr);
            tr_info("Application's jump address: 0x%" PRIX32, app_jump_addr);
            tr_info("Application's stack address: 0x%" PRIX32, app_stack_ptr);
            tr_info("Reseting Device...\r\n");
        }
        else
        {
            tr_info("No firmwere was found in device, Clearing Hint and reseting");
        }
        //Clear Hint from non volatile memory
        #define HINT 0x0000000020000001
        uint8_t * Hint = (uint8_t *)HINT;
        *Hint = 0;
    }
    //Reset device -> jump to ROM boot
    boot_reset();
    
    /* coverity[no_escape] */
    for (;;) {
        __WFI();
    }
}
