#include "keyboard.h"
#include "../vga/vga.h"
#include "../../arch/i386/io.h"
#include "../../shell.h"

/* PS/2 keyboard controller I/O ports:
 *   - 0x60: Data port — read scancodes here
 *   - 0x64: Status/command port (status bit 0 = output buffer full) */
#define KEYBOARD_DATA_PORT   0x60
#define KEYBOARD_STATUS_PORT 0x64

/* Scancodes for the left and right Shift keys (make codes) */
#define SCANCODE_LSHIFT 0x2A
#define SCANCODE_RSHIFT 0x36

/* Tracks whether a Shift key is currently held down.
 * 0 = no shift, 1 = shift is held (either left or right). */
static int shift_pressed = 0;

/* Scancode → ASCII translation table for the NORMAL (unshifted) state.
 *
 * The PS/2 scancode set 1 assigns each key a unique byte value (the "make code").
 * This table maps those scancodes to their corresponding ASCII characters.
 *
 * Index 0 is unused because scancode 0 is not a valid key.
 * Index 1 = ESC, index 14 = Backspace, index 15 = Tab, index 28 = Enter, etc.
 * Entries with 0 are unmapped (e.g., modifier keys, function keys). */
static const char scancode_ascii_normal[] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
  '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
     0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
     0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
   '*',   0, ' '
};

/* Scancode → ASCII translation table for the SHIFTED state.
 * Same layout as the normal table, but each entry is the character
 * produced when Shift is held down (uppercase letters, symbols like !@#$%^&*). */
static const char scancode_ascii_shift[] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
  '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
     0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
     0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
   '*',   0, ' '
};

/* Called from the IRQ1 handler (via keyboard_handler_main in idt.c).
 * Reads the scancode from the keyboard controller and converts it
 * into an ASCII character, then feeds it to the shell. */
void keyboard_handle_interrupt(void) {
    /* Read the scancode from the keyboard's data port */
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);

    /* --- Handle Shift key press (make code) ---
     * Either left or right Shift sets the shift flag. */
    if (scancode == SCANCODE_LSHIFT || scancode == SCANCODE_RSHIFT) {
        shift_pressed = 1;
        return;
    }

    /* --- Handle Shift key release (break code) ---
     * Break codes have the high bit (0x80) set:
     *   LShift release = 0x2A | 0x80 = 0xAA
     *   RShift release = 0x36 | 0x80 = 0xB6
     * Both clear the shift flag. */
    if (scancode == (SCANCODE_LSHIFT | 0x80) || scancode == (SCANCODE_RSHIFT | 0x80)) {
        shift_pressed = 0;
        return;
    }

    /* --- Ignore all other key RELEASE events ---
     * Any scancode with bit 7 set is a break (release) code.
     * We only care about key presses. */
    if (scancode & 0x80) {
        return;
    }

    /* --- Choose the correct translation table ---
     * If Shift is held, use the shifted map; otherwise the normal map. */
    const char *current_map = shift_pressed ? scancode_ascii_shift : scancode_ascii_normal;

    /* --- Bounds check before indexing ---
     * Prevents reading out of bounds if the scancode is larger than the table. */
    if (scancode < sizeof(scancode_ascii_normal)) {
        char c = current_map[scancode];

        /* Only forward mapped keys (skip 0 entries like function keys,
         * modifier keys we don't handle, etc.) */
        if (c != 0) {
            shell_input(c);
        }
    }
}