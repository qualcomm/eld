.text
.globl _start
_start:
    ret

// Truncated SFrame section (incomplete header)
.section .sframe,"a"
.align 8
    .short 0xdee2
    .byte 2
    // Missing rest of header - should cause truncation error
