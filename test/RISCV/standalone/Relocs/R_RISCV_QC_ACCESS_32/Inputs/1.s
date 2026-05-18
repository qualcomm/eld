
  .text
  .p2align 2

  .option exact

  .set test_zero, 0

QUALCOMM:

test_lb:
  lb a0, 0(a1)
  .reloc test_lb, R_RISCV_VENDOR, QUALCOMM
  .reloc test_lb, R_RISCV_CUSTOM197, test_zero

test_lbu:
  lbu a0, 0(a1)
  .reloc test_lbu, R_RISCV_VENDOR, QUALCOMM
  .reloc test_lbu, R_RISCV_CUSTOM197, test_zero

test_lh:
  lh a0, 0(a1)
  .reloc test_lh, R_RISCV_VENDOR, QUALCOMM
  .reloc test_lh, R_RISCV_CUSTOM197, test_zero

test_lhu:
  lhu a0, 0(a1)
  .reloc test_lhu, R_RISCV_VENDOR, QUALCOMM
  .reloc test_lhu, R_RISCV_CUSTOM197, test_zero

test_lw:
  lw a0, 0(a1)
  .reloc test_lw, R_RISCV_VENDOR, QUALCOMM
  .reloc test_lw, R_RISCV_CUSTOM197, test_zero

test_sb:
  sb a0, 0(a1)
  .reloc test_sb, R_RISCV_VENDOR, QUALCOMM
  .reloc test_sb, R_RISCV_CUSTOM197, test_zero

test_sh:
  sh a0, 0(a1)
  .reloc test_sh, R_RISCV_VENDOR, QUALCOMM
  .reloc test_sh, R_RISCV_CUSTOM197, test_zero

test_sw:
  sw a0, 0(a1)
  .reloc test_sw, R_RISCV_VENDOR, QUALCOMM
  .reloc test_sw, R_RISCV_CUSTOM197, test_zero
