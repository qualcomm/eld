//===- DiagRelocations.inc-------------------------------------------------===//
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

DIAG(unsupported_bitclass, DiagnosticEngine::Fatal,
     "Only supports 32 and 64 bits targets. (Target: %0, bitclass:%1)")
DIAG(undefined_reference, DiagnosticEngine::Error,
     "%1(%2+%3): undefined reference to `%0'")
DIAG(undefined_reference_text, DiagnosticEngine::Error,
     "%1:%2:function %3: undefined reference to `%0'")
DIAG(non_pic_relocation, DiagnosticEngine::Fatal,
     "Current link configuration does not support relocation type `%0' for "
     "symbol `%1' referred from %2 , recompile with -fPIC")
DIAG(base_relocation, DiagnosticEngine::Fatal,
     "relocation type `%0' is not supported for symbol `%1'")
DIAG(dynamic_relocation, DiagnosticEngine::Fatal,
     "unexpected relocation type `%0' in object file")
DIAG(unknown_reloc_section_type, DiagnosticEngine::Unreachable,
     "unknown relocation section type: `%0' in section `%1'")
DIAG(unsupport_cond_branch_reloc, DiagnosticEngine::Error,
     "applying relocation `%0', conditional branch to PLT in THUMB-2 not "
     "supported yet")
DIAG(copyrelocs_is_error, DiagnosticEngine::Error,
     "Cannot copy symbol %0 referenced in %1 from dynamic library %1 due to -z "
     "nocopyrelocs ")
DIAG(reloc_trace, DiagnosticEngine::Note,
     "Found relocation %0 for symbol %1 in file %2")
DIAG(tls_non_tls_mix, DiagnosticEngine::Error,
     "Relocation %0 is TLS type but symbol %1 is non-TLS type")
DIAG(result_overflow_moreinfo, DiagnosticEngine::Error,
     "Relocation overflow when applying relocation `%0' for symbol `%1' "
     "referred from %2 symbol defined in %3")
DIAG(result_badreloc_moreinfo, DiagnosticEngine::Error,
     "Relocation error when applying relocation `%0' for symbol `%1' referred "
     "from %2 symbol defined in %3")
DIAG(unknown_relocation_moreinfo, DiagnosticEngine::Fatal,
     "Unknown relocation found during relocation scan `%0' for symbol `%1' "
     "referred from %2 overflows defined in %3")
DIAG(unsupported_relocation_moreinfo, DiagnosticEngine::Fatal,
     "Unsupported relocation found during relocation scan `%0' for symbol `%1' "
     "referred from %2 overflows defined in %3")
DIAG(not_aligned, DiagnosticEngine::Warning,
     "Relocation `%0' for symbol `%1' referred from `%2' and defined in `%3' "
     "has alignment %4 but is not aligned")
DIAG(reloc_truncated, DiagnosticEngine::Warning,
     "Relocation `%0' for symbol `%1' referred from `%2' and defined in `%3' "
     "is truncated to fit relocation field")
DIAG(undef_sym_visibility, DiagnosticEngine::Error,
     "%0: relocation %1 against undefined %3 symbol `%2' cannot be used when "
     "making %4")
DIAG(prohibited_cross_ref, DiagnosticEngine::Error,
     "%1:(%2:%3): prohibited cross reference from %4 to `%0'(%6) in %5")
DIAG(prohibited_cross_ref_warn, DiagnosticEngine::Warning,
     "%1:(%2:%3): prohibited cross reference from %4 to `%0'(%6) in %5")
DIAG(addend_not_supported, DiagnosticEngine::Error,
     "Unable to fetch addend for merging strings for section %0")
DIAG(reading_relocs, DiagnosticEngine::Verbose,
     "Reading relocations from Input %0")
DIAG(unsupported_reloc, DiagnosticEngine::Fatal,
     "Found unsupported reloc type %0 in section %1 in input file %2")
DIAG(resolve_undef_weak_guard, DiagnosticEngine::Warning,
     "Undefined weak symbol %0 referred from %1 is resolved to %2")
DIAG(error_relocations_plugin, DiagnosticEngine::Error, "unable to apply relocations")
