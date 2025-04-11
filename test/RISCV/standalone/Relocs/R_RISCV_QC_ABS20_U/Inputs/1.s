
  .text
  .p2align 1

  .set test_zero, 0
  .set test_hi, 524287
  .set test_lo, -524288
  .set test_minus_one, -1
  .set test_distinct, 0xfffabcde

QUALCOMM:

test1:
  qc.li x5, 0x0
  .reloc test1, R_RISCV_VENDOR, QUALCOMM
  .reloc test1, R_RISCV_CUSTOM192, test_zero

test2:
  qc.li x5, 0x0
  .reloc test2, R_RISCV_VENDOR, QUALCOMM
  .reloc test2, R_RISCV_CUSTOM192, test_hi

test3:
  qc.li x5, 0x0
  .reloc test3, R_RISCV_VENDOR, QUALCOMM
  .reloc test3, R_RISCV_CUSTOM192, test_lo

test4:
  qc.li x5, 0x0
  .reloc test4, R_RISCV_VENDOR, QUALCOMM
  .reloc test4, R_RISCV_CUSTOM192, test_minus_one

test5:
  qc.li x5, 0x0
  .reloc test5, R_RISCV_VENDOR, QUALCOMM
  .reloc test5, R_RISCV_CUSTOM192, test_distinct
