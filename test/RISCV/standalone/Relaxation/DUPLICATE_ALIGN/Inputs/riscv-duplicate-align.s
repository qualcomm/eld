  .option rvc
  .text
  .globl _start
_start:
  .reloc ., R_RISCV_ALIGN, 4
  .reloc ., R_RISCV_ALIGN, 4
  .fill 3, 2, 1
  ret
