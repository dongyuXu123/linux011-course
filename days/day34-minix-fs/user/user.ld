ENTRY(_start)
SECTIONS
{
  . = 0x0;
  .text   : { KEEP(*(.text.start)) *(.text .text.*) }
  .rodata : { *(.rodata .rodata.*) }
  .data   : { *(.data .data.*) *(COMMON) }
  .bss    : { *(.bss .bss.*) }
  /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame) *(.dynamic) *(.plt) *(.got*) }
}
