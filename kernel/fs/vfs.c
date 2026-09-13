#include "vfs.h"
#include "../memory/heap.h"
#include "../lib/string.h"
#include "../drivers/vga/vga.h"

/* In-memory file table — a simple flat array of file nodes.
 * This is NOT a real filesystem: files live only in RAM and
 * disappear on reboot. It's a RAM-disk / tmpfs-style VFS. */
static vfs_node_t file_table[MAX_FILES];

/* Number of files currently stored in the table.
 * Files occupy indices [0, file_count), in insertion order. */
static uint32_t file_count = 0;


/* Initialize the VFS by resetting all file slots to a clean state.
 * Called once at boot before any file operations. */
void vfs_init(void) {
    file_count = 0;

    /* Zero out every slot: empty name, no flags, no data */
    for (int i = 0; i < MAX_FILES; i++) {
        file_table[i].name[0] = '\0';   /* Empty string → slot is unused */
        file_table[i].flags = 0;        /* No flags (not FS_FILE) */
        file_table[i].length = 0;       /* Zero-length */
        file_table[i].buffer = NULL;    /* No data buffer */
    }
}

/* Create a new file with the given name and content.
 *
 * Returns:
 *    0  = success
 *   -1  = file table is full
 *   -2  = a file with this name already exists
 *   -3  = memory allocation failed */
int vfs_create_file(const char* name, const char* content) {
    /* Reject if the table is full */
    if (file_count >= MAX_FILES) return -1;

    /* Check for duplicate names — filenames must be unique */
    for (uint32_t i = 0; i < file_count; i++) {
        if (strcmp(file_table[i].name, name) == 0) {
            return -2;
        }
    }

    /* Measure content length and allocate a buffer for it.
     * +1 for the null terminator so it's a valid C string. */
    size_t len = strlen(content);
    uint8_t* buf = (uint8_t*)kmalloc(len + 1);
    if (!buf) return -3;

    /* Copy the content into the heap buffer and null-terminate it */
    memcpy(buf, content, len);
    buf[len] = '\0';

    /* Copy the filename into the file node, truncating if too long.
     * MAX_FILENAME includes the null terminator, so the longest
     * usable name is MAX_FILENAME - 1 characters. */
    size_t name_len = strlen(name);
    if (name_len >= MAX_FILENAME) name_len = MAX_FILENAME - 1;
    memcpy(file_table[file_count].name, name, name_len);
    file_table[file_count].name[name_len] = '\0';

    /* Fill in the rest of the file node */
    file_table[file_count].flags = FS_FILE;      /* Mark as a regular file */
    file_table[file_count].length = len;         /* Byte length (no terminator) */
    file_table[file_count].buffer = buf;         /* Pointer to content on heap */

    /* Consume one slot */
    file_count++;
    return 0;
}


/* Look up a file by name.
 *
 * Returns a pointer to the file node if found, or NULL otherwise.
 * The returned pointer is valid as long as the file isn't deleted —
 * it points directly into the static file_table, not a copy. */
vfs_node_t* vfs_read_file(const char* name) {
    for (uint32_t i = 0; i < file_count; i++) {
        if (strcmp(file_table[i].name, name) == 0) {
            return &file_table[i];
        }
    }
    return NULL;
}

/* Print a directory listing of all files to the VGA screen,
 * formatted as a table with columns "Name" and "Size (Bytes)". */
void vfs_list_files(void) {
    /* Special case: empty directory */
    if (file_count == 0) {
        vga_write("No files found.\n");
        return;
    }

    /* Print the header rows */
    vga_write("Name                             Size (Bytes)\n");
    vga_write("---------------------------------------------\n");
    for (uint32_t i = 0; i < file_count; i++) {
        vga_write(file_table[i].name);
        
        int spaces = 33 - strlen(file_table[i].name);
        if (spaces < 1) spaces = 1;
        for (int s = 0; s < spaces; s++) vga_putchar(' ');

        char num_str[12];
        int val = file_table[i].length;
        int idx = 0;
        if (val == 0) {
            num_str[idx++] = '0';
        } else {
            char temp[12];
            int t_idx = 0;
            while (val > 0) {
                temp[t_idx++] = '0' + (val % 10);
                val /= 10;
            }
            while (t_idx > 0) num_str[idx++] = temp[--t_idx];
        }
        num_str[idx] = '\0';

        vga_write(num_str);
        vga_write("\n");
    }
}