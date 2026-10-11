ENTRY(_start)
SECTIONS
{
  . = 2M;

  .text BLOCK(4K) : ALIGN(4K)
  {
    *(.multiboot)
    *(.text .text.*)
  }

  .rodata BLOCK(4K) : ALIGN(4K)
  {
    *(.rodata .rodata.*)
    *(.userblob)
  }

  .data BLOCK(4K) : ALIGN(4K)
  {
    *(.data .data.*)
  }
  PROVIDE(_kernel_load_end = .);

  .bss BLOCK(4K) : ALIGN(4K)
  {
    *(COMMON)
    *(.bss .bss.*)
  }

  . = ALIGN(4096);
  PROVIDE(_kernel_end = .);

  /DISCARD/ : { *(.eh_frame) *(.comment) *(.note*) }
}
