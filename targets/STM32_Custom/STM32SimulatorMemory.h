#ifndef __STM32SimulatorMemory
#define __STM32SimulatorMemory

#include "ARMSimulatorMemoryImplementation.h"

class STM32SimulatorMemoryImpl : public ARMSimulatorMemoryImplementation
{
public:
  STM32SimulatorMemoryImpl();
  ~STM32SimulatorMemoryImpl();
  bool setSpecification(bool le, unsigned argc, const char *argv[]);
  void reset();  
  void eraseAll();  
  MemoryRegion *findMemoryRegion(unsigned address, unsigned size, unsigned &offset);
private:
  MemoryRegion *flash, *sram, *sram2, *peripherals, *fsmc_nor_psram[4], *fsmc, *scs, *eppb, *dataram;  
};

#endif
