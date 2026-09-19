.syntax unified
.thumb

.section .text.01,"ax",%progbits
.balign 4
.global _start
.thumb_func
_start:
  .inst.w 0xf85ff000
  .reloc 0, R_ARM_THM_PC12, target

.section .text.02,"ax",%progbits
.balign 4
.global target
target:
  .word 42
