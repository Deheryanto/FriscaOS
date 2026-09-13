#include "drivers/vga/vga.h"
#include "drivers/timer/timer.h"
#include "arch/i386/idt.h"
#include "memory/pmm.h"
#include "memory/heap.h"
#include "memory/paging.h"
#include "fs/vfs.h"
#include "shell.h"

/* ─────────────────────────────────────────────────────────────────
 *  kernel_main — entry point of the kernel (C portion)
 *
 *  Called from the assembly bootstrap (_start) after the CPU is
 *  switched to 32-bit protected mode and the stack is set up.
 *
 *  Responsibilities, in order:
 *    1. Initialize display (so we can print diagnostics)
 *    2. Initialize interrupts (IDT + PIC), then enable interrupts
 *    3. Initialize the timer (starts periodic IRQ0)
 *    4. Initialize memory management (PMM → heap → paging)
 *    5. Initialize the filesystem
 *    6. Initialize the shell (prints banner + first prompt)
 *    7. Enter an idle loop, waking only on interrupts
 *
 *  Order matters: later subsystems depend on earlier ones.
 *  For example:
 *    - heap_init() needs pmm_init()  (heap gets blocks from PMM)
 *    - paging_init() needs pmm_init() (page tables are PMM blocks)
 *    - shell_init() needs vga_init()  (to print the banner)
 *    - Any command using sleep() needs timer_init() + idt_init()
 *
 *  This function never returns — there's nothing to return to.
 * ───────────────────────────────────────────────────────────────── */
void kernel_main(void) {
    /* ── 1. Display ─────────────────────────────────────────
     * Set up VGA text mode: clear the screen, set default color,
     * initialize the cursor. Must come first so we can see output
     * from the other initializers (and any errors). */
    vga_init();

    /* ── 2. Interrupts ──────────────────────────────────────
     * Build the IDT, remap the PIC (so IRQs land at 0x20+), and
     * register handlers for IRQ0 (timer) and IRQ1 (keyboard).
     * Ends with `sti` — interrupts are now enabled globally. */
    idt_init();

    /* ── 3. Timer ───────────────────────────────────────────
     * Program the PIT to fire IRQ0 at 100 Hz (one tick per 10 ms).
     * From now on, `timer_get_ticks()` advances and `sleep()` works.
     * The timer handler was already registered by idt_init(). */
    timer_init(100);

    /* ── 4. Memory management ───────────────────────────────
     * Layered, in this order:
     *
     *   pmm_init(16 MB) → sets up the physical bitmap, marks the
     *                     first 1 MB as reserved, and frees the
     *                     rest (1 MB – 16 MB).
     *
     *   heap_init()     → allocates a few PMM blocks for the kernel
     *                     heap and sets up the first free node.
     *                     After this, kmalloc/kfree work.
     *
     *   paging_init()   → allocates a page directory and 4 page
     *                     tables from the PMM, sets up an identity
     *                     map for 0 – 16 MB, then loads CR3 and
     *                     enables paging via CR0.PG.
     *
     * Note: heap must come before paging so the heap's memory is
     * already reserved before the identity map is finalized. In
     * this kernel it doesn't strictly matter (both use PMM blocks),
     * but it's a good habit as the kernel grows. */
    pmm_init(16 * 1024 * 1024);   /* 16 MB of physical RAM */
    heap_init();
    paging_init();

    /* ── 5. Filesystem ──────────────────────────────────────
     * Initialize the RAM-based VFS: clear the file table. From
     * here, the shell can create/list/read files. No files exist
     * yet — the user creates them with the `write` command. */
    vfs_init();

    /* ── 6. Shell ───────────────────────────────────────────
     * Print the welcome banner and the first `frisca-os> ` prompt.
     * The keyboard IRQ (already wired up in idt_init) now feeds
     * characters into `shell_input()`. */
    shell_init();

    /* ── 7. Idle loop ───────────────────────────────────────
     * Everything from here on happens in interrupt handlers —
     * the main thread of execution has nothing left to do.
     *
     * `hlt` puts the CPU into a low-power state until the next
     * interrupt (timer tick, key press, etc.). The interrupt
     * handler runs, returns, and we hlt again.
     *
     * This is preferable to a busy loop (`while(1);`) because:
     *   - It uses far less power.
     *   - It generates less heat (important on real hardware).
     *   - It leaves the memory bus free for DMA (future: disks, NIC).
     *
     * `volatile` prevents the compiler from optimizing the loop
     * away or hoisting the hlt out of it. */
    while (1) {
        __asm__ volatile("hlt");
    }
}