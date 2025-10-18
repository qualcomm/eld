.text
.globl _start
_start:
    ret

// SFrame section with invalid magic number
.section .sframe,"a"
.align 8
    .short 0xBAD0
    .byte 2
    .byte 1
    .long 0
    .long 0
    .byte 24
    .byte 1
    .short 0
    .long 0
    .long 0
    .quad _start
