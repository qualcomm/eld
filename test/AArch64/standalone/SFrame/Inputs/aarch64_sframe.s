.text
.globl _start
_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    ldp x29, x30, [sp], #16
    ret

// AArch64-specific SFrame section
.section .sframe,"a"
.align 8
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

    // FRE for AArch64 function
    .quad _start
    .long 16
    .byte 0x10
    .byte 0x10
    .byte 0x08
    .byte 0x00
