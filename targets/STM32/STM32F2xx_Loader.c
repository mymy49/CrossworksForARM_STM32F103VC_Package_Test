/*****************************************************************************
 *                                                                           *
 * Copyright (c) 2011 Rowley Associates Limited.                             *
 *                                                                           *
 * This file may be distributed under the terms of the License Agreement     *
 * provided with this software.                                              *
 *                                                                           *
 * THIS FILE IS PROVIDED AS IS WITH NO WARRANTY OF ANY KIND, INCLUDING THE   *
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. *
 *****************************************************************************/

#include <libmem.h>
#include <stm32f2xx.h>
#include <libmem_loader.h>

extern unsigned char __RAM_segment_start__[];
extern unsigned char __RAM_segment_used_end__[];

uint8_t *FLASH_START_ADDRESS=(uint8_t*)0x08000000;
uint8_t *ITCM_FLASH_START_ADDRESS=(uint8_t*)0x00200000;
unsigned FLASH_SIZE=((1024*2)*1024);
unsigned DB1M;

libmem_geometry_t geometry1[] = 
{
  { 4, 16 * 1024 },
  { 1, 64 * 1024 },
  { 11, 128 * 1024 },
  { 0, 0 }
};

libmem_geometry_t geometry2[] = 
{
  { 4, 16 * 1024 },
  { 1, 64 * 1024 },
  { 7, 128 * 1024 },
  { 4, 16 * 1024 },
  { 1, 64 * 1024 },
  { 7, 128 * 1024 },
  { 0, 0 }
};

libmem_geometry_t geometry3[] =
{
  { 4, 32 * 1024 },
  { 1, 128 * 1024 },
  { 7, 256 * 1024 },
  { 0, 0 }
};

libmem_geometry_t geometry4[] = 
{
  { 4, 16 * 1024 },
  { 1, 64 * 1024 },
  { 3, 128 * 1024 },
  { 4, 16 * 1024 },
  { 1, 64 * 1024 },
  { 3, 128 * 1024 }, 
  { 0, 0 }
};

#define KEEPALIVE IWDG->KR=0xAAAA

static void 
FLASH_ProgramHalfWord(unsigned address, unsigned short v)
{  
  FLASH->CR = (FLASH_CR_PG | FLASH_CR_PSIZE_0);
  *(volatile unsigned short *)(address) = v;
  __asm("dsb");
  while (FLASH->SR & FLASH_SR_BSY) KEEPALIVE;
  FLASH->CR &= ~FLASH_CR_PG;
}

static int
libmem_write_impl(libmem_driver_handle_t *h, uint8_t *dest, const uint8_t *src, size_t size)
{
  if (dest < FLASH_START_ADDRESS)
    {
      if (dest >= ITCM_FLASH_START_ADDRESS)
        dest += ((int)FLASH_START_ADDRESS-(int)ITCM_FLASH_START_ADDRESS);
      else
        dest += (int)FLASH_START_ADDRESS;
    }
  unsigned sdest = (unsigned)dest & ~1;
  while (size>=2)
    {
      uint16_t v = *(src+1)<<8 | *src;
      FLASH_ProgramHalfWord(sdest, v);
      size -= 2;
      sdest += 2;
      src += 2;
    }
  if (size)
    FLASH_ProgramHalfWord(sdest, 0xff<<8|*src);     
  return LIBMEM_STATUS_SUCCESS;
}

static int
flash_erase_sector(libmem_driver_handle_t *h, libmem_sector_info_t *si)
{
  int res = LIBMEM_STATUS_SUCCESS;
  FLASH->CR = FLASH_CR_SER;
  if (DB1M && (si->number >= 8))
    FLASH->CR |= (si->number + 4) << 3;
  else if (si->number < 12)
    FLASH->CR |= si->number << 3;
  else if ((DBGMCU->IDCODE & 0xFFF)==0x463) // STM32F413x
    FLASH->CR |= si->number << 3;
  else
    FLASH->CR |= (si->number + 4) << 3;
  FLASH->CR |= FLASH_CR_STRT;
  while (FLASH->SR & FLASH_SR_BSY) KEEPALIVE;
  FLASH->CR &= ~FLASH_CR_SER;
  if (FLASH->SR & FLASH_SR_WRPERR) 
    res = LIBMEM_STATUS_LOCKED;
  else if (FLASH->SR)
    res = LIBMEM_STATUS_ERROR;
  return res;
}

static int
libmem_erase_impl(libmem_driver_handle_t *h, uint8_t *start, size_t size, uint8_t **erased_start, size_t *erased_size)
{
  int res = LIBMEM_STATUS_SUCCESS;
  if (LIBMEM_RANGE_WITHIN_RANGE(h->start, h->start + h->size - 1, start, start + size - 1))
    {
      if (erased_start)
        *erased_start = h->start;
      if (erased_size)
        *erased_size = h->size;
      // erase all
      FLASH->CR = FLASH_CR_MER;
      FLASH->CR |= FLASH_CR_STRT;
      while (FLASH->SR & FLASH_SR_BSY) KEEPALIVE;
      FLASH->CR &= ~FLASH_CR_MER;
      if (FLASH->SR & FLASH_SR_WRPERR) 
        res = LIBMEM_STATUS_LOCKED;
      else if (FLASH->SR)
        res = LIBMEM_STATUS_ERROR;
    }
  else
    {
      uint8_t *start2;
      if (start < FLASH_START_ADDRESS)
        if (start >= ITCM_FLASH_START_ADDRESS)
          start2 = start + ((int)FLASH_START_ADDRESS-(int)ITCM_FLASH_START_ADDRESS);
        else
          start2 = start + (int)FLASH_START_ADDRESS;
      else
        start2 = start;
      res = libmem_foreach_sector_in_range(h, start2, size, flash_erase_sector, erased_start, erased_size);
      if (erased_start && start2 != start)
        if (start >= ITCM_FLASH_START_ADDRESS)
          *erased_start -= ((int)FLASH_START_ADDRESS-(int)ITCM_FLASH_START_ADDRESS);
        else
          *erased_start -= (int)FLASH_START_ADDRESS;
    }
  return res;
}

static int
libmem_inrange_impl(libmem_driver_handle_t *h, const uint8_t *dest)
{
  return LIBMEM_ADDRESS_IN_RANGE(dest, (uint8_t *)FLASH_START_ADDRESS, (uint8_t *)(FLASH_START_ADDRESS + FLASH_SIZE - 1)) ||
         LIBMEM_ADDRESS_IN_RANGE(dest, (uint8_t *)ITCM_FLASH_START_ADDRESS, (uint8_t *)(ITCM_FLASH_START_ADDRESS + FLASH_SIZE - 1)) ||
         LIBMEM_ADDRESS_IN_RANGE(dest, (uint8_t *)0, (uint8_t *)(FLASH_SIZE - 1)); // Allow FLASH to be programmed at 0x00000000 alias
}

static const libmem_driver_functions_t driver_functions =
{       
  libmem_write_impl,
  0,
  libmem_erase_impl, 
  0,
  0,
  0
};

static const libmem_ext_driver_functions_t ext_driver_functions =
{
  libmem_inrange_impl,
  0,
  0
};

int
main(unsigned long param0)
{   
  unsigned ramsize;
  libmem_geometry_t *geometry;
  switch (DBGMCU->IDCODE & 0xFFF)
    {
      case 0x411: // STM32F205xx/STM32F207xx/STM32F215xx/STM32F217xx
      case 0x413: // STM32F40x/STM32F41x
      case 0x421: // STM32F446xx
      case 0x423: // STM32F401x
      case 0x431: // STM32F411xE
      case 0x433: // STM32F401x
      case 0x441: // STM32F412
      case 0x458: // STM32F410x
      case 0x463: // STM32F413x
      case 0x452: // STM32F72x/STM32F73x
        ramsize = (32*1024);
        geometry = geometry1;
        break;
      case 0x419: // STM32F42x/STM32F43x
        ramsize = (48*1024);
        geometry = geometry2;
        break;
      case 0x434: // STM32F469/STM32F479       
        {
          ramsize = (48*1024);
          if (*(volatile unsigned short *)0x1FFF7A22 == 0x400)
            {
              DB1M = FLASH->OPTCR & (1<<30);  // DB1M=1 option bit i.e. two banks
              if (DB1M)
                geometry = geometry4;
              else
                geometry = geometry1;
            }
          else
            geometry = geometry2;
        }
        break;
      case 0x449: // STM32F7x5/STM32F7x6
        ramsize = (32*1024);
        geometry = geometry3;
        break;
      case 0x451: // STM32F7x8/STM32F7x9
        ramsize = (32*1024);
        if (FLASH->OPTCR & (1<<29)) // nDBANK=1 option bit i.e. one bank
          geometry = geometry3;
        else
          {
            DB1M = *(volatile unsigned short*)0x1FF0F442 == 0x400;
            geometry = geometry2;
          }
        break;
      default:
        libmem_rpc_loader_exit(LIBMEM_STATUS_ERROR, "Unsupported device");
        break;
    }
  // Unlock
  FLASH->KEYR = 0x45670123;
  FLASH->KEYR = 0xCDEF89AB; 
  libmem_driver_handle_t h;
  libmem_register_driver(&h, FLASH_START_ADDRESS, FLASH_SIZE, geometry, 0, &driver_functions, &ext_driver_functions);
#ifdef QUADSPI_LOADER
  libmem_register_quadspi_driver();
#endif
#if 0
  {
    uint8_t *erase_start=(uint8_t*)0x08160000;
    size_t erase_size;    
    static unsigned char buffer[1024];
    for (int i=0;i<sizeof(buffer);i++)
      buffer[i] = i+1;
    int res = libmem_erase(erase_start, sizeof(buffer), &erase_start, &erase_size);
    res = libmem_write(erase_start, buffer, sizeof(buffer));    
    res = libmem_flush();
    memset(buffer, 0x00, sizeof(buffer));
    res = libmem_read(buffer, erase_start, sizeof(buffer));
    if (libmem_crc32(erase_start, sizeof(buffer), 0xFFFFFFFF) != libmem_crc32(buffer, sizeof(buffer), 0xFFFFFFFF))
      while (1);  
  }
#endif
  int res = libmem_rpc_loader_start(__RAM_segment_used_end__, __RAM_segment_start__ + ramsize - 1); 
  libmem_rpc_loader_exit(res, 0);
  return 0;
}
