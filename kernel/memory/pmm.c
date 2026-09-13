#include "pmm.h"
#include "../lib/string.h"

/* Bitmap-based Physical Memory Manager.
 *
 * Each bit in `pmm_bitmap` represents one physical memory block (page frame):
 *   - 0 = free
 *   - 1 = used/reserved
 *
 * Default assumption: 16 MB of RAM, 4 KB per block.
 *   16 MB / 4 KB = 4096 blocks
 *   Bitmap storage: 4096 bits / 32 bits-per-uint32 = 128 uint32_t entries*/
#define BITMAP_SIZE 128

/* The bitmap itself: one bit per physical 4 KB block.
 * 128 * 32 = 4096 bits = enough to track 4096 blocks = 16 MB. */
static uint32_t pmm_bitmap[BITMAP_SIZE];

/* Total number of blocks the PMM is allowed to manage */
static uint32_t max_blocks = 4096;

/* Number of blocks currently marked as used (allocated or reserved).
 * Starts at 4096 (everything used) until pmm_init marks free regions. */
static uint32_t used_blocks = 4096;

/* Mark bit `bit` as used (set to 1).
 * Uses bitwise OR with a mask where only bit (bit % 32) is set. */
static inline void set_bit(uint32_t bit) {
    pmm_bitmap[bit / 32] |= (1 << (bit % 32));
}

/* Mark bit `bit` as free (clear to 0).
 * Uses bitwise AND with the complement of the mask. */
static inline void clear_bit(uint32_t bit) {
    pmm_bitmap[bit / 32] &= ~(1 << (bit % 32));
}

/* Test whether bit `bit` is set (1 = used, 0 = free).
 * Returns non-zero if set, zero if clear. */
static inline int test_bit(uint32_t bit) {
    return pmm_bitmap[bit / 32] & (1 << (bit % 32));
}


/* Initialize the PMM with the total amount of physical RAM (in bytes).
 *
 * Strategy:
 *   1. Compute how many 4 KB blocks fit in `mem_size`.
 *   2. Clamp to the bitmap's maximum capacity (BITMAP_SIZE * 32 blocks).
 *   3. Mark ALL blocks as used initially (safe default — nothing is free yet).
 *   4. Free all blocks starting from 1 MB onward.
 *
 * Why start at 1 MB? The first megabyte of physical memory is typically
 * reserved for BIOS, the IVT, BDA, VGA memory (0xB8000), and other
 * legacy hardware structures. The kernel itself usually sits at 1 MB. */
void pmm_init(uint32_t mem_size) {
    /* Total blocks = RAM size / block size (4 KB) */
    max_blocks = mem_size / PMM_BLOCK_SIZE;

    /* Clamp to the maximum the bitmap can represent */
    if (max_blocks > BITMAP_SIZE * 32) {
        max_blocks = BITMAP_SIZE * 32;
    }
    
    /* Mark every block as USED (0xFFFFFFFF = all 32 bits set) */
    for (size_t i = 0; i < BITMAP_SIZE; i++) {
        pmm_bitmap[i] = 0xFFFFFFFF;
    }
    used_blocks = max_blocks;

    /* Free all blocks from 1 MB onward.
     *   1 MB / 4 KB = 256 → start_block = 256
     * Everything below 1 MB stays reserved. */
    uint32_t start_block = (1 * 1024 * 1024) / PMM_BLOCK_SIZE;
    for (uint32_t i = start_block; i < max_blocks; i++) {
        clear_bit(i);
        used_blocks--;
    }
}

/* Allocate one free 4 KB physical block.
 *
 * Returns the physical address of the block (as a void*),
 * or NULL (0) if no free block is available. */
void* pmm_alloc_block(void) {
    /* Quick check: any free blocks left? */
    if (max_blocks - used_blocks == 0) return 0;

    /* Linear scan — find the first free bit.
     * For 4096 blocks this is fast enough; a real OS would use
     * a free-list or a "last allocated index" hint for O(1) allocation. */
    for (uint32_t i = 0; i < max_blocks; i++) {
        if (!test_bit(i)) {
            set_bit(i); /* Mark as Used*/
            used_blocks++;
            return (void*)(i * PMM_BLOCK_SIZE); /* Physical address */
        }
    }
    return 0; /* Should never reach here if the check above was correct */
}

/* Free a previously allocated physical block.
 *
 * `addr` must be the address returned by pmm_alloc_block().
 * Silently ignores invalid or already-free addresses. */
void pmm_free_block(void* addr) {
    uint32_t block = (uint32_t)addr / PMM_BLOCK_SIZE;
    /* Bounds check + double-free protection */
    if (block < max_blocks && test_bit(block)) {
        clear_bit(block);
        used_blocks--;
    }
}

/* Return the amount of free physical memory in bytes */
uint32_t pmm_get_free_memory(void) {
    return (max_blocks - used_blocks) * PMM_BLOCK_SIZE;
}