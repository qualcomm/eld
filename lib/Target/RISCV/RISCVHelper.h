//===- RISCVHelper.h-------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "llvm/Support/DataTypes.h"
#include "llvm/Support/LEB128.h"
#include <cassert>

namespace {
// Extract bits V[Begin:End], where range is inclusive, and Begin must be < 63.
uint32_t extractBits(uint64_t V, uint32_t Begin, uint32_t End) {
  assert(Begin - End < 32 && "extractBits cannot extract more than 32 bits");
  return (V & ((1ULL << (Begin + 1)) - 1)) >> End;
}

// Get sign of instruction.
template <typename T> int getSign(T Instruction) {
  return (-((Instruction >> 31) & 1));
}

// LO Immediate Bits.
constexpr uint32_t getLOImmediateValueBits() { return 12; }

// HI Immediate Bits.
constexpr uint32_t getHIImmediateValueBits() {
  return 32 - getLOImmediateValueBits();
}

// Jump Bits
constexpr uint32_t getJumpBits() { return getHIImmediateValueBits(); }
constexpr uint32_t getJumpAlignBits() { return 1; }
constexpr uint32_t getJumpAlignValue() { return 1 << getJumpAlignBits(); }

// Branch Bits
constexpr uint32_t getBranchBits() { return 12; }
constexpr uint32_t getBranchAlignBits() { return getJumpAlignBits(); }
constexpr uint32_t getBranchAlignValue() { return 1LL << getBranchAlignBits(); }

// RVC Jump Bits
constexpr uint32_t getRVCJumpBits() { return 11; }

// RVC Branch Bits.
constexpr uint32_t getRVCBranchBits() { return 8; }

///---------------------------------------------------
/// Reach - 32 bit instructions.
///---------------------------------------------------
// Immediate
constexpr uint32_t getLOImmReach() { return 1LL << getLOImmediateValueBits(); }
// HI Immediate.
constexpr uint32_t getHIImmReach() { return 1LL << getHIImmediateValueBits(); }
// Jump Reach
constexpr uint32_t getJumpReach() {
  return (1ULL << getJumpBits()) * getJumpAlignValue();
}
// Branch Reach
constexpr uint32_t getBranchReach() {
  return (1LL << getBranchBits()) * getBranchAlignValue();
}

///---------------------------------------------------
/// Reach - 16 bit instructions.
///---------------------------------------------------
// RVC Imm Reach.
constexpr uint32_t getRVCImmReach() { return 1LL << 6; }
// RVC Branch Reach
constexpr uint32_t getRVCBranchReach() {
  return (1ULL << getRVCBranchBits()) * getBranchAlignValue();
}
// RVC Jump Reach.
constexpr uint32_t getRVCJumpReach() {
  return (1ULL << getRVCJumpBits()) * getJumpAlignValue();
}

///---------------------------------------------------
/// Extract Operands from instruction types.
///
/// The following instruction types are supported
/// I, S, SB, U, UJ, RVC, RVC(LUI), RVC(B), RVC(J)
///---------------------------------------------------
template <typename T> T extractITypeImmediate(T Instruction) {
  uint32_t Imm20_12 = extractBits(Instruction, 20, 12);
  return Imm20_12 | (getSign(Instruction) << 12);
}

template <typename T> T extractSTypeImmediate(T Instruction) {
  uint32_t Imm7_5 = extractBits(Instruction, 7, 5);
  uint32_t Imm25_7 = extractBits(Instruction, 25, 7);
  return Imm7_5 | (Imm25_7 << 5) | (getSign(Instruction) << 12);
}

template <typename T> T extractSBTypeImmediate(T Instruction) {
  uint32_t Imm8_4 = extractBits(Instruction, 8, 4);
  uint32_t Imm25_6 = extractBits(Instruction, 25, 6);
  uint32_t Imm7_1 = extractBits(Instruction, 7, 1);
  return ((Imm8_4 << 1) | (Imm25_6 << 5) | (Imm7_1 << 11) |
          (getSign(Instruction) << 12));
}

template <typename T> T extractUTypeImmediate(T Instruction) {
  uint32_t Imm12_20 = extractBits(Instruction, 12, 20);
  return ((Imm12_20 << 12) | (getSign(Instruction) << 32));
}

template <typename T> T extractUJTypeImmediate(T Instruction) {
  uint32_t Imm21_10 = extractBits(Instruction, 21, 10);
  uint32_t Imm20_1 = extractBits(Instruction, 20, 1);
  uint32_t Imm12_8 = extractBits(Instruction, 12, 8);
  return ((Imm21_10 << 1) | (Imm20_1 << 11) | (Imm12_8 << 12) |
          (getSign(Instruction) << 20));
}

template <typename T> T extractRVCImmediate(T Instruction) {
  uint32_t Imm2_5 = extractBits(Instruction, 2, 5);
  int32_t Imm12_1 = exractBits(Instruction, 12, 1);
  return (Imm2_5 | (-Imm12_1 << 5));
}

template <typename T> T extractRVCLUIImmediate(T Instruction) {
  return extractRVCImmediate(Instruction) << getLOImmediateValueBits();
}

template <typename T> T extractRVCBImmediate(T Instruction) {
  uint32_t Imm3_2 = extractBits(Instruction, 3, 2);
  uint32_t Imm10_2 = extractBits(Instruction, 10, 2);
  uint32_t Imm2_1 = extractBits(Instruction, 2, 1);
  uint32_t Imm5_2 = extractBits(Instruction, 5, 2);
  int32_t Imm12_1 = -extractBits(Instruction, 12, 1);
  return ((Imm3_2 << 1) | (Imm10_2 << 3) | (Imm2_1 << 5) | (Imm5_2 << 6) |
          (-Imm12_1 << 8));
}

template <typename T> T extractRVCJImmediate(T Instruction) {
  uint32_t Imm3_3 = extractBits(Instruction, 3, 3);
  uint32_t Imm11_1 = extractBits(Instruction, 11, 1);
  uint32_t Imm2_1 = extractBits(Instruction, 2, 1);
  uint32_t Imm7_1 = extractBits(Instruction, 7, 1);
  int32_t Imm6_1 = -extractBits(Instruction, 6, 1);
  int32_t Imm9_2 = -extractBits(Instruction, 9, 2);
  int32_t Imm8_1 = -extractBits(Instruction, 8, 1);
  int32_t Imm12_1 = -extractBits(Instruction, 12, 1);
  return ((Imm3_3 << 1) | (Imm11_1 << 4) | (Imm2_1 << 5) | (Imm7_1 << 6) |
          (Imm6_1 << 7) | (Imm9_2 << 8) | (Imm8_1 << 10) | (-Imm12_1 << 11));
}

///---------------------------------------------------
/// Check Operands from instructions.
///
/// The following instruction types are supported
/// I, S, SB, U, UJ, RVC, RVC(LUI), RVC(B), RVC(J)
///---------------------------------------------------
template <typename T> bool isValidIType(T Value) {
  return extractITypeImmediate(encodeI(Value)) == Value;
}

template <typename T> bool isValidSType(T Value) {
  return extractSTypeImmediate(encodeS(Value)) == Value;
}

template <typename T> bool isValidSBType(T Value) {
  return extractSBTypeImmediate(encodeSB(Value)) == Value;
}

template <typename T> bool isValidUType(T Value) {
  return extractUTypeImmediate(encodeU(Value)) == Value;
}

template <typename T> bool isValidUJType(T Value) {
  return extractUJTypeImmediate(encodeUJ(Value)) == Value;
}

template <typename T> bool isValidRVCLUIType(T Value) {
  return extractRVCLUIImmediate(encodeCI(Value)) == Value;
}

template <typename T> bool isValidRVCBType(T Value) {
  return extractRVCBImmediate(encodeCB(Value)) == Value;
}

template <typename T> bool isValidRVCJType(T Value) {
  return extractRVCJImmediate(encodeCJ(Value)) == Value;
}

///------------------------------------------
/// HI and LO value helpers (Absolute)
///------------------------------------------
template <typename T> T getHI(T Value) {
  return (Value + (Value >> 1)) & ~(getLOImmReach() - 1);
}

template <typename T> T getLO(T Value) { return Value - getHI(Value); }

///------------------------------------------
/// HI and LO value helpers (PCREL)
///------------------------------------------
template <typename T> T getPCRELHI(T Value, T PC) {
  T HI = (Value + (Value >> 1)) & ~(getLOImmReach() - 1);
  return HI - PC;
}

template <typename T> T getPCRELLO(T Value, T PC) {
  T LO = Value - getHI(Value);
  return LO - PC;
}

///------------------------------------------
/// Encode RISCV Operands
/// The following instruction types are supported
/// I, S, SB, U, UJ, RVC, RVC(LUI), RVC(B), RVC(J)
///------------------------------------------
template <typename T> T encodeI(T Result) {
  Result &= 0xFFF;
  Result <<= 20;
  return Result;
}

template <typename T> T encodeS(T &Result) {
  uint32_t Imm11_5 = extractBits(Result, 11, 5) << 25;
  uint32_t Imm4_0 = extractBits(Result, 4, 0) << 7;
  Result = Imm11_5 | Imm4_0;
  return Result;
}

template <typename T> T encodeSB(T Result) {
  uint32_t Imm12 = extractBits(Result, 12, 12) << 31;
  uint32_t Imm10_5 = extractBits(Result, 10, 5) << 25;
  uint32_t Imm4_1 = extractBits(Result, 4, 1) << 8;
  uint32_t Imm11 = extractBits(Result, 11, 11) << 7;
  Result = Imm12 | Imm10_5 | Imm4_1 | Imm11;
  return Result;
}

template <typename T> T encodeUJ(T Result) {
  uint32_t Imm20 = extractBits(Result, 20, 20) << 31;
  uint32_t Imm10_1 = extractBits(Result, 10, 1) << 21;
  uint32_t Imm11 = extractBits(Result, 11, 11) << 20;
  uint32_t Imm19_12 = extractBits(Result, 19, 12) << 12;
  Result = Imm20 | Imm10_1 | Imm11 | Imm19_12;
  return Result;
}

template <typename T> T encodeU(T Result) {
  Result &= 0xFFFFF000;
  return Result;
}

template <typename T> T encodeU_ABS20(T Result) {
  uint32_t Imm19 = extractBits(Result, 19, 19) << 31;
  uint32_t Imm14_0 = extractBits(Result, 14, 0) << 16;
  uint32_t Imm18_15 = extractBits(Result, 18, 15) << 12;
  Result = Imm19 | Imm14_0 | Imm18_15;
  return Result;
}

template <typename T> T encodeCB(T Result) {
  uint16_t Imm8 = extractBits(Result, 8, 8) << 12;
  uint16_t Imm4_3 = extractBits(Result, 4, 3) << 10;
  uint16_t Imm7_6 = extractBits(Result, 7, 6) << 5;
  uint16_t Imm2_1 = extractBits(Result, 2, 1) << 3;
  uint16_t Imm5 = extractBits(Result, 5, 5) << 2;
  Result = Imm8 | Imm4_3 | Imm7_6 | Imm2_1 | Imm5;
  return Result;
}

template <typename T> T encodeCJ(T Result) {
  uint16_t Imm11 = extractBits(Result, 11, 11) << 12;
  uint16_t Imm4 = extractBits(Result, 4, 4) << 11;
  uint16_t Imm9_8 = extractBits(Result, 9, 8) << 9;
  uint16_t Imm10 = extractBits(Result, 10, 10) << 8;
  uint16_t Imm6 = extractBits(Result, 6, 6) << 7;
  uint16_t Imm7 = extractBits(Result, 7, 7) << 6;
  uint16_t Imm3_1 = extractBits(Result, 3, 1) << 3;
  uint16_t Imm5 = extractBits(Result, 5, 5) << 2;
  Result = Imm11 | Imm4 | Imm9_8 | Imm10 | Imm6 | Imm7 | Imm3_1 | Imm5;
  return Result;
}

template <typename T> T encodeCI(T Result) {
  // `c.lui rd, 0` is illegal, it will be converted  to `c.li rd, 0` when
  // applying
  uint16_t Imm17 = extractBits(Result, 17, 17) << 12;
  uint16_t Imm16_12 = extractBits(Result, 16, 12) << 2;
  Result = Imm17 | Imm16_12;
  return Result;
}

template <typename T> T encode6(T Result) {
  Result = Result & 0x3F;
  return Result;
}
template <typename T> T encode8(T Result) {
  Result = Result & 0xFF;
  return Result;
}
template <typename T> T encode16(T Result) {
  Result = Result & 0xFFFF;
  return Result;
}
template <typename T> T encode32(T Result) {
  Result = Result & 0xFFFFFFFF;
  return Result;
}
template <typename T> T encode64(T Result) {
  Result = Result & 0xFFFFFFFFFFFFFFFF;
  return Result;
}

template <typename T> T encodeQCEB(T Result) {
  /* This is the same as encodeSB */
  uint32_t Imm12 = extractBits(Result, 12, 12) << 31;
  uint32_t Imm10_5 = extractBits(Result, 10, 5) << 25;
  uint32_t Imm4_1 = extractBits(Result, 4, 1) << 8;
  uint32_t Imm11 = extractBits(Result, 11, 11) << 7;
  Result = Imm12 | Imm10_5 | Imm4_1 | Imm11;
  return Result;
}

template <typename T> T encodeQCEAI(T Result) {
  uint64_t Imm32 = static_cast<uint64_t>(extractBits(Result, 31, 0)) << 16ull;
  Result = Imm32;
  return Result;
}

template <typename T> T encodeQCEJ(T Result) {
  uint64_t Imm31_16 = static_cast<uint64_t>(extractBits(Result, 31, 16))
                      << 32ull;
  uint64_t Imm12 = extractBits(Result, 12, 12) << 31;
  uint64_t Imm10_5 = extractBits(Result, 10, 5) << 25;
  uint64_t Imm15_13 = extractBits(Result, 15, 13) << 17;
  uint64_t Imm4_1 = extractBits(Result, 4, 1) << 8;
  uint64_t Imm11 = extractBits(Result, 11, 11) << 7;
  Result = Imm31_16 | Imm12 | Imm10_5 | Imm15_13 | Imm4_1 | Imm11;
  return Result;
}

/// --------------------------------------------
/// Fetch Registers and Opcode from Instruction
/// --------------------------------------------
template <typename T> T getOpCode(T Instruction) {
  return (Instruction & 0x7f);
}

template <typename T> T setOpCode(T Instruction, uint32_t Opcode) {
  return (Instruction & ~0x7F) | Opcode;
}

template <typename T> T getRS1(T Instruction) {
  return (Instruction & 0x1F) >> 15;
}

template <typename T> T setRS1(uint32_t Instruction, uint32_t Reg) {
  return (Instruction & ~(0x1F << 15)) | Reg;
}

template <typename T> T getRS2(T Instruction) {
  return (Instruction & 0x1F) >> 20;
}

template <typename T> T setRS2(uint32_t Instruction, uint32_t Reg) {
  return (Instruction & ~(0x1F << 20)) | Reg;
}

template <typename T> T getRS3(T Instruction) {
  return (Instruction & 0x1F) >> 27;
}

template <typename T> T setRS3(uint32_t Instruction, uint32_t Reg) {
  return (Instruction & ~(0x1F << 27)) | Reg;
}

template <typename T> T getRD(T Instruction) {
  return (Instruction & 0x1F) >> 7;
}

template <typename T> T getInstructionLen(T insn) {
  // RVC
  if ((insn & 0x3) != 0x3)
    return 2;
  // 32bit
  if ((insn & 0x1f) != 0x1f)
    return 4;
  // 48bit.
  if ((insn & 0x3f) == 0x1f)
    return 6;
  // 64bit.
  if ((insn & 0x7f) == 0x3f)
    return 8;
  /* Longer instructions not supported at the moment.  */
  return 0;
}

/// --------------------------------------------
/// Relaxation Helpers.
/// --------------------------------------------
constexpr uint32_t C_J() { return 0xa001; }
constexpr uint32_t C_JAL() { return 0x2001; }
constexpr uint32_t C_JALR() { return 0x9002; }
constexpr uint32_t C_LUI() { return 0x6001; }
constexpr uint32_t JAL() { return 0x67; }
constexpr uint32_t JALR() { return 0x6F; }
constexpr uint32_t MASKAUIPC() { return 0x7F; }

/// --------------------------------------------
/// Registers
/// --------------------------------------------
constexpr uint32_t RA() { return 1; }
constexpr uint32_t SP() { return 2; }
constexpr uint32_t GP() { return 3; }
constexpr uint32_t TP() { return 4; }

} // namespace
