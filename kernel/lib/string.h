#ifndef STRING_H
#define STRING_H

/* Include guard: prevents double-inclusion in the same translation unit.
 * Naming it STRING_H matches the real <string.h>, so if the standard
 * library is ever available, only one definition will be used. */

/* Bring in the fixed-width integer types (uint8_t, uint32_t, ...)
 * and the size/pointer-difference types (size_t, ptrdiff_t, NULL). */
#include "stdint.h"
#include "stddef.h"

/* ─────────────────────────────────────────────────────────────────
 *  Freestanding reimplementation of <string.h>
 *
 *  In a hosted C environment, these functions are provided by the
 *  C library (glibc, musl, etc.). In a kernel, you must implement
 *  them yourself. This header just declares them — the actual
 *  implementations live in string.c.
 *
 *  Note: the `restrict` qualifier on memcpy tells the compiler that
 *  the source and destination pointers do not overlap, enabling
 *  better optimization. It's a C99 feature.
 * ───────────────────────────────────────────────────────────────── */

/* Return the length of a null-terminated string, excluding the
 * terminating '\0'. Returns 0 for an empty string.
 *
 *   strlen("hello") == 5 */
size_t strlen(const char* str);

/* Compare two null-terminated strings lexicographically.
 *
 * Returns:
 *   < 0  if s1 < s2
 *     0  if s1 == s2
 *   > 0  if s1 > s2
 *
 * The exact return value is implementation-defined (typically the
 * difference of the first mismatching bytes). */
int strcmp(const char* s1, const char* s2);

/* Like strcmp, but compares at most `n` characters.
 * Stops early if either string ends first.
 *
 * Useful for checking prefixes:
 *   strncmp("hello world", "hello", 5) == 0   // true */
int strncmp(const char* s1, const char* s2, size_t n);

/* Copy the null-terminated string `src` into `dest`, including the
 * terminating '\0'. Returns `dest` (for call chaining).
 *
 * WARNING: `dest` must be large enough to hold `src`, otherwise
 * this causes a buffer overflow. No bounds checking is performed. */
char* strcpy(char* dest, const char* src);

/* Find the first occurrence of character `ch` in the string `str`.
 *
 * Returns a pointer to the matching character, or NULL if not found.
 * If `ch` is '\0', it returns a pointer to the terminating null byte.
 *
 * `ch` is declared as `int` (not `char`) to match the standard
 * signature — the character is converted to `char` internally. */
char* strchr(const char* str, int ch);

/* Fill the first `size` bytes of memory at `bufptr` with the byte
 * value `value`. Returns `bufptr` (for call chaining).
 *
 * Commonly used to zero memory:
 *   memset(ptr, 0, 4096);      // clear a page
 *   memset(&node, 0, sizeof(node)); */
void* memset(void* bufptr, int value, size_t size);

/* Copy `size` bytes from `srcptr` to `dstptr`. Returns `dstptr`.
 *
 * The `restrict` keyword promises that the two buffers do not overlap.
 * If they might overlap, use memmove instead (which you haven't
 * implemented yet).
 *
 * WARNING: This is a raw byte copy. It does NOT stop at '\0' —
 * use strcpy for strings, memcpy for raw binary data. */
void* memcpy(void* restrict dstptr, const void* restrict srcptr, size_t size);

/* Print a 32-bit unsigned integer in hexadecimal, 0x-prefixed.
 * Example: print_hex(0xDEADBEEF) → "0xdeadbeef" */
void print_uint(uint32_t value);   /* print decimal */
void print_hex(uint32_t value);    /* print hex, 0x-prefixed */


#endif  /* STRING_H */