# driver model

## overview

nv0ken drivers live in `kernel/drivers/`. there is no loadable module system — all drivers are compiled into the kernel binary. a driver consists of an init function, an IRQ handler (if needed), and a set of operations exposed to the rest of the kernel through a direct function call interface.

## writing a driver

every driver follows this pattern:

```c
#include "mydriver.h"
#include "../arch/x86_64/irq.h"
#include "../arch/x86_64/io.h"
#include "../lib/kprintf.h"

#define MYDEV_PORT  0x3F8

static void mydriver_irq_handler(registers_t *regs) {
    (void)regs;
    /* handle interrupt */
}

void mydriver_init(void) {
    outb(MYDEV_PORT + 1, 0x00);
    irq_register(4, mydriver_irq_handler);
    kprintf("[mydriver] initialised\n");
}

int mydriver_read(char *buf, int len) {
    /* read from device */
    return 0;
}
```

## IRQ registration

```c
#include "kernel/arch/x86_64/irq.h"

void irq_register(int irq, irq_handler_t handler);
void irq_unregister(int irq);
```

IRQ numbers 0–15 map to x86 hardware IRQs. the PIC remaps them to interrupt vectors 32–47. a driver passes the hardware IRQ number (e.g. 1 for keyboard, 4 for COM1) — the kernel adds 32 internally when setting the IDT gate.

## PIC IRQ map

| IRQ | vector | device              |
|-----|--------|---------------------|
| 0   | 32     | PIT timer           |
| 1   | 33     | PS/2 keyboard       |
| 2   | 34     | PIC cascade         |
| 3   | 35     | COM2                |
| 4   | 36     | COM1                |
| 5   | 37     | LPT2                |
| 6   | 38     | floppy              |
| 7   | 39     | LPT1                |
| 8   | 40     | CMOS RTC            |
| 9   | 41     | ACPI / free         |
| 10  | 42     | free                |
| 11  | 43     | free                |
| 12  | 44     | PS/2 mouse          |
| 13  | 45     | FPU                 |
| 14  | 46     | ATA primary         |
| 15  | 47     | ATA secondary       |

## port I/O macros

```c
#include "kernel/arch/x86_64/io.h"

static inline void outb(uint16_t port, uint8_t val);
static inline uint8_t inb(uint16_t port);
static inline void outw(uint16_t port, uint16_t val);
static inline uint16_t inw(uint16_t port);
static inline void outl(uint16_t port, uint32_t val);
static inline uint32_t inl(uint16_t port);
static inline void io_wait(void);
```

`io_wait` writes a dummy byte to port 0x80, giving old hardware time to respond to a previous command.

## PCI driver registration

PCI drivers match on vendor/device ID:

```c
#include "kernel/drivers/pci.h"

typedef struct {
    uint16_t vendor_id;
    uint16_t device_id;
    void (*probe)(pci_device_t *dev);
} pci_driver_t;

void pci_driver_register(pci_driver_t *drv);
```

`pci_scan` runs during kernel init. for each device found it walks registered drivers and calls `probe` on a match. `probe` receives a `pci_device_t` with BAR addresses, IRQ line, bus/slot/func, and class.

## framebuffer driver

the framebuffer driver does not use IRQs. it wraps the physical framebuffer address provided by limine.

```c
#include "kernel/drivers/framebuffer.h"

void fb_init(struct limine_framebuffer *fb);
void fb_put_pixel(int x, int y, uint32_t argb);
void fb_fill_rect(int x, int y, int w, int h, uint32_t argb);
void fb_copy_rect(int dx, int dy, int sx, int sy, int w, int h);
void fb_scroll_up(int lines);
uint32_t *fb_addr(void);
int fb_width(void);
int fb_height(void);
int fb_pitch(void);
```

## init order in kmain

drivers are initialised in this order to respect hardware dependencies:

```
1. serial      (debug output available immediately)
2. framebuffer (visual output)
3. kprintf     (uses both serial and framebuffer)
4. pic         (must remap before enabling interrupts)
5. timer       (needs PIC remapped)
6. keyboard    (needs PIC remapped)
7. acpi        (needs heap)
8. apic        (needs ACPI MADT for LAPIC address)
9. mouse       (needs PIC)
10. pci        (needs heap for device list)
11. ata        (needs PCI scan done)
12. rtl8139    (needs PCI scan done)
```