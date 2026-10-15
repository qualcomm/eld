.syntax unified
.thumb

.section .text,"ax",%progbits
.global thumb_func
.type thumb_func, %function
.thumb_func
thumb_func:
  bx lr
.size thumb_func, .-thumb_func

.section .data,"aw",%progbits
.global abs32_value
.type abs32_value, %object
abs32_value:
  .word 0
  .reloc abs32_value, R_ARM_ABS32, thumb_func

.global abs32_noi_value
.type abs32_noi_value, %object
abs32_noi_value:
  .word 0
  .reloc abs32_noi_value, R_ARM_ABS32_NOI, thumb_func
