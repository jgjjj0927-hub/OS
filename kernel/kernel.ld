/* language: LD, file: kernel/kernel.ld, target: x86_64 kernel ELF */
OUTPUT_FORMAT("elf64-x86-64")
ENTRY(kmain)
SECTIONS {
    . = 0x100000;
    .text   : { *(.text*)   }
    .rodata : { *(.rodata*) }
    .data   : { *(.data*)   }
    .bss    : { *(COMMON) *(.bss*) }
    /DISCARD/ : { *(.eh_frame) *(.note*) *(.comment) }
}
