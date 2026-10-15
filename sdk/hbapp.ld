ENTRY(payload_entry)
SECTIONS {
 . = 0;
 .text : { *(.text.entry) *(.text*) }
 .rodata : { *(.rodata*) . = ALIGN(4); }
 .ARM.extab : { *(.ARM.extab*) }
 .ARM.exidx : { __exidx_start = .; *(.ARM.exidx*) __exidx_end = .; }
 /* SDK packer copies only PROGBITS. Fold constructor arrays into .data. */
 .data : {
  *(.data*) . = ALIGN(4);
  __init_array_start = .; KEEP(*(.init_array*)) __init_array_end = .;
  __fini_array_start = .; KEEP(*(.fini_array*)) __fini_array_end = .;
 }
 . = ALIGN(8);
 __bss_start__ = .;
 .bss (NOLOAD) : { *(.bss*) *(COMMON) . = ALIGN(8); }
 __bss_end__ = .;
 /DISCARD/ : { *(.comment) *(.note*) }
}
