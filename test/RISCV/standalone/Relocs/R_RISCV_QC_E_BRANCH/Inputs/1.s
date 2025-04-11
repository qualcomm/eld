
  .text
  .p2align 1

QUALCOMM:

test_before_4kib:
  .space 0x100, 0x00

test1:
  qc.e.beqi x5, 32767, 0x0
  .reloc test1, R_RISCV_VENDOR, QUALCOMM
  .reloc test1, R_RISCV_CUSTOM193, test_before_4kib

test2:
  qc.e.bgei x5, -32768, 0x0
  .reloc test2, R_RISCV_VENDOR, QUALCOMM
  .reloc test2, R_RISCV_CUSTOM193, test2

test3:
  qc.e.bgeui x5, 65535, 0x0
  .reloc test3, R_RISCV_VENDOR, QUALCOMM
  .reloc test3, R_RISCV_CUSTOM193, test_distinct

  .space 0xa4, 0x0
test_distinct:
  nop

test4:
  qc.e.bltui x5, 65535, 0x0
  .reloc test4, R_RISCV_VENDOR, QUALCOMM
  .reloc test4, R_RISCV_CUSTOM193, test_after_4kib

  /* 6 bytes per qc.e.b<cmp>i, PC-relative offset is measured from start of instruction */
  .space 0x100 - 6, 0x00
test_after_4kib:
  nop
