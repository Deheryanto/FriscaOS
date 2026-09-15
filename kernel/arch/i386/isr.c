#include "isr.h"
#include "../../lib/panic.h"
#include "../../lib/string.h"
#include "../../drivers/vga/vga.h"


/* ─────────────────────────────────────────────────────────────────
 *  isr_handler — C dispatcher for CPU exceptions (0-31)
 *
 *  Called from isr_common_stub for every exception. Dispatches to
 *  specific handlers based on the interrupt number.
 *
 *  For most exceptions, we panic — there's no recovery from a CPU
 *  exception in a kernel without proper exception handling.
 *  Page fault is special: it calls page_fault_handler() with the
 *  error code for a detailed diagnostic.
 * ───────────────────────────────────────────────────────────────── */
void isr_handler(registers_t* regs) {
    /* Page fault gets its own detailed handler */
    if (regs->int_no == 14) {
        page_fault_handler(regs->err_code);
        /* page_fault_handler doesn't return */
    }

    /* All other exceptions → panic with a descriptive message */
    const char* name = "Unknown exception";
    switch (regs->int_no) {
        case 0:  name = "Divide-by-zero"; break;
        case 1:  name = "Debug"; break;
        case 2:  name = "Non-maskable interrupt"; break;
        case 3:  name = "Breakpoint"; break;
        case 4:  name = "Overflow"; break;
        case 5:  name = "Bound range exceeded"; break;
        case 6:  name = "Invalid opcode"; break;
        case 7:  name = "Device not available"; break;
        case 8:  name = "Double fault"; break;
        case 9:  name = "Coprocessor segment overrun"; break;
        case 10: name = "Invalid TSS"; break;
        case 11: name = "Segment not present"; break;
        case 12: name = "Stack-segment fault"; break;
        case 13: name = "General protection fault"; break;
        case 14: name = "Page fault"; break;   /* handled above */
        case 15: name = "Reserved"; break;
        case 16: name = "x87 floating-point exception"; break;
        case 17: name = "Alignment check"; break;
        case 18: name = "Machine check"; break;
        case 19: name = "SIMD floating-point exception"; break;
        case 20: name = "Virtualization exception"; break;
        case 21: name = "Control protection exception"; break;
        case 30: name = "Security exception"; break;
        default: name = "Reserved exception"; break;
    }

    __asm__ volatile ("cli");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
    vga_clear();

    vga_write("\n\n");
    vga_write("+==========================================+\n");
    vga_write("|       *** CPU EXCEPTION ***              |\n");
    vga_write("+==========================================+\n");
    vga_write("\n");

    vga_write("Exception: ");
    vga_write(name);
    vga_write(" (#");
    print_uint(regs->int_no);
    vga_write(")\n");

    vga_write("Error code: 0x");
    print_hex(regs->err_code);
    vga_write("\n");

    vga_write("EIP: 0x");
    print_hex(regs->eip);
    vga_write("\n");

    vga_write("CS:  0x");
    print_hex(regs->cs);
    vga_write("  EFLAGS: 0x");
    print_hex(regs->eflags);
    vga_write("\n");

    /* Helpful hints for common exceptions */
    if (regs->int_no == 0) {
        vga_write("\nHint: Division by zero. Check your math.\n");
    } else if (regs->int_no == 6) {
        vga_write("\nHint: Bad instruction. EIP may point into data, or\n");
        vga_write("      the instruction is invalid for this CPU.\n");
    } else if (regs->int_no == 8) {
        vga_write("\nHint: Double fault. Usually means an exception handler\n");
        vga_write("      itself crashed. Check the IDT and stack.\n");
    } else if (regs->int_no == 13) {
        vga_write("\nHint: General protection fault. Common causes:\n");
        vga_write("  - Accessing unmapped segment\n");
        vga_write("  - Writing to read-only segment\n");
        vga_write("  - Privilege violation\n");
    }

    vga_write("\nSystem halted. Please reboot.\n");
    while (1) __asm__ volatile ("hlt");
}