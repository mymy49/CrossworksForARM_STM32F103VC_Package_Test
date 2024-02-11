/*****************************************************************************
 *                                                                           *
 * Copyright (c) 2010 Rowley Associates Limited.                             *
 *                                                                           *
 * This file may be distributed under the terms of the License Agreement     *
 * provided with this software.                                              *
 *                                                                           *
 * THIS FILE IS PROVIDED AS IS WITH NO WARRANTY OF ANY KIND, INCLUDING THE   *
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. *
 *****************************************************************************/

#include <libmem.h>
#ifdef STM32L1XX_MD
#include <stm32l100xb.h>
#else
#include <stm32l053xx.h>
#endif
#include <libmem_loader.h>

extern unsigned char __RAM_segment_start__[];
extern unsigned char __RAM_segment_used_end__[];

#define FLASH_START_ADDRESS ((uint8_t*)0x08000000)
unsigned FLASH_SIZE;
#define EEPROM_START_ADDRESS ((uint8_t*)0x08080000)
unsigned EEPROM_SIZE;

#define WRITE_BUFFER_SIZE 128 // 32 words 

static uint8_t write_buffer[WRITE_BUFFER_SIZE];
static libmem_driver_paged_write_ctrlblk_t paged_write_ctrlblk;

static libmem_geometry_t geometry[2];
static libmem_geometry_t eeprom_geometry[2];

static void 
unlock()
{
  FLASH->PEKEYR = 0x89ABCDEF;
  FLASH->PEKEYR = 0x02030405;
  FLASH->PRGKEYR = 0x8C9DAEBF;
  FLASH->PRGKEYR = 0x13141516;
}

static void 
lock()
{
  FLASH->PECR |= FLASH_PECR_PELOCK;
}

static int
flash_write_page(libmem_driver_handle_t *h, unsigned char *dest, const unsigned char *src)
{      
  unlock();
  FLASH->PECR |= FLASH_PECR_FPRG;  
  FLASH->PECR |= FLASH_PECR_PROG;               
  while (FLASH->SR & 1);
  int i;
  for (i=0;i<WRITE_BUFFER_SIZE;i+=4)
    {
      *(unsigned*)(dest+i) = *(unsigned *)(src+i);
    } 
  lock();
  return LIBMEM_STATUS_SUCCESS;
}

static int
libmem_write_impl(libmem_driver_handle_t *h, uint8_t *dest, const uint8_t *src, size_t size)
{
  return libmem_driver_paged_write(h, dest, src, size, &paged_write_ctrlblk);
}

static int
flash_erase_sector(libmem_driver_handle_t *h, libmem_sector_info_t *si)
{
  int res = LIBMEM_STATUS_SUCCESS;
  unlock();
  FLASH->PECR |= FLASH_PECR_ERASE;  
  FLASH->PECR |= FLASH_PECR_PROG;               
  while (FLASH->SR & 1);                                
  *(uint32_t *)si->start = 0x00000000;
  lock();                  
  return res;
}

static int
libmem_erase_impl(libmem_driver_handle_t *h, uint8_t *start, size_t size, uint8_t **erased_start, size_t *erased_size)
{
  int res = LIBMEM_STATUS_SUCCESS;
  uint8_t *start2;
  if (start < FLASH_START_ADDRESS)
    start2 = start + (int)FLASH_START_ADDRESS;
  else
    start2 = start;
  res = libmem_foreach_sector_in_range(h, start2, size, flash_erase_sector, erased_start, erased_size);
  if (erased_start && start2 != start)
    *erased_start -= (int)FLASH_START_ADDRESS;
  return res;
}

static int
libmem_flush_impl(libmem_driver_handle_t *h)
{
  return libmem_driver_paged_write_flush(h, &paged_write_ctrlblk);
}

static int
libmem_inrange_impl(libmem_driver_handle_t *h, const uint8_t *dest)
{
  return LIBMEM_ADDRESS_IN_RANGE(dest, (uint8_t *)FLASH_START_ADDRESS, (uint8_t *)(FLASH_START_ADDRESS + FLASH_SIZE - 1)) ||
         LIBMEM_ADDRESS_IN_RANGE(dest, (uint8_t *)0, (uint8_t *)(FLASH_SIZE - 1)); // Allow FLASH to be programmed at 0x00000000 alias
}

static const libmem_driver_functions_t driver_functions =
{       
  libmem_write_impl,
  0,
  libmem_erase_impl, 
  0,
  0,
  libmem_flush_impl
};

static const libmem_ext_driver_functions_t ext_driver_functions =
{
  libmem_inrange_impl,
  0,
  0
};

static int
libmem_eeprom_erase_impl(libmem_driver_handle_t *h, uint8_t *start, size_t size, uint8_t **erased_start, size_t *erased_size)
{
  if ((unsigned)start & 3)
    start = (uint8_t*)((unsigned)start & ~3);
  if (erased_start)
    *erased_start = start;
  if (erased_size)
    *erased_size = 0;
  while (size)
    {
      unlock();                 
      *(uint32_t *)start = 0x00000000;
      lock();
      if (size >= 4)
        size -= 4;
      else
        size = 0;
      start += 4;
      if (erased_size)
        *erased_size += 4;
    }
  return LIBMEM_STATUS_SUCCESS;
}

static int
libmem_eeprom_write_impl(libmem_driver_handle_t *h, uint8_t *dest, const uint8_t *src, size_t size)
{  
  while (size)
    {
      if (*src)
        {
          unlock();
#ifdef STM32L1XX_MD
          FLASH->PECR &= ~FLASH_PECR_FTDW;
#else
          FLASH->PECR &= ~FLASH_PECR_FIX;
#endif
          *dest = *src;
          lock();
        }
      dest++;
      src++;
      size--;
    }
  return LIBMEM_STATUS_SUCCESS;
}

static const libmem_driver_functions_t eeprom_driver_functions =
{       
  libmem_eeprom_write_impl,
  0,
  libmem_eeprom_erase_impl, 
  0,
  0,
  0
};
 
int
main(unsigned long param0)
{ 
  unsigned ramsize;
  switch ((DBGMCU->IDCODE & DBGMCU_IDCODE_DEV_ID))
    {
#ifdef STM32L1XX_MD
      case 0x416: // low and medium density stm32l - cat.1
      case 0x429: // cat.2
        FLASH_SIZE = (128 * 1024);       
        geometry[0].count = 512;
        geometry[0].size = 256;
        ramsize = (4*1024);
        EEPROM_SIZE = (4 * 1024);
        eeprom_geometry[0].count = EEPROM_SIZE/4;
        eeprom_geometry[0].size = 4;
        break;
      case 0x427: // cat.3
      case 0x436: // high density stm32l - cat.4
        FLASH_SIZE = (384 * 1024);
        geometry[0].count = 1536;
        geometry[0].size = 256;
        ramsize = (32*1024);
        EEPROM_SIZE = (12 * 1024);
        eeprom_geometry[0].count = EEPROM_SIZE/4;
        eeprom_geometry[0].size = 4;
        break;
      case 0x437: // high density stm32l - cat.5
        FLASH_SIZE = (512 * 1024);
        geometry[0].count = 2048;
        geometry[0].size = 256;
        ramsize = (80*1024);
        EEPROM_SIZE = (16 * 1024);
        eeprom_geometry[0].count = EEPROM_SIZE/4;
        eeprom_geometry[0].size = 4;
        break;
#else
#ifdef LIBMEM_LIGHT
      case 0x457: // stm32l0x1 - category 1
        FLASH_SIZE = (16 * 1024);       
        geometry[0].count = 128;
        geometry[0].size = 128;
        ramsize = (2*1024);
        break;
#else
      case 0x425: // stm32l0x2 - category 2
        FLASH_SIZE = (32 * 1024);       
        geometry[0].count = 256;
        geometry[0].size = 128;
        ramsize = (8*1024);
        EEPROM_SIZE = (2 * 1024);
        eeprom_geometry[0].count = EEPROM_SIZE/4;
        eeprom_geometry[0].size = 4;
        break;
      case 0x417: // stm32l0x3 - category 3
        FLASH_SIZE = (64 * 1024);       
        geometry[0].count = 512;
        geometry[0].size = 128;
        ramsize = (8*1024);
        EEPROM_SIZE = (2 * 1024);
        eeprom_geometry[0].count = EEPROM_SIZE/4;
        eeprom_geometry[0].size = 4;
        break;
      case 0x447: // stm32l0x3 - category 5
        FLASH_SIZE = (192 * 1024);       
        geometry[0].count = 1536;
        geometry[0].size = 128;
        ramsize = (20*1024);
        EEPROM_SIZE = (6 * 1024);
        eeprom_geometry[0].count = EEPROM_SIZE/4;
        eeprom_geometry[0].size = 4;
        break;
#endif
#endif
      default:
        libmem_rpc_loader_exit(LIBMEM_STATUS_ERROR, "Unsupported device");
        break;
    }  
  libmem_driver_handle_t h,h1;     
#ifdef LIBMEM_LIGHT
  libmem_register_driver(&h, (uint8_t *)FLASH_START_ADDRESS, FLASH_SIZE, geometry, 0, &driver_functions, 0);
#else
  libmem_register_driver(&h, (uint8_t *)FLASH_START_ADDRESS, FLASH_SIZE, geometry, 0, &driver_functions, &ext_driver_functions);
  libmem_register_driver(&h1, (uint8_t *)EEPROM_START_ADDRESS, EEPROM_SIZE, eeprom_geometry, 0, &eeprom_driver_functions, 0);
#endif
  libmem_driver_paged_write_init(&paged_write_ctrlblk, write_buffer, sizeof(write_buffer), flash_write_page, 4, 0);
  int res = libmem_rpc_loader_start(__RAM_segment_used_end__, __RAM_segment_start__ + ramsize - 1);
  libmem_rpc_loader_exit(res, 0);
  return 0;
}
