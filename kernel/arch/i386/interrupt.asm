[BITS 32]
global irq0_handler
global irq1_handler

extern timer_handler_main
extern keyboard_handler_main

irq0_handler:
    pusha
    call timer_handler_main
    popa
    iretd

irq1_handler:
    pusha
    call keyboard_handler_main
    popa
    iretd