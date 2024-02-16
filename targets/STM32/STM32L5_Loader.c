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
#include <stm32l5xx.h>
#include <libmem_loader.h>

extern unsigned char __RAM_segment_start__[];
extern unsigned char __RAM_segment_used_end__[];

#define FLASH_START_ADDRESS (uint8_t*)0x08000000
#define FLASH_START_ADDRESS2 (uint8_t*)0x0C000000

static unsigned char write_buffer[8];
static libmem_driver_paged_write_ctrlblk_t paged_write_ctrlblk;
static libmem_geometry_t geometry[2];

#define KEEPALIVE IWDG->KR=0xAAAA

static int
flash_write_page(libmem_driver_handle_t *h, unsigned char *dest, const unsigned char *src)
{  
  if (!(FLASH->OPTR & FLASH_OPTR_TZEN))
    {
      FLASH->NSCR = FLASH_NSCR_NSPG;
      *(unsigned *)dest = *(unsigned *)src;
      *(unsigned *)(dest+4) = *(unsigned *)(src+4);
      __asm("dsb");
      while (FLASH->NSSR & FLASH_NSSR_NSBSY) KEEPALIVE;
      FLASH->NSCR &= ~FLASH_NSCR_NSPG;
      if (FLASH->NSSR & 0xffff)
        return LIBMEM_STATUS_ERROR;
      else
        return LIBMEM_STATUS_SUCCESS;
    }
  else
    {
      FLASH->SECCR = FLASH_SECCR_SECPG;
      *(unsigned *)dest = *(unsigned *)src;
      *(unsigned *)(dest+4) = *(unsigned *)(src+4);
      __asm("dsb");
      while (FLASH->SECSR & FLASH_SECSR_SECBSY) KEEPALIVE;
      FLASH->SECCR &= ~FLASH_SECCR_SECPG;
      if (FLASH->SECSR & 0xffff)
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
  if (!(FLASH->OPTR & FLASH_OPTR_TZEN))
    {
      FLASH->NSSR = (FLASH_NSSR_NSOPERR  | FLASH_NSSR_NSPROGERR | FLASH_NSSR_NSWRPERR  | FLASH_NSSR_NSPGAERR | FLASH_NSSR_NSSIZERR | FLASH_NSSR_NSPGSERR | FLASH_NSSR_OPTWERR);
      FLASH->NSCR = FLASH_NSCR_NSPER | (si->number << 3);
      if ((FLASH->OPTR & FLASH_OPTR_DBANK) && (si->number >= 128))
        FLASH->NSCR |= FLASH_NSCR_NSBKER;
      FLASH->NSCR |= FLASH_NSCR_NSSTRT;
      while (FLASH->NSSR & FLASH_NSSR_NSBSY) KEEPALIVE;
      FLASH->NSCR = 0;
      if (FLASH->NSSR & FLASH_NSSR_NSWRPERR)
        res = LIBMEM_STATUS_LOCKED;
      else if (FLASH->NSSR & 0xffff)
        res = LIBMEM_STATUS_ERROR;
    }
  else
    {
      FLASH->SECSR = (FLASH_SECSR_SECOPERR  | FLASH_SECSR_SECPROGERR | FLASH_SECSR_SECWRPERR  | FLASH_SECSR_SECPGAERR | FLASH_SECSR_SECSIZERR | FLASH_SECSR_SECPGSERR);
      FLASH->SECCR = FLASH_SECCR_SECPER | (si->number << 3);
      if ((FLASH->OPTR & FLASH_OPTR_DBANK) && (si->number >= 128))
        FLASH->SECCR |= FLASH_SECCR_SECBKER;
      FLASH->SECCR |= FLASH_SECCR_SECSTRT;
      while (FLASH->SECSR & FLASH_SECSR_SECBSY) KEEPALIVE;
      FLASH->SECCR = 0;
      if (FLASH->SECSR & FLASH_SECSR_SECWRPERR)
        res = LIBMEM_STATUS_LOCKED;
      else if (FLASH->SECSR & 0xffff)
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
      if (!(FLASH->OPTR & FLASH_OPTR_TZEN))
        {
          FLASH->NSSR = (FLASH_NSSR_NSOPERR  | FLASH_NSSR_NSPROGERR | FLASH_NSSR_NSWRPERR | FLASH_NSSR_NSPGAERR | FLASH_NSSR_NSSIZERR | FLASH_NSSR_NSPGSERR);
          FLASH->NSCR = FLASH_NSCR_NSMER1 | FLASH_NSCR_NSMER2;
          FLASH->NSCR |= FLASH_NSCR_NSSTRT;
          while (FLASH->NSSR & FLASH_NSSR_NSBSY) KEEPALIVE;
          FLASH->NSCR &= ~(FLASH_NSCR_NSMER1 | FLASH_NSCR_NSMER2);
          if (FLASH->NSSR & FLASH_NSSR_NSWRPERR) 
            res = LIBMEM_STATUS_LOCKED;
          else if (FLASH->NSSR & 0xffff)
            res = LIBMEM_STATUS_ERROR;
        }
      else
        {
          FLASH->SECSR = (FLASH_SECSR_SECOPERR  | FLASH_SECSR_SECPROGERR | FLASH_SECSR_SECWRPERR | FLASH_SECSR_SECPGAERR | FLASH_SECSR_SECSIZERR | FLASH_SECSR_SECPGSERR);
          FLASH->SECCR = FLASH_SECCR_SECMER1 | FLASH_SECCR_SECMER2;
          FLASH->SECCR |= FLASH_SECCR_SECSTRT;
          while (FLASH->SECSR & FLASH_SECSR_SECBSY) KEEPALIVE;
          FLASH->SECCR &= ~(FLASH_SECCR_SECMER1 | FLASH_SECCR_SECMER2);
          if (FLASH->SECSR & FLASH_SECSR_SECWRPERR) 
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
  unsigned ramsize=64*1024;
  switch (DBGMCU->IDCODE & 0xFFF)
    {
      case 0x472: // STM32L552/STM32L562
        {
          unsigned sectorsize;
          if (FLASH->OPTR & FLASH_OPTR_DBANK) // Dual bank
            sectorsize = 2 * 1024;
          else
            sectorsize = 4 * 1024;          
          geometry[0].count = FLASH_SIZE/sectorsize;
          geometry[0].size = sectorsize;
        }
        break;
      default:
        libmem_rpc_loader_exit(LIBMEM_STATUS_ERROR, "Unsupported device");
        break;
    }
  // Unlock
  if (!(FLASH->OPTR & FLASH_OPTR_TZEN))
    {
      FLASH->NSKEYR = 0x45670123;
      FLASH->NSKEYR = 0xCDEF89AB;   
    }
  else
    {
      if (FLASH->OPTR & FLASH_OPTR_DBANK)
        {
          FLASH->SECBB1R1 = 0xffffffff;
          FLASH->SECBB1R2 = 0xffffffff;
          FLASH->SECBB1R3 = 0xffffffff;
          FLASH->SECBB1R4 = 0xffffffff;
          FLASH->SECBB2R1 = 0xffffffff;
          FLASH->SECBB2R2 = 0xffffffff;
          FLASH->SECBB2R3 = 0xffffffff;
          FLASH->SECBB2R4 = 0xffffffff;
        }
      else
        {
          FLASH->SECBB1R1 = 0xffffffff;
          FLASH->SECBB1R1 = 0xffffffff;
          FLASH->SECBB1R1 = 0xffffffff;
          FLASH->SECBB1R1 = 0xffffffff;
        }
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
    uint8_t *erase_start=0x08040000;
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
