// language: C, file: boot/bootx64.efi.c, target: UEFI x86_64 (PE32+)
// минимальный UEFI app: Grabs GOP framebuffer, загружает kernel.elf из того же раздела,
// передаёт управление в kmain(fb_base, fb_w, fb_h, fb_pitch)

#include <stdint.h>
#include <stddef.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#define EFIAPI __attribute__((ms_abi))
#define EFI_SUCCESS 0

typedef struct { u32 Data1; u16 Data2; u16 Data3; u8 Data4[8]; } EFI_GUID;
typedef u64 EFI_STATUS;
typedef u64 EFI_PHYSICAL_ADDRESS;
typedef u64 EFI_VIRTUAL_ADDRESS;

typedef struct {
    u32 Type; u32 Pad; EFI_PHYSICAL_ADDRESS PhysStart; EFI_VIRTUAL_ADDRESS VirtStart;
    u64 NumPages; u64 Attribute;
} EFI_MEMORY_DESCRIPTOR;

typedef struct {
    u8 Pad1[24]; u64 Signature; u32 Revision; u32 HeaderSize; u32 CRC32; u32 Reserved;
} EFI_TABLE_HEADER;

typedef struct { EFI_TABLE_HEADER Hdr; } EFI_SIMPLE_TEXT_OUTPUT;

typedef struct {
    EFI_TABLE_HEADER Hdr;
    EFI_STATUS (EFIAPI *RaiseTPL)(u64);
    void (EFIAPI *RestoreTPL)(u64);
    EFI_STATUS (EFIAPI *AllocatePages)(u32, u32, u64, EFI_PHYSICAL_ADDRESS*);
    EFI_STATUS (EFIAPI *FreePages)(EFI_PHYSICAL_ADDRESS, u64);
    EFI_STATUS (EFIAPI *GetMemoryMap)(u64*, EFI_MEMORY_DESCRIPTOR*, u64*, u64*, u32*);
    EFI_STATUS (EFIAPI *AllocatePool)(u32, u64, void**);
    EFI_STATUS (EFIAPI *FreePool)(void*);
} EFI_BOOT_SERVICES;

typedef struct {
    EFI_TABLE_HEADER Hdr;
    void *FirmwareVendor; u32 FirmwareRevision;
    void *ConsoleInHandle; void *ConIn;
    void *ConsoleOutHandle; EFI_SIMPLE_TEXT_OUTPUT *ConOut;
    void *StdErrHandle; void *StdErr;
    void *RuntimeServices;
    EFI_BOOT_SERVICES *BootServices;
    u64 NumberOfTableEntries;
    void *ConfigurationTable;
} EFI_SYSTEM_TABLE;

typedef struct {
    u32 Version; u32 HorizontalResolution; u32 VerticalResolution;
    u32 PixelFormat; u32 PixelInformation; u32 PixelsPerScanLine;
} EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;

typedef struct {
    u32 MaxMode; u32 Mode; EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
    u64 SizeOfInfo; EFI_PHYSICAL_ADDRESS FrameBufferBase; u64 FrameBufferSize;
} EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;

typedef struct {
    EFI_STATUS (EFIAPI *QueryMode)(void*, u32, u64*, EFI_GRAPHICS_OUTPUT_MODE_INFORMATION**);
    EFI_STATUS (EFIAPI *SetMode)(void*, u32);
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *Mode;
} EFI_GRAPHICS_OUTPUT_PROTOCOL;

static EFI_GUID gop_guid = {0x9042A9DE,0x23DC,0x4A38,{0x96,0xFB,0x7A,0xDE,0xD0,0x80,0x51,0x6A}};
static EFI_GUID fs_guid  = {0x964E5B22,0x6459,0x11D2,{0x8E,0x39,0x00,0xA0,0xC9,0x69,0x72,0x3B}};

typedef struct {
    u32 Revision; void *OpenVolume; void *Close; void *Delete; void *Read;
    void *Write; void *GetPosition; void *SetPosition; void *GetInfo;
    void *SetInfo; void *Flush;
} EFI_FILE_PROTOCOL;

typedef struct {
    u64 Revision;
    EFI_STATUS (EFIAPI *OpenVolume)(void*, EFI_FILE_PROTOCOL**);
} EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;

typedef struct {
    u32 Revision; void *Open; void *Close; void *Delete; void *Read;
    void *Write; void *GetPosition; void *SetPosition; void *GetInfo;
    void *SetInfo; void *Flush;
} EFI_FILE_PROTOCOL_REAL;

/* efilib-стабы для LoadImage/StartImage не нужны — грузим ELF вручную, простой копией */

extern void kmain(u32 *fb, u32 w, u32 h, u32 pitch);

static u64 read_file(EFI_FILE_PROTOCOL *root, const char *name, void *buf, u64 max){
    EFI_FILE_PROTOCOL_REAL *f = (void*)root;
    EFI_STATUS (*open)(void*, void**, const u16*, u64, u64) = (void*)f->Open;
    /* filename в UCS-2 — конвертируем ASCII→UCS-2 */
    u16 wname[64]; int i=0;
    for(; name[i] && i<63; i++) wname[i] = (u16)(u8)name[i];
    wname[i]=0;
    void *fh=NULL;
    if(open(root,&fh,wname,1,0)!=0) return 0;
    EFI_FILE_PROTOCOL_REAL *ff=(void*)fh;
    u64 (*rd)(void*, u64*, void*) = (void*)ff->Read;
    u64 sz=max;
    if(rd(fh,&sz,buf)!=0){ ((void(*)(void*))ff->Close)(fh); return 0; }
    ((void(*)(void*))ff->Close)(fh);
    return sz;
}

/* простой ELF64 загрузчик: копирует PT_LOAD сегменты на их vaddr (identity-mapped) */
static int load_elf64(void *elf, u64 sz){
    u8 *e = elf;
    if(!(e[0]==0x7F && e[1]=='E' && e[2]=='L' && e[3]=='F')) return 0;
    u64 entry = *(u64*)(e+24);
    u64 phoff = *(u64*)(e+32);
    u16 phentsize = *(u16*)(e+54);
    u16 phnum = *(u16*)(e+56);
    for(int i=0;i<phnum;i++){
        u8 *ph = e + phoff + i*phentsize;
        u32 p_type = *(u32*)(ph+0);
        if(p_type != 1) continue; /* PT_LOAD */
        u64 p_offset = *(u64*)(ph+8);
        u64 p_vaddr  = *(u64*)(ph+16);
        u64 p_filesz = *(u64*)(ph+32);
        u64 p_memsz  = *(u64*)(ph+40);
        u8 *dst=(u8*)p_vaddr; u8 *src=e+p_offset;
        for(u64 k=0;k<p_filesz;k++) dst[k]=src[k];
        for(u64 k=p_filesz;k<p_memsz;k++) dst[k]=0;
    }
    (void)sz;
    ((void(*)(u32*,u32,u32,u32))entry)(0,0,0,0); /* вызов перезапишем ниже */
    return 1;
}

EFI_STATUS EFIAPI efi_main(EFI_SYSTEM_TABLE *ST, void *img){
    (void)img;
    EFI_BOOT_SERVICES *BS = ST->BootServices;

    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop=NULL;
    EFI_GUID g = gop_guid;
    if(BS->LocateProtocol(&g, NULL, (void**)&gop)!=0 || !gop) return 1;
    gop->SetMode(gop, gop->Mode->MaxMode-1); /* лучший режим */

    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs=NULL;
    EFI_GUID g2 = fs_guid;
    if(BS->LocateProtocol(&g2, NULL, (void**)&fs)!=0 || !fs) return 1;

    void *root=NULL;
    if(fs->OpenVolume(fs, (EFI_FILE_PROTOCOL**)&root)!=0) return 1;

    static u8 kbuf[4*1024*1024];
    u64 ksz = read_file((EFI_FILE_PROTOCOL*)root, "kernel.elf", kbuf, sizeof(kbuf));
    if(!ksz){ ST->ConOut->Hdr.Signature=0; for(;;) __asm__ volatile("hlt"); }

    u64 fb   = gop->Mode->FrameBufferBase;
    u32 w    = gop->Mode->Info->HorizontalResolution;
    u32 h    = gop->Mode->Info->VerticalResolution;
    u32 pitch= gop->Mode->Info->PixelsPerScanLine * 4;

    /* грузим ELF, но вызов kmain делаем с аргументами */
    u8 *e = kbuf;
    u64 entry = *(u64*)(e+24);
    u64 phoff = *(u64*)(e+32);
    u16 phentsize = *(u16*)(e+54);
    u16 phnum = *(u16*)(e+56);
    for(int i=0;i<phnum;i++){
        u8 *ph=e+phoff+i*phentsize;
        if(*(u32*)(ph+0)!=1) continue;
        u64 off=*(u64*)(ph+8), va=*(u64*)(ph+16), fsz=*(u64*)(ph+32), msz=*(u64*)(ph+40);
        for(u64 k=0;k<fsz;k++) ((u8*)va)[k]=e[off+k];
        for(u64 k=fsz;k<msz;k++) ((u8*)va)[k]=0;
    }

    void (*kentry)(u32*,u32,u32,u32) = (void(*)(u32*,u32,u32,u32))entry;
    kentry((u32*)fb, w, h, pitch);

    for(;;) __asm__ volatile("hlt");
    return EFI_SUCCESS;
}
