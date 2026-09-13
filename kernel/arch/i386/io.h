#ifndef ARCH_IO_H
#define ARCH_IO_H

/* Include guard: prevents this header from being included twice
 * in the same translation unit, which would cause duplicate
 * definition errors. */

#include "../../lib/stdint.h"   /* For uint8_t, uint16_t */

/* ─────────────────────────────────────────────────────────────────
 *  x86 Port I/O Helpers
 *
 *  On x86, hardware devices (PIC, PIT, keyboard controller, VGA CRTC,
 *  ATA, serial ports, ...) are accessed through a separate 16-bit
 *  "I/O port" address space — not through normal memory.
 *
 *  These tiny inline functions wrap the CPU's `in` and `out`
 *  instructions, which are the only way to talk to that space.
 * ───────────────────────────────────────────────────────────────── */

/* Write one byte (`val`) to the given I/O port (`port`).
 *
 *   AT&T syntax:  outb %al, %dx
 *   Intel syntax: out dx, al
 *
 * The compiler constraints tell GCC how to set up the registers:
 *   - "a"(val)   → put `val` in AL (8-bit accumulator)
 *   - "Nd"(port) → put `port` in DX (a 16-bit register) OR as
 *                  an immediate constant if the port is known.
 *
 * `volatile` prevents the compiler from optimizing away or
 * reordering the instruction — I/O has side effects on hardware. */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

/* Read one byte from the given I/O port and return it.
 *
 *   AT&T syntax:  inb %dx, %al
 *   Intel syntax: in al, dx
 *
 * Constraints:
 *   - "=a"(ret)  → the result goes into AL and is stored in `ret`
 *   - "Nd"(port) → same as above (DX register or immediate)
 *
 * Again `volatile` is essential — the read has side effects
 * (e.g., reading the keyboard data port acknowledges the IRQ). */
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

#endif  /* ARCH_IO_H */