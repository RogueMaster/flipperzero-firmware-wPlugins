ENTRY(main)
MEMORY { FLASH(rx) : ORIGIN = 0x08000000, LENGTH = 1M
         RAM(rwx) : ORIGIN = 0x20000000, LENGTH = 1M }
SECTIONS {
 .text : { *(.text*) *(.rodata*) } > FLASH
 .ARM.exidx : { *(.ARM.exidx*) } > FLASH
 .data : { *(.data*) } > RAM
 .bss : { *(.bss*) *(COMMON) } > RAM
 . = ALIGN(8); end = .; _end = .;
}
