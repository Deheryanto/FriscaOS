#include "timer.h"
#include "../../arch/i386/io.h"
#include "../../task/task.h"

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

static volatile int need_reschedule = 0;

/* ─────────────────────────────────────────────────────────────────
 *  timer_init — program the PIT to fire IRQ0 at `frequency` Hz
 * ───────────────────────────────────────────────────────────────── */
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

/* ─────────────────────────────────────────────────────────────────
 *  timer_handler — called from IRQ0 on every timer tick
 *static volatile int need_reschedule = 0
 *  Three responsibilities:
 *    1. Increment the tick counter (timekeeping)
 *    2. Send End-Of-Interrupt (EOI) to the PIC (so future IRQs fire)
 *    3. Voluntarily yield to the next task (PREEMPTION!)
 *
 *  Because this runs on every timer tick (100 times per second at
 *  100 Hz), every task gets at most 10 ms of CPU before it's
 *  forcibly swapped out. This upgrades the cooperative scheduler
 *  from the previous chapter into a PREEMPTIVE one.
 *
 *  Without this, a CPU-bound task would run forever, starving
 *  all others. With it, the scheduler enforces fairness.
 * ───────────────────────────────────────────────────────────────── */
void timer_handler(void) {
    ticks++;                    /* Timekeeping */
    need_reschedule = 1;
    outb(0x20, 0x20);           /* Send EOI to master PIC */

    //task_schedule();            /* Preempt: yield to next runnable task */
}

/* Return the total number of ticks elapsed since boot.
 * Useful for measuring elapsed time or implementing delays. */
uint32_t timer_get_ticks(void) {
    return ticks;
}

int timer_needs_reschedule(void) {
    return need_reschedule;
}

void timer_clear_reschedule(void) {
    need_reschedule = 0;
}

/* ─────────────────────────────────────────────────────────────────
 *  sleep — block the current task for the given number of milliseconds
 *
 *  Implementation changed from the previous version: instead of
 *  `hlt` (which would freeze the CPU entirely — no other task could
 *  run), we now call `task_schedule()` in a loop. This yields the
 *  CPU to other tasks while we wait, then checks the time again.
 *
 *  So `sleep` no longer blocks the *system* — just the calling task.
 *  Other tasks continue to run while we wait.
 * ───────────────────────────────────────────────────────────────── */
void sleep(uint32_t ms) {
    /* Convert milliseconds to ticks:
     *     ticks = ms / (ms per tick)
     * At 100 Hz, ms-per-tick = 10, so sleep(1000) = 100 ticks.
     *
     * Round up: if the requested delay is shorter than one tick,
     * we still wait one tick — the finest granularity the timer
     * can offer. */
    uint32_t ticks_to_wait = ms / (1000 / target_freq);
    if (ticks_to_wait == 0) ticks_to_wait = 1;

    uint32_t start_ticks = ticks;

    /* Busy-yield loop: keep yielding CPU until enough ticks pass.
     * Uses unsigned subtraction so it works correctly even if
     * `ticks` wraps around after ~497 days at 100 Hz. */
    while ((ticks - start_ticks) < ticks_to_wait) {
        __asm__ volatile("hlt");   /* Sleep until next IRQ */
    }
}