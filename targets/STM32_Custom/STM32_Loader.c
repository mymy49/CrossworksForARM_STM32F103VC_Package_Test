/*****************************************************************************
 *                                                                           *
 * Copyright (c) 2009, 2010 Rowley Associates Limited.                       *
 *                                                                           *
 * This file may be distributed under the terms of the License Agreement     *
 * provided with this software.                                              *
 *                                                                           *
 * THIS FILE IS PROVIDED AS IS WITH NO WARRANTY OF ANY KIND, INCLUDING THE   *
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. *
 *****************************************************************************/

#include <libmem.h>
#if defined(STM32F030x6)
#include <stm32f0xx.h>
#elif defined(STM32W108xB) || defined(STM32W108xC)
#define STM32F10X_LD
#include <stm32f10x.h>
#undef FLASH
#define FLASH ((FLASH_TypeDef *) (0x40008000))
#else
#define STM32F10X_XL
#include <stm32f10x.h>
#endif
#include <libmem_loader.h>

extern unsigned char __RAM_segment_start__[];
extern unsigned char __RAM_segment_used_end__[];

#define FLASH_START_ADDRESS (0x08000000)
#define FLASH_BANK_SIZE (0x80000)
#ifdef STM32F10X_XL
#define FLASH2_START_ADDRESS (FLASH_START_ADDRESS+FLASH_BANK_SIZE)
#endif

unsigned FLASH_SIZE;
libmem_geometry_t geometry[2];

#define KEEPALIVE IWDG->KR=0xAAAA

static void 
FLASH_ProgramHalfWord(unsigned address, unsigned short v)
{
#ifdef STM32F10X_XL
  if (address >= FLASH2_START_ADDRESS)
    {
      FLASH->CR2 |= FLASH_CR_PG;
      *(unsigned short *)(address) = v;
      while (FLASH->SR2 & 1) KEEPALIVE;
      FLASH->CR2 &= ~FLASH_CR_PG;
    }
  else
#endif
    {
      FLASH->CR |= FLASH_CR_PG;
      *(unsigned short *)(address) = v;
      while (FLASH->SR & 1) KEEPALIVE;
      FLASH->CR &= ~FLASH_CR_PG;
    }  
}

static int
libmem_write_impl(libmem_driver_handle_t *h, uint8_t *dest, const uint8_t *src, size_t size)
{
  if (dest < (uint8_t *)FLASH_START_ADDRESS)
    dest += FLASH_START_ADDRESS;
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
libmem_erase_impl(libmem_driver_handle_t *h, uint8_t *start, size_t size, uint8_t **erased_start, size_t *erased_size)
{
  int res = LIBMEM_STATUS_SUCCESS;
  if (LIBMEM_RANGE_WITHIN_RANGE(h->start, h->start + h->size - 1, start, start + size - 1))
    {
      if (erased_start)
        *erased_start = h->start;
      if (erased_size)
        *erased_size = h->size;
      FLASH->CR |= FLASH_CR_MER;  
      FLASH->CR |= FLASH_CR_STRT;
      while (FLASH->SR & 1) KEEPALIVE;      
      FLASH->CR &= ~FLASH_CR_MER;
#ifdef STM32F10X_XL
      if (FLASH_SIZE > FLASH_BANK_SIZE)
        {
          FLASH->CR2 |= FLASH_CR_MER;  
          FLASH->CR2 |= FLASH_CR_STRT;
          while (FLASH->SR2 & 1);      
          FLASH->CR2 &= ~FLASH_CR_MER;
        }
#endif
    }
  else
    {
      uint8_t *start2;
      if (start < (uint8_t *)FLASH_START_ADDRESS)
        start2 = start + FLASH_START_ADDRESS;
      else
        start2 = start;
      int found = 0;
      int j = geometry[0].count;
      int blocksize = geometry[0].size;
      uint8_t *end = start2 + size - 1;
      uint8_t *flashstart = h->start;      
      if (erased_size)
        *erased_size = 0;
      while (j--)
        {
          if (LIBMEM_RANGE_OCCLUDES_RANGE(start2, end, flashstart, flashstart + blocksize - 1))
            {
              if (!found)
                {
                  if (erased_start)
                    *erased_start = flashstart;
                  found = 1;
                }
              if (res == LIBMEM_STATUS_SUCCESS)
                {
#ifdef STM32F10X_XL
                  if ((unsigned)flashstart >= FLASH2_START_ADDRESS)
                    {
                      FLASH->CR2 |= FLASH_CR_PER;  
                      FLASH->AR2 = (unsigned)flashstart; 
                      FLASH->CR2 |= FLASH_CR_STRT;
                      while (FLASH->SR2 & 1) KEEPALIVE;                     
                      FLASH->CR2 &= ~FLASH_CR_PER;
                    }
                  else
#endif
                    {
                      FLASH->CR |= FLASH_CR_PER;  
                      FLASH->AR = (unsigned)flashstart; 
                      FLASH->CR |= FLASH_CR_STRT;
                      while (FLASH->SR & 1) KEEPALIVE;                     
                      FLASH->CR &= ~FLASH_CR_PER;
                    }
                }
              if (erased_size)
                *erased_size += blocksize;
            }
          else
            {
              if (found)
                return res;
            }
          flashstart += blocksize;         
        }         
      if (erased_start && start2 != start)
        *erased_start -= FLASH_START_ADDRESS;           
    }
  return res;
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
  0
};

static const libmem_ext_driver_functions_t ext_driver_functions =
{
  libmem_inrange_impl,
  0,
  0
};

unsigned 
stm32_register_driver(libmem_driver_handle_t *h)
{
  unsigned ramsize = 0;
#if defined(STM32W108xB)
  FLASH_SIZE = (128 * 1024);
  geometry[0].count = 128;
  geometry[0].size = 1024;
  ramsize = (8*1024);
#elif defined(STM32W108xC)
  FLASH_SIZE = (128 * 2048);
  geometry[0].count = 128;
  geometry[0].size = 2048;
  ramsize = (12*1024);
#else
  switch ((DBGMCU->IDCODE & DBGMCU_IDCODE_DEV_ID))
    {
#if defined(STM32F030x6)
      case 0x440: // STM32F05x/STM32F051xx/STM32F030x8
        FLASH_SIZE = (64 * 1024);
        geometry[0].count = 64;
        geometry[0].size = 1024;
        ramsize = (4*1024);
        break;
      case 0x442: // STM32F09x
        FLASH_SIZE = (256 * 1024);
        geometry[0].count = 128;
        geometry[0].size = 2048;
        ramsize = (32*1024);
        break;
      case 0x444: // STM32F03x/STM32F050xx/STM32F030x4/STM32F030x6
      case 0x445: // STM32F04x
        FLASH_SIZE = (32 * 1024);
        geometry[0].count = 32;
        geometry[0].size = 1024;
        ramsize = (4*1024);
        break;
      case 0x448: // STM32F07x
        FLASH_SIZE = (128 * 1024);
        geometry[0].count = 64;
        geometry[0].size = 2048;
        ramsize = (16*1024);
        break;
#else
      case 0x412: // low density
        FLASH_SIZE = (32 * 1024);       
        geometry[0].count = 32;
        geometry[0].size = 1024;
        ramsize = (4*1024);
        break;
      case 0x410: // medium density
        FLASH_SIZE = (128 * 1024);       
        geometry[0].count = 128;
        geometry[0].size = 1024;
        ramsize = (10*1024);
        break;
      case 0x420: // value 
        FLASH_SIZE = (128 * 1024);       
        geometry[0].count = 128;
        geometry[0].size = 1024;
        ramsize = (4*1024);
        break;
      case 0x414: // high density
        FLASH_SIZE = (256 * 2048);       
        geometry[0].count = 256;
        geometry[0].size = 2048;
        ramsize = (32*1024);
        break;
      case 0x428: // value
        FLASH_SIZE = (256 * 2048);       
        geometry[0].count = 256;
        geometry[0].size = 2048;
        ramsize = (24*1024);
        break;
      case 0x418: // connectivity
        FLASH_SIZE = (128 * 2048);       
        geometry[0].count = 128;
        geometry[0].size = 2048;
        ramsize = (16*1024);
        break;           
      case 0x430: // xl density
        FLASH_SIZE = (512 * 2048);
        geometry[0].count = 512;
        geometry[0].size = 2048;
        ramsize = (80*1024);
        break;
      case 0x422: // f30x
      case 0x432: // f37x
      case 0x439: // f30x
        FLASH_SIZE = (128 * 2048);
        geometry[0].count = 128;
        geometry[0].size = 2048;
        ramsize = (16*1024);
        break;
      case 0x438: // STM32F334x
        FLASH_SIZE = (32 * 2048);
        geometry[0].count = 32;
        geometry[0].size = 2048;
        ramsize = (12*1024);
        break;
      case 0x446: // STM32F303xD/E
        FLASH_SIZE = (256 * 2048);
        geometry[0].count = 256;
        geometry[0].size = 2048;
        ramsize = (16*1024);
        break;
#endif
      default:
        libmem_rpc_loader_exit(LIBMEM_STATUS_ERROR, "Unsupported device");
        break;
    }
#endif
  // Unlock
  FLASH->KEYR = 0x45670123;
  FLASH->KEYR = 0xCDEF89AB;
#ifdef STM32F10X_XL
  if (FLASH_SIZE > FLASH_BANK_SIZE)
    {
      FLASH->KEYR2 = 0x45670123;
      FLASH->KEYR2 = 0xCDEF89AB;
    }
#endif
#if defined(STM32W108xB) || defined(STM32W108xC)
  *(volatile unsigned int *)0x4000402C = 1; // FLASH_CLKER
  while (!(*(volatile unsigned int *)0x40004030)); // FLASH_CLKSR
#endif
  libmem_register_driver(h, (uint8_t *)FLASH_START_ADDRESS, FLASH_SIZE, geometry, 0, &driver_functions, &ext_driver_functions);
  return ramsize;
}

#ifndef NOMAIN
int
main(unsigned long param0)
{     
  libmem_driver_handle_t h;
  unsigned ramsize = stm32_register_driver(&h);
#if 0
  {
    uint8_t *erase_start;
    size_t erase_size;
    int res;
    res = libmem_erase((uint8_t *)0x08000000, 8, &erase_start, &erase_size);
    const unsigned char buffer[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };     
    res = libmem_write((uint8_t *)0x08000000, buffer, sizeof(buffer));
    res = libmem_flush();
  }
#endif
  int res = libmem_rpc_loader_start(__RAM_segment_used_end__, __RAM_segment_start__ + ramsize - 1); 
  libmem_rpc_loader_exit(res, 0);
  return 0;
}
#endif
