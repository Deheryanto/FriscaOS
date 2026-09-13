#include "shell.h"
#include "memory/pmm.h"
#include "memory/heap.h"
#include "fs/vfs.h"
#include "drivers/vga/vga.h"
#include "drivers/timer/timer.h"
#include "lib/string.h"
#include "arch/i386/io.h"

/* Maximum length of a command line (including terminator).
 * Lines longer than this are silently truncated — extra characters
 * are dropped because buffer_idx stops advancing. */
#define MAX_COMMAND_LEN 256

/* Command line buffer — stores the current line as the user types.
 * Persists across keystrokes; cleared after each command is executed. */
static char command_buffer[MAX_COMMAND_LEN];

/* Current write position in command_buffer.
 * Also equals the visible cursor position on the current line. */
static size_t buffer_idx = 0;

/* ─────────────────────────────────────────────────────────────────
 *  print_prompt — display the shell prompt
 *
 *  Prints "frisca-os> " in light cyan, then restores white text
 *  so the user's typed characters appear in the default color.
 * ───────────────────────────────────────────────────────────────── */
static void print_prompt(void) {
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_write("frisca-os> ");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
}

/* ─────────────────────────────────────────────────────────────────
 *  reboot — reset the machine via the 8042 keyboard controller
 *
 *  There's no standard "reboot" instruction on x86 (until recent
 *  chipsets added one). The classic method is to pulse the CPU
 *  reset line via the keyboard controller's command port.
 *
 *  Steps:
 *    1. Wait for the controller's input buffer to be empty
 *       (bit 1 of status = 0 means "ready for command").
 *    2. Drain any pending output (bit 0 = 1 means "data available").
 *    3. Write command 0xFE to port 0x64 — triggers a CPU reset.
 *
 *  The loop reads status and, if a byte is available, consumes it.
 *  It exits when the controller is ready to accept the reset command.
 * ───────────────────────────────────────────────────────────────── */
static void reboot(void) {
    vga_write("Rebooting system...\n");

    uint8_t temp;
    do {
        temp = inb(0x64);          /* Read keyboard controller status */
        if (temp & 1) inb(0x60);   /* If output buffer full, drain it */
    } while (temp & 2);            /* Wait until input buffer is empty */

    outb(0x64, 0xFE);              /* Pulse CPU reset line */
}

/* ─────────────────────────────────────────────────────────────────
 *  print_uint — print an unsigned integer in decimal
 *
 *  There's no printf() in this kernel, so we implement a minimal
 *  integer-to-string conversion inline.
 *
 *  Algorithm:
 *    1. Extract digits from least-significant to most-significant
 *       by repeatedly dividing by 10 and taking the remainder.
 *    2. Fill a buffer from the END backwards (buf[11] is the
 *       terminator, buf[10] is the last digit).
 *    3. Print starting from the first non-empty position.
 *
 *  Maximum value of uint32_t is 4294967295 (10 digits), so a
 *  12-byte buffer is plenty (10 digits + terminator + safety).
 * ───────────────────────────────────────────────────────────────── */
static void print_uint(uint32_t num) {
    if (num == 0) {
        vga_write("0");
        return;
    }

    char buf[12];
    int i = 10;
    buf[11] = '\0';

    /* Fill digits from right to left */
    while (num > 0) {
        buf[i--] = '0' + (num % 10);
        num /= 10;
    }

    /* buf[i+1] is the first digit (most significant) */
    vga_write(&buf[i + 1]);
}

/* ─────────────────────────────────────────────────────────────────
 *  execute_command — parse and run the current command line
 *
 *  Called when the user presses Enter. Dispatches on the command
 *  string via a chain of strcmp / strncmp comparisons.
 *
 *  Commands supported:
 *    help, version, timer, testdelay, mem, testmem, testheap,
 *    ls, cat <file>, write <file> <content>, clear, reboot
 * ───────────────────────────────────────────────────────────────── */
static void execute_command(void) {
    vga_write("\n");   /* Move to a fresh line after the user's input */

    /* Empty command (just pressed Enter) — print a new prompt */
    if (buffer_idx == 0) {
        print_prompt();
        return;
    }

    /* Null-terminate the command line so strcmp/strncmp work */
    command_buffer[buffer_idx] = '\0';

    /* ── help ─────────────────────────────────────────────── */
    if (strcmp(command_buffer, "help") == 0) {
        vga_write("Available commands:\n");
        vga_write("  help      - Show this help message\n");
        vga_write("  version   - Display Frisca OS version\n");
        vga_write("  timer     - Show system uptime in seconds and ticks\n");
        vga_write("  testdelay - Test 2 seconds sleep delay\n");
        vga_write("  testmem   - Test Physical Memory Manager (PMM)\n");
        vga_write("  testheap  - Test Heap Allocator (kmalloc/kfree)\n");
        vga_write("  ls        - List all files in VFS RAMDisk\n");
        vga_write("  cat       - Display file content (Usage: cat <filename>)\n");
        vga_write("  write     - Write to file (Usage: write <filename> <content>)\n");
        vga_write("  clear     - Clear the screen\n");
        vga_write("  reboot    - Restart the system\n");

    /* ── version ──────────────────────────────────────────── */
    } else if (strcmp(command_buffer, "version") == 0) {
        vga_write("Frisca OS v0.1.0 (32-bit Protected Mode)\n");

    /* ── timer — show uptime ──────────────────────────────── */
    } else if (strcmp(command_buffer, "timer") == 0) {
        uint32_t ticks = timer_get_ticks();
        uint32_t seconds = ticks / 100;   /* Timer runs at 100 Hz */

        vga_write("System Uptime: ");
        print_uint(seconds);
        vga_write(" s (Total Ticks: ");
        print_uint(ticks);
        vga_write(")\n");

    /* ── testdelay — block for 2 seconds ──────────────────── */
    } else if (strcmp(command_buffer, "testdelay") == 0) {
        vga_write("Waiting 2 seconds...\n");
        sleep(2000);
        vga_write("Done!\n");

    /* ── mem — show free physical memory ──────────────────── */
    /* NOTE: This command is implemented but NOT listed in help! */
    } else if (strcmp(command_buffer, "mem") == 0) {
        uint32_t free_mem = pmm_get_free_memory();
        vga_write("Free Memory: ");
        print_uint(free_mem / 1024);      /* Convert bytes to KB */
        vga_write(" KB\n");

    /* ── testmem — allocate and free a PMM block ──────────── */
    } else if (strcmp(command_buffer, "testmem") == 0) {
        vga_write("Allocating 1 Block (4KB)...\n");
        void* ptr1 = pmm_alloc_block();
        vga_write("Allocated Address: 0x");
        /* NOTE: prints the address in DECIMAL, not hex —
         * the "0x" prefix is misleading here. */
        print_uint((uint32_t)ptr1);
        vga_write("\nFreeing block...\n");
        pmm_free_block(ptr1);
        vga_write("Done!\n");

    /* ── testheap — allocate and free a heap block ────────── */
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

    /* ── ls — list VFS files ──────────────────────────────── */
    } else if (strcmp(command_buffer, "ls") == 0) {
        vfs_list_files();

    /* ── cat <filename> — display file content ────────────── */
    } else if (strncmp(command_buffer, "cat ", 4) == 0) {
        char* filename = command_buffer + 4;   /* Skip "cat " prefix */

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

    /* ── write <filename> <content> — create a file ───────── */
    } else if (strncmp(command_buffer, "write ", 6) == 0) {
        char* args = command_buffer + 6;       /* Skip "write " prefix */
        char* space_ptr = strchr(args, ' ');   /* Find separator */

        if (space_ptr) {
            /* Split "filename content" into two strings at the first space.
             * We modify the buffer in place — this is safe because the
             * command will be discarded anyway. */
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

    /* ── clear — wipe the screen ──────────────────────────── */
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

    /* Reset the buffer and print a new prompt for the next command */
    buffer_idx = 0;
    print_prompt();
}

/* ─────────────────────────────────────────────────────────────────
 *  shell_init — print the banner and first prompt
 *
 *  Called once at boot from kernel_main, after all subsystems
 *  (VGA, timer, PMM, heap, VFS) are initialized.
 * ───────────────────────────────────────────────────────────────── */
void shell_init(void) {
    buffer_idx = 0;

    /* Print a welcome banner in light green */
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_write("=== Welcome to Frisca OS Shell ===\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);

    print_prompt();
}

/* ─────────────────────────────────────────────────────────────────
 *  shell_input — feed a single character into the shell
 *
 *  Called from the keyboard interrupt handler for every key press
 *  that maps to a printable character (or '\n' / '\b').
 *
 *  Behavior:
 *    - '\n'  → execute the current command
 *    - '\b'  → erase the last character (if any)
 *    - other → append to buffer and echo to screen
 * ───────────────────────────────────────────────────────────────── */
void shell_input(char c) {
    if (c == '\n') {
        execute_command();

    } else if (c == '\b') {
        /* Backspace: only if there's something to delete */
        if (buffer_idx > 0) {
            buffer_idx--;
            vga_putchar('\b');   /* VGA driver handles erase + cursor move */
        }

    } else {
        /* Normal character: append if there's room */
        if (buffer_idx < MAX_COMMAND_LEN - 1) {
            command_buffer[buffer_idx++] = c;
            vga_putchar(c);      /* Echo the character to screen */
        }
        /* else: silently drop — buffer full */
    }
}