// Copyright (c) 2011-2014 Rowley Associates Limited.
//
// This file may be distributed under the terms of the License Agreement
// provided with this software.
//
// THIS FILE IS PROVIDED AS IS WITH NO WARRANTY OF ANY KIND, INCLUDING THE
// WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.

#include <ctl_api.h>

void SystemInit (void) __attribute__ ((section (".init")));

#if defined (__SYSTEM_STM32C0XX)
#include "system_stm32c0xx.c"
#elif defined (__SYSTEM_STM32F0XX)
#include "system_stm32f0xx.c"
#elif defined(__SYSTEM_STM32F10X)
#include "system_stm32f10x.c"
#elif defined(__SYSTEM_STM32F1XX)
#include "system_stm32f1xx.c"
#elif defined(__SYSTEM_STM32F2XX)
#include "system_stm32f2xx.c"
#elif defined(__SYSTEM_STM32F3XX)
#include "system_stm32f3xx.c"
#elif defined(__SYSTEM_STM32F4XX)
#include "system_stm32f4xx.c"
#elif defined(__SYSTEM_STM32G0XX)
#include "system_stm32g0xx.c"
#elif defined(__SYSTEM_STM32G4XX)
#include "system_stm32g4xx.c"
#elif defined(__SYSTEM_STM32F7XX)
#include "system_stm32f7xx.c"
#elif defined(__SYSTEM_STM32H5XX)
#include "system_stm32h5xx.c"
#elif defined(__SYSTEM_STM32H7XX)
#include "system_stm32h7xx.c"
#elif defined(__SYSTEM_STM32L0XX)
#include "system_stm32l0xx.c"
#elif defined(__SYSTEM_STM32L1XX)
#include "system_stm32l1xx.c"
#elif defined(__SYSTEM_STM32L4XX)
#include "system_stm32l4xx.c"
#elif defined(__SYSTEM_STM32L5XX)
#include "system_stm32l5xx.c"
#elif defined(__SYSTEM_STM32U5XX)
#include "system_stm32u5xx.c"
#elif defined(__SYSTEM_STM32W108XX)
#include "system_stm32w108xx.c"
#elif defined(__SYSTEM_STM32WBXX)
#include "system_stm32wbxx.c"
#elif defined(__SYSTEM_STM32WBAXX)
#include "system_stm32wbaxx.c"
#elif defined(__SYSTEM_STM32WLXX)
#include "system_stm32wlxx.c"
#else
#error no target
#endif

void
delay(int count)
{
  volatile int i=0;
  while (i++<count);
}

void
ctl_board_init(void)
{
  //...
}

void 
ctl_board_set_leds(unsigned on)
{
  //...
}

static CTL_ISR_FN_t userButtonISR;

void 
ctl_board_on_button_pressed(CTL_ISR_FN_t isr)
{
  userButtonISR = isr;
  // ...
}
