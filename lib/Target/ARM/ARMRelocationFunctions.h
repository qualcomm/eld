//===- ARMRelocationFunctions.h--------------------------------------------===//
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

#ifndef ARM_RELOCATION_FUNCTIONS_H
#define ARM_RELOCATION_FUNCTIONS_H

// These relocations are used internally when constructing ARM PLT entries.
// They are kept outside the public relocation IDs defined by LLVM.
#define R_ARM_ADD_PREL_20_8 0x83
#define R_ARM_ADD_PREL_12_8 0x8b
#define R_ARM_LDR_PREL_12 0x8c

#define DECL_ARM_APPLY_RELOC_FUNC(Name)                                        \
  static ARMRelocator::Result Name(Relocation &pEntry, ARMRelocator &pParent);

#define DECL_ARM_APPLY_RELOC_FUNCS                                             \
  DECL_ARM_APPLY_RELOC_FUNC(none)                                              \
  DECL_ARM_APPLY_RELOC_FUNC(abs32)                                             \
  DECL_ARM_APPLY_RELOC_FUNC(rel32)                                             \
  DECL_ARM_APPLY_RELOC_FUNC(gotoff32)                                          \
  DECL_ARM_APPLY_RELOC_FUNC(base_prel)                                         \
  DECL_ARM_APPLY_RELOC_FUNC(got_brel)                                          \
  DECL_ARM_APPLY_RELOC_FUNC(call)                                              \
  DECL_ARM_APPLY_RELOC_FUNC(thm_call)                                          \
  DECL_ARM_APPLY_RELOC_FUNC(movw_prel_nc)                                      \
  DECL_ARM_APPLY_RELOC_FUNC(movw_abs_nc)                                       \
  DECL_ARM_APPLY_RELOC_FUNC(movt_abs)                                          \
  DECL_ARM_APPLY_RELOC_FUNC(movt_prel)                                         \
  DECL_ARM_APPLY_RELOC_FUNC(thm_movw_abs_nc)                                   \
  DECL_ARM_APPLY_RELOC_FUNC(thm_movw_prel_nc)                                  \
  DECL_ARM_APPLY_RELOC_FUNC(thm_movw_brel)                                     \
  DECL_ARM_APPLY_RELOC_FUNC(thm_movt_abs)                                      \
  DECL_ARM_APPLY_RELOC_FUNC(thm_movt_prel)                                     \
  DECL_ARM_APPLY_RELOC_FUNC(target2)                                           \
  DECL_ARM_APPLY_RELOC_FUNC(prel31)                                            \
  DECL_ARM_APPLY_RELOC_FUNC(got_prel)                                          \
  DECL_ARM_APPLY_RELOC_FUNC(tls)                                               \
  DECL_ARM_APPLY_RELOC_FUNC(tls_ldo32)                                         \
  DECL_ARM_APPLY_RELOC_FUNC(tls_le32)                                          \
  DECL_ARM_APPLY_RELOC_FUNC(thm_jump8)                                         \
  DECL_ARM_APPLY_RELOC_FUNC(thm_jump11)                                        \
  DECL_ARM_APPLY_RELOC_FUNC(thm_jump19)                                        \
  DECL_ARM_APPLY_RELOC_FUNC(alu_pc)                                            \
  DECL_ARM_APPLY_RELOC_FUNC(ldr_pc_g2)                                         \
  DECL_ARM_APPLY_RELOC_FUNC(ldr_pc_g0)                                         \
  DECL_ARM_APPLY_RELOC_FUNC(relocAddPREL1)                                     \
  DECL_ARM_APPLY_RELOC_FUNC(relocAddPREL2)                                     \
  DECL_ARM_APPLY_RELOC_FUNC(relocLDR12)                                        \
  DECL_ARM_APPLY_RELOC_FUNC(unsupport)

// Handler overrides for relocations whose implementation is available in
// ELD. All other LLVM-defined ARM relocations default to unsupport().
// clang-format off
#define DECL_ARM_APPLY_RELOC_FUNC_OVERRIDES(Func)                              \
  Func(llvm::ELF::R_ARM_NONE, none, "R_ARM_NONE")                              \
  Func(llvm::ELF::R_ARM_PC24, call, "R_ARM_PC24")                              \
  Func(llvm::ELF::R_ARM_ABS32, abs32, "R_ARM_ABS32")                           \
  Func(llvm::ELF::R_ARM_REL32, rel32, "R_ARM_REL32")                           \
  Func(llvm::ELF::R_ARM_SBREL32, rel32, "R_ARM_SBREL32")                       \
  Func(llvm::ELF::R_ARM_THM_CALL, thm_call, "R_ARM_THM_CALL")                  \
  Func(llvm::ELF::R_ARM_GOTOFF32, gotoff32, "R_ARM_GOTOFF32")                  \
  Func(llvm::ELF::R_ARM_BASE_PREL, base_prel, "R_ARM_BASE_PREL")               \
  Func(llvm::ELF::R_ARM_GOT_BREL, got_brel, "R_ARM_GOT_BREL")                  \
  Func(llvm::ELF::R_ARM_PLT32, call, "R_ARM_PLT32")                            \
  Func(llvm::ELF::R_ARM_CALL, call, "R_ARM_CALL")                              \
  Func(llvm::ELF::R_ARM_JUMP24, call, "R_ARM_JUMP24")                          \
  Func(llvm::ELF::R_ARM_THM_JUMP24, thm_call, "R_ARM_THM_JUMP24")              \
  Func(llvm::ELF::R_ARM_TARGET1, abs32, "R_ARM_TARGET1")                       \
  Func(llvm::ELF::R_ARM_V4BX, none, "R_ARM_V4BX")                              \
  Func(llvm::ELF::R_ARM_TARGET2, target2, "R_ARM_TARGET2")                     \
  Func(llvm::ELF::R_ARM_PREL31, prel31, "R_ARM_PREL31")                        \
  Func(llvm::ELF::R_ARM_MOVW_ABS_NC, movw_abs_nc, "R_ARM_MOVW_ABS_NC")         \
  Func(llvm::ELF::R_ARM_MOVT_ABS, movt_abs, "R_ARM_MOVT_ABS")                  \
  Func(llvm::ELF::R_ARM_MOVW_PREL_NC, movw_prel_nc, "R_ARM_MOVW_PREL_NC")      \
  Func(llvm::ELF::R_ARM_MOVT_PREL, movt_prel, "R_ARM_MOVT_PREL")               \
  Func(llvm::ELF::R_ARM_THM_MOVW_ABS_NC, thm_movw_abs_nc,                      \
       "R_ARM_THM_MOVW_ABS_NC")                                                \
  Func(llvm::ELF::R_ARM_THM_MOVT_ABS, thm_movt_abs, "R_ARM_THM_MOVT_ABS")      \
  Func(llvm::ELF::R_ARM_THM_MOVW_PREL_NC, thm_movw_prel_nc,                    \
       "R_ARM_THM_MOVW_PREL_NC")                                               \
  Func(llvm::ELF::R_ARM_THM_MOVT_PREL, thm_movt_prel, "R_ARM_THM_MOVT_PREL")   \
  Func(llvm::ELF::R_ARM_THM_JUMP19, thm_jump19, "R_ARM_THM_JUMP19")            \
  Func(llvm::ELF::R_ARM_ALU_PC_G0, alu_pc, "R_ARM_ALU_PC_G0")                  \
  Func(llvm::ELF::R_ARM_LDR_PC_G2, ldr_pc_g2, "R_ARM_LDR_PC_G2")               \
  Func(llvm::ELF::R_ARM_LDR_PC_G0, ldr_pc_g0, "R_ARM_LDR_PC_G0")               \
  Func(llvm::ELF::R_ARM_THM_MOVW_BREL_NC, thm_movw_brel,                       \
       "R_ARM_THM_MOVW_BREL_NC")                                               \
  Func(llvm::ELF::R_ARM_THM_MOVT_BREL, thm_movt_prel, "R_ARM_THM_MOVT_BREL")   \
  Func(llvm::ELF::R_ARM_THM_MOVW_BREL, thm_movw_brel, "R_ARM_THM_MOVW_BREL")   \
  Func(llvm::ELF::R_ARM_GOT_PREL, got_prel, "R_ARM_GOT_PREL")                  \
  Func(llvm::ELF::R_ARM_THM_JUMP11, thm_jump11, "R_ARM_THM_JUMP11")            \
  Func(llvm::ELF::R_ARM_THM_JUMP8, thm_jump8, "R_ARM_THM_JUMP8")               \
  Func(llvm::ELF::R_ARM_TLS_GD32, tls, "R_ARM_TLS_GD32")                       \
  Func(llvm::ELF::R_ARM_TLS_LDM32, tls, "R_ARM_TLS_LDM32")                     \
  Func(llvm::ELF::R_ARM_TLS_LDO32, tls_ldo32, "R_ARM_TLS_LDO32")               \
  Func(llvm::ELF::R_ARM_TLS_IE32, tls, "R_ARM_TLS_IE32")                       \
  Func(llvm::ELF::R_ARM_TLS_LE32, tls_le32, "R_ARM_TLS_LE32")                  \
  Func(R_ARM_ADD_PREL_20_8, relocAddPREL1, "R_ARM_ADD_PREL_20_8")              \
  Func(R_ARM_ADD_PREL_12_8, relocAddPREL2, "R_ARM_ADD_PREL_12_8")              \
  Func(R_ARM_LDR_PREL_12, relocLDR12, "R_ARM_LDR_PREL_12")
// clang-format on

#endif
