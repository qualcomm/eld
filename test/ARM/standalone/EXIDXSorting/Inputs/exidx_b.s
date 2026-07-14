 .syntax unified

 .section .text.fb2, "ax",%progbits
 .globl _start
_start:
 .fnstart
 bx lr
 .cantunwind
 .fnend

 .section .text.fb1, "ax", %progbits
 .globl fb1
fb1:
 .fnstart
 bx lr
 .cantunwind
 .fnend
