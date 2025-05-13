
  .text
  .p2align 1

  .option exact

  .set test_zero, 0
  .set test_ffff, 0xffffffff
  .set test_distinct, 0x89abcdef

QUALCOMM:

test1:
  qc.e.li x5, 0x0
  .reloc test1, R_RISCV_VENDOR, QUALCOMM
  .reloc test1, R_RISCV_CUSTOM194, test_zero

test2:
  qc.e.li x5, 0x0
  .reloc test2, R_RISCV_VENDOR, QUALCOMM
  .reloc test2, R_RISCV_CUSTOM194, test2

test3:
  qc.e.li x5, 0x0
  .reloc test3, R_RISCV_VENDOR, QUALCOMM
  .reloc test3, R_RISCV_CUSTOM194, test_ffff

test4:
  qc.e.li x5, 0x0
  .reloc test4, R_RISCV_VENDOR, QUALCOMM
  .reloc test4, R_RISCV_CUSTOM194, test_distinct
