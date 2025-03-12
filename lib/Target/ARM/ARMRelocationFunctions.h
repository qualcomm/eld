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

#define R_ARM_ADD_PREL_20_8 131
#define R_ARM_ADD_PREL_12_8 132
#define R_ARM_LDR_PREL_12 133

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
  DECL_ARM_APPLY_RELOC_FUNC(prel31)                                            \
  DECL_ARM_APPLY_RELOC_FUNC(got_prel)                                          \
  DECL_ARM_APPLY_RELOC_FUNC(tls)                                               \
  DECL_ARM_APPLY_RELOC_FUNC(tls_ldo32)                                         \
  DECL_ARM_APPLY_RELOC_FUNC(tls_le32)                                          \
  DECL_ARM_APPLY_RELOC_FUNC(thm_jump8)                                         \
  DECL_ARM_APPLY_RELOC_FUNC(thm_jump11)                                        \
  DECL_ARM_APPLY_RELOC_FUNC(thm_jump19)                                        \
  DECL_ARM_APPLY_RELOC_FUNC(alu_pc)                                            \
  DECL_ARM_APPLY_RELOC_FUNC(relocAddPREL1)                                     \
  DECL_ARM_APPLY_RELOC_FUNC(relocAddPREL2)                                     \
  DECL_ARM_APPLY_RELOC_FUNC(relocLDR12)                                        \
  DECL_ARM_APPLY_RELOC_FUNC(unsupport)

#define DECL_ARM_APPLY_RELOC_FUNC_PTRS                                         \
  {&none, 0, "R_ARM_NONE"}, {&call, 1, "R_ARM_PC24"},                          \
      {&abs32, 2, "R_ARM_ABS32"}, {&rel32, 3, "R_ARM_REL32"},                  \
      {&unsupport, 4, "R_ARM_LDR_PC_G0"}, {&unsupport, 5, "R_ARM_ABS16"},      \
      {&unsupport, 6, "R_ARM_ABS12"}, {&unsupport, 7, "R_ARM_THM_ABS5"},       \
      {&unsupport, 8, "R_ARM_ABS8"}, {&rel32, 9, "R_ARM_SBREL32"},             \
      {&thm_call, 10, "R_ARM_THM_CALL"}, {&unsupport, 11, "R_ARM_THM_PC8"},    \
      {&unsupport, 12, "R_ARM_BREL_ADJ"}, {&unsupport, 13, "R_ARM_TLS_DESC"},  \
      {&unsupport, 14, "R_ARM_THM_SWI8"}, {&unsupport, 15, "R_ARM_XPC25"},     \
      {&unsupport, 16, "R_ARM_THM_XPC22"},                                     \
      {&unsupport, 17, "R_ARM_TLS_DTPMOD32"},                                  \
      {&unsupport, 18, "R_ARM_TLS_DTPOFF32"},                                  \
      {&unsupport, 19, "R_ARM_TLS_TPOFF32"}, {&unsupport, 20, "R_ARM_COPY"},   \
      {&unsupport, 21, "R_ARM_GLOB_DAT"}, {&unsupport, 22, "R_ARM_JUMP_SLOT"}, \
      {&unsupport, 23, "R_ARM_RELATIVE"}, {&gotoff32, 24, "R_ARM_GOTOFF32"},   \
      {&base_prel, 25, "R_ARM_BASE_PREL"}, {&got_brel, 26, "R_ARM_GOT_BREL"},  \
      {&call, 27, "R_ARM_PLT32"}, {&call, 28, "R_ARM_CALL"},                   \
      {&call, 29, "R_ARM_JUMP24"}, {&thm_call, 30, "R_ARM_THM_JUMP24"},        \
      {&unsupport, 31, "R_ARM_BASE_ABS"},                                      \
      {&unsupport, 32, "R_ARM_ALU_PCREL_7_0"},                                 \
      {&unsupport, 33, "R_ARM_ALU_PCREL_15_8"},                                \
      {&unsupport, 34, "R_ARM_ALU_PCREL_23_15"},                               \
      {&unsupport, 35, "R_ARM_LDR_SBREL_11_0_NC"},                             \
      {&unsupport, 36, "R_ARM_ALU_SBREL_19_12_NC"},                            \
      {&unsupport, 37, "R_ARM_ALU_SBREL_27_20_CK"},                            \
      {&abs32, 38, "R_ARM_TARGET1"}, {&unsupport, 39, "R_ARM_SBREL31"},        \
      {&none, 40, "R_ARM_V4BX"}, {&got_prel, 41, "R_ARM_TARGET2"},             \
      {&prel31, 42, "R_ARM_PREL31"}, {&movw_abs_nc, 43, "R_ARM_MOVW_ABS_NC"},  \
      {&movt_abs, 44, "R_ARM_MOVT_ABS"},                                       \
      {&movw_prel_nc, 45, "R_ARM_MOVW_PREL_NC"},                               \
      {&movt_prel, 46, "R_ARM_MOVT_PREL"},                                     \
      {&thm_movw_abs_nc, 47, "R_ARM_THM_MOVW_ABS_NC"},                         \
      {&thm_movt_abs, 48, "R_ARM_THM_MOVT_ABS"},                               \
      {&thm_movw_prel_nc, 49, "R_ARM_THM_MOVW_PREL_NC"},                       \
      {&thm_movt_prel, 50, "R_ARM_THM_MOVT_PREL"},                             \
      {&thm_jump19, 51, "R_ARM_THM_JUMP19"},                                   \
      {&unsupport, 52, "R_ARM_THM_JUMP6"},                                     \
      {&unsupport, 53, "R_ARM_THM_ALU_PREL_11_0"},                             \
      {&unsupport, 54, "R_ARM_THM_PC12"}, {&unsupport, 55, "R_ARM_ABS32_NOI"}, \
      {&unsupport, 56, "R_ARM_REL32_NOI"},                                     \
      {&unsupport, 57, "R_ARM_ALU_PC_G0_NC"},                                  \
      {&alu_pc, 58, "R_ARM_ALU_PC_G0"},                                        \
      {&unsupport, 59, "R_ARM_ALU_PC_G1_NC"},                                  \
      {&unsupport, 60, "R_ARM_ALU_PC_G1"},                                     \
      {&unsupport, 61, "R_ARM_ALU_PC_G2"},                                     \
      {&unsupport, 62, "R_ARM_LDR_PC_G1"},                                     \
      {&unsupport, 63, "R_ARM_LDR_PC_G2"},                                     \
      {&unsupport, 64, "R_ARM_LDRS_PC_G0"},                                    \
      {&unsupport, 65, "R_ARM_LDRS_PC_G1"},                                    \
      {&unsupport, 66, "R_ARM_LDRS_PC_G2"},                                    \
      {&unsupport, 67, "R_ARM_LDC_PC_G0"},                                     \
      {&unsupport, 68, "R_ARM_LDC_PC_G1"},                                     \
      {&unsupport, 69, "R_ARM_LDC_PC_G2"},                                     \
      {&unsupport, 70, "R_ARM_ALU_SB_G0_NC"},                                  \
      {&unsupport, 71, "R_ARM_ALU_SB_G0"},                                     \
      {&unsupport, 72, "R_ARM_ALU_SB_G1_NC"},                                  \
      {&unsupport, 73, "R_ARM_ALU_SB_G1"},                                     \
      {&unsupport, 74, "R_ARM_ALU_SB_G2"},                                     \
      {&unsupport, 75, "R_ARM_LDR_SB_G0"},                                     \
      {&unsupport, 76, "R_ARM_LDR_SB_G1"},                                     \
      {&unsupport, 77, "R_ARM_LDR_SB_G2"},                                     \
      {&unsupport, 78, "R_ARM_LDRS_SB_G0"},                                    \
      {&unsupport, 79, "R_ARM_LDRS_SB_G1"},                                    \
      {&unsupport, 80, "R_ARM_LDRS_SB_G2"},                                    \
      {&unsupport, 81, "R_ARM_LDC_SB_G0"},                                     \
      {&unsupport, 82, "R_ARM_LDC_SB_G1"},                                     \
      {&unsupport, 83, "R_ARM_LDC_SB_G2"},                                     \
      {&unsupport, 84, "R_ARM_MOVW_BREL_NC"},                                  \
      {&unsupport, 85, "R_ARM_MOVT_BREL"},                                     \
      {&unsupport, 86, "R_ARM_MOVW_BREL"},                                     \
      {&thm_movw_brel, 87, "R_ARM_THM_MOVW_BREL_NC"},                          \
      {&thm_movt_prel, 88, "R_ARM_THM_MOVT_BREL"},                             \
      {&thm_movw_brel, 89, "R_ARM_THM_MOVW_BREL"},                             \
      {&unsupport, 90, "R_ARM_TLS_GOTDESC"},                                   \
      {&unsupport, 91, "R_ARM_TLS_CALL"},                                      \
      {&unsupport, 92, "R_ARM_TLS_DESCSEQ"},                                   \
      {&unsupport, 93, "R_ARM_THM_TLS_CALL"},                                  \
      {&unsupport, 94, "R_ARM_PLT32_ABS"}, {&unsupport, 95, "R_ARM_GOT_ABS"},  \
      {&got_prel, 96, "R_ARM_GOT_PREL"}, {&unsupport, 97, "R_ARM_GOT_PREL12"}, \
      {&unsupport, 98, "R_ARM_GOTOFF12"}, {&unsupport, 99, "R_ARM_GOTRELAX"},  \
      {&unsupport, 100, "R_ARM_GNU_VTENTRY"},                                  \
      {&unsupport, 101, "R_ARM_GNU_VTINERIT"},                                 \
      {&thm_jump11, 102, "R_ARM_THM_JUMP11"},                                  \
      {&thm_jump8, 103, "R_ARM_THM_JUMP8"}, {&tls, 104, "R_ARM_TLS_GD32"},     \
      {&tls, 105, "R_ARM_TLS_LDM32"}, {&tls_ldo32, 106, "R_ARM_TLS_LDO32"},    \
      {&tls, 107, "R_ARM_TLS_IE32"}, {&tls_le32, 108, "R_ARM_TLS_LE32"},       \
      {&unsupport, 109, "R_ARM_TLS_LDO12"},                                    \
      {&unsupport, 110, "R_ARM_TLS_LE12"},                                     \
      {&unsupport, 111, "R_ARM_TLS_IE12GP"},                                   \
      {&unsupport, 112, "R_ARM_PRIVATE_0"},                                    \
      {&unsupport, 113, "R_ARM_PRIVATE_1"},                                    \
      {&unsupport, 114, "R_ARM_PRIVATE_2"},                                    \
      {&unsupport, 115, "R_ARM_PRIVATE_3"},                                    \
      {&unsupport, 116, "R_ARM_PRIVATE_4"},                                    \
      {&unsupport, 117, "R_ARM_PRIVATE_5"},                                    \
      {&unsupport, 118, "R_ARM_PRIVATE_6"},                                    \
      {&unsupport, 119, "R_ARM_PRIVATE_7"},                                    \
      {&unsupport, 120, "R_ARM_PRIVATE_8"},                                    \
      {&unsupport, 121, "R_ARM_PRIVATE_9"},                                    \
      {&unsupport, 122, "R_ARM_PRIVATE_10"},                                   \
      {&unsupport, 123, "R_ARM_PRIVATE_11"},                                   \
      {&unsupport, 124, "R_ARM_PRIVATE_12"},                                   \
      {&unsupport, 125, "R_ARM_PRIVATE_13"},                                   \
      {&unsupport, 126, "R_ARM_PRIVATE_14"},                                   \
      {&unsupport, 127, "R_ARM_PRIVATE_15"},                                   \
      {&unsupport, 128, "R_ARM_ME_TOO"},                                       \
      {&unsupport, 129, "R_ARM_THM_TLS_DESCSEQ16"},                            \
      {&unsupport, 130, "R_ARM_THM_TLS_DESCSEQ32"},                            \
      {&relocAddPREL1, 131, "R_ARM_ADD_PREL_20_8"},                            \
      {&relocAddPREL2, 132, "R_ARM_ADD_PREL_12_8"},                            \
      {&relocLDR12, 133, "R_ARM_LDR_PREL_12"}

#define ARM_MAXRELOCS (R_ARM_LDR_PREL_12 + 1)

#endif
