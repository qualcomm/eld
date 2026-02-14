//===- AArch64RelocationFunctions.h----------------------------------------===//
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

#define DECL_AARCH64_APPLY_RELOC_FUNC(Name)                                    \
  static AArch64Relocator::Result Name(Relocation &pEntry,                     \
                                       AArch64Relocator &pParent);

#define DECL_AARCH64_APPLY_RELOC_FUNCS                                         \
  DECL_AARCH64_APPLY_RELOC_FUNC(none)                                          \
  DECL_AARCH64_APPLY_RELOC_FUNC(abs)                                           \
  DECL_AARCH64_APPLY_RELOC_FUNC(rel)                                           \
  DECL_AARCH64_APPLY_RELOC_FUNC(call)                                          \
  DECL_AARCH64_APPLY_RELOC_FUNC(condbr)                                        \
  DECL_AARCH64_APPLY_RELOC_FUNC(ld_prel_lo19)                                  \
  DECL_AARCH64_APPLY_RELOC_FUNC(adr_prel_lo21)                                 \
  DECL_AARCH64_APPLY_RELOC_FUNC(adr_prel_pg_hi21)                              \
  DECL_AARCH64_APPLY_RELOC_FUNC(add_abs_lo12)                                  \
  DECL_AARCH64_APPLY_RELOC_FUNC(adr_got_page)                                  \
  DECL_AARCH64_APPLY_RELOC_FUNC(ld64_got_lo12)                                 \
  DECL_AARCH64_APPLY_RELOC_FUNC(ld64_gotpage_lo15)                             \
  DECL_AARCH64_APPLY_RELOC_FUNC(ldst_abs_lo12)                                 \
  DECL_AARCH64_APPLY_RELOC_FUNC(movw_abs_g)                                    \
  DECL_AARCH64_APPLY_RELOC_FUNC(tls_gottprel_page)                             \
  DECL_AARCH64_APPLY_RELOC_FUNC(tls_gottprel_lo)                               \
  DECL_AARCH64_APPLY_RELOC_FUNC(tls_tprel)                                     \
  DECL_AARCH64_APPLY_RELOC_FUNC(tls_tlsdesc_page)                              \
  DECL_AARCH64_APPLY_RELOC_FUNC(tls_tlsdesc_lo)                                \
  DECL_AARCH64_APPLY_RELOC_FUNC(tls_tlsdesc_add)                               \
  DECL_AARCH64_APPLY_RELOC_FUNC(tls_call)                                      \
  DECL_AARCH64_APPLY_RELOC_FUNC(copyInstruction)                               \
  DECL_AARCH64_APPLY_RELOC_FUNC(unsupport)

#define DECL_AARCH64_APPLY_RELOC_FUNC_PTRS(ValueType, MappedType)              \
  ValueType(0x0, MappedType(&none, "R_AARCH64_nullptr")),                      \
      ValueType(0x100, MappedType(&none, "R_AARCH64_NONE")),                   \
      ValueType(0x101, MappedType(&abs, "R_AARCH64_ABS64", 64)),               \
      ValueType(0x102, MappedType(&abs, "R_AARCH64_ABS32", 32)),               \
      ValueType(0x103, MappedType(&abs, "R_AARCH64_ABS16", 16)),               \
      ValueType(0x104, MappedType(&rel, "R_AARCH64_PREL64", 64)),              \
      ValueType(0x105, MappedType(&rel, "R_AARCH64_PREL32", 32)),              \
      ValueType(0x106, MappedType(&rel, "R_AARCH64_PREL16", 16)),              \
      ValueType(0x107, MappedType(&movw_abs_g, "R_AARCH64_MOVW_UABS_G0", 32)), \
      ValueType(0x108,                                                         \
                MappedType(&movw_abs_g, "R_AARCH64_MOVW_UABS_G0_NC", 32)),     \
      ValueType(0x109, MappedType(&movw_abs_g, "R_AARCH64_MOVW_UABS_G1", 32)), \
      ValueType(0x10a,                                                         \
                MappedType(&movw_abs_g, "R_AARCH64_MOVW_UABS_G1_NC", 32)),     \
      ValueType(0x10b, MappedType(&movw_abs_g, "R_AARCH64_MOVW_UABS_G2", 32)), \
      ValueType(0x10c,                                                         \
                MappedType(&movw_abs_g, "R_AARCH64_MOVW_UABS_G2_NC", 32)),     \
      ValueType(0x10d, MappedType(&movw_abs_g, "R_AARCH64_MOVW_UABS_G3", 32)), \
      ValueType(0x10e, MappedType(&movw_abs_g, "R_AARCH64_MOVW_SABS_G0", 32)), \
      ValueType(0x10f, MappedType(&movw_abs_g, "R_AARCH64_MOVW_SABS_G1", 32)), \
      ValueType(0x110, MappedType(&movw_abs_g, "R_AARCH64_MOVW_SABS_G2", 32)), \
      ValueType(0x111,                                                         \
                MappedType(&ld_prel_lo19, "R_AARCH64_LD_PREL_LO19", 32)),      \
      ValueType(0x112,                                                         \
                MappedType(&adr_prel_lo21, "R_AARCH64_ADR_PREL_LO21", 32)),    \
      ValueType(0x113, MappedType(&adr_prel_pg_hi21,                           \
                                  "R_AARCH64_ADR_PREL_PG_HI21", 32)),          \
      ValueType(0x114, MappedType(&adr_prel_pg_hi21,                           \
                                  "R_AARCH64_ADR_PREL_PG_HI21_NC", 32)),       \
      ValueType(0x115,                                                         \
                MappedType(&add_abs_lo12, "R_AARCH64_ADD_ABS_LO12_NC", 32)),   \
      ValueType(0x116, MappedType(&ldst_abs_lo12,                              \
                                  "R_AARCH64_LDST8_ABS_LO12_NC", 32)),         \
      ValueType(0x117, MappedType(&condbr, "R_AARCH64_TSTBR14", 32)),          \
      ValueType(0x118, MappedType(&condbr, "R_AARCH64_CONDBR19", 32)),         \
      ValueType(0x11a, MappedType(&call, "R_AARCH64_JUMP26", 32)),             \
      ValueType(0x11b, MappedType(&call, "R_AARCH64_CALL26", 32)),             \
      ValueType(0x11c, MappedType(&ldst_abs_lo12,                              \
                                  "R_AARCH64_LDST16_ABS_LO12_NC", 32)),        \
      ValueType(0x11d, MappedType(&ldst_abs_lo12,                              \
                                  "R_AARCH64_LDST32_ABS_LO12_NC", 32)),        \
      ValueType(0x11e, MappedType(&ldst_abs_lo12,                              \
                                  "R_AARCH64_LDST64_ABS_LO12_NC", 32)),        \
      ValueType(0x11f,                                                         \
                MappedType(&unsupport, "R_AARCH64_MOVW_PREL_G0", 32)),         \
      ValueType(0x120,                                                         \
                MappedType(&unsupport, "R_AARCH64_MOVW_PREL_G0_NC", 32)),      \
      ValueType(0x121,                                                         \
                MappedType(&unsupport, "R_AARCH64_MOVW_PREL_G1", 32)),         \
      ValueType(0x122,                                                         \
                MappedType(&unsupport, "R_AARCH64_MOVW_PREL_G1_NC", 32)),      \
      ValueType(0x123,                                                         \
                MappedType(&unsupport, "R_AARCH64_MOVW_PREL_G2", 32)),         \
      ValueType(0x124,                                                         \
                MappedType(&unsupport, "R_AARCH64_MOVW_PREL_G2_NC", 32)),      \
      ValueType(0x125,                                                         \
                MappedType(&unsupport, "R_AARCH64_MOVW_PREL_G3", 32)),         \
      ValueType(0x12b, MappedType(&ldst_abs_lo12,                              \
                                  "R_AARCH64_LDST128_ABS_LO12_NC", 32)),       \
      ValueType(0x12c,                                                         \
                MappedType(&unsupport, "R_AARCH64_MOVW_GOTOFF_G0", 32)),       \
      ValueType(0x12d,                                                         \
                MappedType(&unsupport, "R_AARCH64_MOVW_GOTOFF_G0_NC", 32)),    \
      ValueType(0x12e,                                                         \
                MappedType(&unsupport, "R_AARCH64_MOVW_GOTOFF_G1", 32)),       \
      ValueType(0x12f,                                                         \
                MappedType(&unsupport, "R_AARCH64_MOVW_GOTOFF_G1_NC", 32)),    \
      ValueType(0x130,                                                         \
                MappedType(&unsupport, "R_AARCH64_MOVW_GOTOFF_G2", 32)),       \
      ValueType(0x131,                                                         \
                MappedType(&unsupport, "R_AARCH64_MOVW_GOTOFF_G2_NC", 32)),    \
      ValueType(0x132,                                                         \
                MappedType(&unsupport, "R_AARCH64_MOVW_GOTOFF_G3", 32)),       \
      ValueType(0x133,                                                         \
                MappedType(&unsupport, "R_AARCH64_GOTREL64", 64)),             \
      ValueType(0x134,                                                         \
                MappedType(&unsupport, "R_AARCH64_GOTREL32", 32)),             \
      ValueType(0x135,                                                         \
                MappedType(&unsupport, "R_AARCH64_GOT_LD_PREL19", 32)),        \
      ValueType(0x136,                                                         \
                MappedType(&unsupport, "R_AARCH64_LD64_GOTOFF_LO15", 32)),     \
      ValueType(0x137,                                                         \
                MappedType(&adr_got_page, "R_AARCH64_ADR_GOT_PAGE", 32)),      \
      ValueType(0x138,                                                         \
                MappedType(&ld64_got_lo12, "R_AARCH64_LD64_GOT_LO12_NC", 32)), \
      ValueType(0x139,                                                         \
                MappedType(&ld64_gotpage_lo15,                                 \
			   "R_AARCH64_LD64_GOTPAGE_LO15", 32)),                \
      ValueType(0x13a,                                                         \
                MappedType(&unsupport, "R_AARCH64_PLT32", 32)),                \
      ValueType(0x13b,                                                         \
                MappedType(&unsupport, "R_AARCH64_GOTPCREL32", 32)),           \
      ValueType(0x13c,                                                         \
                MappedType(&unsupport, "R_AARCH64_PATCHINST", 32)),            \
      ValueType(0x13d,                                                         \
                MappedType(&unsupport, "R_AARCH64_FUNCINIT64", 64)),           \
      ValueType(0x200,                                                         \
                MappedType(&unsupport, "R_AARCH64_TLSGD_ADR_PREL21", 32)),     \
      ValueType(0x201,                                                         \
                MappedType(&unsupport, "R_AARCH64_TLSGD_ADR_PAGE21", 32)),     \
      ValueType(0x202,                                                         \
                MappedType(&unsupport, "R_AARCH64_TLSGD_ADD_LO12_NC", 32)),    \
      ValueType(0x203,                                                         \
                MappedType(&unsupport, "R_AARCH64_TLSGD_MOVW_G0_NC", 32)),     \
      ValueType(0x204,                                                         \
                MappedType(&unsupport, "R_AARCH64_TLSGD_MOVW_G1", 32)),        \
      ValueType(0x205,                                                         \
                MappedType(&unsupport, "R_AARCH64_TLSLD_ADR_PREL21")),         \
      ValueType(0x206,                                                         \
                MappedType(&unsupport, "R_AARCH64_TLSLD_ADR_PAGE21")),         \
      ValueType(0x207,                                                         \
                MappedType(&unsupport, "R_AARCH64_TLSLD_ADD_LO12_NC")),        \
      ValueType(0x208,                                                         \
                MappedType(&unsupport, "R_AARCH64_TLSLD_MOVW_G1")),            \
      ValueType(0x209,                                                         \
                MappedType(&unsupport, "R_AARCH64_TLSLD_MOVW_G0_NC")),         \
      ValueType(0x20a,                                                         \
                MappedType(&unsupport, "R_AARCH64_TLSLD_LD_PREL19")),          \
      ValueType(0x20b,                                                         \
                MappedType(&unsupport, "R_AARCH64_TLSLD_MOVW_DTPREL_G2")),     \
      ValueType(0x20c,                                                         \
                MappedType(&unsupport, "R_AARCH64_TLSLD_MOVW_DTPREL_G1")),     \
      ValueType(0x20d,                                                         \
                MappedType(&unsupport, "R_AARCH64_TLSLD_MOVW_DTPREL_G1_NC")),  \
      ValueType(0x20e,                                                         \
                MappedType(&unsupport, "R_AARCH64_TLSLD_MOVW_DTPREL_G0")),     \
      ValueType(0x20f,                                                         \
                MappedType(&unsupport, "R_AARCH64_TLSLD_MOVW_DTPREL_G0_NC")),  \
      ValueType(0x210,                                                         \
                MappedType(&unsupport, "R_AARCH64_TLSLD_ADD_DTPREL_HI12")),    \
      ValueType(0x211,                                                         \
                MappedType(&unsupport, "R_AARCH64_TLSLD_ADD_DTPREL_LO12")),    \
      ValueType(0x212,                                                         \
                MappedType(&unsupport, "R_AARCH64_TLSLD_ADD_DTPREL_LO12_NC")), \
      ValueType(0x213,                                                         \
                MappedType(&unsupport, "R_AARCH64_TLSLD_LDST8_DTPREL_LO12")),  \
      ValueType(0x214, MappedType(&unsupport,                                  \
                                  "R_AARCH64_TLSLD_LDST8_DTPREL_LO12_NC")),    \
      ValueType(0x215,                                                         \
                MappedType(&unsupport, "R_AARCH64_TLSLD_LDST16_DTPREL_LO12")), \
      ValueType(0x216, MappedType(&unsupport,                                  \
                                  "R_AARCH64_TLSLD_LDST16_DTPREL_LO12_NC")),   \
      ValueType(0x217,                                                         \
                MappedType(&unsupport, "R_AARCH64_TLSLD_LDST32_DTPREL_LO12")), \
      ValueType(0x218, MappedType(&unsupport,                                  \
                                  "R_AARCH64_TLSLD_LDST32_DTPREL_LO12_NC")),   \
      ValueType(0x219,                                                         \
                MappedType(&unsupport, "R_AARCH64_TLSLD_LDST64_DTPREL_LO12")), \
      ValueType(0x21a, MappedType(&unsupport,                                  \
                                  "R_AARCH64_TLSLD_LDST64_DTPREL_LO12_NC")),   \
      ValueType(0x21b,                                                         \
                MappedType(&unsupport, "R_AARCH64_TLSIE_MOVW_GOTTPREL_G1")),   \
      ValueType(0x21c, MappedType(&unsupport,                                  \
                                  "R_AARCH64_TLSIE_MOVW_GOTTPREL_G0_NC")),     \
      ValueType(0x21d, MappedType(&tls_gottprel_page,                          \
                                  "R_AARCH64_TLSIE_ADR_GOTTPREL_PAGE21", 32)), \
      ValueType(0x21e,                                                         \
                MappedType(&tls_gottprel_lo,                                   \
                           "R_AARCH64_TLSIE_LD64_GOTTPREL_LO12_NC", 32)),      \
      ValueType(0x21f,                                                         \
                MappedType(&unsupport, "R_AARCH64_TLSIE_LD_GOTTPREL_PREL19")), \
      ValueType(0x220,                                                         \
                MappedType(&unsupport, "R_AARCH64_TLSLE_MOVW_TPREL_G2")),      \
      ValueType(0x221,                                                         \
                MappedType(&unsupport, "R_AARCH64_TLSLE_MOVW_TPREL_G1")),      \
      ValueType(0x222,                                                         \
                MappedType(&unsupport, "R_AARCH64_TLSLE_MOVW_TPREL_G1_NC")),   \
      ValueType(0x223,                                                         \
                MappedType(&unsupport, "R_AARCH64_TLSLE_MOVW_TPREL_G0")),      \
      ValueType(0x224,                                                         \
                MappedType(&unsupport, "R_AARCH64_TLSLE_MOVW_TPREL_G0_NC")),   \
      ValueType(0x225,                                                         \
                MappedType(&tls_tprel, "R_AARCH64_TLSLE_ADD_TPREL_HI12", 32)), \
      ValueType(0x226,                                                         \
                MappedType(&tls_tprel, "R_AARCH64_TLSLE_ADD_TPREL_LO12", 32)), \
      ValueType(0x227, MappedType(&tls_tprel,                                  \
                                  "R_AARCH64_TLSLE_ADD_TPREL_LO12_NC", 32)),   \
      ValueType(0x228,                                                         \
                MappedType(&unsupport, "R_AARCH64_TLSLE_LDST8_TPREL_LO12")),   \
      ValueType(0x229, MappedType(&unsupport,                                  \
                                  "R_AARCH64_TLSLE_LDST8_TPREL_LO12_NC")),     \
      ValueType(0x22a,                                                         \
                MappedType(&unsupport, "R_AARCH64_TLSLE_LDST16_TPREL_LO12")),  \
      ValueType(0x22b, MappedType(&unsupport,                                  \
                                  "R_AARCH64_TLSLE_LDST16_TPREL_LO12_NC")),    \
      ValueType(0x22c,                                                         \
                MappedType(&unsupport, "R_AARCH64_TLSLE_LDST32_TPREL_LO12")),  \
      ValueType(0x22d, MappedType(&unsupport,                                  \
                                  "R_AARCH64_TLSLE_LDST32_TPREL_LO12_NC")),    \
      ValueType(0x22e,                                                         \
                MappedType(&unsupport, "R_AARCH64_TLSLE_LDST64_TPREL_LO12")),  \
      ValueType(0x22f, MappedType(&unsupport,                                  \
                                  "R_AARCH64_TLSLE_LDST64_TPREL_LO12_NC")),    \
      ValueType(0x230, MappedType(&unsupport,                                  \
                                  "R_AARCH64_TLSDESC_LD_PREL19", 32)),         \
      ValueType(0x231, MappedType(&unsupport,                                  \
                                  "R_AARCH64_TLSDESC_ADR_PREL21", 32)),        \
      ValueType(0x232, MappedType(&tls_tlsdesc_page,                           \
                                  "R_AARCH64_TLSDESC_ADR_PAGE21", 32)),        \
      ValueType(0x233, MappedType(&tls_tlsdesc_lo,                             \
                                  "R_AARCH64_TLSDESC_LD64_LO12", 32)),         \
      ValueType(0x234, MappedType(&tls_tlsdesc_add,                            \
                                  "R_AARCH64_TLSDESC_ADD_LO12", 32)),          \
      ValueType(0x235, MappedType(&unsupport,                                  \
                                  "R_AARCH64_TLSDESC_OFF_G1", 32)),            \
      ValueType(0x236, MappedType(&unsupport,                                  \
                                  "R_AARCH64_TLSDESC_OFF_G0_NC", 32)),         \
      ValueType(0x237, MappedType(&unsupport,                                  \
                                  "R_AARCH64_TLSDESC_LDR", 32)),               \
      ValueType(0x238, MappedType(&unsupport,                                  \
                                  "R_AARCH64_TLSDESC_ADD", 32)),               \
      ValueType(0x239, MappedType(&tls_call, "R_AARCH64_TLSDESC_CALL", 32)),   \
      ValueType(0x23a, MappedType(&unsupport,                                  \
                                  "R_AARCH64_TLSLE_LDST128_TPREL_LO12", 32)),  \
      ValueType(0x23b,                                                         \
                MappedType(&unsupport,                                         \
                           "R_AARCH64_TLSLE_LDST128_TPREL_LO12_NC", 32)),      \
      ValueType(0x23c, MappedType(&unsupport,                                  \
                                  "R_AARCH64_TLSLE_LDST128_DTPREL_LO12", 32)), \
      ValueType(0x23d,                                                         \
                MappedType(&unsupport,                                         \
                           "R_AARCH64_TLSLE_LDST128_DTPREL_LO12_NC", 32)),     \
      ValueType(1024, MappedType(&unsupport, "R_AARCH64_COPY")),               \
      ValueType(1025, MappedType(&unsupport, "R_AARCH64_GLOB_DAT")),           \
      ValueType(1026, MappedType(&unsupport, "R_AARCH64_JUMP_SLOT")),          \
      ValueType(1027, MappedType(&unsupport, "R_AARCH64_RELATIVE")),           \
      ValueType(1028, MappedType(&unsupport, "R_AARCH64_TLS_DTPREL64")),       \
      ValueType(1029, MappedType(&unsupport, "R_AARCH64_TLS_DTPMOD64")),       \
      ValueType(1030, MappedType(&unsupport, "R_AARCH64_TLS_TPREL64")),        \
      ValueType(1031, MappedType(&unsupport, "R_AARCH64_TLSDESC")),            \
      ValueType(1032, MappedType(&unsupport, "R_AARCH64_IRELATIVE")),          \
      ValueType(1033, MappedType(&copyInstruction, "R_AARCH64_COPY_INSN", 32))

#define AARCH64_MAXRELOCS (R_AARCH64_COPY_INSN + 1)
