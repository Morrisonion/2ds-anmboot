OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)
MEMORY { RAM (rwx) : ORIGIN = 0x1FFB0000, LENGTH = 0x0000E000 }
SECTIONS {
  .text : ALIGN(16) { KEEP(*(.text.start)) *(.text*) *(.rodata*) *(.firmstub*) . = ALIGN(16); } > RAM
  .data : ALIGN(16) { *(.data*) . = ALIGN(16); } > RAM
  .bss (NOLOAD) : ALIGN(16) { __bss_start__ = .; *(.bss*) *(COMMON) . = ALIGN(16); __bss_end__ = .; } > RAM
  __end__ = .;
}
