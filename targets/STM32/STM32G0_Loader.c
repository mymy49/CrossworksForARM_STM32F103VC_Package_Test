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
#if defined(STM32G484xx)
#include <stm32g4xx.h>
#define FLASH_SR_BSY1 FLASH_SR_BSY
#define DBG DBGMCU
#else
#include <stm32g0xx.h>
#endif
#ifndef FLASH_SR_RDERR
#define FLASH_SR_RDERR (1<<14)
#endif
#include <libmem_loader.h>

extern unsigned char __RAM_segment_start__[];
extern unsigned char __RAM_segment_used_end__[];

#define FLASH_START_ADDRESS (uint8_t*)0x08000000
#ifndef FLASH_SIZE
#define FLASH_SIZE_DATA_REGISTER ((uint32_t)0x1FFF75E0)

#define FLASH_SIZE               (((((*((uint32_t *)FLASH_SIZE_DATA_REGISTER)) & (0x0000FFFFU))== 0x0000FFFFU)) ? (0x400U << 10U) : \
                                  (((*((uint32_t *)FLASH_SIZE_DATA_REGISTER)) & (0x0000FFFFU)) << 10U))
#endif
#define SECTOR_SIZE (geometry[0].size)

#define ISBANK2(N) ((N*SECTOR_SIZE)>=(FLASH_SIZE/2))
int GETBANK2(int N) 
{
  if (FLASH_SIZE == 0x80000)
    N -= 128;
  else if (FLASH_SIZE == 0x40000)
    N -= 64;
  else if (FLASH_SIZE == 0x20000)
    N -= 32;
  return N;
}
static int has_DBANK;
static unsigned char write_buffer[8];
static libmem_driver_paged_write_ctrlblk_t paged_write_ctrlblk;
static libmem_geometry_t geometry[2];

#define KEEPALIVE IWDG->KR=0xAAAA

static int
flash_write_page(libmem_driver_handle_t *h, unsigned char *dest, const unsigned char *src)
{  
  FLASH->CR = FLASH_CR_PG;
  *(unsigned *)dest = *(unsigned *)src;
  *(unsigned *)(dest+4) = *(unsigned *)(src+4);
  __asm("dsb");
  while (FLASH->SR & FLASH_SR_BSY1) KEEPALIVE;
  FLASH->CR &= ~FLASH_CR_PG;
  if (FLASH->SR & 0xffff)
    return LIBMEM_STATUS_ERROR;
  else
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
  FLASH->SR = (FLASH_SR_OPERR  | FLASH_SR_PROGERR | FLASH_SR_WRPERR  | FLASH_SR_PGAERR | FLASH_SR_SIZERR | FLASH_SR_PGSERR  | FLASH_SR_MISERR | FLASH_SR_FASTERR | FLASH_SR_RDERR | FLASH_SR_OPTVERR);
  if (has_DBANK && ISBANK2(si->number))
    FLASH->CR = FLASH_CR_PER | (GETBANK2(si->number) << 3) | FLASH_CR_BKER;
  else
    FLASH->CR = FLASH_CR_PER | (si->number << 3);
  FLASH->CR |= FLASH_CR_STRT;
  while (FLASH->SR & FLASH_SR_BSY1) KEEPALIVE;
  FLASH->CR = 0;
  if (FLASH->SR & FLASH_SR_WRPERR)
    res = LIBMEM_STATUS_LOCKED;
  else if (FLASH->SR & 0xffff)
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
      FLASH->SR = (FLASH_SR_OPERR  | FLASH_SR_PROGERR | FLASH_SR_WRPERR  | FLASH_SR_PGAERR | FLASH_SR_SIZERR | FLASH_SR_PGSERR  | FLASH_SR_MISERR | FLASH_SR_FASTERR | FLASH_SR_RDERR | FLASH_SR_OPTVERR);
      if (has_DBANK)
        FLASH->CR = (FLASH_CR_MER1 | FLASH_CR_MER2);
      else
        FLASH->CR = (FLASH_CR_MER1);
      FLASH->CR |= FLASH_CR_STRT;
      while (FLASH->SR & FLASH_SR_BSY1) KEEPALIVE;
#if defined(FLASH_SR_BSY2)
      if (has_DBANK)
        while (FLASH->SR & FLASH_SR_BSY2) KEEPALIVE;
#endif
      if (has_DBANK)
        FLASH->CR &= ~(FLASH_CR_MER1 | FLASH_CR_MER2);
      else
        FLASH->CR &= ~FLASH_CR_MER1;
      if (FLASH->SR & FLASH_SR_WRPERR) 
        res = LIBMEM_STATUS_LOCKED;
      else if (FLASH->SR & 0xffff)
        res = LIBMEM_STATUS_ERROR;
    }
  else
    {
      uint8_t *start2;
      if (start < FLASH_START_ADDRESS)
        start2 = start + (int)FLASH_START_ADDRESS;
      else
        start2 = start;
      res = libmem_foreach_sector_in_range(h, start2, size, flash_erase_sector, erased_start, erased_size);
      if (erased_start && start2 != start)
        *erased_start -= (int)FLASH_START_ADDRESS;
    }
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

int
main(unsigned long param0)
{   
  unsigned ramsize;
  SECTOR_SIZE = 2 * 1024;
  switch (DBG->IDCODE & 0xFFF)
    {
      case 0x456: // STM32G050/STM32G051/STM32G061
        ramsize = 16 * 1024;
        break;
      case 0x460: // STM32G070/STM32G071/STM32G081
        ramsize = 32 * 1024;
        break;      
      case 0x466: // STM32G030/STM32G031/STM32G041
        ramsize = 8 * 1024;
        break;
      case 0x467: // STM32G0B0/STM32G0B1/STM32G0C1
#ifdef STM32G0B0xx
        if (FLASH->OPTR & FLASH_OPTR_DUAL_BANK)
          has_DBANK = 1;
#endif
        ramsize = 128 * 1024;
        break;
      case 0x468: // STM32G4 category 2 devices
        ramsize = 16 * 1024;
        break;
      case 0x469: // STM32G4 category 3 devices 
        ramsize = 80 * 1024;        
#ifdef STM32G484xx
        if ((FLASH->OPTR & FLASH_OPTR_DBANK)==0)
          SECTOR_SIZE = 4 * 1024;
        else
          has_DBANK = 1;
#endif
        break;
      case 0x479: // STM32G4 category 4 devices 
        ramsize = 80 * 1024;
        break;
      default:
        libmem_rpc_loader_exit(LIBMEM_STATUS_ERROR, "Unsupported device");
        break;
    }
  geometry[0].count = FLASH_SIZE/SECTOR_SIZE; 
  // Unlock
  FLASH->KEYR = 0x45670123;
  FLASH->KEYR = 0xCDEF89AB; 
  FLASH->ACR = (1<<18);
  libmem_driver_handle_t h;
  libmem_register_driver(&h, FLASH_START_ADDRESS, FLASH_SIZE, geometry, 0, &driver_functions, &ext_driver_functions);
  libmem_driver_paged_write_init(&paged_write_ctrlblk, write_buffer, sizeof(write_buffer), flash_write_page, 4, 0);
#if 0
  {  
    libmem_erase_all();
    uint8_t *erase_start=0x08040000;
    size_t erase_size;    
    static unsigned char buffer[1024];
    for (int i=0;i<sizeof(buffer);i++)
      buffer[i] = i+1;
    int res = libmem_erase(erase_start, sizeof(buffer), &erase_start, &erase_size);
    if (res != LIBMEM_STATUS_SUCCESS)
      while (1);
    res = libmem_write(erase_start, buffer, sizeof(buffer));    
    res = libmem_flush();
    //memset(buffer, 0x00, sizeof(buffer));
    //res = libmem_read(buffer, erase_start, sizeof(buffer));
    //if (libmem_crc32(erase_start, sizeof(buffer), 0xFFFFFFFF) != libmem_crc32(buffer, sizeof(buffer), 0xFFFFFFFF))
    //  while (1);  
  }
#endif
  int res = libmem_rpc_loader_start(__RAM_segment_used_end__, __RAM_segment_start__ + ramsize - 1); 
  libmem_rpc_loader_exit(res, 0);
  return 0;
}
