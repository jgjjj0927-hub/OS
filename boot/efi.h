/* language: C, file: boot/efi.h — типы UEFI, MS ABI на всех указателях */
#pragma once
#include <stdint.h>

#define EFIAPI __attribute__((ms_abi))
#define EFI_SUCCESS 0ULL
#define EFI_ERROR(s) ((int64_t)(s) < 0)

typedef uint64_t EFI_STATUS;
typedef void*    EFI_HANDLE;
typedef uint64_t EFI_PHYSICAL_ADDRESS;
typedef uint64_t EFI_VIRTUAL_ADDRESS;
typedef uint64_t UINTN;
typedef uint32_t UINT32;
typedef uint16_t UINT16;
typedef uint8_t  UINT8;
typedef uint16_t CHAR16;
typedef void     VOID;

typedef struct { uint32_t Data1; uint16_t Data2, Data3; uint8_t Data4[8]; } EFI_GUID;

typedef struct { uint32_t Type; uint32_t Pad; uint64_t PhysicalStart, VirtualStart, NumberOfPages, Attribute; } EFI_MEMORY_DESCRIPTOR;

typedef struct {
    uint32_t Version;
    uint32_t HorizontalResolution;
    uint32_t VerticalResolution;
    uint32_t PixelFormat;        /* 0 = RGB, 1 = BGR, 2 = BitMask, 3 = BltOnly */
    uint32_t PixelInformation[4];
    uint32_t PixelsPerScanLine;
} EFI_GOP_MODE_INFO;

typedef struct {
    uint32_t             MaxMode;
    uint32_t             Mode;
    EFI_GOP_MODE_INFO*   Info;
    UINTN                SizeOfInfo;
    EFI_PHYSICAL_ADDRESS FrameBufferBase;
    UINTN                FrameBufferSize;
} EFI_GOP_MODE;

typedef struct {
    EFI_STATUS (EFIAPI *QueryMode)(void*, uint32_t, UINTN*, EFI_GOP_MODE_INFO**);
    EFI_STATUS (EFIAPI *SetMode)(void*, uint32_t);
    EFI_STATUS (EFIAPI *Blt)(void*, void*, uint32_t, UINTN, UINTN, UINTN, UINTN, UINTN, UINTN, uint32_t);
    EFI_GOP_MODE* Mode;
} EFI_GRAPHICS_OUTPUT_PROTOCOL;

typedef struct EFI_FILE_PROTOCOL EFI_FILE_PROTOCOL;
struct EFI_FILE_PROTOCOL {
    EFI_STATUS (EFIAPI *Open)(EFI_FILE_PROTOCOL*, EFI_FILE_PROTOCOL**, CHAR16*, uint64_t, uint64_t);
    EFI_STATUS (EFIAPI *Close)(EFI_FILE_PROTOCOL*);
    EFI_STATUS (EFIAPI *Delete)(EFI_FILE_PROTOCOL*);
    EFI_STATUS (EFIAPI *Read)(EFI_FILE_PROTOCOL*, UINTN*, void*);
    EFI_STATUS (EFIAPI *Write)(EFI_FILE_PROTOCOL*, UINTN*, void*);
    EFI_STATUS (EFIAPI *GetPosition)(EFI_FILE_PROTOCOL*, uint64_t*);
    EFI_STATUS (EFIAPI *SetPosition)(EFI_FILE_PROTOCOL*, uint64_t);
    EFI_STATUS (EFIAPI *GetInfo)(EFI_FILE_PROTOCOL*, EFI_GUID*, UINTN*, void*);
    EFI_STATUS (EFIAPI *SetInfo)(EFI_FILE_PROTOCOL*, EFI_GUID*, UINTN, void*);
    EFI_STATUS (EFIAPI *Flush)(EFI_FILE_PROTOCOL*);
};

typedef struct {
    uint64_t Revision;
    EFI_STATUS (EFIAPI *OpenVolume)(void*, EFI_FILE_PROTOCOL**);
    /* остальные поля не нужны */
} EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;

typedef struct {
    EFI_HANDLE               ImageHandle;
    void*                    _pad[8];
    void*                    ConsoleOutHandle;
    void*                    ConsoleOut;
    void*                    ConsoleErrHandle;
    void*                    ConsoleErr;
    void*                    _pad2;
    void*                    BootServices;
    void*                    RuntimeServices;
} EFI_SYSTEM_TABLE_FRONT;

typedef struct EFI_BOOT_SERVICES EFI_BOOT_SERVICES;
struct EFI_BOOT_SERVICES {
    char     Hdr[24];
    /* RaiseTPL, RestoreTPL, AllocatePages, FreePages, GetMemoryMap, AllocatePool,
       FreePool, CreateEvent, SetTimer, WaitForEvent, SignalEvent, CloseEvent,
       CheckEvent, InstallProtocolInterface, ReinstallProtocolInterface,
       UninstallProtocolInterface, HandleProtocol — 17 функций до LocateProtocol */
    EFI_STATUS (EFIAPI *RaiseTPL)(UINTN);
    void       (EFIAPI *RestoreTPL)(UINTN);
    EFI_STATUS (EFIAPI *AllocatePages)(uint32_t, uint32_t, UINTN, EFI_PHYSICAL_ADDRESS*);
    EFI_STATUS (EFIAPI *FreePages)(EFI_PHYSICAL_ADDRESS, UINTN);
    EFI_STATUS (EFIAPI *GetMemoryMap)(UINTN*, EFI_MEMORY_DESCRIPTOR*, UINTN*, UINTN*, uint32_t*);
    EFI_STATUS (EFIAPI *AllocatePool)(uint32_t, UINTN, void**);
    EFI_STATUS (EFIAPI *FreePool)(void*);
    EFI_STATUS (EFIAPI *CreateEvent)(uint32_t, UINTN, void*, void*, void**);
    EFI_STATUS (EFIAPI *SetTimer)(void*, uint32_t, uint64_t);
    EFI_STATUS (EFIAPI *WaitForEvent)(UINTN, void**, UINTN*);
    EFI_STATUS (EFIAPI *SignalEvent)(void*);
    EFI_STATUS (EFIAPI *CloseEvent)(void*);
    EFI_STATUS (EFIAPI *CheckEvent)(void*);
    EFI_STATUS (EFIAPI *InstallProtocolInterface)(EFI_HANDLE*, EFI_GUID*, uint32_t, void*);
    EFI_STATUS (EFIAPI *ReinstallProtocolInterface)(EFI_HANDLE, EFI_GUID*, void*, void*);
    EFI_STATUS (EFIAPI *UninstallProtocolInterface)(EFI_HANDLE, EFI_GUID*, void*);
    EFI_STATUS (EFIAPI *HandleProtocol)(EFI_HANDLE, EFI_GUID*, void**);
    EFI_STATUS (EFIAPI *LocateHandleBuffer)(uint32_t, EFI_GUID*, void*, UINTN*, EFI_HANDLE**);
    EFI_STATUS (EFIAPI *LocateProtocol)(EFI_GUID*, void*, void**);
    EFI_STATUS (EFIAPI *ExitBootServices)(EFI_HANDLE, UINTN);
};
