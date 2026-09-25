.syntax unified
.arm

.text
.balign 4

.global _start
.type _start, %function
_start:
  .inst 0xe14f00d0
  .reloc _start, R_ARM_LDRS_PC_G1, target

# Make target - _start = 0x1234.
.space 0x1230

.global target
target:
  .word 42
