  # Required until https://github.com/llvm/llvm-project/pull/146184 lands
  .option exact

  # Enable relaxations on qc.e.li instructions
  .option relax

  .text
  .p2align 2
  .globl main
  .type main, @function

QUALCOMM:

main:

  ## Case 1: Abs-std relaxation
  ## sym_abs_std = 0x100 is within isInt<12>, so Abs-std applies:
  ##   qc.e.li + lw -> lw a0, 256(zero), deletes 6 bytes

  qc.e.li a1, sym_abs_std
1:
  lw a0, 0(a1)
  .reloc 1b, R_RISCV_VENDOR, QUALCOMM
  .reloc 1b, R_RISCV_CUSTOM197, sym_abs_std
  .reloc 1b, R_RISCV_RELAX

  ## Case 2: ACCESS without R_RISCV_RELAX on the load/store
  ## R_RISCV_RELAX is required on the ACCESS reloc for pair relaxation.
  ## Without it, qc.e.li falls back to qc.li-only relaxation.

  qc.e.li a1, sym_abs_std
2:
  lw a0, 0(a1)
  .reloc 2b, R_RISCV_VENDOR, QUALCOMM
  .reloc 2b, R_RISCV_CUSTOM197, sym_abs_std
  # Intentionally no R_RISCV_RELAX on the ACCESS reloc

  ## Case 3: Base register mismatch — ACCESS decode fails, falls back to
  ## qc.e.li-only relaxation. qc.e.li loads into a1 but the lw uses a2.

  qc.e.li a1, sym_gprel_std
3:
  lw a0, 0(a2)
  .reloc 3b, R_RISCV_VENDOR, QUALCOMM
  .reloc 3b, R_RISCV_CUSTOM197, sym_gprel_std
  .reloc 3b, R_RISCV_RELAX

  ## Case 4: Store (sw) with ACCESS_32, GP-std range
  ## sym_gprel_std is within isInt<12>(sym - GP), so GP-std applies:
  ##   qc.e.li + sw -> sw a0, 32(gp)

  qc.e.li a1, sym_gprel_std
4:
  sw a0, 0(a1)
  .reloc 4b, R_RISCV_VENDOR, QUALCOMM
  .reloc 4b, R_RISCV_CUSTOM197, sym_gprel_std
  .reloc 4b, R_RISCV_RELAX

  ## Case 5: c.sw (ACCESS_16, CS format), GP-std range
  ## 16-bit store; GP-std applies, result is a 4-byte sw.

  qc.e.li a1, sym_gprel_std
5:
  c.sw a0, 0(a1)
  .reloc 5b, R_RISCV_VENDOR, QUALCOMM
  .reloc 5b, R_RISCV_CUSTOM196, sym_gprel_std
  .reloc 5b, R_RISCV_RELAX

  ## Case 6: c.lbu (ACCESS_16, Zcb), GP-std range

  qc.e.li a1, sym_gprel_std
6:
  c.lbu a0, 0(a1)
  .reloc 6b, R_RISCV_VENDOR, QUALCOMM
  .reloc 6b, R_RISCV_CUSTOM196, sym_gprel_std
  .reloc 6b, R_RISCV_RELAX

  ## Case 7: c.sh (ACCESS_16, Zcb), GP-std range

  qc.e.li a1, sym_gprel_std
7:
  c.sh a0, 0(a1)
  .reloc 7b, R_RISCV_VENDOR, QUALCOMM
  .reloc 7b, R_RISCV_CUSTOM196, sym_gprel_std
  .reloc 7b, R_RISCV_RELAX

  .size main, .-main
