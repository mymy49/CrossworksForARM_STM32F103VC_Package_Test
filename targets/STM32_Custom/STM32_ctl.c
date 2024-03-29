/*****************************************************************************
 * Copyright (c) 2006, 2009, 2010, 2014 Rowley Associates Limited.           *
 *                                                                           *
 * This file may be distributed under the terms of the License Agreement     *
 * provided with this software.                                              *
 *                                                                           *
 * THIS FILE IS PROVIDED AS IS WITH NO WARRANTY OF ANY KIND, INCLUDING THE   *
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. *
 *
  This file implements the CTL system timer using the Cortex-M
  SysTick timer. The timer is configured to interrupt at a 10 millisecond rate and increment the CTL system timer by 10 to give a 
  millisecond timer. The implementation uses the CMSIS <b>SystemFrequency</b> global variable to determine the CPU clock
  frequency. The CTL samples that are provided in this package have board specific files that implement this variable.</p>

  <p>The CTL interrupt support functions <b>ctl_global_interrupts_set</b>, <b>ctl_set_priority</b>, <b>ctl_unmask_isr</b> 
  and <b>ctl_mask_isr</b> are implemented in this file. The Cortex-M3 and Cortex-M4 implementations uses the lowest half of 
  the available NVIC priorities (top bit set in the priority) for CTL interrupts and disables global interrupts by raising t
  he NVIC basepriority above the highest CTL priority. This enables you to use the upper half of the NVIC priorities for interrupts that do not
  use CTL and should not be disabled by a CTL interrupt. The functions <b>ctl_lowest_isr_priority</b>, <b>ctl_highest_isr_priority</b>,
  and <b>ctl_adjust_isr_priority</b> are provided to assist with setting isr priorities.</p>

 *****************************************************************************/

#include <ctl_api.h>

#if defined (__SYSTEM_STM32C0XX)
#include "stm32c0xx.h"
#elif defined (__SYSTEM_STM32F0XX)
#include "stm32f0xx.h"
#elif defined(__SYSTEM_STM32F10X)
#include "stm32f10x.h"
#elif defined(__SYSTEM_STM32F1XX)
#include "stm32f1xx.h"
#elif defined(__SYSTEM_STM32F2XX)
#include "stm32f2xx.h"
#elif defined(__SYSTEM_STM32F3XX)
#include "stm32f3xx.h"
#elif defined(__SYSTEM_STM32F4XX)
#include "stm32f4xx.h"
#elif defined(__SYSTEM_STM32G0XX)
#include "stm32g0xx.h"
#elif defined(__SYSTEM_STM32G4XX)
#include "stm32g4xx.h"
#elif defined(__SYSTEM_STM32F7XX)
#include "stm32f7xx.h"
#elif defined(__SYSTEM_STM32H5XX)
#include "stm32h5xx.h"
#elif defined(__SYSTEM_STM32H7XX)
#include "stm32h7xx.h"
#elif defined(__SYSTEM_STM32L0XX)
#include "stm32l0xx.h"
#elif defined(__SYSTEM_STM32L1XX)
#include "stm32l1xx.h"
#elif defined(__SYSTEM_STM32L4XX)
#include "stm32l4xx.h"
#elif defined(__SYSTEM_STM32L5XX)
#include "stm32l5xx.h"
#elif defined(__SYSTEM_STM32U5XX)
#include "stm32u5xx.h"
#elif defined (__SYSTEM_STM32W108XX)
#include "stm32w108xx.h"
#elif defined(__SYSTEM_STM32WBXX)
#include "stm32wbxx.h"
#elif defined(__SYSTEM_STM32WBAXX)
#include "stm32wbaxx.h"
#elif defined(__SYSTEM_STM32WLXX)
#include "stm32wlxx.h"
#else
#error no target
#endif

extern uint32_t SystemCoreClock;
#if defined (STM32W108C8) || defined (STM32W108CB) || defined (STM32W108CC) || defined (STM32W108CZ) || defined (STM32W108HB) || defined(STM32H743xx) || defined(STM32H753xx)
#define USEPROCESSORCLOCK
#endif

#ifdef USEPROCESSORCLOCK
#define SYSTICKDIVIDER 1
#else
#define SYSTICKDIVIDER 8
#endif

#define ONE_MS (SystemCoreClock/SYSTICKDIVIDER/1000)
#define TEN_MS (ONE_MS*10)

static CTL_ISR_FN_t userTimerISR;

void
SysTick_Handler()
{ 
  ctl_enter_isr();
  if (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk)
    {
#ifdef CTL_TASKING
      ctl_time_increment = (SysTick->LOAD+1)/ONE_MS;     
#endif
      userTimerISR();
    }
  ctl_exit_isr();
}

int 
ctl_set_isr(unsigned int irq, 
            unsigned int priority,                 
            CTL_ISR_FN_t isr, 
            CTL_ISR_FN_t *oldisr)
{
#if defined(__ARM_ARCH_6M__) // No SCB->VTOR requires SRAM to be mapped to address zero
 if (oldisr)
    *oldisr = *((CTL_ISR_FN_t*)(0x0)+16+irq);
  *((CTL_ISR_FN_t*)(0x0)+16+irq) = isr;
#else
  if (oldisr)
    *oldisr = *((CTL_ISR_FN_t*)(SCB->VTOR)+16+irq);
  *((CTL_ISR_FN_t*)(SCB->VTOR)+16+irq) = isr;
#endif
  ctl_set_priority(irq, priority);
  return 1;
}

void
ctl_set_priority(int irq, int priority)
{
#if defined(CTL_TASKING) && !defined(__ARM_ARCH_6M__)
  NVIC_SetPriority(irq, (1 << (__NVIC_PRIO_BITS - 1)) + priority);
#else
  NVIC_SetPriority(irq, priority);
#endif
}

int
ctl_unmask_isr(unsigned int irq)
{
  NVIC_EnableIRQ(irq);
  return 1;
}

int
ctl_mask_isr(unsigned int irq)
{
  NVIC_DisableIRQ(irq);
  return 1;
}

int ctl_lowest_isr_priority(void)
{
#if defined(CTL_TASKING) && !defined(__ARM_ARCH_6M__)
  return ((1 << (__NVIC_PRIO_BITS - 1)) - 1);
#else
  return ((1 << __NVIC_PRIO_BITS) - 1);
#endif
}

int ctl_highest_isr_priority(void)
{
  return 0;
}

int ctl_adjust_isr_priority(int priority, int n)
{
  return priority - n;
}

void ctl_start_timer(CTL_ISR_FN_t timerFn)
{  
  userTimerISR = timerFn;    
  SysTick->LOAD = TEN_MS-1;
  SysTick->VAL = 0;
#ifdef USEPROCESSORCLOCK
  SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;
#else
  SysTick->CTRL = SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;
#endif
#ifdef CTL_TASKING  
  // Set PendSV priority (PendSV must have lowest priority)
  ctl_set_priority(PendSV_IRQn, ctl_lowest_isr_priority());
#endif
  // Set SysTick priority
  ctl_set_priority(SysTick_IRQn, ctl_highest_isr_priority());
}

unsigned long
ctl_get_ticks_per_second(void)
{
#ifdef CTL_TASKING
  return 1000;
#else
  return 100;
#endif
}

#ifdef CTL_TASKING
void ctl_sleep(unsigned delay)
{
  CTL_TIME_t sleep_time;
  unsigned sleep_priority;
  if (delay > 10)
    {
      ctl_global_interrupts_disable();
      sleep_time = ctl_current_time;      
      SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;
      if ((ONE_MS * delay) > 0x00FFFFFF || (ONE_MS * delay) < ONE_MS)
        SysTick->LOAD = 0x00FFFFFF;
      else
        SysTick->LOAD = (ONE_MS * delay)-1;
      SysTick->VAL = 0;
      SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;
      ctl_interrupt_count = 1;
      sleep_priority = ctl_task_set_priority(ctl_task_executing, 255);
      ctl_interrupt_count = 0;
      ctl_reschedule_on_last_isr_exit = 0;
      ctl_global_interrupts_enable();
    }
  __asm("wfi");
  if (delay > 10)
    {
      ctl_global_interrupts_disable();
      if (ctl_current_time == sleep_time) // something else woke us up and didn't update the time
        {
          if (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk)
            ctl_time_increment = (SysTick->LOAD+1)/ONE_MS;
          else
            ctl_time_increment = (SysTick->LOAD+1-SysTick->VAL)/ONE_MS;
          ctl_increment_tick_from_isr();
          ctl_reschedule_on_last_isr_exit = 0;
        }
      SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;      
      SysTick->LOAD = TEN_MS-1;
      SysTick->VAL = 0;
      SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;
      ctl_global_interrupts_enable();
      ctl_task_set_priority(ctl_task_executing, sleep_priority);
    }
}

void ctl_woken()
{  
  if (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk)
    ctl_time_increment = (SysTick->LOAD+1)/ONE_MS;
  else
    ctl_time_increment = (SysTick->LOAD+1-SysTick->VAL)/ONE_MS;  
  ctl_increment_tick_from_isr();
}
#endif
