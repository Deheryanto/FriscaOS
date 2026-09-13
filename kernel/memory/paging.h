#ifndef PAGING_H
#define PAGING_H

/* Include guard: prevents double-inclusion in the same translation unit. */

#include "../lib/stdint.h"   /* For uint32_t */

/* ─────────────────────────────────────────────────────────────────
 *  x86 Paging (32-bit protected mode)
 *
 *  Paging translates virtual addresses to physical addresses using
 *  a two-level page table hierarchy:
 *
 *    Virtual address (32 bits)
 *    ┌───────────┬───────────┬──────────────┐
 *    │ PDE index │ PTE index │ Page offset     │
 *    │ 10 bits   │ 10 bits   │  12 bits        │
 *    └───────────┴───────────┴──────────────┘
 *         │           │             │
 *         │           │             └─ 0..4095 within a 4 KB page
 *         │           └─ 0..1023 entries in the page table
 *         └─ 0..1023 entries in the page directory
 *
 *  The CPU walks:
 *    CR3 → Page Directory → Page Table → Physical Page
 *
 *  Each PDE and PTE is a 32-bit value:
 *    ┌───────────────────────┬───────────────┐
 *    │ Physical address      │ Flags            │
 *    │ bits 31..12           │ bits 11..0       │
 *    └───────────────────────┴───────────────┘
 * ───────────────────────────────────────────────────────────────── */

/* ── Page/PDE/PTE flag bits ───────────────────────────────────── */

/* Bit 0: Present.
 * If 0, accessing this page causes a page fault (#PF, vector 14).
 * Must be set for the entry to be usable. */
#define PAGE_PRESENT  0x1

/* Bit 1: Read/Write.
 * If 0, the page is read-only.
 * If 1, the page is writable.
 * (Write to a read-only page in ring 0 causes #PF.) */
#define PAGE_RW       0x2

/* Bit 2: User/Supervisor.
 * If 0, only ring 0 (kernel) can access the page.
 * If 1, ring 3 (user mode) can also access it.
 * Currently unused — this kernel has no user mode yet. */
#define PAGE_USER     0x4

/* Other flags exist but aren't needed yet:
 *   0x008  PWT   — Page Write-Through (cache behavior)
 *   0x010  PCD   — Page Cache Disable
 *   0x020  A     — Accessed (set by CPU on access)
 *   0x040  D     — Dirty (set by CPU on write)
 *   0x080  PS    — Page Size (for 4 MB pages in PDE)
 *   0x100  PAT   — Page Attribute Table (cache type)
 *   0x200  G     — Global (don't flush on CR3 reload)
 */

/* ─────────────────────────────────────────────────────────────────
 *  Public API
 * ───────────────────────────────────────────────────────────────── */

/* Initialize paging.
 *
 * Currently:
 *   - Allocates a page directory from the PMM
 *   - Allocates 4 page tables to identity-map the first 16 MB
 *   - Loads CR3 and enables paging
 *
 * "Identity map" means virtual address == physical address for
 * the mapped region — the kernel continues running at the same
 * addresses it was loaded at. Without this, enabling paging would
 * instantly crash (the very next instruction fetch would be unmapped).
 *
 * Called once at boot, after pmm_init and heap_init. */
void paging_init(void);

/* Load a page directory into the CPU's CR3 register.
 *
 *   mov cr3, page_directory
 *
 * This changes the active address space. All memory accesses now
 * go through the new page directory.
 *
 * WARNING: The page directory must be physically 4 KB-aligned
 * (which pmm_alloc_block satisfies) and must already contain a
 * valid mapping for the *currently executing* code — otherwise
 * the CPU faults immediately after the mov.
 *
 * Exposed so future code can switch address spaces (e.g., for
 * user processes). For now, only called from paging_init. */
void load_page_directory(uint32_t* page_directory);

/* Enable paging.
 *
 * Sets bit 31 (PG) of CR0:
 *   mov cr0, cr0 | 0x80000000
 *
 * After this instruction, the MMU is active. Every memory access
 * goes through the page tables.
 *
 * The page directory must already be loaded into CR3 and must
 * contain a valid mapping for the current instruction pointer
 * and stack — otherwise the CPU triple-faults.
 *
 * Also exposed for symmetry with load_page_directory — real code
 * usually calls these together when switching address spaces. */
void enable_paging(void);

#endif  /* PAGING_H */