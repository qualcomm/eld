  # Required until https://github.com/llvm/llvm-project/pull/146184 lands
  .option exact

  # Enable Relaxations
  .option relax

  .text
  .p2align 2
  .globl main
  .type main, @function

QUALCOMM:

main:

  # ACCESS_32: 32-bit load (lw) test cases

  qc.e.li a1, sym_gprel_std
1:
  lw a0, 0(a1)
  .reloc 1b, R_RISCV_VENDOR, QUALCOMM
  .reloc 1b, R_RISCV_CUSTOM197, sym_gprel_std
  .reloc 1b, R_RISCV_RELAX

  qc.e.li a1, sym_gprel_xqci
2:
  lw a0, 0(a1)
  .reloc 2b, R_RISCV_VENDOR, QUALCOMM
  .reloc 2b, R_RISCV_CUSTOM197, sym_gprel_xqci
  .reloc 2b, R_RISCV_RELAX

  qc.e.li a1, sym_too_far
3:
  lw a0, 0(a1)
  .reloc 3b, R_RISCV_VENDOR, QUALCOMM
  .reloc 3b, R_RISCV_CUSTOM197, sym_too_far
  .reloc 3b, R_RISCV_RELAX

  # ACCESS_16: 16-bit compressed load (c.lw) test case
  # a1 (x11) is in the CL-format register range (x8-x15)

  qc.e.li a1, sym_gprel_std
4:
  c.lw a0, 0(a1)
  .reloc 4b, R_RISCV_VENDOR, QUALCOMM
  .reloc 4b, R_RISCV_CUSTOM196, sym_gprel_std
  .reloc 4b, R_RISCV_RELAX

  .size main, .-main
