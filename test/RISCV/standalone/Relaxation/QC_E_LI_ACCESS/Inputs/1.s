  .option exact
  .option relax

  .text
  .p2align 2
  .globl main
  .type main, @function

QUALCOMM:

main:

  ## Five cases for lw (ACCESS_32):
  ##   GP-std, GP-xqci, Abs-std, Abs-xqci, missed.

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

  qc.e.li a1, sym_abs_std
3:
  lw a0, 0(a1)
  .reloc 3b, R_RISCV_VENDOR, QUALCOMM
  .reloc 3b, R_RISCV_CUSTOM197, sym_abs_std
  .reloc 3b, R_RISCV_RELAX

  qc.e.li a1, sym_abs_xqci
4:
  lw a0, 0(a1)
  .reloc 4b, R_RISCV_VENDOR, QUALCOMM
  .reloc 4b, R_RISCV_CUSTOM197, sym_abs_xqci
  .reloc 4b, R_RISCV_RELAX

  qc.e.li a1, sym_too_far
5:
  lw a0, 0(a1)
  .reloc 5b, R_RISCV_VENDOR, QUALCOMM
  .reloc 5b, R_RISCV_CUSTOM197, sym_too_far
  .reloc 5b, R_RISCV_RELAX

  ## Five cases for c.lw (ACCESS_16):
  ##   GP-std, GP-xqci, Abs-std, Abs-xqci, missed.

  qc.e.li a1, sym_gprel_std
6:
  c.lw a0, 0(a1)
  .reloc 6b, R_RISCV_VENDOR, QUALCOMM
  .reloc 6b, R_RISCV_CUSTOM196, sym_gprel_std
  .reloc 6b, R_RISCV_RELAX

  qc.e.li a1, sym_gprel_xqci
7:
  c.lw a0, 0(a1)
  .reloc 7b, R_RISCV_VENDOR, QUALCOMM
  .reloc 7b, R_RISCV_CUSTOM196, sym_gprel_xqci
  .reloc 7b, R_RISCV_RELAX

  qc.e.li a1, sym_abs_std
8:
  c.lw a0, 0(a1)
  .reloc 8b, R_RISCV_VENDOR, QUALCOMM
  .reloc 8b, R_RISCV_CUSTOM196, sym_abs_std
  .reloc 8b, R_RISCV_RELAX

  qc.e.li a1, sym_abs_xqci
9:
  c.lw a0, 0(a1)
  .reloc 9b, R_RISCV_VENDOR, QUALCOMM
  .reloc 9b, R_RISCV_CUSTOM196, sym_abs_xqci
  .reloc 9b, R_RISCV_RELAX

  qc.e.li a1, sym_too_far
10:
  c.lw a0, 0(a1)
  .reloc 10b, R_RISCV_VENDOR, QUALCOMM
  .reloc 10b, R_RISCV_CUSTOM196, sym_too_far
  .reloc 10b, R_RISCV_RELAX

  ## One GP-std case for each remaining load type (ACCESS_32).

  qc.e.li a1, sym_gprel_std
11:
  lb a0, 0(a1)
  .reloc 11b, R_RISCV_VENDOR, QUALCOMM
  .reloc 11b, R_RISCV_CUSTOM197, sym_gprel_std
  .reloc 11b, R_RISCV_RELAX

  qc.e.li a1, sym_gprel_std
12:
  lbu a0, 0(a1)
  .reloc 12b, R_RISCV_VENDOR, QUALCOMM
  .reloc 12b, R_RISCV_CUSTOM197, sym_gprel_std
  .reloc 12b, R_RISCV_RELAX

  qc.e.li a1, sym_gprel_std
13:
  lh a0, 0(a1)
  .reloc 13b, R_RISCV_VENDOR, QUALCOMM
  .reloc 13b, R_RISCV_CUSTOM197, sym_gprel_std
  .reloc 13b, R_RISCV_RELAX

  qc.e.li a1, sym_gprel_std
14:
  lhu a0, 0(a1)
  .reloc 14b, R_RISCV_VENDOR, QUALCOMM
  .reloc 14b, R_RISCV_CUSTOM197, sym_gprel_std
  .reloc 14b, R_RISCV_RELAX

  ## One GP-std case for each store type (ACCESS_32).

  qc.e.li a1, sym_gprel_std
15:
  sw a0, 0(a1)
  .reloc 15b, R_RISCV_VENDOR, QUALCOMM
  .reloc 15b, R_RISCV_CUSTOM197, sym_gprel_std
  .reloc 15b, R_RISCV_RELAX

  qc.e.li a1, sym_gprel_std
16:
  sb a0, 0(a1)
  .reloc 16b, R_RISCV_VENDOR, QUALCOMM
  .reloc 16b, R_RISCV_CUSTOM197, sym_gprel_std
  .reloc 16b, R_RISCV_RELAX

  qc.e.li a1, sym_gprel_std
17:
  sh a0, 0(a1)
  .reloc 17b, R_RISCV_VENDOR, QUALCOMM
  .reloc 17b, R_RISCV_CUSTOM197, sym_gprel_std
  .reloc 17b, R_RISCV_RELAX

  ## One GP-std case for each remaining 16-bit compressed type (ACCESS_16).

  qc.e.li a1, sym_gprel_std
18:
  c.sw a0, 0(a1)
  .reloc 18b, R_RISCV_VENDOR, QUALCOMM
  .reloc 18b, R_RISCV_CUSTOM196, sym_gprel_std
  .reloc 18b, R_RISCV_RELAX

  qc.e.li a1, sym_gprel_std
19:
  c.lbu a0, 0(a1)
  .reloc 19b, R_RISCV_VENDOR, QUALCOMM
  .reloc 19b, R_RISCV_CUSTOM196, sym_gprel_std
  .reloc 19b, R_RISCV_RELAX

  qc.e.li a1, sym_gprel_std
20:
  c.lhu a0, 0(a1)
  .reloc 20b, R_RISCV_VENDOR, QUALCOMM
  .reloc 20b, R_RISCV_CUSTOM196, sym_gprel_std
  .reloc 20b, R_RISCV_RELAX

  qc.e.li a1, sym_gprel_std
21:
  c.lh a0, 0(a1)
  .reloc 21b, R_RISCV_VENDOR, QUALCOMM
  .reloc 21b, R_RISCV_CUSTOM196, sym_gprel_std
  .reloc 21b, R_RISCV_RELAX

  qc.e.li a1, sym_gprel_std
22:
  c.sb a0, 0(a1)
  .reloc 22b, R_RISCV_VENDOR, QUALCOMM
  .reloc 22b, R_RISCV_CUSTOM196, sym_gprel_std
  .reloc 22b, R_RISCV_RELAX

  qc.e.li a1, sym_gprel_std
23:
  c.sh a0, 0(a1)
  .reloc 23b, R_RISCV_VENDOR, QUALCOMM
  .reloc 23b, R_RISCV_CUSTOM196, sym_gprel_std
  .reloc 23b, R_RISCV_RELAX

  ## Non-zero offsets: the offset in the access instruction is folded into the
  ## addend; with sym_gprel_std = GP+32, all effective addresses remain within
  ## isInt<12> of GP and GP-std fires.

  qc.e.li a1, sym_gprel_std
24:
  lw a0, 4(a1)
  .reloc 24b, R_RISCV_VENDOR, QUALCOMM
  .reloc 24b, R_RISCV_CUSTOM197, sym_gprel_std
  .reloc 24b, R_RISCV_RELAX

  qc.e.li a1, sym_gprel_std
25:
  sw a0, 4(a1)
  .reloc 25b, R_RISCV_VENDOR, QUALCOMM
  .reloc 25b, R_RISCV_CUSTOM197, sym_gprel_std
  .reloc 25b, R_RISCV_RELAX

  qc.e.li a1, sym_gprel_std
26:
  c.lw a0, 4(a1)
  .reloc 26b, R_RISCV_VENDOR, QUALCOMM
  .reloc 26b, R_RISCV_CUSTOM196, sym_gprel_std
  .reloc 26b, R_RISCV_RELAX

  qc.e.li a1, sym_gprel_std
27:
  c.lbu a0, 1(a1)
  .reloc 27b, R_RISCV_VENDOR, QUALCOMM
  .reloc 27b, R_RISCV_CUSTOM196, sym_gprel_std
  .reloc 27b, R_RISCV_RELAX

  qc.e.li a1, sym_gprel_std
28:
  c.lhu a0, 2(a1)
  .reloc 28b, R_RISCV_VENDOR, QUALCOMM
  .reloc 28b, R_RISCV_CUSTOM196, sym_gprel_std
  .reloc 28b, R_RISCV_RELAX

  qc.e.li a1, sym_gprel_std
29:
  c.lh a0, 2(a1)
  .reloc 29b, R_RISCV_VENDOR, QUALCOMM
  .reloc 29b, R_RISCV_CUSTOM196, sym_gprel_std
  .reloc 29b, R_RISCV_RELAX

  qc.e.li a1, sym_gprel_std
30:
  c.sb a0, 1(a1)
  .reloc 30b, R_RISCV_VENDOR, QUALCOMM
  .reloc 30b, R_RISCV_CUSTOM196, sym_gprel_std
  .reloc 30b, R_RISCV_RELAX

  qc.e.li a1, sym_gprel_std
31:
  c.sh a0, 2(a1)
  .reloc 31b, R_RISCV_VENDOR, QUALCOMM
  .reloc 31b, R_RISCV_CUSTOM196, sym_gprel_std
  .reloc 31b, R_RISCV_RELAX

  ## Edge cases: ACCESS without R_RISCV_RELAX, and base-register mismatch.

  # ACCESS without R_RISCV_RELAX: R_RISCV_RELAX is required on the ACCESS reloc
  # for pair relaxation; without it qc.e.li falls back to qc.li-only relaxation.
  qc.e.li a1, sym_abs_std
32:
  lw a0, 0(a1)
  .reloc 32b, R_RISCV_VENDOR, QUALCOMM
  .reloc 32b, R_RISCV_CUSTOM197, sym_abs_std
  # Intentionally no R_RISCV_RELAX on the ACCESS reloc

  # Base-register mismatch: qc.e.li loads into a1 but the lw uses a2,
  # so ACCESS decode fails and qc.e.li falls back to standalone relaxation.
  qc.e.li a1, sym_gprel_std
33:
  lw a0, 0(a2)
  .reloc 33b, R_RISCV_VENDOR, QUALCOMM
  .reloc 33b, R_RISCV_CUSTOM197, sym_gprel_std
  .reloc 33b, R_RISCV_RELAX

  .size main, .-main
