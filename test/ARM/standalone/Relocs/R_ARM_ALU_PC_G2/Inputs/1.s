.arm

.section .text.1, "ax", %progbits
.space 0x28
.global _start
.type _start, %function
_start:
  add r0, r0, #0
  .reloc 0x28, R_ARM_ALU_PC_G2, dat2

.section .text.2, "ax", %progbits
dat2:
  .word 0
