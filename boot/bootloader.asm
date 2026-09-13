[BITS 16]
[ORG 0x7c00]

; CONSTANT 
KERNEL_OFFSET  equ 0x1000
KERNEL_SECTORS equ 30

start:
      cli                         ; Disable interrupts during segment setup
    
      ; Initialize segment registers to 0x0000 (0x7C00 relative base)
      xor ax, ax                  ; AX = 0
      mov ds, ax                  ; Data Segment = 0x0000
      mov es, ax                  ; Extra Segment = 0x0000 (crucial for INT 0x13 buffer)
      mov ss, ax                  ; Stack Segment = 0x0000
      mov sp, 0x7C00              ; Stack pointer set below bootloader (grows downward)
    
      sti                         ; Re-enable interrupts

      ; Save BIOS boot drive number passed via DL
      mov [boot_drive], dl        ; Store DL into RAM before registers get overwritten

      ; Clear Screen & Set Video Mode
      mov ah, 0x00                ; BIOS Video Mode function
      mov al, 0x03                ; 80x25 16-color text mode
      int 0x10                    ; Call Video Interrupt

      ; Welcome message
      mov si, boot_message
      call print_string

      ; Load Kernel from Disk to Memory
      mov bx, KERNEL_OFFSET       ; ES:BX buffer pointer -> 0x0000:0x1000
      
      mov ah, 0x02                ; BIOS Read Sectors from Drive function
      mov al, KERNEL_SECTORS      ; Number of sectors to read
      mov ch, 0x00                ; Cylinder number = 0
      mov dh, 0x00                ; Head number = 0
      mov cl, 0x02                ; Sector number = 2 (Sector 1 is bootloader)
      mov dl, [boot_drive]        ; Target drive number
      int 0x13                    ; Call BIOS Disk Interrupt
      jc disk_error               ; Jump if carry flag set (Read error occurred)

      cmp al, KERNEL_SECTORS
      jne disk_error              ; If not equal, jump to error handler

      jmp switch_to_pm


print_string:
      pusha
      mov ah, 0x0E            ; BIOS Teletype function
.loop:
      lodsb                       ; Load byte at [DS:SI] into AL and increment SI
      test al, al                 ; Check if character is null byte (0x00)
      jz .done                    ; If null byte, exit loop
      int 0x10                    ; Call BIOS video service to print character
      jmp .loop                   ; Repeat for next character
.done:
      popa
      ret


disk_error:
      mov si, error_message
      call print_string
      hlt                         ; Halt the CPU to save power
      jmp $                       ; Infinite loop fallback if interrupt occurs


switch_to_pm:
    cli                     ;
    lgdt [gdt_descriptor]   ;

    mov eax, cr0            ;
    or eax, 0x1
    mov cr0, eax

    ; Far jump ke kode 32-bit (flushes CPU pipeline)
    jmp CODE_SEG:init_pm

[BITS 32]
init_pm:
    mov ax, DATA_SEG
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov ebp, 0x90000
    mov esp, ebp

    jmp KERNEL_OFFSET


gdt_start:

gdt_null:                   ; NULL Descriptor wajib 8 byte angka 0
    dd 0x0
    dd 0x0

gdt_code:                   ; Code Segment Descriptor
    dw 0xffff
    dw 0x0000
    db 0x00
    db 10011010b
    db 11001111b
    db 0x00

gdt_data:                   ; Data Segment Descriptor
    dw 0xffff
    dw 0x0000
    db 0x00
    db 10010010b
    db 11001111b
    db 0x00

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start



boot_drive    db 0
boot_message  db 'Welcome to Friscaboot manager', 0x0D, 0x0A, 0
error_message db 'Read disk failed!', 0x0D, 0x0A, 0

; Fill remaining byte space up to byte 510 with zeros
times 510-($-$$) db 0 

; Master Boot Record (MBR) Signature required by BIOS
dw 0xAA55