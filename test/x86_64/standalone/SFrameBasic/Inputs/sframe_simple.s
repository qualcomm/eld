.text
.globl _start
_start:
    nop
    ret

// Simple SFrame section with header and one FRE
.section .sframe,"a"
.align 8
sframe_header:
    .short 0xdee2
    .byte 2
    .byte 8
    .long 0
    .long 0
    .byte 24
    .byte 1
    .short 0
    .long 0
    .long 0
    .quad _start

sframe_fre:
    .quad _start
    .long 4
    .byte 0x10
    .byte 0x07
    .byte 0x01
    .byte 0x00
