  .option exact
  .option relax

  .text
  .p2align 2
  .globl main
  .type main, @function

QUALCOMM:

main:

  ## GP-std range boundary tests (--relax-xqci with GP relax).
  ## isInt<12>(sym - GP): range is [-2048, +2047].

  ## At the positive edge of GP-std range (+2047): → GP-std relaxation.
  qc.e.li a1, sym_gp_std_pos_edge
1:
  lw a0, 0(a1)
  .reloc 1b, R_RISCV_VENDOR, QUALCOMM
  .reloc 1b, R_RISCV_CUSTOM197, sym_gp_std_pos_edge
  .reloc 1b, R_RISCV_RELAX

  ## At the negative edge of GP-std range (-2048): → GP-std relaxation.
  qc.e.li a1, sym_gp_std_neg_edge
2:
  lw a0, 0(a1)
  .reloc 2b, R_RISCV_VENDOR, QUALCOMM
  .reloc 2b, R_RISCV_CUSTOM197, sym_gp_std_neg_edge
  .reloc 2b, R_RISCV_RELAX

  ## Just outside the positive GP-std range (+2048): → GP-xqci relaxation.
  qc.e.li a1, sym_gp_std_pos_out
3:
  lw a0, 0(a1)
  .reloc 3b, R_RISCV_VENDOR, QUALCOMM
  .reloc 3b, R_RISCV_CUSTOM197, sym_gp_std_pos_out
  .reloc 3b, R_RISCV_RELAX

  ## Just outside the negative GP-std range (-2049): → GP-xqci relaxation.
  qc.e.li a1, sym_gp_std_neg_out
4:
  lw a0, 0(a1)
  .reloc 4b, R_RISCV_VENDOR, QUALCOMM
  .reloc 4b, R_RISCV_CUSTOM197, sym_gp_std_neg_out
  .reloc 4b, R_RISCV_RELAX

  ## GP-xqci range boundary tests.
  ## isInt<26>(sym - GP): range is [-33554432, +33554431].

  ## At the positive edge of GP-xqci range (+33554431): → GP-xqci relaxation.
  qc.e.li a1, sym_gp_xqci_pos_edge
5:
  lw a0, 0(a1)
  .reloc 5b, R_RISCV_VENDOR, QUALCOMM
  .reloc 5b, R_RISCV_CUSTOM197, sym_gp_xqci_pos_edge
  .reloc 5b, R_RISCV_RELAX

  ## At the negative edge of GP-xqci range (-33554432): → GP-xqci relaxation.
  qc.e.li a1, sym_gp_xqci_neg_edge
6:
  lw a0, 0(a1)
  .reloc 6b, R_RISCV_VENDOR, QUALCOMM
  .reloc 6b, R_RISCV_CUSTOM197, sym_gp_xqci_neg_edge
  .reloc 6b, R_RISCV_RELAX

  ## Just outside the positive GP-xqci range (+33554432): → missed.
  qc.e.li a1, sym_gp_xqci_pos_out
7:
  lw a0, 0(a1)
  .reloc 7b, R_RISCV_VENDOR, QUALCOMM
  .reloc 7b, R_RISCV_CUSTOM197, sym_gp_xqci_pos_out
  .reloc 7b, R_RISCV_RELAX

  ## Just outside the negative GP-xqci range (-33554433): → missed.
  qc.e.li a1, sym_gp_xqci_neg_out
8:
  lw a0, 0(a1)
  .reloc 8b, R_RISCV_VENDOR, QUALCOMM
  .reloc 8b, R_RISCV_CUSTOM197, sym_gp_xqci_neg_out
  .reloc 8b, R_RISCV_RELAX

  ## Abs-std range boundary tests (ZeroRelax enabled, no GP relax).
  ## isInt<12>(sym): range is [-2048, +2047]; sym must be non-zero.

  ## At the positive edge of Abs-std range (2047): → Abs-std relaxation.
  qc.e.li a1, sym_abs_std_edge
9:
  lw a0, 0(a1)
  .reloc 9b, R_RISCV_VENDOR, QUALCOMM
  .reloc 9b, R_RISCV_CUSTOM197, sym_abs_std_edge
  .reloc 9b, R_RISCV_RELAX

  ## Just outside the positive Abs-std range (2048): → Abs-xqci relaxation.
  qc.e.li a1, sym_abs_std_out
10:
  lw a0, 0(a1)
  .reloc 10b, R_RISCV_VENDOR, QUALCOMM
  .reloc 10b, R_RISCV_CUSTOM197, sym_abs_std_out
  .reloc 10b, R_RISCV_RELAX

  ## Abs-xqci range boundary tests (ZeroRelax disabled, no GP relax).
  ## isInt<26>(sym): range is [1, 33554431].

  ## At the positive edge of Abs-xqci range (33554431): → Abs-xqci relaxation.
  qc.e.li a1, sym_abs_xqci_edge
11:
  lw a0, 0(a1)
  .reloc 11b, R_RISCV_VENDOR, QUALCOMM
  .reloc 11b, R_RISCV_CUSTOM197, sym_abs_xqci_edge
  .reloc 11b, R_RISCV_RELAX

  ## Just outside the positive Abs-xqci range (33554432): → missed.
  qc.e.li a1, sym_abs_xqci_out
12:
  lw a0, 0(a1)
  .reloc 12b, R_RISCV_VENDOR, QUALCOMM
  .reloc 12b, R_RISCV_CUSTOM197, sym_abs_xqci_out
  .reloc 12b, R_RISCV_RELAX

  .size main, .-main
