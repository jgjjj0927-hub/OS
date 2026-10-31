# language: Makefile, file: Makefile, target: anon-os x86_64 UEFI
CC        := x86_64-elf-gcc
LD        := x86_64-elf-ld
NASM      := nasm
OBJCOPY   := objcopy
XORRISO   := xorriso
QEMU      := qemu-system-x86_64

KCFLAGS   := -ffreestanding -fno-stack-protector -mno-red-zone \
             -mcmodel=kernel -Wall -Wextra -O2 -std=c11 \
             -fno-pic -fno-pie -c
KLDFLAGS  := -T kernel/kernel.ld -nostdlib -static

EFI_CFLAGS := -ffreestanding -fno-stack-protector -mno-red-zone \
              -fshort-wchar -fno-pic -fno-pie -DEFI_FUNCTION_WRAPPER \
              -I/usr/include/efi -I/usr/include/efi/x86_64 \
              -Wall -Wextra -O2 -c
EFI_CRT0   := /usr/lib/crt0-efi-x86_64.o
EFI_LDS    := /usr/lib/elf_x86_64_efi.lds
EFI_LIBS   := -lefi -lgnuefi

BUILD     := build
ISO_DIR   := $(BUILD)/iso
KOBJS     := $(BUILD)/kmain.o $(BUILD)/input.o $(BUILD)/isr.o
KERNEL    := $(BUILD)/kernel.elf
LOADER    := $(BUILD)/bootx64.efi
ISO       := $(BUILD)/anon-os.iso

.PHONY: all kernel loader iso check clean help

all: iso

help:
	@echo "make kernel  — kernel.elf"
	@echo "make loader  — bootx64.efi"
	@echo "make iso     — anon-os.iso"
	@echo "make check   — QEMU 10s"
	@echo "make clean   — rm -rf build/"

$(BUILD):
	@mkdir -p $(BUILD)
$(ISO_DIR)/EFI/BOOT:
	@mkdir -p $@
$(ISO_DIR)/boot:
	@mkdir -p $@

kernel: $(KERNEL)
$(KERNEL): $(KOBJS) kernel/kernel.ld | $(BUILD)
	$(LD) $(KLDFLAGS) -o $@ $(KOBJS)

$(BUILD)/%.o: kernel/%.c kernel/input.h | $(BUILD)
	$(CC) $(KCFLAGS) -Ikernel $< -o $@
$(BUILD)/isr.o: kernel/isr.asm | $(BUILD)
	$(NASM) -f elf64 $< -o $@

loader: $(LOADER)
$(LOADER): boot/loader.c | $(BUILD)
	$(CC) $(EFI_CFLAGS) $< -o $(BUILD)/boot.obj
	$(LD) -nostdlib -znocombreloc -T $(EFI_LDS) -shared -Bsymbolic \
	      -o $(BUILD)/boot.so $(EFI_CRT0) $(BUILD)/boot.obj $(EFI_LIBS)
	$(OBJCOPY) -j .text -j .sdata -j .data -j .dynamic -j .dynsym \
	           -j .rel -j .rela -j .reloc --target=efi-app-x86_64 \
	           $(BUILD)/boot.so $@

iso: $(ISO)
$(ISO): $(KERNEL) $(LOADER) | $(ISO_DIR)/EFI/BOOT $(ISO_DIR)/boot
	cp $(LOADER) $(ISO_DIR)/EFI/BOOT/BOOTX64.EFI
	cp $(KERNEL) $(ISO_DIR)/boot/kernel.elf
	$(XORRISO) -as mkisofs -R -r -J \
	    -e EFI/BOOT/BOOTX64.EFI \
	    -no-emul-boot -append_partition 2 0xef $(ISO_DIR)/EFI/BOOT/BOOTX64.EFI \
	    -o $@ $(ISO_DIR)
	@echo "ISO ready: $@  ($$(du -h $@ | cut -f1))"

OVMF ?= /usr/share/OVMF/OVMF_CODE.fd
check: $(ISO)
	timeout 10 $(QEMU) -machine q35 -m 512 -smp 2 \
	    -bios $(OVMF) \
	    -drive file=$(ISO),format=raw,media=cdrom,readonly=on \
	    -display none -serial stdio -no-reboot || true

clean:
	rm -rf $(BUILD)
