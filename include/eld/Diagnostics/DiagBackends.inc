//===- DiagBackends.inc----------------------------------------------------===//
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

DIAG(default_threads_disabled, DiagnosticEngine::Note,
     "Disabling threads by default, Use --threads to enable it")
DIAG(non_contiguous_TLS, DiagnosticEngine::Error,
     "TLS sections %0 and %1 are not contiguous")
DIAG(small_data_not_found, DiagnosticEngine::Fatal, "Small data area not found")
DIAG(sda_base_not_found, DiagnosticEngine::Warning,
     "_SDA_BASE_ not found, defaulting to start of .sdata")
DIAG(msg_base_not_found_linker_script, DiagnosticEngine::Fatal,
     "Please define _MSG_BASE_ in the linker script")
DIAG(msg_base_not_found_no_linker_script, DiagnosticEngine::Fatal,
     "_MSG_BASE_ area not found")
DIAG(warn_armexidx_no_link, DiagnosticEngine::Warning,
     "ARM_EXIDX Section %1 from %0 does not have link section set")
DIAG(physical_address_not_in_syn_with_offset, DiagnosticEngine::Warning,
     "Compact Option needs physical address aligned with File offsets. "
     "Mismatch found at section %0")
DIAG(physical_address_and_offset_are_not_congruent, DiagnosticEngine::Warning,
     "Physical address and the offset of a segment must be congruent modulo "
     "the alignment of the segment. "
     "Mismatch found at segment %0")
DIAG(sbrel_reloc_different_seg, DiagnosticEngine::Error,
     "R_ARM_SBREL32 Relocation Mismatch for symbol %0 defined in %1 has a "
     "different load segment")
DIAG(unsupported_abi, DiagnosticEngine::Error,
     "Unsupported ABI specification: %0")
DIAG(unsupported_rv_reloc, DiagnosticEngine::Error,
     "Unsupported relocation `%0' inside `%2' for symbol `%1'")
DIAG(rv_hi20_not_found, DiagnosticEngine::Error,
     "%2: Could not find HI relocation for relocation `%1' referring symbol "
     "`%0' ")
DIAG(error_rv_vendor_not_found, DiagnosticEngine::Error,
     "%1: Could not find R_RISCV_VENDOR relocation for relocation '%0'")
DIAG(error_rv_unknown_vendor_symbol, DiagnosticEngine::Error,
     "%2: Unknown Vendor Symbol '%0' for relocation '%1'")
DIAG(error_rv_unknown_vendor_relocation, DiagnosticEngine::Error,
     "%2: Unknown Vendor Relocation '%1' for Vendor Symbol '%0'")
DIAG(deprecated_emulation, DiagnosticEngine::Error,
     "Error : Emulation %0 is deprecated in this version of the toolchain")
DIAG(riscv_relaxation_error, DiagnosticEngine::Fatal,
     "Error in RISCV relaxation computation, unable to continue")
DIAG(error_riscv_relaxation_align, DiagnosticEngine::Error,
     "Insufficient number of spare bytes for alignment relaxation: present %0, "
     "needed %1 in section %2+0x%3 file %4")
DIAG(incompatible_architecture, DiagnosticEngine::Warning,
     "Mixing incompatible object files in the link process %0")
DIAG(deprecated_isa, DiagnosticEngine::Warning,
     "Support for ISA %0 is deprecated for input file %1")
DIAG(deprecated_and_no_support_exists, DiagnosticEngine::Error,
     "Support for ISA %0 is deprecated and no support exists for input file %1")
DIAG(not_supported_isa, DiagnosticEngine::Error,
     "ISA %0 is not supported for input file %1")
DIAG(sbrel_reloc_no_section, DiagnosticEngine::Error,
     "SBREL Relocation points to no segment for symbol %0")
DIAG(incompatible_architecture_versions, DiagnosticEngine::Error,
     "Incompatible object files when reading %1, mixing %0 and %2")
DIAG(attribute_parsing_error, DiagnosticEngine::Error,
     "Reading attributes failed for file %0, Error : %1")
DIAG(riscv_attribute_parsing_mix_warn, DiagnosticEngine::Warning,
     "Reading incompatible attributes in file %1, Warning mixing attribute %2, seen "
     "previous value %4 from input file %0, now seen %3")
DIAG(riscv_attribute_parsing_mix_error, DiagnosticEngine::Error,
     "Reading incompatible attributes in file %1, Error mixing attribute %2, seen "
     "previous value %4 from input file %0, now seen %3")
DIAG(incompatible_input_architecture, DiagnosticEngine::Warning,
     "Mixing incompatible object file %0, object file arch is %1, output file "
     "arch is %2")
DIAG(gnu_property_read_error, DiagnosticEngine::Fatal,
     "Reading .note.gnu.property from %0 has error %1")
DIAG(no_feature_found_in_file, DiagnosticEngine::Warning, "No %0 in %1")
DIAG(arm_enum_prohibited, DiagnosticEngine::Error, "Enum prohibited in %0")
DIAG(record_arm_attribute, DiagnosticEngine::Verbose,
     "Recording %0 feature when reading file %1")
DIAG(branch_to_arm_code_not_allowed, DiagnosticEngine::Fatal,
     "Cannot branch to ARM symbol %0 referred from %1")
DIAG(not_supported, DiagnosticEngine::Error, "%0 is not supported for %1")
DIAG(chaining_sections, DiagnosticEngine::Verbose, "Chaining sections %0 to %1")
DIAG(stripping_symbol, DiagnosticEngine::Note, "Stripping symbol %0")
DIAG(riscv_shared_libraries_unsupported, DiagnosticEngine::Warning,
     "Shared libraries are not supported for RISCV target")
DIAG(riscv_pie_unsupported, DiagnosticEngine::Error,
     "PIE executables are not supported for RISCV target")
DIAG(pcrel_reloc_overflow, DiagnosticEngine::Note,
     "Symbol Address = 0x%0, Addend = 0x%1, Place = 0x%2, Relocation Value = 0x%3")
DIAG(expected_group_section, DiagnosticEngine::InternalError,
     "Expected %0 to be a 'group' section.")
DIAG(trace_arch_options, DiagnosticEngine::Trace, "Arch option %0 : %1")
DIAG(trace_arch_options_map, DiagnosticEngine::Trace, "Arch option map %0 : %1")
DIAG(warn_ignore_pcrel_lo_addend, DiagnosticEngine::Warning,
     "%2: ignoring addend for relocation `%1' referring symbol %0")
DIAG(error_build_id_not_hex, DiagnosticEngine::Error,
    "build id specified is not hexadecimal : %0")
DIAG(error_unknown_build_id, DiagnosticEngine::Error,
     "unknown build style specified %0, valid values are "
     "none/sha1/md5/uuid/0x<val>")
DIAG(error_entropy_source_failure, DiagnosticEngine::Error,
     "Entropy source failure : %0")
DIAG(warn_attribute_parse_fail, DiagnosticEngine::Warning, "%0:(%1): %2")
DIAG(error_relocation_not_paired, DiagnosticEngine::Error,
    "%0:(%1+%2): %3 not paired with %4")
