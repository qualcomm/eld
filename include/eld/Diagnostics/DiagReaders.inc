//===- DiagReaders.inc-----------------------------------------------------===//
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

DIAG(fatal_cannot_read_input, DiagnosticEngine::Fatal, "cannot read file %0")
DIAG(err_cannot_read_input, DiagnosticEngine::Error, "cannot read file %0")
DIAG(fatal_cannot_read_input_err, DiagnosticEngine::Fatal,
     "cannot read file %0, because of error %1")
DIAG(fatal_cannot_make_module, DiagnosticEngine::Fatal,
     "cannot make module out of bitcode file %0 for file, error : %1 ")
DIAG(fatal_no_codegen, DiagnosticEngine::Fatal,
     "cannot create code generator object %0")
DIAG(fatal_no_codegen_compile, DiagnosticEngine::Fatal,
     "cannot compile code generator object %0")
DIAG(user_syntax_error, DiagnosticEngine::Fatal,
     "Syntax error found at line %1 while parsing file `%0'")
DIAG(idx_sect_not_found, DiagnosticEngine::Fatal,
     "Section index `%0` out of bounds in file `%1`")
DIAG(file_has_error, DiagnosticEngine::Fatal,
     "Linker script %0 has errors, Please check")
DIAG(section_does_not_have_proper_permissions, DiagnosticEngine::Warning,
     "Section names %0 in input file %1 doesnot have right permissions")
DIAG(corrupt_input_file, DiagnosticEngine::Fatal,
     "Corrupt input file %0, Please inspect the file that everything is proper")
DIAG(error_group_flags, DiagnosticEngine::Error,
     "Unsupported SHT_GROUP flags in `%0' (%1) in file %2: 0x%3")
DIAG(discarding_section, DiagnosticEngine::Verbose,
     "Discard section %0 in file %1")
DIAG(eh_frame_read_error, DiagnosticEngine::Error,
     "EhFrame Read Error : %0 from file %1")
DIAG(eh_frame_read_warn, DiagnosticEngine::Warning,
     "EhFrame Read Error : %0 from file %1")
DIAG(reading_inline_information, DiagnosticEngine::Verbose,
     "Mapping Inline information %0 -> %1")
DIAG(unexpected_section_size, DiagnosticEngine::Error,
     "Unexpected section size %0 in section %1 from input file %2")
DIAG(invalid_timing_value, DiagnosticEngine::Error,
     "Invalid start time %0 in section %1 from input file %2")
DIAG(invalid_section_flag, DiagnosticEngine::Error,
     "Invalid section flag %0 in section %1 from input file %2")
DIAG(no_string_in_timing, DiagnosticEngine::Error,
     "Could not find module name in timing section %0 from input file %1")
DIAG(ini_invalid_character, DiagnosticEngine::Error,
     "ini file %0 contains non-ascii characters")
DIAG(discard_reloc_section_map, DiagnosticEngine::Note,
     "Discarding section .note.qc.reloc.section.map from input file %0")
DIAG(input_file_has_zero_size, DiagnosticEngine::Note,
     "Input file %0 has no contents")
DIAG(string_not_null_terminated, DiagnosticEngine::Error,
    "%0:(%1+0x%2) string is not null terminated")
DIAG(invalid_elf_class, DiagnosticEngine::Error,
     "Invalid ELF file %0 for target %1")
DIAG(fatal_big_endian_target, DiagnosticEngine::Fatal,
     "big-endian targets are not supported: %0")
DIAG(fatal_unsupported_target, DiagnosticEngine::Fatal,
     "Unsupported target configuration: %0")
DIAG(fatal_missing_target_triple, DiagnosticEngine::Fatal,
     "Missing target triple!")
DIAG(error_unsupported_section_in_executable, DiagnosticEngine::Error,
     "Unsupported section %0 in executable input file %1")
DIAG(verbose_symbol_has_aliases, DiagnosticEngine::Verbose,
     "Global symbol %0 from input file %1 has aliases: %2")
DIAG(error_create_archive_member_input, DiagnosticEngine::Error,
     "Failed to read archive member '%0' of the archive '%1'")
DIAG(error_cannot_resolve_path, DiagnosticEngine::Error,
     "Can't resolve path '%0'")
DIAG(warn_unsupported_archive_member, DiagnosticEngine::Warning,
     "Archive member '%1'('%0') is not an object")
DIAG(error_read_archive, DiagnosticEngine::Error, "failed to read archive %0")
DIAG(error_failed_to_read_thin_archive_mem, DiagnosticEngine::Error,
     "Failed to read thin archive member %0(%1): %2")
DIAG(warn_whole_archive_enabled, DiagnosticEngine::Warning,
     "'whole-archive' enabled for '%0'")
DIAG(warn_archive_mem_repeated_content, DiagnosticEngine::Warning,
     "Archive member '%0:%1' at index %2 has same content as archive member '%0:%3' "
     "at index %4")
DIAG(verbose_performing_archive_symbol_resolution, DiagnosticEngine::Verbose,
     "Performing symbol resolution for archive '%0'")
DIAG(verbose_mapping_file_into_memory, DiagnosticEngine::Verbose,
     "Mapping input file '%0' into memory")
DIAG(verbose_reusing_prev_memory_mapping, DiagnosticEngine::Verbose,
     "Reusing previous memory mapping for input file '%0'")
DIAG(verbose_reusing_archive_file_info, DiagnosticEngine::Verbose,
     "Reusing archive file info for '%0'")
DIAG(error_patch_dynamic_input, DiagnosticEngine::Error,
     "Dynamic input '%0' cannot be used with patching")
DIAG(error_patch_invalid_symbol, DiagnosticEngine::Error,
     "Symbol '%0' from file '%1' cannot be patched. Only global non-common "
     "symbols can be patched")
