//===- AArch64InsnHelpers.h------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
//
//                     The MCLinker Project
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#ifndef AARCH64_AARCH64INSNHELPERS_H
#define AARCH64_AARCH64INSNHELPERS_H

#include "llvm/ADT/APInt.h"

namespace eld {

class AArch64InsnHelpers {
public:
  typedef uint32_t InsnType;

  static constexpr int InsnSize = 4;

  // Zero register encoding - 31.
  static constexpr unsigned int AARCH64_ZR = 31;

  static unsigned int bit(InsnType insn, int pos) {
    return ((1 << pos) & insn) >> pos;
  }

  static unsigned int bits(InsnType insn, int pos, int l) {
    return (insn >> pos) & ((1 << l) - 1);
  }

  // Get the encoding field "op31" of 3-source data processing insns. "op31" is
  // the name defined in armv8 insn manual C3.5.9.
  static unsigned int op31(InsnType insn) { return bits(insn, 21, 3); }

  // Get the encoding field "ra" of 3-source data processing insns. "ra" is the
  // third source register. See armv8 insn manual C3.5.9.
  static unsigned int ra(InsnType insn) { return bits(insn, 10, 5); }

  static bool is_adr(const InsnType insn) {
    return (insn & 0x9F000000) == 0x10000000;
  }

  static bool is_adrp(const InsnType insn) {
    return (insn & 0x9F000000) == 0x90000000;
  }

  static unsigned int rm(const InsnType insn) { return bits(insn, 16, 5); }

  static unsigned int rn(const InsnType insn) { return bits(insn, 5, 5); }

  static unsigned int rd(const InsnType insn) { return bits(insn, 0, 5); }

  static unsigned int rt(const InsnType insn) { return bits(insn, 0, 5); }

  static unsigned int rt2(const InsnType insn) { return bits(insn, 10, 5); }

  // Encode imm21 into adr. Signed imm21 is in the range of [-1M, 1M).
  static InsnType adr_encode_imm(InsnType adr, int imm21) {
    const int mask19 = (1 << 19) - 1;
    const int mask2 = 3;
    adr &= ~((mask19 << 5) | (mask2 << 29));
    adr |= ((imm21 & mask2) << 29) | (((imm21 >> 2) & mask19) << 5);
    return adr;
  }

  // Retrieve encoded adrp 33-bit signed imm value. This value is obtained by
  // 21-bit signed imm encoded in the insn multiplied by 4k (page size) and
  // 64-bit sign-extended, resulting in [-4G, 4G) with 12-lsb being 0.
  static int64_t adrp_decode_imm(const InsnType adrp) {
    const int mask19 = (1 << 19) - 1;
    const int mask2 = 3;
    // 21-bit imm encoded in adrp.
    uint64_t imm = ((adrp >> 29) & mask2) | (((adrp >> 5) & mask19) << 2);
    // Retrieve msb of 21-bit-signed imm for sign extension.
    uint64_t msbt = (imm >> 20) & 1;
    // Real value is imm multipled by 4k. Value now has 33-bit information.
    int64_t value = imm << 12;
    // Sign extend to 64-bit by repeating msbt 31 (64-33) times and merge it
    // with value.
    return ((((uint64_t)(1) << 32) - msbt) << 33) | value;
  }

  static bool b(const InsnType insn) {
    return (insn & 0xFC000000) == 0x14000000;
  }

  static bool bl(const InsnType insn) {
    return (insn & 0xFC000000) == 0x94000000;
  }

  static bool blr(const InsnType insn) {
    return (insn & 0xFFFFFC1F) == 0xD63F0000;
  }

  static bool br(const InsnType insn) {
    return (insn & 0xFFFFFC1F) == 0xD61F0000;
  }

  // All ld/st ops.  See C4-182 of the ARM ARM.  The encoding space for
  // LD_PCREL, LDST_RO, LDST_UI and LDST_UIMM cover prefetch ops.
  static bool ld(InsnType insn) { return bit(insn, 22) == 1; }

  static bool ldst(InsnType insn) { return (insn & 0x0a000000) == 0x08000000; }

  static bool ldst_ex(InsnType insn) {
    return (insn & 0x3f000000) == 0x08000000;
  }

  static bool ldst_pcrel(InsnType insn) {
    return (insn & 0x3b000000) == 0x18000000;
  }

  static bool ldst_nap(InsnType insn) {
    return (insn & 0x3b800000) == 0x28000000;
  }

  static bool ldstp_pi(InsnType insn) {
    return (insn & 0x3b800000) == 0x28800000;
  }

  static bool ldstp_o(InsnType insn) {
    return (insn & 0x3b800000) == 0x29000000;
  }

  static bool ldstp_pre(InsnType insn) {
    return (insn & 0x3b800000) == 0x29800000;
  }

  static bool ldst_ui(InsnType insn) {
    return (insn & 0x3b200c00) == 0x38000000;
  }

  static bool ldst_piimm(InsnType insn) {
    return (insn & 0x3b200c00) == 0x38000400;
  }

  static bool ldst_u(InsnType insn) {
    return (insn & 0x3b200c00) == 0x38000800;
  }

  static bool ldst_preimm(InsnType insn) {
    return (insn & 0x3b200c00) == 0x38000c00;
  }

  static bool ldst_ro(InsnType insn) {
    return (insn & 0x3b200c00) == 0x38200800;
  }

  static bool ldst_uimm(InsnType insn) {
    return (insn & 0x3b000000) == 0x39000000;
  }

  static bool ldst_simd_m(InsnType insn) {
    return (insn & 0xbfbf0000) == 0x0c000000;
  }

  static bool ldst_simd_m_pi(InsnType insn) {
    return (insn & 0xbfa00000) == 0x0c800000;
  }

  static bool ldst_simd_s(InsnType insn) {
    return (insn & 0xbf9f0000) == 0x0d000000;
  }

  static bool ldst_simd_s_pi(InsnType insn) {
    return (insn & 0xbf800000) == 0x0d800000;
  }

  static uint32_t buildBranchInsn() { return 0x14000000; }

  // Classify an INSN if it is indeed a load/store. Return true if INSN is a
  // LD/ST instruction otherwise return false. For scalar LD/ST instructions
  // PAIR is FALSE, RT is returned and RT2 is set equal to RT. For LD/ST pair
  // instructions PAIR is TRUE, RT and RT2 are returned.
  static bool mem_op_p(InsnType insn, unsigned int *Rt, unsigned int *Rt2,
                       bool *pair, bool *load) {
    uint32_t opcode;
    unsigned int r;
    uint32_t opc = 0;
    uint32_t v = 0;
    uint32_t opc_v = 0;

    /* Bail out quickly if INSN doesn't fall into the the load-store
       encoding space.  */
    if (!ldst(insn))
      return false;

    *pair = false;
    *load = false;
    if (ldst_ex(insn)) {
      *Rt = rt(insn);
      *Rt2 = *Rt;
      if (bit(insn, 21) == 1) {
        *pair = true;
        *Rt2 = rt2(insn);
      }
      *load = ld(insn);
      return true;
    } else if (ldst_nap(insn) || ldstp_pi(insn) || ldstp_o(insn) ||
               ldstp_pre(insn)) {
      *pair = true;
      *Rt = rt(insn);
      *Rt2 = rt2(insn);
      *load = ld(insn);
      return true;
    } else if (ldst_pcrel(insn) || ldst_ui(insn) || ldst_piimm(insn) ||
               ldst_u(insn) || ldst_preimm(insn) || ldst_ro(insn) ||
               ldst_uimm(insn)) {
      *Rt = rt(insn);
      *Rt2 = *Rt;
      if (ldst_pcrel(insn))
        *load = true;
      opc = bits(insn, 22, 2);
      v = bit(insn, 26);
      opc_v = opc | (v << 2);
      *load =
          (opc_v == 1 || opc_v == 2 || opc_v == 3 || opc_v == 5 || opc_v == 7);
      return true;
    } else if (ldst_simd_m(insn) || ldst_simd_m_pi(insn)) {
      *Rt = rt(insn);
      *load = bit(insn, 22);
      opcode = (insn >> 12) & 0xf;
      switch (opcode) {
      case 0:
      case 2:
        *Rt2 = *Rt + 3;
        break;

      case 4:
      case 6:
        *Rt2 = *Rt + 2;
        break;

      case 7:
        *Rt2 = *Rt;
        break;

      case 8:
      case 10:
        *Rt2 = *Rt + 1;
        break;

      default:
        return false;
      }
      return true;
    } else if (ldst_simd_s(insn) || ldst_simd_s_pi(insn)) {
      *Rt = rt(insn);
      r = (insn >> 21) & 1;
      *load = bit(insn, 22);
      opcode = (insn >> 13) & 0x7;
      switch (opcode) {
      case 0:
      case 2:
      case 4:
        *Rt2 = *Rt + r;
        break;

      case 1:
      case 3:
      case 5:
        *Rt2 = *Rt + (r == 0 ? 2 : 3);
        break;

      case 6:
        *Rt2 = *Rt + r;
        break;

      case 7:
        *Rt2 = *Rt + (r == 0 ? 2 : 3);
        break;

      default:
        return false;
      }
      return true;
    }
    return false;
  } // End of "mem_op_p".

  // Return true if INSN is mac insn.
  static bool mac(InsnType insn) { return (insn & 0xff000000) == 0x9b000000; }

  // Return true if INSN is multiply-accumulate.
  // (This is similar to implementaton in elfnn-aarch64.c.)
  static bool mlxl(InsnType insn) {
    uint32_t Op31 = op31(insn);
    if (mac(insn) &&
        (Op31 == 0 || Op31 == 1 || Op31 == 5)
        /* Exclude MUL instructions which are encoded as a multiple-accumulate
           with RA = XZR.  */
        && ra(insn) != AARCH64_ZR) {
      return true;
    }
    return false;
  }
};
} // namespace eld

#endif // AARCH64_AARCH64INSNHELPERS_H
