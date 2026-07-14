 .syntax unified

 .section .text.fa2, "ax",%progbits
 .globl fa2
fa2:
 .fnstart
 bx lr
 .cantunwind
 .fnend
 .globl fa3
fa3:
 .fnstart
 bx lr
 .cantunwind
 .fnend

 .section .text.fa1, "ax", %progbits
 .globl fa1
fa1:
 .fnstart
 bx lr
 .cantunwind
 .fnend
