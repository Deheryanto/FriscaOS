#ifndef ISR_H
#define ISR_H

#include "../../lib/stdint.h"

/* ─────────────────────────────────────────────────────────────────
 *  Interrupt Service Routine (ISR) declarations
 *
 *  Each ISR is defined in `isr.asm` as an assembly stub that:
 *    1. Pushes a dummy error code (if the CPU didn't push one)
 *    2. Pushes the interrupt number
 *    3. Jumps to a common handler (isr_common_stub)
 *
 *  The common stub then calls the C dispatcher `isr_handler`,
 *  which dispatches to the appropriate handler below.
 * ───────────────────────────────────────────────────────────────── */

/* Register struct that the common stub passes to the C dispatcher.
 * Layout must match the push order in isr.asm. */
typedef struct registers {
    uint32_t ds;                                     /* Data segment selector */
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; /* Pushed by pusha */
    uint32_t int_no, err_code;                       /* Interrupt number + error code */
    uint32_t eip, cs, eflags, useresp, ss;           /* Pushed by CPU automatically */
} registers_t;

/* C-level dispatcher — called from isr_common_stub.
 * Handles exception-specific logic (panic on page fault, etc.) */
void isr_handler(registers_t* regs);

#endif