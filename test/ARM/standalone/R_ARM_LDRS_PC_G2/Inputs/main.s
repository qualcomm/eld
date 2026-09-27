.syntax unified
.arm

.text
.balign 4

.global _start
.type _start, %function
_start:
  .inst 0xe14f00d0
  .reloc _start, R_ARM_LDRS_PC_G2, target

# Make target - _start = 0x123456.
.space 0x123452

.global target
target:
  .word 42
