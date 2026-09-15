[BITS 32]

extern _bss_start
extern _bss_end
extern kernel_main

global _start

; ── Stack in its own section ─────────────────────────────
section .bss_stack nobits
align 16
stack_bottom:
    resb 16384
stack_top:

section .text
_start:
    cli

    ; Set up stack first
    mov esp, stack_top
    mov ebp, esp

    ; Clear .bss (stack is safe — different section)
    mov edi, _bss_start
    mov ecx, _bss_end
    sub ecx, edi
    xor eax, eax
    rep stosb

    ; Call C entry
    call kernel_main

.hang:
    cli
    hlt
    jmp .hang