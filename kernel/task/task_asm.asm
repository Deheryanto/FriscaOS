[BITS 32]
global switch_to_task

; ─────────────────────────────────────────────────────────────────
;  switch_to_task — save current task's context, restore another's
;
;  C prototype (from task.h):
;     void switch_to_task(uint32_t *old_esp, uint32_t new_esp);
;
;  Arguments (cdecl, pushed by caller right-to-left):
;     [esp+4]  = old_esp   (pointer — we WRITE the current ESP here)
;     [esp+8]  = new_esp   (value   — we LOAD it into ESP)
;
;  What this function does:
;     1. Push all registers + EFLAGS onto the CURRENT stack
;     2. Save the current ESP into *old_esp
;     3. Load the NEW task's ESP
;     4. Pop the NEW task's registers + EFLAGS
;     5. Return to wherever the NEW task was suspended
;
;  After this function returns (on the new task's side), the new
;  task continues executing as if it had just called switch_to_task.
; ─────────────────────────────────────────────────────────────────

switch_to_task:
    ; ── Step 1: Save current task's registers + EFLAGS ───────────
    ; Order matters — the restore step must pop in EXACT reverse.
    push eax
    push ebx
    push ecx
    push edx
    push esi
    push edi
    push ebp
    pushfd                  ; Save EFLAGS (includes IF — interrupt enable)

    ; ── Step 2: Save ESP into *old_esp ───────────────────────────
    ; We pushed 8 values (32 bytes) onto the stack, so the
    ; original arguments moved from [esp+4], [esp+8] to
    ; [esp+36], [esp+40].
    ;
    ;   offset   contents (from top of stack after pushes)
    ;   ──────   ────────────────────────────────────────
    ;   [esp+0]  EFLAGS (top of stack now)
    ;   [esp+4]  EBP
    ;   [esp+8]  EDI
    ;   [esp+12] ESI
    ;   [esp+16] EDX
    ;   [esp+20] ECX
    ;   [esp+24] EBX
    ;   [esp+28] EAX
    ;   [esp+32] return address (caller's EIP)
    ;   [esp+36] old_esp   ← our first argument
    ;   [esp+40] new_esp   ← our second argument
    mov eax, [esp + 36]     ; eax = old_esp (pointer)
    mov [eax], esp          ; *old_esp = current ESP (saved!)

    ; ── Step 3: Load new task's ESP ──────────────────────────────
    mov esp, [esp + 40]     ; esp = new_esp (context switch happens here!)

    ; ── Step 4: Restore new task's EFLAGS + registers ────────────
    ; IMPORTANT: `popfd` may immediately re-enable interrupts if
    ; the new task had IF set. Between `mov esp` and `popfd` there
    ; are 1-2 instructions where we're running on the new stack
    ; but with the OLD task's flags. This is fine because IF is
    ; still set from the old context (no IRQ will nest in a
    ; dangerous way during these few instructions).
    popfd                   ; Restore EFLAGS (may re-enable interrupts)
    pop ebp
    pop edi
    pop esi
    pop edx
    pop ecx
    pop ebx
    pop eax

    ; ── Step 5: Return to new task's saved EIP ───────────────────
    ; This `ret` pops the return address from the NEW task's stack
    ; and jumps there. For a freshly created task, that address
    ; is the entry_point we pushed in create_task().
    ;
    ;    ECX, EBX, EAX — but this function pops exactly the same
    ;    order plus expects a return address on top. Let's check
    ;    compatibility below.
    ret