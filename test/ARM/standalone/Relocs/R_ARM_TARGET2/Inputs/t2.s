 .syntax unified
 .text
 .globl _start
 .align 2
_start:
 .type function, %function
 .fnstart
 bx lr
 .personality   __gxx_personality_v0
 .handlerdata
 .word  _ZTIi(TARGET2)
 .text
 .fnend
 .global __gxx_personality_v0
 .type function, %function
__gxx_personality_v0:
 bx lr

 .data
_ZTIi:  .word 0
