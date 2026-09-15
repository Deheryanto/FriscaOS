#include "panic.h"
#include "string.h"
#include "../drivers/vga/vga.h"

/* ─────────────────────────────────────────────────────────────────
 *  kernel_panic — fatal error, halt the system
 * ───────────────────────────────────────────────────────────────── */
void kernel_panic(const char* msg) {
    /* 1. Stop all interrupts — we're done. */
    __asm__ volatile ("cli");

    /* 2. Red screen for attention. */
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
    vga_clear();

    /* 3. Print the panic banner. */
    vga_write("\n\n");
    vga_write("+==========================================+\n");
    vga_write("|          *** KERNEL PANIC ***            |\n");
    vga_write("+==========================================+\n");
    vga_write("\n");

    /* 4. Print the caller's message. */
    if (msg) {
        vga_write(msg);
        vga_write("\n");
    }

    vga_write("\nSystem halted. Please reboot.\n");

    /* 5. Halt forever. If an NMI wakes the CPU, halt again. */
    while (1) {
        __asm__ volatile ("hlt");
    }
}

/* ─────────────────────────────────────────────────────────────────
 *  page_fault_handler — vector 14 (#PF)
 *
 *  Called when the CPU tries to access an invalid virtual address.
 *  The faulting address is in CR2; the error code on the stack
 *  tells us why the fault happened.
 *
 *  Error code bit layout (Intel SDM Vol 3, 4.7):
 *    bit 0 (P)   : 0 = not-present, 1 = protection violation
 *    bit 1 (W/R) : 0 = read, 1 = write
 *    bit 2 (U/S) : 0 = kernel, 1 = user
 *    bit 3 (RSVD): 1 = reserved bit set in page table
 *    bit 4 (I/D) : 1 = instruction fetch (with NX)
 * ───────────────────────────────────────────────────────────────── */
void page_fault_handler(uint32_t error_code) {
    /* Read the faulting linear address from CR2 */
    uint32_t fault_addr;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(fault_addr));

    __asm__ volatile ("cli");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
    vga_clear();

    vga_write("\n\n");
    vga_write("+==========================================+\n");
    vga_write("|          *** PAGE FAULT ***              |\n");
    vga_write("+==========================================+\n");
    vga_write("\n");

    vga_write("Faulting address: 0x");
    print_hex(fault_addr);
    vga_write("\n");

    vga_write("Error code:       0x");
    print_hex(error_code);
    vga_write("  (");

    /* Decode the error code into human-readable text */
    vga_write((error_code & 0x1) ? "protection" : "not-present");
    vga_write(", ");
    vga_write((error_code & 0x2) ? "write" : "read");
    vga_write(", ");
    vga_write((error_code & 0x4) ? "user" : "kernel");
    if (error_code & 0x8) vga_write(", reserved-bit");
    if (error_code & 0x10) vga_write(", instruction-fetch");

    vga_write(")\n\n");

    /* Try to be helpful — explain the most common cases */
    if (!(error_code & 0x1)) {
        vga_write("Hint: The page is not mapped. Common causes:\n");
        vga_write("  - Dereferencing a NULL or wild pointer\n");
        vga_write("  - Accessing memory beyond the identity-mapped 16 MB\n");
        vga_write("  - Forgetting to map a page before using it\n");
    } else if ((error_code & 0x1) && (error_code & 0x2)) {
        vga_write("Hint: Write to a read-only page.\n");
    } else if (error_code & 0x4) {
        vga_write("Hint: User-mode access to kernel-only memory.\n");
    }

    vga_write("\nSystem halted. Please reboot.\n");

    while (1) {
        __asm__ volatile ("hlt");
    }
}