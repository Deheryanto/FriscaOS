#ifndef HEAP_H
#define HEAP_H

/* Include guard: prevents double-inclusion in the same translation unit. */

#include "../lib/stdint.h"     /* For uint8_t, uint32_t (used indirectly) */
#include "../lib/stddef.h"     /* For size_t — used in kmalloc's signature */

/* ─────────────────────────────────────────────────────────────────
 *  Kernel Heap Allocator
 *
 *  Provides dynamic memory allocation inside the kernel. This is
 *  the kernel equivalent of C's malloc/free — but:
 *    - Works on top of the PMM (physical memory manager)
 *    - Has no virtual memory / swapping yet
 *    - Uses a simple first-fit linked list with splitting & coalescing
 *
 *  The heap starts small (a few PMM blocks) and lives in physical
 *  memory directly — no paging magic yet.
 *
 *  Usage:
 *      char* buf = kmalloc(100);
 *      if (!buf) { out of memory }
 *      ... use buf ...
 *      kfree(buf);
 * ───────────────────────────────────────────────────────────────── */

/* Initialize the heap.
 *
 * Allocates the initial region from the PMM (currently 4 contiguous
 * 4 KB blocks = 16 KB) and sets up the first free node covering
 * that whole region.
 *
 * Must be called after pmm_init() and before any kmalloc/kfree. */
void heap_init(void);
#ifndef HEAP_H
#define HEAP_H

#include "../lib/stdint.h"
#include "../lib/stddef.h"

void heap_init(void);
void* kmalloc(size_t size);
void kfree(void* ptr);

#endif
/* Allocate `size` bytes from the kernel heap.
 *
 * Returns:
 *   - A pointer to the allocated block (4-byte aligned)
 *   - NULL if size == 0 or no free block is large enough
 *
 * Strategy:
 *   1. Round `size` up to a multiple of 4 (alignment)
 *   2. Walk the heap linked list, first-fit
 *   3. If the chosen block is much larger than needed, split it —
 *      the leftover becomes a new free node
 *
 * The returned pointer points to the *payload*, not the internal
 * header. The header lives immediately before it in memory.
 *
 * The caller is responsible for calling kfree() on the pointer
 * when done. */
void* kmalloc(size_t size);

/* Free a previously allocated block.
 *
 * `ptr` must be a pointer returned by kmalloc (or NULL, which is
 * silently ignored, matching free(3)).
 *
 * After marking the block free, kfree walks the heap to merge any
 * adjacent free nodes (coalescing) — this fights fragmentation.
 *
 * WARNING: passing an invalid pointer, a pointer to non-heap memory,
 * or freeing the same pointer twice will corrupt the heap. There is
 * no validation. */
void kfree(void* ptr);

#endif  /* HEAP_H */