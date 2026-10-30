/* language: LD, file: boot/loader.ld, target: PE32+ UEFI app */
OUTPUT_FORMAT("elf64-x86-64")
ENTRY(efi_main)
SECTIONS {
    . = 0;
    .text   : { *(.text*)   }
    .rodata : { *(.rodata*) }
    .data   : { *(.data*)   }
    .bss    : { *(COMMON) *(.bss*) }
    /DISCARD/ : { *(.eh_frame) *(.note*) }
}
