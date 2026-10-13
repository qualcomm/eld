.syntax unified
.arm

# Negative displacement: X = -16.
.section .text.01,"ax",%progbits
.balign 4

.global target_neg
target_neg:
  .word 42

.space 12

.global _start_neg
.type _start_neg, %function
_start_neg:
  .inst 0xe14f00d0
  .reloc _start_neg, R_ARM_LDRS_PC_G0, target_neg

# Positive displacement: X = +16.
.section .text.02,"ax",%progbits
.balign 4

.global _start_pos
.type _start_pos, %function
_start_pos:
  .inst 0xe14f00d0
  .reloc _start_pos, R_ARM_LDRS_PC_G0, target_pos

.space 12

.global target_pos
target_pos:
  .word 42
