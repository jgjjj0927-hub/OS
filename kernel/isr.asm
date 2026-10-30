; language: NASM, file: kernel/isr.asm, target: x86_64
section .text
global irq0_stub, irq1_stub, irq12_stub, isr_default
extern irq_dispatch

%macro SAVE_REGS 0
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
%endmacro

%macro RESTORE_REGS 0
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
%endmacro

%macro IRQ_STUB 2
%1:
    SAVE_REGS
    mov  rdi, %2
    call irq_dispatch
    RESTORE_REGS
    iretq
%endmacro

IRQ_STUB irq0_stub,  0
IRQ_STUB irq1_stub,  1
IRQ_STUB irq12_stub, 12

isr_default:
    cli
.hang:
    hlt
    jmp .hang
