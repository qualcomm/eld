.text
.globl _start
.globl unused_func

_start:
    call used_func
    ret

used_func:
    nop
    ret

unused_func:
    nop
    ret

// SFrame section that should be preserved
.section .sframe,"a"
.align 8
    .short 0xdee2
    .byte 2
    .byte 1
    .long 0
    .long 0
    .byte 24
    .byte 2
    .short 0
    .long 0
    .long 0
    .quad _start

    // FRE for _start
    .quad _start
    .long 7
    .byte 0x10
    .byte 0x07
    .byte 0x01
    .byte 0x00

    // FRE for used_func
    .quad used_func
    .long 2
    .byte 0x10
    .byte 0x07
    .byte 0x01
    .byte 0x00
