
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif

#include "mbed.h"
#include <inttypes.h>
#include "excuteFlashScript.h"
#include "sdCardBlockDeviceDriver.h"
#include "internalFlashDriver.h"
#include "bootloader_common.h"
#include "update-client-paal/arm_uc_paal_update.h"

extern applyFunctionsPtr_st entryFunctions;

bool externalStorageSDInit(void)
{
    //TODO: TBD
    return false;
}

void externalStorageSDDeinit(void)
{
    //TODO: TBD
}

bool externalStorageSDRead(applyScriptEntry_st * entry, size_t Offset, arm_uc_buffer_t * buffer)
{
    arm_uc_error_t ucp_status = ARM_UCP_Read((uint32_t)entry->fromAddress, Offset, buffer);            
    /* wait for event if the call is accepted */
    if (ucp_status.error == ERR_NONE) {
        while (event_callback == CLEAR_EVENT) {
            __WFI();
        }
    }            
    if ((event_callback == ARM_UC_PAAL_EVENT_READ_DONE) && (buffer->size > 0)){
        return true;
    }
    return false;
}

bool externalStorageSDProgram(applyScriptEntry_st * entry)
{
    //TODO: TBD
    return false;
}

bool externalStorageSDEraseSector(uint32_t addr)
{
    //TODO: TBD
    return false;
}

uint32_t externalStorageSDGetSectorSize(uint32_t addr)
{
    //TODO: TBD
    return 0;
}

uint32_t externalStorageSDGetPageSize(void)
{
    //TODO: TBD
    return 0;
}

uint32_t externalStorageSDGetFlashStart(void)
{
    //TODO: TBD
    return 0;
}

uint32_t externalStorageSDGetFlashSize(void)
{
    //TODO: TBD
    return 0;
}
