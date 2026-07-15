 .syntax unified

 .section .text.f0, "ax",%progbits
 .globl f0
f0:
 .fnstart
 bx lr
 .cantunwind
 .fnend

 .section .text.f1, "ax",%progbits
 .globl f1
f1:
 .fnstart
 bx lr
 .save {r7, lr}
 .setfp r7, sp, #0
 .fnend

 .section .text.f2, "ax",%progbits
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

 .section .text.f3, "ax",%progbits
 .globl f4
f4:
 .fnstart
 bx lr
 .personality __gxx_personality_v0
 .handlerdata
 .long 0
 .fnend

 .section .text.__gcc_personality_v0, "ax", %progbits
 .global __gxx_personality_v0
__gxx_personality_v0:
 .fnstart
 bx lr
 .cantunwind
 .fnend

 .section .text.__aeabi_unwind_cpp_pr0, "ax", %progbits
 .global __aeabi_unwind_cpp_pr0
__aeabi_unwind_cpp_pr0:
 .fnstart
 bx lr
 .cantunwind
 .fnend

 .text
 .global _start
_start:
 bl f0
 bl f1
 bl f2
 bx lr
