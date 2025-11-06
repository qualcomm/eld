.text
.globl func1
.globl func2
func1:
    nop
    ret
func2:
    nop
    nop
    ret

// SFrame section with two FREs
.section .sframe,"a"
.align 8
    .short 0xdee2
    .byte 2
    .byte 8
    .long 0
    .long 0
    .byte 24
    .byte 2
    .short 0
    .long 0
    .long 0
    .quad func1

    // First FRE
    .quad func1
    .long 4
    .byte 0x10
    .byte 0x07
    .byte 0x01
    .byte 0x00

    // Second FRE
    .quad func2
    .long 8
    .byte 0x10
    .byte 0x07
    .byte 0x01
    .byte 0x00
