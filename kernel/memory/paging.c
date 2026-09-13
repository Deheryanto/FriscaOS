#include "paging.h"
#include "pmm.h"


/* Initialize x86 paging with identity mapping for the first 16 MB.
 *
 * x86 uses a two-level page table structure (32-bit protected mode):
 *
 *   Virtual address (32 bits)
 *   ├─ bits 31-22: Page Directory index (10 bits → 1024 entries)
 *   ├─ bits 21-12: Page Table index     (10 bits → 1024 entries)
 *   └─ bits 11-0 : Offset within page   (12 bits → 4096 bytes = 4 KB)
 *
 * Each entry is 32 bits:
 *   ├─ bits 31-12: Physical address of page table / page frame
 *   └─ bits 11-0 : Flags (present, RW, user, etc.)
 *
 * Identity mapping = virtual address == physical address.
 * So writing to 0x100000 (virtual) also writes to 0x100000 (physical). */
void paging_init(void) {
    /* Allocate a physical 4 KB block to hold the page directory.
     * A page directory is exactly 4 KB = 1024 entries × 4 bytes. */
    uint32_t* page_directory = (uint32_t*)pmm_alloc_block();
    
    /* Initialize all 1024 page directory entries (PDEs).
     *
     * 0x00000002 = flags only:
     *   bit 0 (PAGE_PRESENT) = 0  → NOT present
     *   bit 1 (PAGE_RW)      = 1  → writable
     *   address bits         = 0  → no page table attached
     *
     * So every PDE is initially "not present" — we'll fill in the ones
     * we need below. Any access to an unmapped region will page fault. */
    for (int i = 0; i < 1024; i++) {
        page_directory[i] = 0x00000002;
    }

    /* Create 4 page tables to identity-map the first 16 MB of RAM.
     *
     * Why 4? Each page table maps 1024 pages × 4 KB = 4 MB.
     * 4 page tables × 4 MB = 16 MB, matching our PMM's max_blocks limit. */
    for (int t = 0; t < 4; t++) {
        /* Allocate a physical 4 KB block for this page table */
        uint32_t* page_table = (uint32_t*)pmm_alloc_block();

        /* Fill the 1024 entries (PTEs) of this page table.
         * Each entry maps one 4 KB page. */
        for (int i = 0; i < 1024; i++) {
            /* Compute the physical address this entry should point to.
             *
             *   t * 4 MB  → base of this page table's 4 MB region
             *   i * 4 KB  → offset of this specific page within that region
             *
             * Example: t=1, i=5 → 4 MB + 20 KB = 0x00405000 */
            uint32_t address = (t * 4 * 1024 * 1024) + (i * 0x1000);

            /* Store the address with flags:
             *   PAGE_PRESENT (bit 0) = 1 → page is valid
             *   PAGE_RW      (bit 1) = 1 → writable (not read-only)
             *
             * The low 12 bits of the address are always 0 because pages
             * are 4 KB aligned — so those bits are free for flags. */
            page_table[i] = address | PAGE_PRESENT | PAGE_RW;
        }

        /* Register this page table in the page directory.
         *
         * Store the page table's physical address with flags:
         *   PAGE_PRESENT = 1 (table exists)
         *   PAGE_RW      = 1 (writable)
         *
         * Index `t` in the PDE array corresponds to the top 10 bits of
         * the virtual address. PDE[t] covers virtual range [t*4MB, (t+1)*4MB). */
        page_directory[t] = ((uint32_t)page_table) | PAGE_PRESENT | PAGE_RW;
    }

    /* Load the page directory's physical address into CR3.
     * CR3 is the CPU register that points to the active page directory. */
    load_page_directory(page_directory);

    /* Set bit 31 (PG) in CR0 to enable paging.
     * From this moment on, every memory access goes through the MMU. */
    enable_paging();
}