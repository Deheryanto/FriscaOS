#ifndef VFS_H
#define VFS_H

/* Include guard: prevents double-inclusion in the same translation unit. */

#include "../lib/stdint.h"     /* For uint8_t, uint32_t */
#include "../lib/stddef.h"     /* For size_t (not used yet, but standard) */

/* ─────────────────────────────────────────────────────────────────
 *  Virtual File System (VFS) — minimal RAM-based filesystem
 *
 *  This VFS stores files entirely in RAM (in the kernel heap).
 *  It's a flat namespace: no directories, no paths, no permissions.
 *  Files are indexed by name and live until reboot.
 *
 *  Design goals:
 *    - Provide a file abstraction the shell can use (ls, cat, touch)
 *    - Keep the API small and simple
 *    - Serve as a placeholder for a real on-disk filesystem later
 * ───────────────────────────────────────────────────────────────── */

/* ── File type flags ──────────────────────────────────────────── */

/* Regular file (has content, can be read/written).
 * Defined as a bit flag so future types can be OR'd together. */
#define FS_FILE       0x01

/* Directory — reserved for future use. Currently no code creates
 * or handles directories, but the flag exists so the VFS API can
 * grow without breaking existing code. */
#define FS_DIRECTORY  0x02

/* ── Limits ──────────────────────────────────────────────────── */

/* Maximum length of a filename, INCLUDING the null terminator.
 * So the longest usable name is MAX_FILENAME - 1 = 31 characters.
 * Names longer than this are silently truncated in vfs_create_file. */
#define MAX_FILENAME  32

/* Maximum number of files the VFS can hold.
 * The file table is a fixed-size static array — no dynamic growth. */
#define MAX_FILES     16

/* ─────────────────────────────────────────────────────────────────
 *  File node — one entry in the VFS file table
 *
 *  This represents a single file in memory:
 *
 *    ┌──────────────────────────────┐
 *    │ name[MAX_FILENAME]           │  ← null-terminated filename
 *    ├──────────────────────────────┤
 *    │ flags                        │  ← FS_FILE or FS_DIRECTORY
 *    ├──────────────────────────────┤
 *    │ length                       │  ← size of content in bytes
 *    ├──────────────────────────────┤
 *    │ buffer ──────────────────────┼──► heap-allocated content
 *    └──────────────────────────────┘
 *
 *  The `buffer` points to heap memory allocated by kmalloc().
 *  It is NOT part of the struct — it's a separate allocation.
 * ───────────────────────────────────────────────────────────────── */

typedef struct vfs_node {
    char name[MAX_FILENAME];   /* Filename (null-terminated) */
    uint32_t flags;            /* FS_FILE, FS_DIRECTORY, ... */
    uint32_t length;           /* Size of file content in bytes */
    uint8_t* buffer;           /* Pointer to heap-allocated content */
} vfs_node_t;

/* ─────────────────────────────────────────────────────────────────
 *  Public API
 * ───────────────────────────────────────────────────────────────── */

/* Initialize the VFS: clear all file slots in the file table.
 * Called once at boot before any file operations. */
void vfs_init(void);

/* Create a new file with the given name and content.
 *
 * The content string is copied into a heap buffer, so the caller
 * can free/modify their copy afterward. The name must be unique.
 *
 * Returns:
 *    0  = success
 *   -1  = file table is full (>= MAX_FILES files exist)
 *   -2  = a file with this name already exists
 *   -3  = heap allocation failed (out of memory)
 *
 * Note: the name is truncated to MAX_FILENAME - 1 chars if longer. */
int vfs_create_file(const char* name, const char* content);

/* Look up a file by name.
 *
 * Returns a pointer to the file node (with its `length` and
 * `buffer` fields populated), or NULL if no such file exists.
 *
 * WARNING: the returned pointer points directly into the internal
 * file table — do not modify it, and do not hold onto it after
 * the VFS is reinitialized. */
vfs_node_t* vfs_read_file(const char* name);

/* Print a listing of all files to the VGA screen.
 * Format: "Name" column (33 chars wide) + "Size (Bytes)" column.
 * Prints "No files found." if the table is empty. */
void vfs_list_files(void);

#endif  /* VFS_H */