//===- DiagAttribute.inc---------------------------------------------------===//
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

// General

// ARM attributes
DIAG(warn_mismatch_cpu_arch_profile, DiagnosticEngine::Warning,
     "conflicting architecture profiles %0 in %1.")
DIAG(error_mismatch_mpextension_use, DiagnosticEngine::Error,
     "conflicting values from Tag_MPextension_use and "
     "Tag_MPextension_use_legacy in %0")
DIAG(warn_mismatch_enum_size, DiagnosticEngine::Warning,
     "the size of enumerated data item in input %0 (value=%1) is not "
     "compatible with the output (value=%2).")
DIAG(warn_mismatch_fp16_format, DiagnosticEngine::Warning,
     "conflicting 16-bit FP number format in %0")
DIAG(warn_unrecognized_virtualization_use, DiagnosticEngine::Warning,
     "value of Tag_Virtualization_use cannot be recognized in %0 (value=%1).")
DIAG(warn_mismatch_abi_wmmx_args, DiagnosticEngine::Warning,
     "%0 uses different way to pass WMMX parameters and results.")
DIAG(warn_mismatch_pcs_config, DiagnosticEngine::Warning,
     "conflicting procedure call standard config in input %0.")
DIAG(err_mismatch_r9_use, DiagnosticEngine::Error,
     "conflicting way to use R9. %2: `%1', previously seen: `%0'")
DIAG(warn_mismatch_wchar_size, DiagnosticEngine::Warning,
     "incompatible size of wchar_t in input %0 (value=%1) with the output "
     "(value=%2).")
DIAG(warn_unknown_mandatory_attribute, DiagnosticEngine::Warning,
     "unknown mandatory attribute with tag %0 was ignored in %1.")
DIAG(warn_unknown_attribute, DiagnosticEngine::Warning,
     "unknown attribute with tag %0 was ignored in %1.")
DIAG(warn_mismatch_arch, DiagnosticEngine::Warning,
     "Command line architecture: `%0' mismatches object (%1) architecture "
     "attribute `%2'")
DIAG(warn_mismatch_attr, DiagnosticEngine::Warning,
     "%0 in input %1 (value=%2) is not "
     "compatible with the output (value=%3).")
DIAG(err_mismatch_attr, DiagnosticEngine::Error,
     "%0 in input %1 (value=%2) is not "
     "compatible with the output (value=%3).")
