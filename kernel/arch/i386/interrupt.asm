[BITS 32]

global irq0_handler
global irq1_handler

extern timer_handler_main
extern keyboard_handler_main
extern timer_needs_reschedule
extern timer_clear_reschedule
extern task_schedule

; ─────────────────────────────────────────────────────────────────
;  IRQ0 — Timer
;
;  Stack layout saat masuk (didorong oleh CPU):
;      [esp+0]  EIP
;      [esp+4]  CS
;      [esp+8]  EFLAGS   ← IF=0 di sini (CPU clear saat IRQ)
;
;  Kita akan:
;    1. pusha              → simpan semua register umum
;    2. Panggil handler C  → timer_handler_main
;    3. Cek need_reschedule
;    4. Jika ya, sti + task_schedule + cli
;    5. popa + iretd       → kembali ke task yang di-interrupt
; ─────────────────────────────────────────────────────────────────
irq0_handler:
    pusha                       ; Simpan EAX..EDI

    ; Panggil C handler
    call timer_handler_main

    ; ── Cek apakah perlu reschedule ──────────────────────────
    call timer_needs_reschedule
    test eax, eax               ; EAX = 0?
    jz .irq0_done               ; Jika tidak, skip

    ; Perlu reschedule — clear flag dulu
    call timer_clear_reschedule

    ; Aktifkan interrupts SEBELUM switch.
    ; Ini penting: switch_to_task akan pushfd dengan IF=1,
    ; sehingga task yang di-switch akan resume dengan IF=1.
    sti

    ; Panggil task_schedule. Ini mungkin switch ke task lain.
    ; Ketika task ini di-resume nanti, dia akan return di sini.
    call task_schedule

    ; Kembali ke interrupt context
    cli

.irq0_done:
    popa                        ; Restore EAX..EDI
    iretd                       ; Return dari interrupt (restore CS/EIP/EFLAGS)

; ─────────────────────────────────────────────────────────────────
;  IRQ1 — Keyboard
;
;  Keyboard tidak memicu reschedule (hanya timer yang melakukannya),
;  jadi IRQ1 handler tetap sederhana.
; ─────────────────────────────────────────────────────────────────
irq1_handler:
    pusha
    call keyboard_handler_main
    popa
    iretd