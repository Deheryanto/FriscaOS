#ifndef STDDEF_H
#define STDDEF_H

/* Include guard: prevents double-inclusion in the same translation unit.
 * Named to match the real C standard header <stddef.h>, so if the
 * standard library ever becomes available, only one definition wins. */

/* ─────────────────────────────────────────────────────────────────
 *  Freestanding reimplementation of <stddef.h>
 *
 *  In a hosted C environment, <stddef.h> is provided by the C library.
 *  In a freestanding environment (like an OS kernel), you must define
 *  these yourself. This is the minimal set needed by most kernel code.
 * ───────────────────────────────────────────────────────────────── */

/* The null pointer constant.
 *
 * Standard C defines NULL as an implementation-defined null pointer
 * constant. In C, `(void*)0` is the canonical choice. In C++, it would
 * be `0` or `nullptr` (since C++ doesn't allow implicit void* → T*).
 *
 * Every kernel uses NULL constantly — for failed allocations, empty
 * lists, sentinel values, etc. */
#define NULL ((void*)0)

/* `size_t` — unsigned integer type used for sizes and counts.
 *
 * On 32-bit i386, it's 4 bytes (unsigned int). The standard requires
 * it to be big enough to hold the size of any object in memory, so
 * on a 32-bit system the max object is 4 GB, and uint32_t is enough.
 *
 * Used by: strlen, memset, memcpy, kmalloc, pmm_alloc_block, etc. */
typedef unsigned int size_t;

/* `ptrdiff_t` — signed integer type for pointer differences.
 *
 * Result of `ptr1 - ptr2`. Must be signed (the difference can be
 * negative) and large enough to hold the difference of any two
 * pointers. On 32-bit i386, it's the signed counterpart of size_t.
 *
 * Used by: pointer subtraction, array indexing with negative offsets,
 *          allocator internal calculations, etc.
 *
 * Note: standard C actually places ptrdiff_t in <stddef.h> alongside
 * size_t, so defining it here is correct. */
typedef int ptrdiff_t;

#endif  /* STDDEF_H */