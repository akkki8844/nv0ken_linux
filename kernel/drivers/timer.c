#include "timer.h"

#include "arch/x86_64/io.h"
#include "arch/x86_64/irq.h"

#define PIT_INPUT_HZ 1193182u
#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43

static uint32_t timer_hz = 100;
static volatile uint64_t ticks;
static timer_callback_t callback;
static void *callback_context;

static void timer_irq(registers_t *regs, void *context)
{
    (void)regs;
    (void)context;
    ++ticks;
    if (callback) {
        callback(callback_context);
    }
}

void timer_init(uint32_t hz)
{
    if (hz == 0) {
        hz = 100;
    }
    timer_hz = hz;
    uint32_t divisor = PIT_INPUT_HZ / hz;
    outb(PIT_COMMAND, 0x36);
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xff));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xff));
    irq_register(0, timer_irq, 0);
}

uint64_t timer_ticks(void)
{
    return ticks;
}

uint64_t timer_uptime_ms(void)
{
    return (ticks * 1000ull) / timer_hz;
}

void timer_sleep_ms(uint64_t ms)
{
    uint64_t end = timer_uptime_ms() + ms;
    while (timer_uptime_ms() < end) {
        __asm__ volatile("hlt");
    }
}

void timer_set_callback(timer_callback_t cb, void *context)
{
    callback = cb;
    callback_context = context;
}
