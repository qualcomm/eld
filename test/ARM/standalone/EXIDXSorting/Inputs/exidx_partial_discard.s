 .syntax unified

 .section .exit.text, "ax", %progbits
 .globl foo
 .type foo, %function
foo:
 .fnstart
 bx lr
 .save {r7, lr}
 .setfp r7, sp, #0
 .fnend

 .text
 .globl _start
 .type _start, %function
_start:
 .fnstart
 bx lr
 .cantunwind
 .fnend

 .section .text.__aeabi_unwind_cpp_pr0, "ax", %progbits
 .global __aeabi_unwind_cpp_pr0
__aeabi_unwind_cpp_pr0:
 bx lr
