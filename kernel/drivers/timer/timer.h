#ifndef TIMER_H
#define TIMER_H

/* Include guard: prevents double-inclusion in the same translation unit. */

#include "../../lib/stdint.h"   /* For uint32_t */

/* ─────────────────────────────────────────────────────────────────
 *  PIT (Programmable Interval Timer) driver interface
 *
 *  The PIT is an Intel 8253/8254 chip that generates periodic
 *  interrupts on IRQ0. It runs at a fixed hardware frequency of
 *  1,193,182 Hz, which the driver divides down to a usable rate
 *  (typically 100 Hz or 1000 Hz).
 *
 *  This driver provides:
 *    - A way to set the tick rate at boot
 *    - A tick counter incremented on each IRQ0
 *    - A blocking sleep() function built on top of the ticks
 *
 *  The actual IRQ0 handler calls timer_handler() from idt.c.
 * ───────────────────────────────────────────────────────────────── */

/* Initialize the PIT to fire IRQ0 at the given frequency (in Hz).
 *
 * Typical values:
 *    100  → one tick every 10 ms   (default, low overhead)
 *   1000  → one tick every 1 ms    (finer resolution, more CPU)
 *
 * The minimum usable frequency is ~18 Hz — below that, the 16-bit
 * divisor would overflow. Frequencies above ~1193182 Hz are
 * impossible (that's the PIT's base frequency).
 *
 * Called once at boot, before enabling interrupts. */
void timer_init(uint32_t frequency);

/* Called on every IRQ0 (from the assembly ISR stub in idt.c).
 *
 * Increments the global tick counter. Kept minimal for speed —
 * any heavy work (scheduling, timeouts) should be deferred to
 * code that runs outside interrupt context.
 *
 * Do not call this directly — it's an internal callback for the
 * IRQ dispatch path. */
void timer_handler(void);

/* Block the calling context for the given number of milliseconds.
 *
 * Implemented as a busy-wait using `hlt` — the CPU sleeps until
 * the next interrupt, waking on each tick to re-check whether
 * the requested delay has elapsed. This avoids burning 100% CPU
 * while waiting.
 *
 * WARNING: This blocks the current execution context. On a
 * single-tasking kernel that's fine. In a multitasking kernel
 * you'd want a scheduler-aware sleep (yield to other tasks). */
void sleep(uint32_t ms);

/* Return the number of timer ticks elapsed since boot.
 *
 * The value wraps around after 2^32 ticks:
 *   - at 100 Hz:  ~497 days
 *   - at 1000 Hz: ~49.7 days
 *
 * Callers that need elapsed time should use unsigned subtraction:
 *     uint32_t start = timer_get_ticks();
 *     ...
 *     uint32_t elapsed = timer_get_ticks() - start;   // wraparound-safe
 *
 * Useful for:
 *   - Measuring elapsed time
 *   - Implementing timeouts
 *   - Driving periodic tasks (e.g., "run every 100 ticks") */
uint32_t timer_get_ticks(void);


int  timer_needs_reschedule(void);
void timer_clear_reschedule(void);
#endif  /* TIMER_H */