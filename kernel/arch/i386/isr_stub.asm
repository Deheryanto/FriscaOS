[BITS 32]

global isr0_handler
; ... (define all 32 exception stubs)

extern isr_handler          ; C dispatcher

; ─────────────────────────────────────────────────────────────────
;  Exception stubs
;
;  Some CPU exceptions push an error code onto the stack; others
;  don't. To keep the stack layout uniform, we push a dummy 0
;  error code for exceptions that don't provide one.
;
;  Exceptions that DO push an error code:
;    8 (double fault), 10 (invalid TSS), 11 (segment not present),
;    12 (stack-segment fault), 13 (general protection),
;    14 (page fault), 17 (alignment check), 21 (control protection)
;
;  All others need a dummy 0.
; ─────────────────────────────────────────────────────────────────

; Macro for exceptions WITHOUT error code
%macro ISR_NOERR 1
global isr%1_handler
isr%1_handler:
    cli
    push dword 0            ; Dummy error code
    push dword %1           ; Interrupt number
    jmp isr_common_stub
%endmacro

; Macro for exceptions WITH error code
%macro ISR_ERR 1
global isr%1_handler
isr%1_handler:
    cli
    ; CPU already pushed the error code
    push dword %1           ; Interrupt number
    jmp isr_common_stub
%endmacro

; ── Exception 0-31 ────────────────────────────────────────────────
ISR_NOERR 0     ; Divide-by-zero
ISR_NOERR 1     ; Debug
ISR_NOERR 2     ; NMI
ISR_NOERR 3     ; Breakpoint
ISR_NOERR 4     ; Overflow
ISR_NOERR 5     ; Bound range
ISR_NOERR 6     ; Invalid opcode
ISR_NOERR 7     ; Device not available
ISR_ERR   8     ; Double fault
ISR_NOERR 9     ; Coprocessor segment overrun (reserved)
ISR_ERR   10    ; Invalid TSS
ISR_ERR   11    ; Segment not present
ISR_ERR   12    ; Stack-segment fault
ISR_ERR   13    ; General protection fault
ISR_ERR   14    ; Page fault
ISR_NOERR 15    ; Reserved
ISR_NOERR 16    ; x87 floating-point exception
ISR_ERR   17    ; Alignment check
ISR_NOERR 18    ; Machine check
ISR_NOERR 19    ; SIMD floating-point exception
ISR_NOERR 20    ; Virtualization exception
ISR_ERR   21    ; Control protection exception
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_ERR   30    ; Security exception
ISR_NOERR 31    ; Reserved

; ── Common stub — saves registers, calls C, restores, iret ──────
isr_common_stub:
    pusha                       ; Push EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI

    mov ax, ds                  ; Save current data segment
    push eax

    mov ax, 0x10                ; Load kernel data segment (0x10)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp                    ; Pass pointer to registers_t as argument
    call isr_handler            ; C dispatcher
    add esp, 4                  ; Clean up argument

    pop eax                     ; Restore original data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa                        ; Restore general-purpose registers
    add esp, 8                  ; Remove int_no and err_code
    iret                        ; Return from interrupt (pops EIP, CS, EFLAGS)