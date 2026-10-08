  .option nopic
  .text
  .globl _start
_start:
  lui a1, %hi(B)
  lw a1, %lo(B)(a1)

  # This initially relaxes to C.JAL, then is rolled back after the zero-page
  # LUI deletion changes final layout.
  call target

  lw a2, %lo(G)(a2)
  .reloc .-4, R_RISCV_RELAX

  ret
