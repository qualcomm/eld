 .syntax unified

 .section .text.1, "ax", %progbits
 .global fn1
 .type fn1, %function
fn1:
 .fnstart
 bx lr
 .save {r8, lr}
 .setfp r8, sp, #0
 .fnend

 .section .text.2, "ax", %progbits
 .global fn2
 .type fn2, %function
fn2:
 .fnstart
 bx lr
 .save {r9, lr}
 .setfp r9, sp, #0
 .fnend

 .section .rodata
 .global __aeabi_unwind_cpp_pr0
__aeabi_unwind_cpp_pr0:
 .word 0

 .text
 .global _start
_start:
 .fnstart
 bx lr
 .save {r7, lr}
 .setfp r7, sp, #0
 .fnend
