MEMORY
{
  UNPLACED_SECTIONS (wx) : ORIGIN = 0x100000000, LENGTH = 0
  RAM (wx) : ORIGIN = 0x20000000, LENGTH = 0x00001000
  FLASH (wx) : ORIGIN = 0x40022000, LENGTH = 0x00000400
}

SECTIONS
{
  __RAM_segment_start__ = 0x20000000;
  __RAM_segment_end__ = 0x20001000;
  __RAM_segment_size__ = 0x00001000;
  __FLASH_segment_start__ = 0x40022000;
  __FLASH_segment_end__ = 0x40022400;
  __FLASH_segment_size__ = 0x00000400;

  __HEAPSIZE__ = 1024;
  __STACKSIZE_ABT__ = 0;
  __STACKSIZE_FIQ__ = 0;
  __STACKSIZE_IRQ__ = 0;
  __STACKSIZE_PROCESS__ = 0;
  __STACKSIZE_SVC__ = 0;
  __STACKSIZE_UND__ = 0;
  __STACKSIZE__ = 256;
  __do_debug_operation = __do_debug_operation_mempoll;
  __vfprintf = __vfprintf_float_long_long;
  __vfscanf = __vfscanf_float_long_long_cc;

  __load_section_load_start__ = __RAM_segment_start__;
  .load_section __RAM_segment_start__ : AT(__RAM_segment_start__)
  {
    __load_section_start__ = .;
    *(.text) *(.text.*) *(.rodata) *(.rodata.*) *(.data) *(.data.*) *(.glue_7t) *(.glue_7) *(.gnu.linkonce.t.*)
  }
  __load_section_end__ = __load_section_start__ + SIZEOF(.load_section);
  __load_section_size__ = SIZEOF(.load_section);
  __load_section_load_end__ = __load_section_end__;

  . = ASSERT(__load_section_start__ == __load_section_end__ || (__load_section_end__ - __RAM_segment_start__) <= __RAM_segment_size__ , "error: .load_section is too large to fit in RAM memory segment");

  __bss_load_start__ = ALIGN(__load_section_end__ , 4);
  .bss ALIGN(__load_section_end__ , 4) (NOLOAD) : AT(ALIGN(__load_section_end__ , 4))
  {
    __bss_start__ = .;
    *(.bss .bss.* .gnu.linkonce.b.*) *(COMMON)
  }
  __bss_end__ = __bss_start__ + SIZEOF(.bss);
  __bss_size__ = SIZEOF(.bss);
  __bss_load_end__ = __bss_end__;

  . = ASSERT(__bss_start__ == __bss_end__ || (__bss_end__ - __RAM_segment_start__) <= __RAM_segment_size__ , "error: .bss is too large to fit in RAM memory segment");

  __stack_load_start__ = ALIGN(__bss_end__ , 4);
  .stack ALIGN(__bss_end__ , 4) (NOLOAD) : AT(ALIGN(__bss_end__ , 4))
  {
    __stack_start__ = .;
    *(.stack .stack.*)
    . = ALIGN(MAX(__stack_start__ + __STACKSIZE__ , .), 4);
  }
  __stack_end__ = __stack_start__ + SIZEOF(.stack);
  __stack_size__ = SIZEOF(.stack);
  __stack_load_end__ = __stack_end__;

  __RAM_segment_used_end__ = ALIGN(__bss_end__ , 4) + SIZEOF(.stack);
  __RAM_segment_used_size__ = __RAM_segment_used_end__ - __RAM_segment_start__;

  . = ASSERT(__stack_start__ == __stack_end__ || (__stack_end__ - __RAM_segment_start__) <= __RAM_segment_size__ , "error: .stack is too large to fit in RAM memory segment");

  . = ASSERT(__STACKSIZE__  == ALIGN(__STACKSIZE__, 4), "error: .stack section size does not match alignment");

  . = ASSERT(__stack_size__ <= __STACKSIZE__ , "error: .stack section is larger than specified size");

}

