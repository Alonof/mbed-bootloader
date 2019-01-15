#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "tlv_stab.h"

static uint8_t * ret = NULL;

uint8_t page_buffer[8] = {0xFF ,0xFF, 0xFF ,0xFF, 0xFF ,0xFF, 0xFF ,0xFF};
uint8_t* journalGetNextFree(void)
{
	uint8_t* nextFree = (uint8_t* )(0x1000);
	int i = 0;

	for(i = 0; i < 100; i ++)
	{
		if(memcmp((void*)nextFree, page_buffer, sizeof(page_buffer)) == 0)
		{
			return nextFree;
		}
		nextFree += sizeof(page_buffer);
	}
    return NULL;
}

uint8_t journalGetHead(uint8_t ** entry)
{return 1;}

uint8_t ** journalGetNextEntry(uint8_t ** entry)
{
    return &ret;
}

uint8_t **journalGetLastEntry(uint8_t ** entry, uint8_t opCode)
{
    return &ret;
}

uint8_t  journalGetDetails(uint8_t * CTX)
{return 1;}


uint8_t  journalWriteEntry(uint8_t * entry, uint8_t  length, uint8_t opCode)
{return 1;}
