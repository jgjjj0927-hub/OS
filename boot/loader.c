// language: C, file: boot/loader.c, target: UEFI x86_64, no gnu-efi libs
#include <efi.h>

extern void kmain(UINT32 *fb, UINT32 w, UINT32 h, UINT32 pitch);

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ih, EFI_SYSTEM_TABLE *ST){
    EFI_BOOT_SERVICES *BS = ST->BootServices;

    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
    EFI_GUID gopg = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    if (BS->LocateProtocol(&gopg, NULL, (void**)&gop) != EFI_SUCCESS || !gop)
        return EFI_LOAD_ERROR;

    UINTN best = gop->Mode->Mode;
    for (UINT32 m = 0; m < gop->Mode->MaxMode; m++){
        UINTN sz; EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
        if (gop->QueryMode(gop, m, &sz, &info) != EFI_SUCCESS) continue;
        if (info->HorizontalResolution == 1024 && info->VerticalResolution == 768){ best = m; break; }
    }
    gop->SetMode(gop, best);

    UINT32 *fb    = (UINT32*)gop->Mode->FrameBufferBase;
    UINT32  w     = gop->Mode->Info->HorizontalResolution;
    UINT32  h     = gop->Mode->Info->VerticalResolution;
    UINT32  pitch = gop->Mode->Info->PixelsPerScanLine * 4;

    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = NULL;
    EFI_GUID fsg = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    if (BS->LocateProtocol(&fsg, NULL, (void**)&fs) != EFI_SUCCESS || !fs)
        return EFI_LOAD_ERROR;

    EFI_FILE *root = NULL;
    if (fs->OpenVolume(fs, &root) != EFI_SUCCESS)
        return EFI_LOAD_ERROR;

    EFI_FILE *kf = NULL;
    if (root->Open(root, &kf, L"kernel.elf", EFI_FILE_MODE_READ, 0) != EFI_SUCCESS)
        return EFI_LOAD_ERROR;

    UINT8 *kbuf = NULL;
    if (BS->AllocatePool(EfiLoaderData, 4*1024*1024, (void**)&kbuf) != EFI_SUCCESS)
        return EFI_OUT_OF_RESOURCES;

    UINTN ksz = 4*1024*1024;
    if (kf->Read(kf, &ksz, kbuf) != EFI_SUCCESS || ksz < 64)
        return EFI_LOAD_ERROR;
    if (!(kbuf[0]==0x7F && kbuf[1]=='E' && kbuf[2]=='L' && kbuf[3]=='F'))
        return EFI_LOAD_ERROR;

    UINT64 entry     = *(UINT64*)(kbuf+24);
    UINT64 phoff     = *(UINT64*)(kbuf+32);
    UINT16 phentsize = *(UINT16*)(kbuf+54);
    UINT16 phnum     = *(UINT16*)(kbuf+56);

    for (UINT16 i = 0; i < phnum; i++){
        UINT8 *ph = kbuf + phoff + i*phentsize;
        if (*(UINT32*)(ph+0) != 1) continue;
        UINT64 off = *(UINT64*)(ph+8);
        UINT64 va  = *(UINT64*)(ph+16);
        UINT64 fsz = *(UINT64*)(ph+32);
        UINT64 msz = *(UINT64*)(ph+40);
        if (msz < fsz) msz = fsz;
        if (off + fsz > ksz) return EFI_LOAD_ERROR;
        for (UINT64 k = 0; k < fsz; k++) ((UINT8*)va)[k] = kbuf[off+k];
        for (UINT64 k = fsz; k < msz; k++) ((UINT8*)va)[k] = 0;
    }

    UINTN mm_size = 0, mm_key = 0, desc_size = 0;
    UINT32 desc_ver = 0;
    BS->GetMemoryMap(&mm_size, NULL, &mm_key, &desc_size, &desc_ver);
    mm_size += 2 * desc_size;

    EFI_MEMORY_DESCRIPTOR *mm = NULL;
    BS->AllocatePool(EfiLoaderData, mm_size, (void**)&mm);

    for (int tries = 0; tries < 4; tries++){
        BS->GetMemoryMap(&mm_size, mm, &mm_key, &desc_size, &desc_ver);
        if (BS->ExitBootServices(ih, mm_key) == EFI_SUCCESS) break;
        mm_size += 2 * desc_size;
    }

    ((void(*)(UINT32*,UINT32,UINT32,UINT32))entry)(fb, w, h, pitch);

    for(;;) __asm__ volatile("hlt");
    return EFI_SUCCESS;
}
