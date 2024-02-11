// Copyright (c) 2010 Rowley Associates Limited.
//
// This file may be distributed under the terms of the License Agreement
// provided with this software.
//
// THIS FILE IS PROVIDED AS IS WITH NO WARRANTY OF ANY KIND, INCLUDING THE
// WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.

#include <string.h>
#include <stdlib.h>
#include "STM32SimulatorMemory.h"

#define PERIPHERALSTART 0x40000000
#define PERIPHERALSIZE  0x20000000

#define STM32F10x_RCC_OFFSET 0x21000
#define STM32F10x_RCC_CR (STM32F10x_RCC_OFFSET+0x000)
#define STM32F10x_RCC_CFGR (STM32F10x_RCC_OFFSET+0x004)

class STM32F10xPeripheralRegion : public LittleSparseMemoryRegion
{
public:
  STM32F10xPeripheralRegion() : LittleSparseMemoryRegion(PERIPHERALSIZE)
    {       
    }
  void reset()
    {
      LittleSparseMemoryRegion::clear(0x0);
      LittleSparseMemoryRegion::pokeWord(STM32F10x_RCC_CR, 0x00000083);
    }
  void pokeWord(unsigned offset, unsigned l)
    {
      if (offset == STM32F10x_RCC_CR)
        {
          if (l & (1<<16)) // HSEON
            l |= (1<<17); // HSERDY
          if (l & (1<<24)) // PLLON
            l |= (1<<25); // PLLRDY
          if (l & (1<<26)) // PLL2ON
            l |= (1<<27); // PLL2RDY
          if (l & (1<<28)) // PLL3ON
            l |= (1<<29); // PLL3RDY
        }
      else if (offset == STM32F10x_RCC_CFGR)
        {
          l |= (l&0x3)<<2; // SWS==SW
        }
      LittleSparseMemoryRegion::pokeWord(offset, l);
    }
};

#define STM32L15x_RCC_OFFSET 0x23800
#define STM32L15x_RCC_CR (STM32L15x_RCC_OFFSET+0x000)
#define STM32L15x_RCC_CFGR (STM32L15x_RCC_OFFSET+0x008)

#define STM32L15x_PWR_OFFSET 0x27400
#define STM32L15x_PWR_CR (STM32L15x_PWR_OFFSET+0x000)
#define STM32L15x_PWR_CSR (STM32L15x_PWR_OFFSET+0x004)


class STM32L15xPeripheralRegion : public LittleSparseMemoryRegion
{
public:
  STM32L15xPeripheralRegion() : LittleSparseMemoryRegion(PERIPHERALSIZE) 
    {
    }
  void reset()
    {
      LittleSparseMemoryRegion::clear(0x0);
      LittleSparseMemoryRegion::pokeWord(STM32L15x_RCC_CR, 0x00000300);
      LittleSparseMemoryRegion::pokeWord(STM32L15x_PWR_CR, 0x00001000);
      LittleSparseMemoryRegion::pokeWord(STM32L15x_PWR_CSR, 0x00000008);
    }
  void pokeWord(unsigned offset, unsigned l)
    {
      if (offset == STM32L15x_RCC_CR)
        {
          if (l & (1<<0)) // HSION
            l |= (1<<1); // HSIRDY
          if (l & (1<<16)) // HSEON
            l |= (1<<17); // HSERDY
          if (l & (1<<24)) // PLLON
            l |= (1<<25); // PLLRDY         
        }
      else if (offset == STM32L15x_RCC_CFGR)
        {
          l |= (l&0x3)<<2; // SWS==SW
        }
      else if (offset == STM32L15x_PWR_CR)
        {
          if (l & (3<<12)) // VOS
            LittleSparseMemoryRegion::pokeWord(STM32L15x_PWR_CSR, LittleSparseMemoryRegion::peekWord(STM32L15x_PWR_CSR)|(1<<4)); // VOSF            
        }
      LittleSparseMemoryRegion::pokeWord(offset, l);
    }
};

#define STM32W108_MAC_TIMER_OFFSET 0x2038

class STM32W108PeripheralRegion : public LittleSparseMemoryRegion
{
public:
  STM32W108PeripheralRegion() : LittleSparseMemoryRegion(PERIPHERALSIZE)
    {
    }
  void reset()
    {
      LittleSparseMemoryRegion::clear(0x0);
    } 
  unsigned peekWord(unsigned address)
    {
      if (address == STM32W108_MAC_TIMER_OFFSET)        
        pokeWord(STM32W108_MAC_TIMER_OFFSET, LittleSparseMemoryRegion::peekWord(address)+10);        
      return LittleSparseMemoryRegion::peekWord(address);
    }
};

#define STM32F2xx_RCC_OFFSET 0x23800
#define STM32F2xx_RCC_CR (STM32F2xx_RCC_OFFSET+0x000)
#define STM32F2xx_RCC_CFGR (STM32F2xx_RCC_OFFSET+0x008)
#define STM32F2xx_PWR_OFFSET 0x7000
#define STM32F2xx_PWR_CR (STM32F2xx_PWR_OFFSET+0x000)
#define STM32F2xx_PWR_CSR (STM32F2xx_PWR_OFFSET+0x004)

class STM32F2xxPeripheralRegion : public LittleSparseMemoryRegion
{
public:
  STM32F2xxPeripheralRegion() : LittleSparseMemoryRegion(PERIPHERALSIZE)
    {       
    }
  void reset()
    {
      LittleSparseMemoryRegion::clear(0x0);
      LittleSparseMemoryRegion::pokeWord(STM32F10x_RCC_CR, 0x00000083);
    }
  void pokeWord(unsigned offset, unsigned l)
    {
      if (offset == STM32F2xx_RCC_CR)
        {
          if (l & (1<<16)) // HSEON
            l |= (1<<17); // HSERDY
          if (l & (1<<24)) // PLLON
            l |= (1<<25); // PLLRDY
          if (l & (1<<26)) // PLL2ON
            l |= (1<<27); // PLL2RDY
          if (l & (1<<28)) // PLL3ON
            l |= (1<<29); // PLL3RDY
        }
      else if (offset == STM32F2xx_RCC_CFGR)
        {
          l |= (l&0x3)<<2; // SWS==SW
        }
      else if (offset == STM32F2xx_PWR_CR)
        {
          if (l & (1<<16)) // ODEN
            LittleSparseMemoryRegion::pokeWord(STM32F2xx_PWR_CSR, LittleSparseMemoryRegion::peekWord(STM32F2xx_PWR_CSR)|(1<<16)); // ODRDY
          if (l & (1<<17)) // ODSWEN
            LittleSparseMemoryRegion::pokeWord(STM32F2xx_PWR_CSR, LittleSparseMemoryRegion::peekWord(STM32F2xx_PWR_CSR)|(1<<17)); // ODSWRDY
        }
      LittleSparseMemoryRegion::pokeWord(offset, l);
    }
};

#define STM32L0x_RCC_OFFSET 0x21000
#define STM32L0x_RCC_CR (STM32F10x_RCC_OFFSET+0x000)
#define STM32L0x_RCC_CFGR (STM32F10x_RCC_OFFSET+0x00C)

class STM32L0xPeripheralRegion : public LittleSparseMemoryRegion
{
public:
  STM32L0xPeripheralRegion() : LittleSparseMemoryRegion(PERIPHERALSIZE)
    {       
    }
  void reset()
    {
      LittleSparseMemoryRegion::clear(0x0);
      LittleSparseMemoryRegion::pokeWord(STM32L0x_RCC_CR, 0x00000083);
    }
  void pokeWord(unsigned offset, unsigned l)
    {
      if (offset == STM32L0x_RCC_CR)
        {
          if (l & (1<<0)) // HSI16ON
            l |= (1<<2); // HSI16RDY
          if (l & (1<<16)) // HSEON
            l |= (1<<17); // HSERDY
          if (l & (1<<24)) // PLLON
            l |= (1<<25); // PLLRDY        
        }
      else if (offset == STM32L0x_RCC_CFGR)
        {
          l |= (l&0x3)<<2; // SWS==SW
        }
      LittleSparseMemoryRegion::pokeWord(offset, l);
    }
};

#define STM32L4x_RCC_OFFSET 0x21000
#define STM32L4x_RCC_CR (STM32L4x_RCC_OFFSET+0x000)
#define STM32L4x_RCC_CFGR (STM32L4x_RCC_OFFSET+0x008)

class STM32L4xPeripheralRegion : public LittleSparseMemoryRegion
{
public:
  STM32L4xPeripheralRegion() : LittleSparseMemoryRegion(PERIPHERALSIZE)
    {       
    }
  void reset()
    {
      LittleSparseMemoryRegion::clear(0x0);
      LittleSparseMemoryRegion::pokeWord(STM32L4x_RCC_CR, 0x00000063);
    }
  void pokeWord(unsigned offset, unsigned l)
    {
      if (offset == STM32L4x_RCC_CR)
        {
          if (l & (1<<16)) // HSEON
            l |= (1<<17); // HSERDY
          if (l & (1<<24)) // PLLON
            l |= (1<<25); // PLLRDY
          if (l & (1<<26)) // PLL2ON
            l |= (1<<27); // PLL2RDY
          if (l & (1<<28)) // PLL3ON
            l |= (1<<29); // PLL3RDY
        }
      else if (offset == STM32L4x_RCC_CFGR)
        {
          l |= (l&0x3)<<2; // SWS==SW
        }
      LittleSparseMemoryRegion::pokeWord(offset, l);
    }
};

STM32SimulatorMemoryImpl::STM32SimulatorMemoryImpl() :
  flash(0), sram(0), sram2(0), peripherals(0), scs(0), eppb(0), fsmc(0), dataram(0)
{
  int i;
  for (i=0;i<4;i++)
    fsmc_nor_psram[i] = 0;
}

STM32SimulatorMemoryImpl::~STM32SimulatorMemoryImpl()
{
  if (flash)
    delete flash;
  if (sram)
    delete sram;
  if (sram2)
    delete sram2;
  if (peripherals)
    delete peripherals;
  if (scs)
    delete scs;
  if (eppb)
    delete eppb;
  if (fsmc)
    delete fsmc;
  int i;
  for (i=0;i<4;i++)
    if (fsmc_nor_psram[i])
      delete fsmc_nor_psram[i];
  if (dataram)
    delete dataram;
}

bool 
STM32SimulatorMemoryImpl::setSpecification(bool le, unsigned argc, const char *argv[])
{
  if (argc < 3 || argc > 7)
    return false;
  if (strstr(argv[0], "STM32C0")==argv[0] || strstr(argv[0], "STM32F0")==argv[0] || strstr(argv[0], "STM32F1")==argv[0] || strstr(argv[0], "STM32G0")==argv[0] || strstr(argv[0], "STM32G4")==argv[0])
    {
      peripherals = new STM32F10xPeripheralRegion();
    }
  else if (strstr(argv[0], "STM32F2")==argv[0] || strstr(argv[0], "STM32F4")==argv[0] || strstr(argv[0], "STM32F7")==argv[0] || strstr(argv[0], "STM32H7")==argv[0] || strstr(argv[0], "STM32H5")==argv[0])
    {
      peripherals = new STM32F2xxPeripheralRegion();
      if (strstr(argv[0], "STM32H")==argv[0])
        sram2 = new LittleMemoryRegion(0x48000);
    }
  else if (strstr(argv[0], "STM32F3")==argv[0])
    {
      peripherals = new STM32F10xPeripheralRegion();
    }
  else if (strstr(argv[0], "STM32L0")==argv[0])
    {
      peripherals = new STM32L0xPeripheralRegion();
    }
  else if (strstr(argv[0], "STM32L1")==argv[0])
    {
      peripherals = new STM32L15xPeripheralRegion();   
    }
  else if (strstr(argv[0], "STM32L4")==argv[0] || strstr(argv[0], "STM32L5")==argv[0] || strstr(argv[0], "STM32U5")==argv[0])
    {
      peripherals = new STM32L4xPeripheralRegion();   
    }
  else if (strstr(argv[0], "STM32W")==argv[0])
    {
      peripherals = new STM32W108PeripheralRegion();     
    }
  else
    return false;
  unsigned flashSize;
  if (strstr(argv[1], "FLASH")==argv[1])
    flashSize = strtoul(argv[1]+sizeof("FLASH,0x08000000"),0,0);
  else
    flashSize = strtoul(argv[1],0,0);
  unsigned sramSize;
  if (strstr(argv[2], "RAM")==argv[2])
    sramSize = strtoul(argv[2]+sizeof("RAM,0x20000000"),0,0);
  else
    sramSize = strtoul(argv[2],0,0);
  flash = new LittleMemoryRegion(flashSize);
  flash->clear(0xff);  
  sram = new LittleMemoryRegion(sramSize);   
  scs = new LittleMemoryRegion(0x1000);
  eppb = new LittleMemoryRegion(0x8);
  fsmc = new LittleMemoryRegion(0x100);
  int i;
  for (i=3;i<7;i++)
    {
      unsigned size = 0;
      if (i<argc)
        size = strtoul(argv[i], 0, 0);
      fsmc_nor_psram[i-3] = new LittleMemoryRegion(size);
    }
  if (strstr(argv[0], "STM32F4")==argv[0])
    dataram = new LittleMemoryRegion(0x10000);
  return true;
}

void 
STM32SimulatorMemoryImpl::reset()
{
  sram->clear(0xcd);
  if (sram2)
    sram2->clear(0xcd);
  peripherals->clear(0x0);
  scs->clear(0x0);
  eppb->clear(0x0); 
  fsmc->clear(0x0);
  if (dataram)
    dataram->clear(0xcd);
}

void 
STM32SimulatorMemoryImpl::eraseAll()
{
  flash->clear(0xff);
}

MemoryRegion *
STM32SimulatorMemoryImpl::findMemoryRegion(unsigned address, unsigned size, unsigned &offset)
{
  MemoryRegion *m = 0;
  if (address < 0x08100000)
    {
      if (address >= 0x08000000)
        offset = address - 0x08000000;
      else
        offset = address;
      m = flash;
    }
  else if (address < 0x20000000)
    {
      if (dataram && (address >= 0x10000000) && (address < (0x10000000 + dataram->size())))
        {
          offset = address - 0x10000000;
          m = dataram;
        }
    }
  else if (address < 0x20100000)
    {
      offset = address - 0x20000000;
      m = sram;
    }
  else if (address >= 0x30000000 && address < 0x30048000)
    {
      offset = address - 0x30000000;
      m = sram2;
    }
  else if (address >= PERIPHERALSTART && address < (PERIPHERALSTART+PERIPHERALSIZE))
    {
      offset = address - PERIPHERALSTART;
      m = peripherals;
    }
  else if (address >= 0x60000000 && address < 0x64000000)
    {
      offset = address - 0x60000000;
      m = fsmc_nor_psram[0];
    }
  else if (address >= 0x64000000 && address < 0x68000000)
    {
      offset = address - 0x64000000;
      m = fsmc_nor_psram[1];
    }
  else if (address >= 0x68000000 && address < 0x6C000000)
    {
      offset = address - 0x68000000;
      m = fsmc_nor_psram[2];
    }
  else if (address >= 0x6C000000 && address < 0x70000000)
    {
      offset = address - 0x6C000000;
      m = fsmc_nor_psram[3];
    }
  else if (address >= 0xA0000000 && address < (0xA0000000 + fsmc->size()))
    {
      offset = address - 0xA0000000;
      m = fsmc;
    }
  else if (address >= 0xE000E000 && address < 0xE000F000)
    {
      offset = address - 0xE000E000;
      m = scs;
    }
  else if (address >= 0xE0042000 && address < (0xE0042000 + eppb->size()))
    {
      offset = address - 0xE0042000;
      m = eppb;
    }
  if (m && offset+size <= m->size())
    return m;
  return 0;
}

#ifdef WIN32

#define WIN32_LEAN_AND_MEAN	
#include <windows.h>

BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 )
{
    return TRUE;
}

#define EXPORT __declspec( dllexport )

#else

#define EXPORT

#endif

extern "C" EXPORT void *AllocateARMSimulatorMemoryInterface(void);
extern "C" EXPORT void ReleaseARMSimulatorMemoryInterface(void *ptr);

void *AllocateARMSimulatorMemoryInterface(void)
{
  return new STM32SimulatorMemoryImpl();
}

void ReleaseARMSimulatorMemoryInterface(void *ptr)
{
  delete (STM32SimulatorMemoryImpl*)ptr;
}
