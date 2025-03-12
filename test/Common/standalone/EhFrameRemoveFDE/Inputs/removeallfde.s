.section .text.foo
.cfi_startproc
 nop
.cfi_endproc

.section .text.bar
.cfi_startproc
 nop
 nop
.cfi_endproc

.text
.globl _start;
_start:

