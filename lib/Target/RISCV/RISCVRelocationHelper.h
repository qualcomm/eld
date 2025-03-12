//===- RISCVRelocationHelper.h---------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef RISCV_RELOCATION_HELPER_H
#define RISCV_RELOCATION_HELPER_H

namespace {

enum Op {
  ADDI = 0x13,
  AUIPC = 0x17,
  JALR = 0x67,
  LD = 0x3003,
  LW = 0x2003,
  SRLI = 0x5013,
  SUB = 0x40000033,
};

enum Reg {
  X_RA = 1,
  X_SP = 2,
  X_GP = 3,
  X_TP = 4,
  X_T0 = 5,
  X_T1 = 6,
  X_T2 = 7,
  X_T3 = 28
};

[[maybe_unused]] uint32_t hi20(uint32_t val) { return (val + 0x800) >> 12; }
[[maybe_unused]] uint32_t lo12(uint32_t val) { return val & 4095; }

[[maybe_unused]] uint32_t itype(uint32_t op, uint32_t rd, uint32_t rs1,
                                uint32_t imm) {
  return op | (rd << 7) | (rs1 << 15) | (imm << 20);
}
[[maybe_unused]] uint32_t rtype(uint32_t op, uint32_t rd, uint32_t rs1,
                                uint32_t rs2) {
  return op | (rd << 7) | (rs1 << 15) | (rs2 << 20);
}
[[maybe_unused]] uint32_t utype(uint32_t op, uint32_t rd, uint32_t imm) {
  return op | (rd << 7) | (imm << 12);
}

// Extract bits v[begin:end], where range is inclusive, and begin must be < 63.
[[maybe_unused]] uint32_t extractBits(uint64_t v, uint32_t begin,
                                      uint32_t end) {
  return (v & ((1ULL << (begin + 1)) - 1)) >> end;
}

[[maybe_unused]] uint32_t setLO12_I(uint32_t insn, uint32_t imm) {
  return (insn & 0xfffff) | (imm << 20);
}
[[maybe_unused]] uint32_t setLO12_S(uint32_t insn, uint32_t imm) {
  return (insn & 0x1fff07f) | (extractBits(imm, 11, 5) << 25) |
         (extractBits(imm, 4, 0) << 7);
}

[[maybe_unused]] unsigned int instruction_length(uint64_t instr) {
  /* Compressed instruction */
  if ((instr & 0x3) != 0x3)
    return 2;
  // 32-bit
  if ((instr & 0x1f) != 0x1f)
    return 4;
  // 48-bit
  if ((instr & 0x3f) == 0x1f)
    return 6;
  // 64bit
  if ((instr & 0x7f) == 0x3f)
    return 8;
  return 0;
}

} // namespace

#endif
