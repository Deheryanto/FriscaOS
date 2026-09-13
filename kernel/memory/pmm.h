#ifndef PMM_H
#define PMM_H

/* Include guard: prevents double-inclusion in the same translation unit. */

#include "../lib/stdint.h"     /* For uint8_t, uint32_t */
#include "../lib/stddef.h"     /* For size_t (used indirectly) */

/* ─────────────────────────────────────────────────────────────────
 *  Physical Memory Manager (PMM)
 *
 *  Manages physical RAM in fixed-size blocks (typically 4 KB,
 *  matching the x86 page size). Tracks which blocks are free vs.
 *  used via a bitmap — one bit per block.
 *
 *  This is the LOWEST layer of memory management. Every other
 *  memory subsystem (heap, paging, VFS buffers) ultimately gets
 *  its memory from the PMM.
 *
 *  Physical memory map (typical PC, 32-bit):
 *
 *    0x00000000 - 0x000FFFFF  (1 MB)    reserved (BIOS, IVT, VGA, ROM)
 *    0x00100000 - ...                    usable RAM (kernel + heap)
 *
 *  The PMM marks the low 1 MB as used and treats the rest as free
 *  (up to a configurable limit — currently 16 MB).
 * ───────────────────────────────────────────────────────────────── */

/* Size of one physical memory block, in bytes.
 *
 * 4096 bytes = 4 KB, matching the x86 page size so that a PMM
 * block can be directly used as a page frame by the paging code.
 *
 * Keeping these in sync is important: if PMM_BLOCK_SIZE differed
 * from 4096, paging and the heap would disagree about alignment. */
#define PMM_BLOCK_SIZE 4096

/* ─────────────────────────────────────────────────────────────────
 *  Public API
 * ───────────────────────────────────────────────────────────────── */

/* Initialize the PMM with the total amount of physical RAM (in bytes).
 *
 * Currently does:
 *   - Computes the number of 4 KB blocks that fit in `mem_size`
 *   - Clamps to the bitmap's maximum capacity (16 MB / 4096 blocks)
 *   - Marks ALL blocks as used (safe default)
 *   - Frees all blocks starting from 1 MB
 *
 * Why start from 1 MB? The first megabyte is reserved for legacy
 * hardware structures (IVT, BDA, VGA memory at 0xB8000, BIOS ROM).
 * The kernel itself is loaded at 1 MB in most bootloaders.
 *
 * Must be called before heap_init and paging_init.
 *
 * Example: pmm_init(16 * 1024 * 1024)  // 16 MB */
void pmm_init(uint32_t mem_size);

/* Allocate one free 4 KB physical block.
 *
 * Returns the PHYSICAL address of the block (as a void*), or NULL
 * if no free block is available.
 *
 * Because the kernel currently uses identity mapping (virtual ==
 * physical), the returned pointer can be dereferenced directly.
 * Once paging moves the kernel to a higher-half virtual address,
 * callers will need to translate the physical address before using it.
 *
 * The returned address is always 4 KB aligned.
 *
 * Usage:
 *     void* block = pmm_alloc_block();
 *     if (!block) { handle out-of-memory } */
void* pmm_alloc_block(void);

/* Free a previously allocated 4 KB physical block.
 *
 * `addr` must be a pointer returned by pmm_alloc_block (4 KB aligned
 * and within the managed range). Passing an invalid address or
 * double-freeing is silently ignored — but doing so is a bug and
 * will not be reported.
 *
 * Usage:
 *     pmm_free_block(block);   // only if block was allocated
 *
 * Note: This does not free the memory's *contents* in any way —
 * there's no concept of zeroing or scrubbing. After freeing, the
 * block's previous data is still there until someone overwrites it. */
void pmm_free_block(void* addr);

/* Return the amount of free physical memory in bytes.
 *
 * Computed as: (max_blocks - used_blocks) * PMM_BLOCK_SIZE
 *
 * Useful for:
 *   - Debug output at boot ("X MB free")
 *   - Runtime memory pressure checks
 *   - Sanity checks in tests
 *
 * Note: the return value can be as large as 16 MB (the current
 * PMM limit), which fits in a uint32_t without overflow. If you
 * increase the limit beyond 4 GB, this would need to be uint64_t. */
uint32_t pmm_get_free_memory(void);

#endif  /* PMM_H */