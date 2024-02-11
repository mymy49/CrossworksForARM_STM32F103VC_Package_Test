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
#include <stm32h7xx.h>
#include <libmem_loader.h>

extern unsigned char __RAM_segment_start__[];
extern unsigned char __RAM_segment_used_end__[];

#ifdef CORE_CM4
#undef FLASH_SIZE
#define FLASH_SIZE (0x10 * 0x20000)
#endif

#define FLASH_START_ADDRESS (uint8_t*)0x08000000
#ifndef FLASH_SIZE
#error FLASH_SIZE not defined
#endif
#ifdef STM32H7A3xx
#define FLASH_WORD_SIZE 16
#else
#define FLASH_WORD_SIZE 32
#endif

void setFlashCR(unsigned char *addr, unsigned v) 
{
  if (addr < (FLASH_START_ADDRESS+FLASH_BANK_SIZE))
    FLASH->CR1 = v;
#ifdef DUAL_BANK
  else 
    FLASH->CR2 = v;
#endif
}
unsigned getFlashCR(unsigned char *addr) 
{
  if (addr < (FLASH_START_ADDRESS+FLASH_BANK_SIZE))
    return FLASH->CR1;
#ifdef DUAL_BANK
  else 
    return FLASH->CR2;
#endif
}
unsigned getFlashSR(unsigned char *addr)
{
  if (addr < (FLASH_START_ADDRESS+FLASH_BANK_SIZE))
    return FLASH->SR1;
#ifdef DUAL_BANK
  else
    return FLASH->SR2;
#endif
}

static unsigned char write_buffer[FLASH_WORD_SIZE]__attribute__((aligned(FLASH_WORD_SIZE)));
static libmem_driver_paged_write_ctrlblk_t paged_write_ctrlblk;
static libmem_geometry_t geometry[2];

#define KEEPALIVE IWDG1->KR=0xAAAA

static int
flash_write_page(libmem_driver_handle_t *h, unsigned char *dest, const unsigned char *src)
{
  setFlashCR(dest, FLASH_CR_PG);
  __asm("dsb");
  for (int i=0;i<(FLASH_WORD_SIZE/sizeof(uint64_t));i++)
    *(uint64_t *)(dest+i*8) = *(uint64_t *)(src+i*8);
  __asm("dsb");
  while (getFlashSR(dest) & FLASH_SR_QW);
  while (getFlashSR(dest) & FLASH_SR_BSY) KEEPALIVE;
  //FLASH_CR(dest) &= ~FLASH_CR_PG;
  setFlashCR(dest, 0);
  if (getFlashSR(dest) & 0xffff)
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
  unsigned char *dest = FLASH_START_ADDRESS + (si->number * FLASH_SECTOR_SIZE);
  setFlashCR(dest, FLASH_CR_SER | (si->number << FLASH_CR_SNB_Pos));
  setFlashCR(dest, getFlashCR(dest) | FLASH_CR_START);
  while (getFlashSR(dest) & FLASH_SR_QW);
  while (getFlashSR(dest) & FLASH_SR_BSY) KEEPALIVE;
  setFlashCR(dest, 0);
  if (getFlashSR(dest) & FLASH_SR_WRPERR)
    res = LIBMEM_STATUS_LOCKED;
  else if (getFlashSR(dest) & 0xffff)
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
      for (int i=0;i<2;i++)
        {
          unsigned char *dest = FLASH_START_ADDRESS + (i * FLASH_BANK_SIZE);
          setFlashCR(dest, FLASH_CR_BER);
          setFlashCR(dest, getFlashCR(dest) | FLASH_CR_START);
          while (getFlashSR(dest) & FLASH_SR_BSY) KEEPALIVE;
          setFlashCR(dest, 0);
          if (getFlashSR(dest) & FLASH_SR_WRPERR) 
            res = LIBMEM_STATUS_LOCKED;
          else if (getFlashSR(dest) & 0xffff)
            res = LIBMEM_STATUS_ERROR;
        }
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
  unsigned ramsize = (128*1024);
  geometry[0].count = FLASH_SIZE/FLASH_SECTOR_SIZE;
  geometry[0].size = FLASH_SECTOR_SIZE;
#if 0
  switch (DBGMCU->IDCODE & 0xFFF)
    {
      case 0x450: 
        ramsize = (128*1024);
        break;
      default:
        libmem_rpc_loader_exit(LIBMEM_STATUS_ERROR, "Unsupported device");
        break;
    }
#endif
  // Unlock
  FLASH->KEYR1 = 0x45670123;
  FLASH->KEYR1 = 0xCDEF89AB; 
#ifdef DUAL_BANK
  FLASH->KEYR2 = 0x45670123;
  FLASH->KEYR2 = 0xCDEF89AB; 
#endif
  libmem_driver_handle_t h;
  libmem_register_driver(&h, FLASH_START_ADDRESS, FLASH_SIZE, geometry, 0, &driver_functions, &ext_driver_functions);
  libmem_driver_paged_write_init(&paged_write_ctrlblk, write_buffer, sizeof(write_buffer), flash_write_page, 0, LIBMEM_DRIVER_PAGED_WRITE_OPTION_DISABLE_DIRECT_WRITES);
#ifdef QUADSPI_LOADER
  libmem_register_quadspi_driver();
#endif
#if 0
  {
    uint8_t *erase_start=(uint8_t *)0x0800ff9c;
    size_t erase_size;    
    static unsigned char buffer[474];
#if 0
    for (int i=0;i<sizeof(buffer);i++)
      buffer[i] = i+1;
    int res = libmem_erase(erase_start, sizeof(buffer), &erase_start, &erase_size);
    if (res != LIBMEM_STATUS_SUCCESS)
      while (1);
    res = libmem_write(0x0800ff9c, buffer, sizeof(buffer));    
    res = libmem_flush();
#endif
    erase_start=(uint8_t *)0x08100400;    
    int res = libmem_erase(erase_start, sizeof(buffer), &erase_start, &erase_size);
    if (res != LIBMEM_STATUS_SUCCESS)
      while (1);
    res = libmem_write(0x08100400, buffer, sizeof(buffer));    
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
