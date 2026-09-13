[BITS 32]
global load_page_directory
global enable_paging

load_page_directory:
    push ebp
    mov ebp, esp
    mov eax, [ebp + 8]
    mov cr3, eax        ; Load Page Directory address into CR3
    mov esp, ebp
    pop ebp
    ret

enable_paging:
    push ebp
    mov ebp, esp
    mov eax, cr0
    or eax, 0x80000000  ; Set Bit 31 (Paging Enable bit) pada CR0
    mov cr0, eax
    mov esp, ebp
    pop ebp
    ret