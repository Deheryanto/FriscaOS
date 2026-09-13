#ifndef IDT_H
#define IDT_H

/* Include guard: prevents double-inclusion in the same translation unit. */

#include "../../lib/stdint.h"   /* For uint8_t, uint16_t, uint32_t */

/* ─────────────────────────────────────────────────────────────────
 *  Interrupt Descriptor Table (IDT) structures
 *
 *  The IDT is an array of 256 descriptors, one per interrupt vector.
 *  The CPU looks up the appropriate entry whenever an interrupt or
 *  exception occurs, and jumps to the handler address stored there.
 *
 *  On x86 (32-bit protected mode), each entry is exactly 8 bytes:
 *
 *    Offset  Size  Field
 *    ──────  ────  ─────────────────────────────
 *    0       2     base_low   (low 16 bits of handler address)
 *    2       2     sel        (code segment selector)
 *    3       1     always0    (reserved, must be 0)
 *    4       1     flags      (type + attributes)
 *    5       2     base_high  (high 16 bits of handler address)
 *
 *  The handler's 32-bit address is split into two 16-bit halves
 *  to maintain compatibility with the 16-bit 80286 IDT layout.
 * ───────────────────────────────────────────────────────────────── */

struct idt_entry {
    uint16_t base_low;    /* Low 16 bits of the ISR's address */
    uint16_t sel;         /* Kernel code segment selector (0x08 in GDT) */
    uint8_t  always0;     /* Reserved — must always be zero */
    uint8_t  flags;       /* Type and attributes (e.g. 0x8E) */
    uint16_t base_high;   /* High 16 bits of the ISR's address */
} __attribute__((packed));

/* The `packed` attribute tells GCC to lay out the struct with NO
 * padding between fields. Without it, the compiler might insert
 * padding bytes to align `base_high` to a 4-byte boundary, making
 * the struct 12 bytes instead of 8 — which would completely break
 * the IDT layout the CPU expects. */

/* ─────────────────────────────────────────────────────────────────
 *  IDT Pointer (IDTR)
 *
 *  This structure is loaded into the CPU's IDTR register via the
 *  `lidt` instruction. It tells the CPU where the IDT lives and
 *  how big it is.
 *
 *    Offset  Size  Field
 *    ──────  ────  ─────────────────────
 *    0       2     limit  (size of IDT in bytes - 1)
 *    2       4     base   (linear address of the IDT)
 *
 *  The `limit` field is "size - 1" because a limit of 0 means
 *  1 byte is valid, and 0xFFFF means 65536 bytes (max). With 256
 *  entries of 8 bytes each, the limit is (256 * 8) - 1 = 2047.
 *
 *  Also `packed` because the CPU expects this exact 6-byte layout
 *  with no padding between `limit` and `base`.
 * ───────────────────────────────────────────────────────────────── */

struct idt_ptr {
    uint16_t limit;       /* Size of the IDT in bytes, minus 1 */
    uint32_t base;        /* Linear (physical) address of the IDT */
} __attribute__((packed));

/* ─────────────────────────────────────────────────────────────────
 *  Public API
 * ───────────────────────────────────────────────────────────────── */

/* Initialize the IDT: set up all 256 gates, remap the PIC, and
 * load the IDT into the CPU. Called once at boot from kernel main. */
void idt_init(void);

/* Remap the 8259 PIC so IRQ 0-15 are delivered at vectors 0x20-0x2F
 * instead of 0x08-0x0F (which collide with CPU exceptions).
 * Called internally by idt_init, but exposed for flexibility. */
void pic_remap(void);

#endif  /* IDT_H */