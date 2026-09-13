#include "string.h"


/* ─────────────────────────────────────────────────────────────────
 *  strlen — count characters until the null terminator
 *
 *  Iterates byte-by-byte using array indexing rather than pointer
 *  arithmetic. Either works; this form is arguably more readable.
 *  No bounds check — relies on the '\0' terminator being present.
 * ───────────────────────────────────────────────────────────────── */
size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len]) len++;  /* '\0' is 0 → loop stops */
    return len;              /* Length excludes the terminator */
}

/* ─────────────────────────────────────────────────────────────────
 *  strcmp — lexicographic comparison of two strings
 *
 *  Walk both strings in lock-step while:
 *    - s1 hasn't hit its terminator, AND
 *    - the current characters are equal
 *
 *  When the loop exits, either:
 *    - characters differ, OR
 *    - s1 hit '\0' (so s2 either also ended, or is longer)
 *
 *  Return the *difference* of the two differing bytes so callers
 *  can use the result for sorting (strcmp(a,b) < 0 means a < b).
 *
 *  Cast to `unsigned char*` is IMPORTANT:
 *    - On x86, `char` is signed.
 *    - Signed char values >127 become negative, which would give
 *      wrong results when comparing (e.g., '\x80' vs '\x01').
 *    - The C standard requires comparison as `unsigned char`.
 * ───────────────────────────────────────────────────────────────── */
int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

/* ─────────────────────────────────────────────────────────────────
 *  strncmp — compare at most `n` characters
 *
 *  Same as strcmp, but the loop also terminates after `n` chars.
 *
 *  Note the check `if (n == 0) return 0;`:
 *    If n reached 0, the strings matched for all n characters,
 *    so they're considered equal *for this comparison*.
 *
 *  If n didn't reach 0, the loop ended early because:
 *    - a character differed, OR
 *    - one string ended
 *    In either case, return the byte difference as before.
 * ───────────────────────────────────────────────────────────────── */
int strncmp(const char* s1, const char* s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

/* ─────────────────────────────────────────────────────────────────
 *  strcpy — copy a null-terminated string
 *
 *  The expression `(*dest++ = *src++)` does four things in one line:
 *    1. Read the character at *src
 *    2. Write it to *dest
 *    3. Advance both pointers
 *    4. Yield the copied character as the expression's value
 *
 *  The loop continues while the copied character is non-zero
 *  (i.e., not '\0'). When '\0' is copied, the loop exits — so
 *  the terminator IS written, then the loop stops.
 *
 *  `saved` remembers the original dest pointer because we advance
 *  `dest` during the copy — the caller expects the *original*
 *  dest pointer back (for chaining, e.g., `printf("%s", strcpy(...))`).
 * ───────────────────────────────────────────────────────────────── */
char* strcpy(char* dest, const char* src) {
    char* saved = dest;
    while ((*dest++ = *src++));
    return saved;
}

/* ─────────────────────────────────────────────────────────────────
 *  strchr — find the first occurrence of a character
 *
 *  Loop while the current byte doesn't match `ch`.
 *  If we hit '\0' before finding `ch`, the string ended → return NULL.
 *
 *  The cast `(char)ch` narrows the `int` argument to a byte for
 *  comparison. The standard defines the search using `(char)ch`.
 *
 *  Special case: if ch == '\0', the loop condition is false on
 *  the terminator itself, so we return a pointer to the '\0' —
 *  matching the standard behavior.
 *
 *  The final `(char*)str` cast is needed because `str` is `const char*`
 *  but the return type is `char*` (a historical oddity of the standard).
 * ───────────────────────────────────────────────────────────────── */
char* strchr(const char* str, int ch) {
    while (*str != (char)ch) {
        if (!*str) return NULL;
        str++;
    }
    return (char*)str;
}

/* ─────────────────────────────────────────────────────────────────
 *  memset — fill `size` bytes with a constant byte value
 *
 *  Cast to `unsigned char*` so we write exactly one byte per step —
 *  if we cast to `int*`, we'd write 4 bytes per iteration (wrong).
 *
 *  `value` is `int` because C promotes small integer arguments to
 *  `int` in function calls; we narrow it back to `unsigned char`
 *  when storing. Only the low 8 bits matter.
 *
 *  Returns the original `bufptr` (not the advanced `buf`) so callers
 *  can chain, e.g.:  `memcpy(memset(buf, 0, n), src, n)`
 * ───────────────────────────────────────────────────────────────── */
void* memset(void* bufptr, int value, size_t size) {
    unsigned char* buf = (unsigned char*) bufptr;
    for (size_t i = 0; i < size; i++)
        buf[i] = (unsigned char) value;
    return bufptr;
}


/* ─────────────────────────────────────────────────────────────────
 *  memcpy — copy `size` bytes from src to dst
 *
 *  The `restrict` qualifiers promise the two buffers don't overlap.
 *  This lets the compiler optimize more aggressively (e.g., use
 *  SIMD instructions, reorder loads/stores).
 *
 *  Casts to `unsigned char*` because we copy bytes, not typed
 *  elements. The `const` on `src` prevents accidental writes.
 *
 *  A byte-by-byte loop is the simplest correct implementation.
 *  A real libc would use wider copies (4/8/16/32 bytes at a time)
 *  and alignment tricks — but this is fine for a hobby kernel.
 * ───────────────────────────────────────────────────────────────── */
void* memcpy(void* restrict dstptr, const void* restrict srcptr, size_t size) {
    unsigned char* dst = (unsigned char*) dstptr;
    const unsigned char* src = (const unsigned char*) srcptr;
    for (size_t i = 0; i < size; i++)
        dst[i] = src[i];
    return dstptr;
}