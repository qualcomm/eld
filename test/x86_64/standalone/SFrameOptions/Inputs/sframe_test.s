.text
.globl main
main:
    movq $0, %rax
    ret

// SFrame section for testing
.section .sframe,"a"
.align 8
    .short 0xdee2
    .byte 2
    .byte 1
    .long 0
    .long 0
    .byte 24
    .byte 1
    .short 0
    .long 0
    .long 0
    .quad main

    // FRE for main
    .quad main
    .long 3
    .byte 0x10
    .byte 0x07
    .byte 0x01
    .byte 0x00
