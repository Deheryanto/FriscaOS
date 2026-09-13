#include "timer.h"
#include "../../arch/i386/io.h"

/* Programmable Interval Timer (PIT, Intel 8253/8254) I/O ports:
 *   - 0x43: Command/mode register (write-only)
 *   - 0x40: Channel 0 data port (used for system timer / IRQ0) */
#define PIT_COMMAND_PORT 0x43
#define PIT_CHANNEL0_PORT 0x40

/* The PIT runs at a fixed base frequency of 1.193182 MHz
 * (exactly 1193182 Hz). We divide this down to get the desired rate. */
#define PIT_BASE_FREQUENCY 1193182

/* Number of timer ticks (interrupts) since boot.
 * Marked `volatile` because it's modified inside an ISR
 * and read from normal code — the compiler must not cache it. */
static volatile uint32_t ticks = 0;

/* Frequency (in Hz) at which the timer fires IRQ0.
 * Default: 100 Hz → one tick every 10 ms. */
static uint32_t target_freq = 100;

/* Initialize the PIT to fire IRQ0 at the given frequency (in Hz).
 * Example: timer_init(1000) → one tick per millisecond. */
void timer_init(uint32_t frequency) {   
    target_freq = frequency;

    /* The PIT counts down from `divisor` to 0 at the base frequency.
     * IRQ0 fires once per full countdown, so:
     *     divisor = base_freq / desired_freq */
    uint32_t divisor = PIT_BASE_FREQUENCY / frequency;

        /* Send command byte 0x36 to the command port:
     *   0b00 11 011 0
     *   ├─ bits 7-6: 00  → Channel 0
     *   ├─ bits 5-4: 11  → Access mode: lo byte, then hi byte
     *   ├─ bits 3-1: 011 → Mode 3: square wave generator
     *   └─ bit  0:   0   → Binary (not BCD) counter */
    outb(PIT_COMMAND_PORT, 0x36);

    /* Send the divisor in two bytes (low first, then high).
     * The PIT expects the low byte first because we selected
     * "lo/hi byte" access mode in the command byte above. */
    outb(PIT_CHANNEL0_PORT, (uint8_t)(divisor & 0xFF));         /* Low byte  */
    outb(PIT_CHANNEL0_PORT, (uint8_t)((divisor >> 8) & 0xFF));  /* High byte */
}

/* Called from the IRQ0 handler on every timer tick.
 * Kept minimal for speed — just increment the tick counter. */
void timer_handler(void) {
    ticks++;
}

/* Return the total number of ticks elapsed since boot.
 * Useful for measuring elapsed time or implementing delays. */
uint32_t timer_get_ticks(void) {
    return ticks;
}

/* Busy-wait (with CPU halt) for the given number of milliseconds.
 *
 * Instead of spinning in a loop burning CPU cycles, we use the `hlt`
 * instruction which puts the CPU to sleep until the next interrupt.
 * Since IRQ0 fires every tick, the CPU wakes up regularly to
 * re-check whether the requested delay has elapsed. */
void sleep(uint32_t ms) {
    uint32_t start_ticks = ticks;

    /* Convert milliseconds to ticks:
     *   ticks = (ms * freq) / 1000
     * Example at 100 Hz: 500 ms → 50 ticks. */
    uint32_t ticks_to_wait = (ms * target_freq) / 1000;
    
    /* Wait until enough ticks have passed.
     *
     * We use unsigned subtraction so this works correctly even if
     * `ticks` wraps around (2^32 overflow after ~497 days at 100 Hz). */
    while (ticks - start_ticks < ticks_to_wait) {
        __asm__ volatile("hlt"); /* Sleep until next interrupt */
    }
}