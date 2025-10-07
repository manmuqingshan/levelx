/***************************************************************************
 * Copyright (c) 2025 Microsoft Corporation 
 * Copyright (c) 2025 STmicroelectronics
 * 
 * This program and the accompanying materials are made available under the
 * terms of the MIT License which is available at
 * https://opensource.org/licenses/MIT.
 * 
 * SPDX-License-Identifier: MIT
 **************************************************************************/

/**************************************************************************/
/**************************************************************************/
/**                                                                       */
/** LevelX Component                                                      */
/**                                                                       */
/**   NOR Flash                                                           */
/**                                                                       */
/**************************************************************************/
/**************************************************************************/

#define LX_SOURCE_CODE


/* Disable ThreadX error checking.  */

#ifndef LX_DISABLE_ERROR_CHECKING
#define LX_DISABLE_ERROR_CHECKING
#endif


/* Include necessary system files.  */

#include "lx_api.h"


/**************************************************************************/
/*                                                                        */
/*  FUNCTION                                               RELEASE        */
/*                                                                        */
/*    _lx_nor_flash_format                                 PORTABLE C     */
/*                                                                        */
/*  DESCRIPTION                                                           */
/*                                                                        */
/*    This function opens a NOR flash instance then erases all the non    */
/*      erased blocks, then writes the erase count                        */
/*                                                                        */
/*  INPUT                                                                 */
/*                                                                        */
/*    nor_flash                             NOR flash instance            */
/*    name                                  Name of NOR flash instance    */
/*    nor_driver_initialize                 Driver initialize             */
/*    nor_driver_info_ptr                   Drivier user data pointer     */
/*                                                                        */
/*  OUTPUT                                                                */
/*                                                                        */
/*    return status                                                       */
/*                                                                        */
/*  CALLS                                                                 */
/*                                                                        */
/*    (nor_driver_initialize)               Driver initialize             */
/*    _lx_nor_flash_driver_read             Driver read                   */
/*    _lx_nor_flash_driver_write            Driver write                  */
/*    (lx_nor_flash_driver_block_erased_verify)                           */
/*                                          NOR flash verify block erased */
/*    _lx_nor_flash_driver_block_erase      Driver block erase            */
/*                                                                        */
/*  CALLED BY                                                             */
/*                                                                        */
/*    Application Code                                                    */
/*                                                                        */
/**************************************************************************/

UINT  _lx_nor_flash_format(LX_NOR_FLASH  *nor_flash, CHAR *name, UINT (*nor_driver_initialize)(LX_NOR_FLASH *), VOID *nor_driver_info_ptr)
{

ULONG           l;
UINT            status;

ULONG           *block_word_ptr;
ULONG           bit_map_words;
ULONG            sectors_per_block;
ULONG           bit_map_mask;
ULONG           block_word;

    LX_INTERRUPT_SAVE_AREA

    LX_PARAMETER_NOT_USED(name);

    /* Clear the NOR flash control block. User extension is not cleared.  */
    LX_MEMSET(nor_flash, 0, (ULONG)((UCHAR*)&(nor_flash -> lx_nor_flash_open_previous) - (UCHAR*)nor_flash) + sizeof(nor_flash -> lx_nor_flash_open_previous));

    nor_flash -> lx_nor_flash_driver_info_ptr = nor_driver_info_ptr;

    /* Call the flash driver's initialization function.  */
    (nor_driver_initialize)(nor_flash);

#ifndef LX_DIRECT_READ

    /* Determine if the driver supplied a RAM buffer for reading the NOR sector if direct read is not
       supported.  */
    if (nor_flash -> lx_nor_flash_sector_buffer == LX_NULL)
    {
        /* Return an error.  */
        return(LX_NO_MEMORY);
    }
#endif

    nor_flash -> lx_nor_flash_block_free_bit_map_offset =  sizeof(LX_NOR_FLASH_BLOCK_HEADER)/sizeof(ULONG);

    LX_DISABLE
    for (l = 0; l < nor_flash -> lx_nor_flash_total_blocks; l++)
    {
        /* Setup the initial erase count to 0, assuming that the block is already erased  */
        block_word =  ((ULONG) 0);

#ifdef LX_NOR_ENABLE_CONTROL_BLOCK_FOR_DRIVER_INTERFACE
        /* Check that the block is already erased */
        status =  (nor_flash -> lx_nor_flash_driver_block_erased_verify)(nor_flash, l);
#else
        status =  (nor_flash -> lx_nor_flash_driver_block_erased_verify)(l);

#endif

        /* the block is not erased, thus make sure to erase it */
        if (status)
        {
            /* the max erase count is not needed just pass the value 0 */
            status =  _lx_nor_flash_driver_block_erase(nor_flash, l, 0);
            /* Check for an error from flash driver */
            if (status)
            {
                /* Call system error handler.  */
                _lx_nor_flash_system_error(nor_flash, status);

                LX_RESTORE
                /* Return an error.  */
               return(LX_ERROR);
            }

            /* check that the sector was correctly erased */
#ifdef LX_NOR_ENABLE_CONTROL_BLOCK_FOR_DRIVER_INTERFACE
            status =  (nor_flash -> lx_nor_flash_driver_block_erased_verify)(nor_flash, l);
#else
            status =  (nor_flash -> lx_nor_flash_driver_block_erased_verify)(l);
#endif
            if (status)
            {

                /* Call system error handler.  */
                _lx_nor_flash_system_error(nor_flash, status);

                LX_RESTORE
                /* Return an error.  */
                return(LX_ERROR);
            }
            else
            {
                /* the block has been erased, thus incrememt its erase_count */
                block_word =  ((ULONG) 1);
            }
        }

        /* Setup the block word pointer to the first word of the first block */
        block_word_ptr =  nor_flash -> lx_nor_flash_base_address + l * nor_flash -> lx_nor_flash_words_per_block;

        /* Calculate the number of bits we need in the free physical sector bit map */
        sectors_per_block =  (nor_flash -> lx_nor_flash_words_per_block / LX_NOR_SECTOR_SIZE) - 1;

        /* Calculate the number of words we need for the free physical sector bit map.  */
        bit_map_words =  (sectors_per_block + 31)/ 32;

        if ((sectors_per_block % 32) != 0)
        {
            bit_map_mask =  (ULONG)(1 << (sectors_per_block % 32));
            bit_map_mask =  bit_map_mask - 1;
        }
        else
        {

            /* Exactly 32 sectors for the bit map mask.  */
            bit_map_mask =  LX_ALL_ONES;
        }
        status =  _lx_nor_flash_driver_write(nor_flash, block_word_ptr + (nor_flash -> lx_nor_flash_block_free_bit_map_offset + (bit_map_words-1)) , &bit_map_mask, 1);
        if (status)
        {
            /* Call system error handler.  */
            _lx_nor_flash_system_error(nor_flash, status);

            LX_RESTORE
            /* Return an error.  */
            return(LX_ERROR);
        }

        block_word =  (block_word | LX_BLOCK_ERASED);

        status =  _lx_nor_flash_driver_write(nor_flash, block_word_ptr, &block_word, 1);
        if (status)
        {
            /* Call system error handler.  */
            _lx_nor_flash_system_error(nor_flash, status);

            LX_RESTORE
            /* Return an error.  */
            return(LX_ERROR);
        }

    }

    /* Restore interrupts.  */
    LX_RESTORE

    return(LX_SUCCESS);
}
