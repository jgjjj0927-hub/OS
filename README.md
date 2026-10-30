# ANON-OS

UEFI-загрузчик → 64-бит ядро → фреймбуфер → GUI. Веха 1+2.

## Сборка

    sudo apt-get install -y nasm xorriso mtools dosfstools \
        gcc-x86-64-elf binutils-x86-64-elf gnu-efi qemu-system-x86 ovmf
    make clean && make iso
    make check

## Что умеет

- UEFI boot, свой ELF64-загрузчик ядра
- ExitBootServices, своё владение железом
- IDT, PIC remap, PIT 100 Hz
- PS/2 клавиатура (ring buffer)
- PS/2 мышь (3-байтный пакет, IRQ12)
- Framebuffer GUI: тёмный стол, панель задач, окна с тенью,
  шестерёнкой, полупрозрачной кнопкой активации, курсор
- ISO собирается xorriso, тест в QEMU с OVMF
- GitHub Actions: сборка + smoke-тест + артефакт

## Дальше

- Веха 3: PE-загрузчик для .exe (PE32/PE32+, секции, релокации, IAT)
- Веха 4: Win32-слой (kernel32/user32/gdi32)
- Веха 5: ntdll-обёртка над своими syscall
- Веха 6: сеть (TCP/IP, TLS)
- Веха 7: Wi-Fi/BT драйверы поверх USB
