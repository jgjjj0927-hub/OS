// language: C, file: kernel/input.c, target: x86_64, freestanding
#include "input.h"

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

static inline void outb(u16 p, u8 v){ __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p)); }
static inline u8  inb(u16 p){ u8 v; __asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p)); return v; }
static inline void io_wait(void){ outb(0x80,0); }

/* ── IDT ─────────────────────────────────────────────────── */
struct __attribute__((packed)) idt_entry {
    u16 off_lo; u16 sel; u8 ist; u8 type; u16 off_mid; u32 off_hi; u32 zero;
};
struct __attribute__((packed)) idt_ptr { u16 limit; u64 base; };

static struct idt_entry idt[256];
static struct idt_ptr   idtp;

static void idt_set(u8 n, u64 handler, u8 type){
    idt[n].off_lo  = (u16)handler;
    idt[n].sel     = 0x08; idt[n].ist = 0; idt[n].type = type;
    idt[n].off_mid = (u16)(handler>>16);
    idt[n].off_hi  = (u32)(handler>>32); idt[n].zero = 0;
}

static void idt_init(void){
    extern void irq0_stub(void), irq1_stub(void), irq12_stub(void), isr_default(void);
    for (int i = 0; i < 256; i++) idt_set(i, (u64)isr_default, 0x8E);
    idt_set(32, (u64)irq0_stub,  0x8E);
    idt_set(33, (u64)irq1_stub,  0x8E);
    idt_set(44, (u64)irq12_stub, 0x8E);
    idtp.limit = sizeof(idt)-1;
    idtp.base  = (u64)&idt;
    __asm__ volatile("lidt %0"::"m"(idtp));
}

/* ── PIC remap ───────────────────────────────────────────── */
#define PIC1 0x20
#define PIC2 0xA0
static void pic_remap(void){
    outb(PIC1,0x11); io_wait();
    outb(PIC2,0x11); io_wait();
    outb(PIC1+1,0x20); io_wait();
    outb(PIC2+1,0x28); io_wait();
    outb(PIC1+1,0x04); io_wait();
    outb(PIC2+1,0x02); io_wait();
    outb(PIC1+1,0x01); io_wait();
    outb(PIC2+1,0x01); io_wait();
    outb(PIC1+1,0x00); outb(PIC2+1,0x00);
}

/* ── PIT 100 Hz ──────────────────────────────────────────── */
static volatile u64 g_ticks = 0;
static void pit_init(u32 hz){
    if (hz == 0 || hz > 1193180) hz = 100;
    u32 div = 1193180/hz;
    outb(0x43,0x36);
    outb(0x40,(u8)(div&0xFF));
    outb(0x40,(u8)((div>>8)&0xFF));
}

/* ── клавиатура ──────────────────────────────────────────── */
#define KBUF 256
static volatile u8  kb_buf[KBUF];
static volatile u32 kb_head=0, kb_tail=0;
static const char kbd_map[128] = {
  0,27,'1','2','3','4','5','6','7','8','9','0','-','=','\b','\t',
 'q','w','e','r','t','y','u','i','o','p','[',']','\n',0,'a','s',
 'd','f','g','h','j','k','l',';','\'','`',0,'\\','z','x','c','v',
 'b','n','m',',','.','/',0,'*',0,' ',0
};

/* ── мышь ────────────────────────────────────────────────── */
static volatile int m_x=0, m_y=0;
static volatile u8  m_btn=0;
static u8 m_cycle=0, m_packet[3];

static void mouse_wait(u8 want_data){
    for (u32 i = 0; i < 100000; i++){
        u8 s = inb(0x64);
        if (want_data ? (s&1) : !(s&2)) return;
    }
}
static void mouse_write(u8 v){
    mouse_wait(1); outb(0x64,0xD4);
    mouse_wait(1); outb(0x60,v);
    mouse_wait(1); (void)inb(0x60); /* ACK */
}
static void mouse_init(void){
    mouse_wait(1); outb(0x64,0xA8);
    mouse_wait(1); outb(0x64,0x20);
    mouse_wait(1); u8 st = inb(0x60)|2;
    mouse_wait(1); outb(0x64,0x60);
    mouse_wait(1); outb(0x60,st);
    mouse_write(0xF6);
    mouse_write(0xF4);
}

/* ── IRQ dispatch ────────────────────────────────────────── */
void irq_dispatch(u64 n){
    switch (n){
        case 0:
            g_ticks++;
            outb(PIC1,0x20);
            break;
        case 1: {
            u8 sc = inb(0x60);
            if (!(sc & 0x80)){
                char c = kbd_map[sc & 0x7F];
                if (c){
                    u32 nh = (kb_head+1)%KBUF;
                    if (nh != kb_tail){ kb_buf[kb_head]=c; kb_head=nh; }
                }
            }
            outb(PIC1,0x20);
            break;
        }
        case 12: {
            u8 s = inb(0x64);
            while (s & 1){
                u8 d = inb(0x60);
                if (m_cycle == 0 && !(d&8)) break;
                m_packet[m_cycle++] = d;
                if (m_cycle == 3){
                    m_cycle = 0;
                    int dx = (int8_t)m_packet[1];
                    int dy = (int8_t)m_packet[2];
                    if (m_packet[0] & 0x10) dx |= 0xFFFFFF00;
                    if (m_packet[0] & 0x20) dy |= 0xFFFFFF00;
                    m_x += dx; m_y -= dy;
                    m_btn = m_packet[0] & 7;
                }
                s = inb(0x64);
            }
            if (m_cycle > 3) m_cycle = 0;
            outb(PIC2,0x20); outb(PIC1,0x20);
            break;
        }
        default:
            outb(PIC1,0x20);
            break;
    }
}

/* ── API ─────────────────────────────────────────────────── */
u64 timer_ticks(void){ return g_ticks; }
int kbd_try_get(char *out){
    if (kb_tail == kb_head) return 0;
    *out = kb_buf[kb_tail]; kb_tail = (kb_tail+1)%KBUF; return 1;
}
void mouse_state(int *x, int *y, u8 *btn){ *x=m_x; *y=m_y; *btn=m_btn; }

void input_init(void){
    idt_init();
    pic_remap();
    pit_init(100);
    mouse_init();
    __asm__ volatile("sti");
}
