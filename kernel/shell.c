#include "shell.h"
#include "memory/pmm.h"
#include "memory/heap.h"
#include "fs/vfs.h"
#include "drivers/vga/vga.h"
#include "drivers/timer/timer.h"
#include "lib/string.h"
#include "arch/i386/io.h"

#define MAX_COMMAND_LEN 256

static char command_buffer[MAX_COMMAND_LEN];
static size_t buffer_idx = 0;

extern volatile uint32_t bg_counter;

static void print_prompt(void) {
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_write("frisca-os> ");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
}

static void reboot(void) {
    vga_write("Rebooting system...\n");

    uint8_t temp;
    do {
        temp = inb(0x64);          /* Read keyboard controller status */
        if (temp & 1) inb(0x60);   /* If output buffer full, drain it */
    } while (temp & 2);            /* Wait until input buffer is empty */
    outb(0x64, 0xFE);              /* Pulse CPU reset line */
}


static void execute_command(void) {
    vga_write("\n");

    if (buffer_idx == 0) {
        print_prompt();
        return;
    }

    command_buffer[buffer_idx] = '\0';

    /* ── help ─────────────────────────────────────────────── */
    if (strcmp(command_buffer, "help") == 0) {
        vga_write("Available commands:\n");
        vga_write("  help      - Show this help message\n");
        vga_write("  version   - Display Frisca OS version\n");
        vga_write("  timer     - Show system uptime in seconds and ticks\n");
        vga_write("  testdelay - Test 2 seconds sleep delay\n");
        vga_write("  mem       - Show free physical memory\n");
        vga_write("  testmem   - Test Physical Memory Manager (PMM)\n");
        vga_write("  testheap  - Test Heap Allocator (kmalloc/kfree)\n");
        vga_write("  ps        - List running tasks & background counter\n");
        vga_write("  ls        - List all files in VFS RAMDisk\n");
        vga_write("  cat       - Display file content (Usage: cat <filename>)\n");
        vga_write("  write     - Write to file (Usage: write <filename> <content>)\n");
        vga_write("  clear     - Clear the screen\n");
        vga_write("  reboot    - Restart the system\n");

    /* ── version ──────────────────────────────────────────── */
    } else if (strcmp(command_buffer, "version") == 0) {
        vga_write("Frisca OS v0.1.0 (32-bit Protected Mode, Multitasking & VFS Active)\n");

    /* ── timer ────────────────────────────────────────────── */
    } else if (strcmp(command_buffer, "timer") == 0) {
        uint32_t ticks = timer_get_ticks();
        uint32_t seconds = ticks / 100;

        vga_write("System Uptime: ");
        print_uint(seconds);
        vga_write(" s (Total Ticks: ");
        print_uint(ticks);
        vga_write(")\n");

    /* ── testdelay ────────────────────────────────────────── */
    } else if (strcmp(command_buffer, "testdelay") == 0) {
        vga_write("Waiting 2 seconds...\n");
        sleep(2000);
        vga_write("Done!\n");

    /* ── mem ──────────────────────────────────────────────── */
    } else if (strcmp(command_buffer, "mem") == 0) {
        uint32_t free_mem = pmm_get_free_memory();
        vga_write("Free Memory: ");
        print_uint(free_mem / 1024);
        vga_write(" KB\n");

    /* ── testmem ──────────────────────────────────────────── */
    } else if (strcmp(command_buffer, "testmem") == 0) {
        vga_write("Allocating 1 Block (4KB)...\n");
        void* ptr1 = pmm_alloc_block();
        vga_write("Allocated Block Address: ");
        print_uint((uint32_t)ptr1);
        vga_write("\nFreeing block...\n");
        pmm_free_block(ptr1);
        vga_write("Done!\n");

    /* ── testheap ─────────────────────────────────────────── */
    } else if (strcmp(command_buffer, "testheap") == 0) {
        vga_write("Testing Heap Allocator (kmalloc)...\n");
        char* str = (char*)kmalloc(32);

        if (str) {
            vga_write("Memory allocated successfully!\n");
            vga_write("Freeing heap memory...\n");
            kfree(str);
            vga_write("Done!\n");
        } else {
            vga_write("Failed to allocate memory!\n");
        }

    } else if (strcmp(command_buffer, "crash") == 0) {
        vga_write("Triggering page fault...\n");
        volatile uint32_t* bad = (uint32_t*)0xDEADBEEF;
        *bad = 42;   /* Dereference invalid pointer */
        vga_write("Should not reach here!\n");
    
    } else if (strcmp(command_buffer, "divzero") == 0) {
        volatile int a = 10;
        volatile int b = 0;
        volatile int c = a / b;
        (void)c;                                   // suppress unused warning
        vga_write("Should not reach here!\n");


    /* ── ps ───────────────────────────────────────────────── */
    } else if (strcmp(command_buffer, "ps") == 0) {
        vga_write("Running Tasks:\n");
        vga_write("  PID 0: Shell (Main Task) [RUNNING]\n");
        vga_write("  PID 1: Background Counter Task [RUNNING]\n");
        
        vga_write("Background Task Counter: ");
        print_uint(bg_counter);
        vga_write("\n");

    /* ── ls ───────────────────────────────────────────────── */
    } else if (strcmp(command_buffer, "ls") == 0) {
        vfs_list_files();

    /* ── cat <filename> ───────────────────────────────────── */
    } else if (strncmp(command_buffer, "cat ", 4) == 0) {
        char* filename = command_buffer + 4;

        if (strlen(filename) == 0) {
            vga_write("Usage: cat <filename>\n");
        } else {
            vfs_node_t* file = vfs_read_file(filename);
            if (file && file->buffer) {
                vga_write((char*)file->buffer);
                vga_write("\n");
            } else {
                vga_write("File not found!\n");
            }
        }

    /* ── write <filename> <content> ───────────────────────── */
    } else if (strncmp(command_buffer, "write ", 6) == 0) {
        char* args = command_buffer + 6;
        char* space_ptr = strchr(args, ' ');

        if (space_ptr) {
            *space_ptr = '\0';
            char* filename = args;
            char* content = space_ptr + 1;

            if (strlen(filename) > 0 && strlen(content) > 0) {
                int res = vfs_create_file(filename, content);
                if (res == 0) {
                    vga_write("File created successfully!\n");
                } else if (res == -2) {
                    vga_write("Error: File already exists!\n");
                } else {
                    vga_write("Error creating file!\n");
                }
            } else {
                vga_write("Usage: write <filename> <content>\n");
            }
        } else {
            vga_write("Usage: write <filename> <content>\n");
        }

    /* ── clear ────────────────────────────────────────────── */
    } else if (strcmp(command_buffer, "clear") == 0) {
        vga_clear();

    /* ── reboot ───────────────────────────────────────────── */
    } else if (strcmp(command_buffer, "reboot") == 0) {
        reboot();

    /* ── unknown command ──────────────────────────────────── */
    } else {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_write("Unknown command: ");
        vga_write(command_buffer);
        vga_write("\nType 'help' for a list of commands.\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    }

    buffer_idx = 0;
    print_prompt();
}

void shell_init(void) {
    buffer_idx = 0;

    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_write("=== Welcome to Frisca OS Shell ===\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);

    print_prompt();
}

void shell_input(char c) {
    if (c == '\n') {
        execute_command();

    } else if (c == '\b') {
        if (buffer_idx > 0) {
            buffer_idx--;
            vga_putchar('\b');
        }

    } else {
        if (buffer_idx < MAX_COMMAND_LEN - 1) {
            command_buffer[buffer_idx++] = c;
            vga_putchar(c);
        }
    }
}