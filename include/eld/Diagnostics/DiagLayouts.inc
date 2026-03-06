//===- DiagLayouts.inc-----------------------------------------------------===//
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

DIAG(warn_unsupported_symbolic_versioning, DiagnosticEngine::Warning,
     "Symbolic versioning has not been fully supported yet.\nsection `%0'.")
DIAG(
    fatal_paddr_ignored, DiagnosticEngine::Fatal,
    "Ignoring physical address for section %0 which is already part of PHDR %1")
DIAG(output_file_not_complete, DiagnosticEngine::Fatal,
     "Output file %0 not fully written, expected size : %1 file system "
     "recorded size : %2.")
DIAG(output_file_error, DiagnosticEngine::Fatal,
     "Output file %0 not fully written because of error %1.")
DIAG(linking_had_errors, DiagnosticEngine::Fatal, "Linking had errors.")
DIAG(unable_to_insert_trampoline, DiagnosticEngine::Fatal,
     "Unable to insert trampoline for symbol %0 from section %1(%3) to section %2")
DIAG(change_section_type, DiagnosticEngine::Warning,
     "Changing section type to %0 from %1 for section %2")
DIAG(change_section_perm, DiagnosticEngine::Warning,
     "Changing section permissions to %0 from %1 for section %2")
DIAG(loadable_section_not_in_load_segment, DiagnosticEngine::Error,
     "Loadable section %0 not in any load segment")
DIAG(err_found_orphan_section, DiagnosticEngine::Error,
     "no linker script rule for section %0")
DIAG(warn_found_orphan_section, DiagnosticEngine::Warning,
     "no linker script rule for section %0")
DIAG(section_ignored, DiagnosticEngine::Fatal,
     "Section %0 is needed, but set to ignored in linker script")
DIAG(unable_to_clone_fragment, DiagnosticEngine::Warning,
     "Unable to clone fragment for symbol %0 from file %1 in output section %2 "
     "as symbol is not leaf")
DIAG(clone_is_not_supported, DiagnosticEngine::Error,
     "Unable to clone fragment for section %0")
DIAG(offset_not_assigned, DiagnosticEngine::Error,
     "Fatal : Referenced Chunk %0 in output section %1 from %2 is not present "
     "in the output file")
DIAG(offset_not_assigned_with_rule, DiagnosticEngine::Error,
     "Plugin Error : Referenced Chunk %0 from %1, last seen from Rule %2 in "
     "Output Section %3 has been deleted!")
DIAG(incompatible_sections, DiagnosticEngine::Error,
     "Incompatible sections %0 from input file %1 being merged into output "
     "section %2")
DIAG(note_incompatible_sections, DiagnosticEngine::Note,
     "Incompatible sections %0 from input file %1 being merged into output "
     "section %2")
DIAG(non_allocatable_section_in_allocatable_output_section,
     DiagnosticEngine::Warning,
     "Non-allocatable section '%0' from input file '%1' is being merged into "
     "loadable output section '%2'")
DIAG(warning_zero_sized_fragment_for_non_zero_symbol, DiagnosticEngine::Warning,
     "Zero sized fragment %0 for non zero sized symbol %1 from input file %2")
DIAG(error_offset_not_assigned_for_output_section, DiagnosticEngine::Error,
     "Requested offset for output section %0 has not yet been assigned")
DIAG(warn_empty_segment, DiagnosticEngine::Warning,
     "Empty segment: '%0'")
DIAG(note_section_address_not_converging, DiagnosticEngine::Note,
     "Section '%0' address did not converge after %1 layout passes")
