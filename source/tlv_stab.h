#ifndef TLV_STAB_H
#define TLV_STAB_H
typedef enum
{
    FLASH_SCRIPT = 0x02,
    COMMIT_MESSAGE = 0x03,

}journalingCommands_e;

/**
 * \brief       Return the address of the next free entry inside the Journal 
 * \input 	    void
 * \return      Address of the next free space, Null if failed
 */ 
uint8_t* journalGetNextFree(void);

/**
 * \brief       Return the address of the head of the Journal 
 * \input 	    entry[OUT] - address to the next postion
 * \return      True -success, FALSE - fail
 */ 
uint8_t journalGetHead(uint8_t ** entry);


/**
 * \brief       Return the address of the next entry inside the Journal 
 * \input 	    current [IN] - current entry to search from 
 *     	        next[OUT] - address of the next entry
 * \return      True -success, FALSE - fail
 */ 
uint8_t ** journalGetNextEntry(uint8_t ** entry);


/**
 * \brief       Return the address last entry with the given opCode 
 * \input 	    entry[OUT] - address to the last flasher Script
 *              opCode[IN] - code type of the requested entry
 * \return      True -success, FALSE - fail
 */ 
uint8_t **journalGetLastEntry(uint8_t ** entry, uint8_t opCode);


/**
 * \brief       Return the address of the next free entry inside the Journal 
 * \input 	    void
 * \return      addres to the next postion 
 */ 
uint8_t  journalGetDetails(uint8_t * CTX);

/**
 * \brief      The write command wil prepare a hint for the main boot
 *              first scan main boot for existing hints than append the new command to the hint
 * \input 	    current [IN] - current entry to search from 
 *     	        next[OUT] - address of the next entry
 * \return      True -success, FALSE - fail
 */ 


uint8_t  journalWriteEntry(uint8_t * entry, uint8_t  length, uint8_t opCode);

#endif//TLV_STAB_H