 .syntax unified
 .section .text.start, "ax",%progbits
 .globl _start
_start:
 .fnstart
 bx lr
 .cantunwind
 .fnend

 .section .text.f1, "ax", %progbits
 .globl f1
f1:
 .fnstart
 bx lr
 .cantunwind
 .fnend

 .section .text.f2, "ax", %progbits
 .globl f2
f2:
 .fnstart
 bx lr
 .cantunwind
 .fnend
 .globl f3
f3:
 .fnstart
 bx lr
 .cantunwind
 .fnend
