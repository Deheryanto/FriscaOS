#include "idt.h"
#include "io.h"
#include "../../drivers/vga/vga.h"
#include "../../drivers/timer/timer.h"

/* Programmable Interrupt Controller (PIC) I/O ports.
 * The 8259 PIC has two chips (master and slave):
 *   - Master PIC: command 0x20, data 0x21  (handles IRQ 0-7)
 *   - Slave  PIC: command 0xA0, data 0xA1  (handles IRQ 8-15) */
#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

/* The Interrupt Descriptor Table (IDT): 256 entries, one per interrupt vector.
 * Each entry tells the CPU where to jump when an interrupt occurs. */
struct idt_entry idt[256];

/* Pointer structure passed to the CPU via the LIDT instruction.
 * Contains the size (limit) and base address of the IDT. */
struct idt_ptr   idtp;

/* Assembly stubs that save/restore CPU state and call the C handlers below.
 * Defined in an .asm file — declared here so we can reference their addresses. */
extern void keyboard_handle_interrupt(void);
extern void irq0_handler(void);
extern void irq1_handler(void);

/* Fill in one IDT gate (entry) for interrupt vector `num`.
 *   num   = interrupt vector (0-255)
 *   base  = 32-bit address of the handler function
 *   sel   = code segment selector (0x08 = kernel code segment in GDT)
 *   flags = type and attributes (0x8E = present, ring 0, 32-bit interrupt gate) */
static void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_low  = base & 0xFFFF;          /* Low 16 bits of handler address */
    idt[num].base_high = (base >> 16) & 0xFFFF;  /* High 16 bits of handler address */
    idt[num].sel       = sel;                    /* Kernel code segment selector */
    idt[num].always0   = 0;                      /* Reserved, must be zero */
    idt[num].flags     = flags;                  /* Gate type + attributes */
}

/* C-level handler for the keyboard IRQ (IRQ 1).
 * Called from the assembly stub `irq1_handler`. */
void keyboard_handler_main(void) {
        /* Send End-Of-Interrupt (EOI) signal to the master PIC,
     * telling it this IRQ has been handled and new ones may arrive. */
    outb(0x20, 0x20);

    /* Re-enable interrupts so other IRQs can fire while we process the key */
    __asm__ volatile ("sti");

    /* Hand off to the actual keyboard driver */
    keyboard_handle_interrupt();
}


/* C-level handler for the timer IRQ (IRQ 0).
 * Called from the assembly stub `irq0_handler`. */
void timer_handler_main(void) {
    /* Update the system tick counter, scheduler, etc. */
    timer_handler();

    /* Send EOI to the master PIC after handling the timer interrupt */
    outb(0x20, 0x20);
}


/* Remap the 8259 PIC so its IRQs don't conflict with CPU exception vectors.
 *
 * By default, the master PIC maps IRQ 0-7 to vectors 0x08-0x0F, which collide
 * with CPU exceptions (e.g., double fault = 0x08). We remap them to 0x20-0x2F
 * so hardware IRQs don't clash with CPU exceptions. */
void pic_remap(void) {
    /* ICW1: Start initialization sequence (0x11 = init + expect ICW4) */
    outb(PIC1_COMMAND, 0x11);
    outb(PIC2_COMMAND, 0x11);

    /* ICW2: Set vector offsets.
     *   Master PIC → IRQ 0-7  map to vectors 0x20-0x27
     *   Slave  PIC → IRQ 8-15 map to vectors 0x28-0x2F */
    outb(PIC1_DATA, 0x20);
    outb(PIC2_DATA, 0x28);

    /* ICW3: Tell master/slave how they're wired together.
     *   Master: slave is on IRQ line 2 (bit 2 set → 0x04)
     *   Slave:  connected to master's IRQ 2 (identity → 0x02) */
    outb(PIC1_DATA, 0x04);
    outb(PIC2_DATA, 0x02);

    /* ICW4: Set 8086/88 mode (0x01 = x86 mode) */
    outb(PIC1_DATA, 0x01);
    outb(PIC2_DATA, 0x01);

    /* OCW1: Set interrupt masks.
     *   0xFC = 1111 1100 → enable IRQ0 (timer) and IRQ1 (keyboard),
     *                       mask all other master IRQs.
     *   0xFF = 1111 1111 → mask all slave IRQs (none enabled yet). */
    outb(PIC1_DATA, 0xFC);
    outb(PIC2_DATA, 0xFF);
}


/* Initialize the Interrupt Descriptor Table and enable interrupts. */
void idt_init(void) {
    /* Set up the IDT pointer: size = (256 entries × 8 bytes) - 1, base = &idt */
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base  = (uint32_t)&idt;

    /* Remap the PIC so hardware IRQs land at vectors 0x20 and above */
    pic_remap();

    /* Register the timer IRQ (IRQ0 → vector 0x20) handler.
     * 0x08 = kernel code segment, 0x8E = present + ring0 + 32-bit interrupt gate */
    idt_set_gate(0x20, (uint32_t)irq0_handler, 0x08, 0x8E);

    /* Register the keyboard IRQ (IRQ1 → vector 0x21) handler */
    idt_set_gate(0x21, (uint32_t)irq1_handler, 0x08, 0x8E);

    /* Load the IDT into the CPU's IDTR register */
    __asm__ volatile ("lidt %0" : : "m"(idtp));

    /* Enable interrupts globally (set the IF flag in EFLAGS) */
    __asm__ volatile ("sti");
}