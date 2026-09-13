#ifndef STDINT_H
#define STDINT_H

/* Include guard: prevents this file from being included more than
 * once per translation unit, which would cause "redefinition" errors.
 * The name STDINT_H matches the one used by the real C standard header
 * <stdint.h>, so if the standard library is ever available, this file
 * would be skipped (or vice versa). */

/* ─────────────────────────────────────────────────────────────────
 *  Fixed-width integer types (freestanding reimplementation of <stdint.h>)
 *
 *  In a freestanding environment (like an OS kernel), you can't
 *  rely on the compiler-provided <stdint.h>. This header defines the
 *  standard fixed-width types so kernel code can use them portably.
 *
 *  Sizes on a 32-bit x86 target:
 *    char      = 1 byte
 *    short     = 2 bytes
 *    int       = 4 bytes
 *    long long = 8 bytes
 * ───────────────────────────────────────────────────────────────── */

/* ── Unsigned integer types ───────────────────────────────────── */

/* 8-bit unsigned: 0 to 255 */
typedef unsigned char      uint8_t;

/* 16-bit unsigned: 0 to 65535 */
typedef unsigned short     uint16_t;

/* 32-bit unsigned: 0 to 4,294,967,295 */
typedef unsigned int       uint32_t;

/* 64-bit unsigned: 0 to 18,446,744,073,709,551,615 */
typedef unsigned long long uint64_t;

/* ── Signed integer types ─────────────────────────────────────── */

/* 8-bit signed: -128 to 127 */
typedef signed char        int8_t;

/* 16-bit signed: -32,768 to 32,767 */
typedef signed short       int16_t;

/* 32-bit signed: -2,147,483,648 to 2,147,483,647 */
typedef signed int         int32_t;

/* 64-bit signed: -9,223,372,036,854,775,808 to 9,223,372,036,854,775,807 */
typedef signed long long   int64_t;

/* ── Pointer-sized integer types ──────────────────────────────── */

/* An unsigned integer large enough to hold a pointer.
 * On 32-bit x86: 4 bytes (same as uint32_t).
 * On 64-bit x86_64: would need to be 8 bytes (unsigned long long). */
typedef unsigned int       uintptr_t;

/* Signed version of uintptr_t — used for pointer differences. */
typedef signed int         intptr_t;

#endif  /* STDINT_H */