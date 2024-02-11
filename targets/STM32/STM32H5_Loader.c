/*****************************************************************************
 *                                                                           *
 * Copyright (c) 2020 Rowley Associates Limited.                             *
 *                                                                           *
 * This file may be distributed under the terms of the License Agreement     *
 * provided with this software.                                              *
 *                                                                           *
 * THIS FILE IS PROVIDED AS IS WITH NO WARRANTY OF ANY KIND, INCLUDING THE   *
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. *
 *****************************************************************************/

#include <libmem.h>
#include <stm32h5xx.h>
#include <libmem_loader.h>

extern unsigned char __RAM_segment_start__[];
extern unsigned char __RAM_segment_used_end__[];

#define FLASH_START_ADDRESS (uint8_t*)0x08000000
#define FLASH_START_ADDRESS2 (uint8_t*)0x0C000000

static unsigned char write_buffer[16];
static libmem_driver_paged_write_ctrlblk_t paged_write_ctrlblk;
static libmem_geometry_t geometry[2];

#define KEEPALIVE IWDG->KR=0xAAAA
#define TZ_DISABLED ((FLASH->OPTSR2_CUR & FLASH_OPTSR2_TZEN) == 0xC3000000)

static int
flash_write_page(libmem_driver_handle_t *h, unsigned char *dest, const unsigned char *src)
{  
  if (TZ_DISABLED)
    {
      FLASH->NSCR = FLASH_CR_PG;
      *(unsigned *)dest = *(unsigned *)src;
      *(unsigned *)(dest+4) = *(unsigned *)(src+4);
      *(unsigned *)(dest+8) = *(unsigned *)(src+8);
      *(unsigned *)(dest+12) = *(unsigned *)(src+12);
      __asm("dsb");
      while (FLASH->NSSR & FLASH_SR_BSY) KEEPALIVE;
      FLASH->NSCR &= ~FLASH_CR_PG;
      if (FLASH->NSSR & 0xfffe0000)
        return LIBMEM_STATUS_ERROR;
      else
        return LIBMEM_STATUS_SUCCESS;
    }
  else
    {
      FLASH->SECCR = FLASH_CR_PG;
      *(unsigned *)dest = *(unsigned *)src;
      *(unsigned *)(dest+4) = *(unsigned *)(src+4);
      *(unsigned *)(dest+8) = *(unsigned *)(src+8);
      *(unsigned *)(dest+12) = *(unsigned *)(src+12);
      __asm("dsb");
      while (FLASH->SECSR & FLASH_SR_BSY) KEEPALIVE;
      FLASH->SECCR &= ~FLASH_CR_PG;
      if (FLASH->SECSR & 0xfffe0000)
        return LIBMEM_STATUS_ERROR;
      else
        return LIBMEM_STATUS_SUCCESS;
    }
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
  if (TZ_DISABLED)
    {
      FLASH->NSCCR = 0xff << 16;//(FLASH_SR_OPERR  | FLASH_SR_PROGERR | FLASH_SR_WRPERR  | FLASH_SR_PGAERR | FLASH_SR_SIZERR | FLASH_SR_PGSERR | FLASH_SR_OPTWERR);   
      if (si->number >= geometry[0].count/2)
        FLASH->NSCR = FLASH_CR_SER | FLASH_CR_BKSEL | ((si->number - (geometry[0].count/2)) << 6);
      else
        FLASH->NSCR = FLASH_CR_SER | (si->number << 6);
      FLASH->NSCR |= FLASH_CR_START;
      while (FLASH->NSSR & FLASH_SR_BSY) KEEPALIVE;
      FLASH->NSCR = 0;
      if (FLASH->NSSR & FLASH_SR_WRPERR)
        res = LIBMEM_STATUS_LOCKED;
      else if (FLASH->NSSR & 0xfffe0000)
        res = LIBMEM_STATUS_ERROR;
    }
  else
    {
      FLASH->SECCCR = 0xff << 16;//(FLASH_SECSR_OPERR  | FLASH_SECSR_PROGERR | FLASH_SECSR_WRPERR  | FLASH_SECSR_PGAERR | FLASH_SECSR_SIZERR | FLASH_SECSR_PGSERR);
      if (si->number >= geometry[0].count/2)
        FLASH->SECCR = FLASH_CR_SER | FLASH_CR_BKSEL | ((si->number - (geometry[0].count/2)) << 6);
      else
        FLASH->SECCR = FLASH_CR_SER | (si->number << 6);
      FLASH->SECCR |= FLASH_CR_START;
      while (FLASH->SECSR & FLASH_SR_BSY) KEEPALIVE;
      FLASH->SECCR = 0;
      if (FLASH->SECSR & FLASH_SR_WRPERR)
        res = LIBMEM_STATUS_LOCKED;
      else if (FLASH->SECSR & 0xfffe0000)
        res = LIBMEM_STATUS_ERROR;
    }
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
      if (TZ_DISABLED)
        {
          FLASH->NSCCR = 0xff << 16;//(FLASH_NSSR_OPERR  | FLASH_NSSR_PROGERR | FLASH_NSSR_WRPERR | FLASH_NSSR_PGAERR | FLASH_NSSR_SIZERR | FLASH_NSSR_PGSERR);
          FLASH->NSCR = FLASH_CR_MER;
          FLASH->NSCR |= FLASH_CR_START;
          while (FLASH->NSSR & FLASH_SR_BSY) KEEPALIVE;
          FLASH->NSCR &= ~(FLASH_CR_MER);
          if (FLASH->NSSR & FLASH_SR_WRPERR) 
            res = LIBMEM_STATUS_LOCKED;
          else if (FLASH->NSSR & 0xffff)
            res = LIBMEM_STATUS_ERROR;
        }
      else
        {
          FLASH->SECCCR = 0xff<<16;//(FLASH_SECSR_OPERR  | FLASH_SECSR_PROGERR | FLASH_SECSR_WRPERR | FLASH_SECSR_PGAERR | FLASH_SECSR_SIZERR | FLASH_SECSR_PGSERR);
          FLASH->SECCR = FLASH_CR_MER;
          FLASH->SECCR |= FLASH_CR_START;
          while (FLASH->SECSR & FLASH_SR_BSY) KEEPALIVE;
          FLASH->SECCR &= ~(FLASH_CR_MER);
          if (FLASH->SECSR & FLASH_SR_WRPERR) 
            res = LIBMEM_STATUS_LOCKED;
          else if (FLASH->SECSR & 0xffff)
            res = LIBMEM_STATUS_ERROR;
        }
    }
  else
    {
      uint8_t *start2;
      if ((unsigned)start & 0x04000000)
        start2 = (uint8_t*)((unsigned)start & ~0x04000000);
      else
        start2 = start;
      res = libmem_foreach_sector_in_range(h, start2, size, flash_erase_sector, erased_start, erased_size);
      if (erased_start && start2 != start)
        *(unsigned*)erased_start |= 0x04000000;
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
         LIBMEM_ADDRESS_IN_RANGE(dest, (uint8_t *)FLASH_START_ADDRESS2, (uint8_t *)(FLASH_START_ADDRESS2 + FLASH_SIZE - 1));
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
  unsigned sectorsize;
  switch (DBGMCU->IDCODE & 0xFFF)
    {
      case 0x474: // STM32H503
        ramsize = 32 * 1024;
        sectorsize = 8 * 1024;
        break;
      case 0x484: // STM32H563/STM32H573/STM32H562     
        ramsize = 64 * 1024;
        sectorsize = 8 * 1024;
        break;
      default:
        libmem_rpc_loader_exit(LIBMEM_STATUS_ERROR, "Unsupported device");
        break;
    }
  geometry[0].count = FLASH_SIZE/sectorsize;
  geometry[0].size = sectorsize;
  // Unlock
  if (TZ_DISABLED)
    {
      FLASH->NSKEYR = 0x45670123;
      FLASH->NSKEYR = 0xCDEF89AB;   
    }
  else
    {      
      FLASH->SECBB1R1 = 0xffffffff;
      FLASH->SECBB1R2 = 0xffffffff;
      FLASH->SECBB1R3 = 0xffffffff;
      FLASH->SECBB1R4 = 0xffffffff;
      FLASH->SECBB2R1 = 0xffffffff;
      FLASH->SECBB2R2 = 0xffffffff;
      FLASH->SECBB2R3 = 0xffffffff;
      FLASH->SECBB2R4 = 0xffffffff; 
      FLASH->SECKEYR = 0x45670123;
      FLASH->SECKEYR = 0xCDEF89AB;   
    }
  libmem_driver_handle_t h;
  libmem_register_driver(&h, FLASH_START_ADDRESS, FLASH_SIZE, geometry, 0, &driver_functions, &ext_driver_functions);
  libmem_driver_paged_write_init(&paged_write_ctrlblk, write_buffer, sizeof(write_buffer), flash_write_page, 4, 0);
#ifdef QUADSPI_LOADER
  libmem_register_quadspi_driver();
#endif
#if 0
  {
    uint8_t *erase_start=0x08100000;
    size_t erase_size;    
    static unsigned char buffer[0xa80/*1024*/];
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
