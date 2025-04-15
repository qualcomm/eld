//===- AArch64RelocationHelpers.h------------------------------------------===//
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

//===- AArch64RelocationHelpers.h -----------------------------------------===//
//===----------------------------------------------------------------------===//
#ifndef TARGET_AARCH64_AARCH64RELOCATIONHELPERS_H
#define TARGET_AARCH64_AARCH64RELOCATIONHELPERS_H

#include "AArch64Relocator.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Support/MsgHandling.h"
#include "eld/Target/ELFFileFormat.h"

namespace eld {
//===----------------------------------------------------------------------===//
// Relocation helper functions
//===----------------------------------------------------------------------===//
// Return true if overflow
static inline bool helper_check_signed_overflow(Relocator::DWord pValue,
                                                unsigned bits) {
  if (bits >= sizeof(int64_t) * 8)
    return false;
  int64_t signed_val = static_cast<int64_t>(pValue);
  int64_t max = (1L << (bits - 1)) - 1;
  int64_t min = -(1L << (bits - 1));
  if (signed_val > max || signed_val < min)
    return true;
  return false;
}

static inline Relocator::Address
helper_get_page_address(Relocator::Address pValue) {
  return (pValue & ~(Relocator::Address)0xFFF);
}

static inline Relocator::Address
helper_get_page_offset(Relocator::Address pValue) {
  return (pValue & (Relocator::Address)0xFFF);
}

static inline uint32_t get_mask(uint32_t pValue) {
  if (pValue == 32)
    return 0xffffffff;
  return (((uint32_t)1U << (pValue)) - 1);
}

static inline uint32_t helper_reencode_adr_imm(uint32_t pInst, uint32_t pImm) {
  return (pInst & ~((get_mask(2) << 29) | (get_mask(19) << 5))) |
         ((pImm & get_mask(2)) << 29) | ((pImm & (get_mask(19) << 2)) << 3);
}

// Re-encode the imm field of add immediate.
static inline uint32_t helper_reencode_add_imm(uint32_t pInst, uint32_t pImm) {
  return (pInst & ~(get_mask(12) << 10)) | ((pImm & get_mask(12)) << 10);
}

// Encode the 26-bit offset of unconditional branch.
static inline uint32_t helper_reencode_branch_offset_26(uint32_t pInst,
                                                        uint32_t pOff) {
  return (pInst & ~get_mask(26)) | (pOff & get_mask(26));
}

// Encode the 19-bit offset of conditional branch and compare & branch.
static inline uint32_t helper_reencode_cond_branch_ofs_19(uint32_t pInst,
                                                          uint32_t pOff) {
  return (pInst & ~(get_mask(19) << 5)) | ((pOff & get_mask(19)) << 5);
}

// Encode the imm field of tbz/tbnz
static inline uint32_t helper_reencode_tbz_imm_14(uint32_t pInst,
                                                  uint32_t pImm) {
  return (pInst & ~(get_mask(14) << 5)) | ((pImm & get_mask(14)) << 5);
}

// Re-encode the imm field of ld/st pos immediate.
static inline uint32_t helper_reencode_ldst_pos_imm(uint32_t pInst,
                                                    uint32_t pImm) {
  return (pInst & ~(get_mask(12) << 10)) | ((pImm & get_mask(12)) << 10);
}

// Encode the literal field ([23:5] of ldr.
static inline uint32_t helper_reencode_ld_literal_19(uint32_t pInst,
                                                     uint32_t pImm) {
  return (pInst & ~(get_mask(19) << 5)) | ((pImm & get_mask(19)) << 5);
}

// Re-encode the imm field of movz/movk
static inline uint32_t helper_reencode_movzk_imm(uint32_t pInst,
                                                 uint32_t pImm) {
  return (pInst & ~(get_mask(16) << 5)) | ((pImm & get_mask(16)) << 5);
}

static inline uint32_t helper_get_upper32(Relocator::DWord pData) {
  return pData >> 32;
}

static inline void helper_put_upper32(uint32_t pData, Relocator::DWord &pDes) {
  *(reinterpret_cast<uint32_t *>(&pDes)) = pData;
}
} // namespace eld
#endif
