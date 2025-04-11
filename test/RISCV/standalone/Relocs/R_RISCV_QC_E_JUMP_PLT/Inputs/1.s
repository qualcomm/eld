
  .text
  .p2align 1

  .set test_zero, 0x0
  .set test_distinct, 0xabcdef78
  .set test_fffe, 0xfffffffe

  .global test_vis_default

  .global test_vis_hidden
  .hidden test_vis_hidden

  .global test_vis_internal
  .internal test_vis_internal

  .global test_vis_protected
  .protected test_vis_protected

QUALCOMM:


test_j_1:
  qc.e.j 0x0
  .reloc test_j_1, R_RISCV_VENDOR, QUALCOMM
  .reloc test_j_1, R_RISCV_CUSTOM195, test_zero

test_j_2:
  qc.e.j 0x0
  .reloc test_j_2, R_RISCV_VENDOR, QUALCOMM
  .reloc test_j_2, R_RISCV_CUSTOM195, test_j_2

test_j_3:
  qc.e.j 0x0
  .reloc test_j_3, R_RISCV_VENDOR, QUALCOMM
  .reloc test_j_3, R_RISCV_CUSTOM195, test_distinct

test_j_4:
  qc.e.j 0x0
  .reloc test_j_4, R_RISCV_VENDOR, QUALCOMM
  .reloc test_j_4, R_RISCV_CUSTOM195, test_fffe

test_j_5:
  qc.e.j 0x0
  .reloc test_j_5, R_RISCV_VENDOR, QUALCOMM
  .reloc test_j_5, R_RISCV_CUSTOM195, test_vis_default

test_j_6:
  qc.e.j 0x0
  .reloc test_j_6, R_RISCV_VENDOR, QUALCOMM
  .reloc test_j_6, R_RISCV_CUSTOM195, test_vis_hidden

test_j_7:
  qc.e.j 0x0
  .reloc test_j_7, R_RISCV_VENDOR, QUALCOMM
  .reloc test_j_7, R_RISCV_CUSTOM195, test_vis_internal

test_j_8:
  qc.e.j 0x0
  .reloc test_j_8, R_RISCV_VENDOR, QUALCOMM
  .reloc test_j_8, R_RISCV_CUSTOM195, test_vis_protected


test_jal_1:
  qc.e.jal 0x0
  .reloc test_jal_1, R_RISCV_VENDOR, QUALCOMM
  .reloc test_jal_1, R_RISCV_CUSTOM195, test_zero

test_jal_2:
  qc.e.jal 0x0
  .reloc test_jal_2, R_RISCV_VENDOR, QUALCOMM
  .reloc test_jal_2, R_RISCV_CUSTOM195, test_jal_2

test_jal_3:
  qc.e.jal 0x0
  .reloc test_jal_3, R_RISCV_VENDOR, QUALCOMM
  .reloc test_jal_3, R_RISCV_CUSTOM195, test_distinct

test_jal_4:
  qc.e.jal 0x0
  .reloc test_jal_4, R_RISCV_VENDOR, QUALCOMM
  .reloc test_jal_4, R_RISCV_CUSTOM195, test_fffe

test_jal_5:
  qc.e.jal 0x0
  .reloc test_jal_5, R_RISCV_VENDOR, QUALCOMM
  .reloc test_jal_5, R_RISCV_CUSTOM195, test_vis_default

test_jal_6:
  qc.e.jal 0x0
  .reloc test_jal_6, R_RISCV_VENDOR, QUALCOMM
  .reloc test_jal_6, R_RISCV_CUSTOM195, test_vis_hidden

test_jal_7:
  qc.e.jal 0x0
  .reloc test_jal_7, R_RISCV_VENDOR, QUALCOMM
  .reloc test_jal_7, R_RISCV_CUSTOM195, test_vis_internal

test_jal_8:
  qc.e.jal 0x0
  .reloc test_jal_8, R_RISCV_VENDOR, QUALCOMM
  .reloc test_jal_8, R_RISCV_CUSTOM195, test_vis_protected
