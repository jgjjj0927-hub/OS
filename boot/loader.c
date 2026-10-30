// language: C, file: boot/loader.c, target: UEFI x86_64 via gnu-efi
// собирается gnu-efi → PE32+ EFI-app
#include <efi.h>
#include <efilib.h>

extern void kmain(UINT32 *fb, UINT32 w, UINT32 h, UINT32 pitch);

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ih, EFI_SYSTEM_TABLE *ST){
    InitializeLib(ih, ST);

    /* ── 1. GOP: framebuffer ─────────────────────────────── */
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
    EFI_GUID gopg = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    if (uefi_call_wrapper(ST->BootServices->LocateProtocol, 3,
            &gopg, NULL, (void**)&gop) != EFI_SUCCESS || !gop)
        return EFI_LOAD_ERROR;

    /* выбрать 1024x768, иначе последний доступный */
    UINTN best = gop->Mode->Mode;
    for (UINT32 m = 0; m < gop->Mode->MaxMode; m++){
        UINTN sz; EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
        if (uefi_call_wrapper(gop->QueryMode, 4, gop, m, &sz, &info) != EFI_SUCCESS) continue;
        if (info->HorizontalResolution == 1024 && info->VerticalResolution == 768){ best = m; break; }
    }
    uefi_call_wrapper(gop->SetMode, 2, gop, best);

    UINT32 *fb   = (UINT32*)gop->Mode->FrameBufferBase;
    UINT32  w    = gop->Mode->Info->HorizontalResolution;
    UINT32  h    = gop->Mode->Info->VerticalResolution;
    UINT32  pitch= gop->Mode->Info->PixelsPerScanLine * 4;

    /* ── 2. EFI-раздел, читаем kernel.elf ────────────────── */
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = NULL;
    EFI_GUID fsg = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    if (uefi_call_wrapper(ST->BootServices->LocateProtocol, 3,
            &fsg, NULL, (void**)&fs) != EFI_SUCCESS || !fs)
        return EFI_LOAD_ERROR;

    EFI_FILE *root = NULL;
    if (uefi_call_wrapper(fs->OpenVolume, 2, fs, &root) != EFI_SUCCESS)
        return EFI_LOAD_ERROR;

    EFI_FILE *kf = NULL;
    if (uefi_call_wrapper(root->Open, 5, root, &kf, L"kernel.elf",
            EFI_FILE_MODE_READ, 0) != EFI_SUCCESS)
        return EFI_LOAD_ERROR;

    UINT8 *kbuf = NULL;
    if (uefi_call_wrapper(ST->BootServices->AllocatePool, 3,
            EfiLoaderData, 4*1024*1024, (void**)&kbuf) != EFI_SUCCESS)
        return EFI_OUT_OF_RESOURCES;

    UINTN ksz = 4*1024*1024;
    if (uefi_call_wrapper(kf->Read, 3, kf, &ksz, kbuf) != EFI_SUCCESS || ksz < 64)
        return EFI_LOAD_ERROR;
    if (!(kbuf[0]==0x7F && kbuf[1]=='E' && kbuf[2]=='L' && kbuf[3]=='F'))
        return EFI_LOAD_ERROR;

    /* ── 3. ELF64: раскладываем PT_LOAD сегменты ─────────── */
    UINT64 entry     = *(UINT64*)(kbuf+24);
    UINT64 phoff     = *(UINT64*)(kbuf+32);
    UINT16 phentsize = *(UINT16*)(kbuf+54);
    UINT16 phnum     = *(UINT16*)(kbuf+56);

    for (UINT16 i = 0; i < phnum; i++){
        UINT8 *ph = kbuf + phoff + i*phentsize;
        if (*(UINT32*)(ph+0) != 1) continue;              /* PT_LOAD */
        UINT64 off  = *(UINT64*)(ph+8);
        UINT64 va   = *(UINT64*)(ph+16);
        UINT64 fsz  = *(UINT64*)(ph+32);
        UINT64 msz  = *(UINT64*)(ph+40);
        if (msz < fsz) msz = fsz;
        if (off + fsz > ksz) return EFI_LOAD_ERROR;
        for (UINT64 k = 0; k < fsz; k++) ((UINT8*)va)[k] = kbuf[off+k];
        for (UINT64 k = fsz; k < msz; k++) ((UINT8*)va)[k] = 0;
    }

    /* ── 4. ExitBootServices: прошивка отдаёт железо ─────── */
    UINTN mm_size = 0, mm_key = 0, desc_size = 0;
    UINT32 desc_ver = 0;
    uefi_call_wrapper(ST->BootServices->GetMemoryMap, 5,
        &mm_size, NULL, &mm_key, &desc_size, &desc_ver);
    mm_size += 2 * desc_size;

    EFI_MEMORY_DESCRIPTOR *mm = NULL;
    uefi_call_wrapper(ST->BootServices->AllocatePool, 3,
        EfiLoaderData, mm_size, (void**)&mm);

    for (int tries = 0; tries < 4; tries++){
        uefi_call_wrapper(ST->BootServices->GetMemoryMap, 5,
            &mm_size, mm, &mm_key, &desc_size, &desc_ver);
        if (uefi_call_wrapper(ST->BootServices->ExitBootServices, 2,
                ih, mm_key) == EFI_SUCCESS) break;
        mm_size += 2 * desc_size;
    }

    /* ── 5. Прыжок в ядро. ST/BS мертвы, не трогать. ─────── */
    ((void(*)(UINT32*,UINT32,UINT32,UINT32))entry)(fb, w, h, pitch);

    for(;;) __asm__ volatile("hlt");
    return EFI_SUCCESS;
}
