/******************************************************************************
  Target Script for STM32H7

  Copyright (c) 2019 Rowley Associates Limited.

  This file may be distributed under the terms of the License Agreement
  provided with this software.

  THIS FILE IS PROVIDED AS IS WITH NO WARRANTY OF ANY KIND, INCLUDING THE
  WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 ******************************************************************************/

var DBGBASE = TargetInterface.implementation()=="j-link" ? 0x5C000000 : 0xE00E0000;
var DBGMCU_IDC = DBGBASE + 0x1000;
var DBGMCU_CR = DBGBASE + 0x1004;
var DBGMCU_APB3FZ1 = DBGBASE + 0x1034;
var DBGMCU_APB3FZ2 = DBGBASE + 0x1038;
var SWO_BASE = DBGBASE + 0x3000;
var SWTF_BASE = DBGBASE + 0x4000;
var TSG_BASE = DBGBASE + 0x5000;
var CTI_BASE = DBGBASE + 0x11000;
var CSTF_BASE = DBGBASE + 0x13000;
var TMC_BASE = DBGBASE + 0x14000;
var TPIU_BASE = DBGBASE + 0x15000;

function EnableTrace(TraceInterfaceType)
{
  var CPUID = TargetInterface.peekWord(0xE000ED00);
  if (((CPUID>>4)&0xf)==4)
    return; // not supported for Cortex-M4
  if (TraceInterfaceType == "TracePort")
    {        
    }
  else if (TraceInterfaceType == "SWO")
    {
      var v = TargetInterface.peekWord(SWTF_BASE); // SWTF
      TargetInterface.pokeWord(SWTF_BASE, v|0x1); // Enable slave
      // Enable P3.3 as SWO
      TargetInterface.pokeWord(0x580244E0, TargetInterface.peekWord(0x580244E0) | 0x2);
      TargetInterface.pokeWord(0x58020400, (TargetInterface.peekWord(0x58020400) & 0xffffff3f) | 0x00000080);
      TargetInterface.pokeWord(0x58020408, TargetInterface.peekWord(0x58020408) | 0x00000080);
      TargetInterface.pokeWord(0x58020420, TargetInterface.peekWord(0x58020420) & 0xFFFF0FFF);
    }
  else if (TraceInterfaceType == "ETB")
    {
      var v = TargetInterface.peekWord(CSTF_BASE); // CSTF
      TargetInterface.pokeWord(CSTF_BASE, v|0x3); // Enable slaves
      TargetInterface.pokeWord(TSG_BASE+0x20, 1000000); // TS Frequency
      TargetInterface.pokeWord(TSG_BASE, 0x3); // Enable TSG
    }
}

function Reset()
{   
  var CPUID = TargetInterface.peekWord(0xE000ED00);
  TargetInterface.pokeWord(0xE000EDF0, 0xA05F0003); // set C_HALT and C_DEBUGEN in DHCSR      
  TargetInterface.waitForDebugState(1000);
  TargetInterface.pokeWord(0xE000EDFC, (0x1<<24)|0x1); // set TRCENA | VC_CORERESET in DEMCR
  if (((CPUID>>4)&0xf)==7 && (TargetInterface.peekWord(0x52002000+0x1c) & (1<<22))) // BOOT_CM4
    {
      TargetInterface.setDebugInterfaceProperty("set_adiv5_AHB_ap_num", 3);
      TargetInterface.pokeWord(0xE000EDFC, (0x1<<24)|0x1); // set TRCENA | VC_CORERESET in DEMCR
      TargetInterface.setDebugInterfaceProperty("set_adiv5_AHB_ap_num", 0);
    }
  else if (((CPUID>>4)&0xf)==4 && (TargetInterface.peekWord(0x52002000+0x1c) & (1<<23))) // BOOT_CM7
    {
      TargetInterface.setDebugInterfaceProperty("set_adiv5_AHB_ap_num", 0);
      TargetInterface.pokeWord(0xE000EDFC, (0x1<<24)|0x1); // set TRCENA | VC_CORERESET in DEMCR
      TargetInterface.setDebugInterfaceProperty("set_adiv5_AHB_ap_num", 3);
    }
  TargetInterface.pokeWord(0xE000ED0C, 0x05FA0004); // set SYSRESETREQ in AIRCR
  TargetInterface.waitForDebugState(1000);  
  TargetInterface.pokeWord(DBGMCU_CR, 7<<20|1<<2|1<<1|1<<0); // enable low-power mode debugging
  TargetInterface.pokeWord(DBGMCU_APB3FZ1, 1<<6); // turn off WWDG1 in debug mode
  if (TargetInterface.peekWord(0x52002000+0x1c) & (1<<22))
    TargetInterface.pokeWord(DBGMCU_APB3FZ2, 1<<6); 
}

function Reset2()
{ 
  TargetInterface.setDebugInterfaceProperty("set_adiv5_AHB_ap_num", 0, 0, 0);
  TargetInterface.stop(100); // STOP CM7
  TargetInterface.setDebugInterfaceProperty("set_adiv5_AHB_ap_num", 3, 0, 0);
  TargetInterface.stop(100); // STOP CM4
  TargetInterface.pokeWord(0xE000EDFC, (0x1<<24)|0x1); // set TRCENA | VC_CORERESET in DEMCR
  TargetInterface.pokeWord(0xE000ED0C, 0x05FA0003); // set VECTRESET in AIRCR
  TargetInterface.waitForDebugState(1000); 
}

function SRAMReset()
{
  if (TargetInterface.implementation() == "j-link" || TargetInterface.implementation() == "crossworks_simulator")
    TargetInterface.resetAndStop(100);
  else
    Reset();
}

function FLASHReset()
{
  if (TargetInterface.implementation() == "j-link" || TargetInterface.implementation() == "crossworks_simulator")
    TargetInterface.resetAndStop(100);
  else
    Reset();
}

function GetPartName()
{ 
  if (DBGBASE==0xE00E0000)
    {
      TargetInterface.setDebugInterfaceProperty("use_adiv5_APB", 0xE00E0000, 0x20000);
    } 
  var IDC = TargetInterface.peekWord(DBGMCU_IDC);
  switch (IDC & 0xfff)
    {
      case 0x450:
      case 0x480:
      case 0x483:
        {
          TargetInterface.setDebugInterfaceProperty("component_base", 0xe000e000); // SCS
          TargetInterface.setDebugInterfaceProperty("component_base", 0xe0001000); // DWT
          TargetInterface.setDebugInterfaceProperty("component_base", 0xe0002000); // FPB
          TargetInterface.setDebugInterfaceProperty("component_base", 0xe0000000); // ITM
          TargetInterface.setDebugInterfaceProperty("component_base", 0xe0041000); // ETM
          TargetInterface.setDebugInterfaceProperty("component_base", 0xe0043000); // CTI
          TargetInterface.pokeWord(DBGMCU_CR, 7<<20); // Enable D3DBG & D1DBG & TRACECLK
          TargetInterface.setDebugInterfaceProperty("component_base", SWO_BASE); // SWO
          TargetInterface.setDebugInterfaceProperty("component_base", SWTF_BASE); // SWTF
          TargetInterface.setDebugInterfaceProperty("component_base", TSG_BASE); // TSG
          TargetInterface.setDebugInterfaceProperty("component_base", CTI_BASE); // CTI
          TargetInterface.setDebugInterfaceProperty("component_base", CSTF_BASE); // CSTF
          TargetInterface.setDebugInterfaceProperty("component_base", TMC_BASE); // TMC
          TargetInterface.setDebugInterfaceProperty("component_base", TPIU_BASE); // TPIU
          var CPUID = TargetInterface.peekWord(0xE000ED00);
          if (((CPUID>>4)&0xf)==7) // Cortex-M7
            {
              if ((IDC & 0xfff)==0x480)
                return "STM32H7A3/STM32H7B3/STM32H7B0";
              else if ((IDC & 0xfff)==0x483)
                return "STM32H72/STM32H73";
              else
                return "STM32H7";
            }
          else if (((CPUID>>4)&0xf)==4) // Cortex-M4
            return "STM32H7_CM4";
        }
      default:
        TargetInterface.message("Unsupported DBGMCU_IDC "+IDC.toString(16));
    }
}

function MatchPartName(name)
{
  var family = name.substring(0, 9);
  switch ((TargetInterface.peekWord(DBGMCU_IDC) & 0xfff))
    {
      case 0x450:
        return (family.indexOf("STM32H74")==0 || family.indexOf("STM32H75")==0);
      case 0x480:
        return (family.indexOf("STM32H7A")==0 || family.indexOf("STM32H7B")==0);
      case 0x483:
        return (family.indexOf("STM32H72")==0 || family.indexOf("STM32H73")==0);
    }
  return false;
}
