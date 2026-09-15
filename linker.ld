/* ============================================================================
 *  Frisca OS — Linker Script
 *
 *  Load address: 0x1000 (matches bootloader's KERNEL_OFFSET)
 * ============================================================================ */

ENTRY(_start)

SECTIONS {
    /* ── Load address — MUST match bootloader's KERNEL_OFFSET ────── */
    . = 0x1000;

    .text : {
        *(.text)
        *(.text.*)
    }

    .rodata : {
        *(.rodata)
        *(.rodata.*)
    }

    .data : {
        *(.data)
        *(.data.*)
    }

    /* ── BSS section with markers for kernel_entry.asm ────────────── */
    .bss : {
        _bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        _bss_end = .;
    }

    /DISCARD/ : {
        *(.comment)
        *(.note.*)
        *(.eh_frame)
    }
}