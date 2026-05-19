
  .text
  .p2align 1

  .option exact

  .set test_zero, 0

QUALCOMM:

test_clw:
  c.lw a0, 0(a2)
  .reloc test_clw, R_RISCV_VENDOR, QUALCOMM
  .reloc test_clw, R_RISCV_CUSTOM196, test_zero

test_csw:
  c.sw a0, 0(a2)
  .reloc test_csw, R_RISCV_VENDOR, QUALCOMM
  .reloc test_csw, R_RISCV_CUSTOM196, test_zero

test_clbu:
  c.lbu a0, 0(a2)
  .reloc test_clbu, R_RISCV_VENDOR, QUALCOMM
  .reloc test_clbu, R_RISCV_CUSTOM196, test_zero

test_clh:
  c.lh a0, 0(a2)
  .reloc test_clh, R_RISCV_VENDOR, QUALCOMM
  .reloc test_clh, R_RISCV_CUSTOM196, test_zero

test_csb:
  c.sb a0, 0(a2)
  .reloc test_csb, R_RISCV_VENDOR, QUALCOMM
  .reloc test_csb, R_RISCV_CUSTOM196, test_zero

test_csh:
  c.sh a0, 0(a2)
  .reloc test_csh, R_RISCV_VENDOR, QUALCOMM
  .reloc test_csh, R_RISCV_CUSTOM196, test_zero
