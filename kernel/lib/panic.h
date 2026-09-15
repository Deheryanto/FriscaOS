#ifndef PANIC_H
#define PANIC_H

#include "../lib/stdint.h"

/* Print a fatal error message and halt the system.
 *
 * This function:
 *   - Disables interrupts (cli)
 *   - Changes screen to red
 *   - Prints "*** KERNEL PANIC ***" and the given message
 *   - Halts the CPU forever
 *
 * Marked `noreturn` so the compiler knows execution stops here. */
void kernel_panic(const char* msg) __attribute__((noreturn));

/* Page fault handler — called from IRQ/exception dispatcher when
 * the CPU raises a page fault (vector 14).
 *
 * Reads CR2 (faulting address) and the error code, prints a
 * detailed diagnostic, and halts. */
void page_fault_handler(uint32_t error_code);

#endif