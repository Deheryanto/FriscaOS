#include "vga.h"
#include "../../arch/i386/io.h"
#include "../../lib/stdint.h"
#include "../../lib/stddef.h"

/* VGA text buffer is memory-mapped at physical address 0xB8000.
 * Each character on screen is represented by a 16-bit value:
 * Lower byte: ASCII character
 * Upper byte: color attribute (foreground | background << 4) */
#define VGA_MEMORY ((uint16_t*) 0xB8000)
#define VGA_WIDTH  80
#define VGA_HEIGHT 25

/* VGA CRT Controller (CRTC) registers are accessed via two I/O ports:
 *0x3D4: index/address register (selects which CRTC register to access)
 *0x3D5: data register (reads/writes the selected register) */
#define VGA_CTRL_PORT 0x3D4
#define VGA_DATA_PORT 0x3D5

/* Pointer to the VGA text buffer (80x25 = 2000 entries of uint16_t) */
static uint16_t* const buffer = VGA_MEMORY;

/* Current cursor position*/
static size_t row;
static size_t column;

/* Current color attribute applied to newly written characters */
static uint8_t current_color;

/* Combine a character and a color attribute into a single 16-bit VGA entry
*Layout: [color:8][char:8]  (little-endian: char in low byte, color in high byte) */
static inline uint16_t vga_entry(unsigned char uc, uint8_t color) {
    return (uint16_t) uc | ((uint16_t) color << 8);
}

/* Build a color byte from foreground and background colors.
 * Layout: [bg:4][fg:4]  (background in high nibble, foreground in low nibble) */
static inline uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg) {
    return fg | (bg << 4);
}

/* Convert 2D screen coordinates (x, y) to a linear index into the buffer */
static inline size_t vga_get_index(size_t x, size_t y) {
    return y * VGA_WIDTH + x;
}

/* Move the hardware text cursor to the given screen position (x, y).
   *The cursor position is a 16-bit value (0-1999), sent in two steps
   * because each CRTC register is only 8 bits wide. */
void vga_update_cursor(size_t x, size_t y) {
      uint16_t pos = y * VGA_WIDTH + x;
     
      /* Send high byte of the cursor position (register 0x0E) */
      outb(VGA_CTRL_PORT, 0x0E);
      outb(VGA_DATA_PORT, (uint8_t)((pos >> 8) & 0xFF));

      /* Send low byte of the cursor position (register 0x0F) */
      outb(VGA_CTRL_PORT, 0x0F);
      outb(VGA_DATA_PORT, (uint8_t)(pos & 0xFF));
}

/* Clear the entire screen by filling it with spaces using the current color,
 * then reset the cursor to the top-left corner (0, 0). */
void vga_clear(void) {
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            buffer[vga_get_index(x, y)] = vga_entry(' ', current_color);
        }
    }
    row = 0;
    column = 0;
}

/* Initialize the VGA driver: set white-on-black as default color and clear screen */
void vga_init(void) {
    current_color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_clear();
}

/* Change the current text color (used for all subsequent writes) */
void vga_set_color(uint8_t foreground, uint8_t background) {
    current_color = vga_entry_color(foreground, background);
}

/* Scroll the screen up by one line when the cursor reaches the bottom.
 * - Each row is copied from the row below it.
 * - The last row is then cleared with spaces. */
static void vga_scroll(void) {
    /* Shift every row up by one */
    for (size_t y = 0; y < VGA_HEIGHT - 1; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            buffer[vga_get_index(x, y)] = buffer[vga_get_index(x, y + 1)];
        }
    }

    /* Clear the last (now empty) row at the bottom */
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        buffer[vga_get_index(x, VGA_HEIGHT - 1)] = vga_entry(' ', current_color);
    }
    
    /* Keep cursor on the last row after scrolling */
    row = VGA_HEIGHT - 1;
}

/* Write a single character to the screen and advance the cursor.
 * Handles special characters: '\n' (newline) and '\b' (backspace). */
void vga_putchar(char c) {
    /* Newline: move to the beginning of the next line */
    if (c == '\n') {
        column = 0;
        if (++row == VGA_HEIGHT) {
            vga_scroll();
        }
        return;
    }

    /* Backspace: move cursor back one cell and erase the previous character.
     * Wraps to the end of the previous line if at the start of a line. */
    if (c == '\b') {
        if (column > 0) {
            column--;
        } else if (row > 0) {
            row--;
            column = VGA_WIDTH - 1;
        }

        /* Overwrite the character at the new position with a space */
        buffer[vga_get_index(column, row)] = vga_entry(' ', current_color);
        vga_update_cursor(column, row);
        return;
    }

    /* Normal character: write it to the buffer at the current position */
    buffer[vga_get_index(column, row)] = vga_entry(c, current_color);

    /* Advance cursor; wrap to next line if we've reached the end of the row */
    if (++column == VGA_WIDTH) {
        column = 0;
        if (++row == VGA_HEIGHT) {
            vga_scroll();
        }
    }

    /* Keep the hardware cursor in sync with our software position */
    vga_update_cursor(column, row);
}


/* Write a null-terminated string to the screen, character by character */
void vga_write(const char* data) {
    for (size_t i = 0; data[i] != '\0'; i++) {
        vga_putchar(data[i]);
    }
}