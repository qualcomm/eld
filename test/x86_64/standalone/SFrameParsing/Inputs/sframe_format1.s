.text
.globl func1
func1:
    nop
    ret

// SFrame section with single FRE
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
    .quad func1

    // Single FRE
    .quad func1
    .long 4
    .byte 0x10
    .byte 0x07
    .byte 0x01
    .byte 0x00
