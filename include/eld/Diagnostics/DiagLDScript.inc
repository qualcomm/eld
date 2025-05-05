//===- DiagLDScript.inc----------------------------------------------------===//
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

DIAG(err_unterminated_comment, DiagnosticEngine::Error,
     "%0:%1:%2: error: unterminated comment\n")
DIAG(err_syntax_error, DiagnosticEngine::Error, "%0:%1:%2: error: %3\n")
DIAG(size_of_used_without_phdrs, DiagnosticEngine::Fatal,
     "%0: Using SIZEOF without PHDR's is not supported for segments")
DIAG(err_cant_figure_which_phdr, DiagnosticEngine::Error,
     "Cannot figure out which PHDR to use for section %0 with unspecified "
     "program headers\n")
DIAG(warn_mix_bss_section, DiagnosticEngine::Warning,
     "Placing non-BSS '%0' after BSS '%1' in one segment by linker script. "
     "This makes BBS section has non zero file size.")
DIAG(err_mix_bss_section, DiagnosticEngine::Error,
     "Mixing BSS and non-BSS sections in segment. non-BSS '%0' is after BSS "
     "'%1' in linker script")
DIAG(promoting_bss_to_progbits, DiagnosticEngine::Note,
     "Section %0(%1) is before %2(%3), Promoting %0 to PROGBITS")
DIAG(undefined_symbol_in_linker_script, DiagnosticEngine::Fatal,
     "%0: Error: undefined symbol '%1' referenced in expression")
DIAG(warn_section_no_segment, DiagnosticEngine::Warning,
     "Section %0 does not have segment assignment in linker script.")
DIAG(fatal_segment_not_defined_ldscript, DiagnosticEngine::Fatal,
     "%0: Segment %1 is not defined in linker script.")
DIAG(assert_failed, DiagnosticEngine::Error, "Assertion failed %0")
DIAG(linker_script_uses_phdrs_no_sections, DiagnosticEngine::Error,
     "Linker Script is using PHDR's but not using SECTIONS command")
DIAG(cannot_set_at_address, DiagnosticEngine::Error,
     "Address for section %0 specified with AT cannot be set, as there is a "
     "dependency issue, check the map file for more analysis")
DIAG(cannot_place_at_section, DiagnosticEngine::Error,
     "The section %0 cannot be placed as the address range is occupied by "
     "section %1, Please remove the AT section name and place it manually")
DIAG(at_section_not_aligned, DiagnosticEngine::Warning,
     "The section %0 is not aligned to the section alignment, Aligning "
     "automatically")
DIAG(assignment_maybe_wrong, DiagnosticEngine::Warning,
     "The section %0 has assignment in the middle. the value is not accurate "
     "in best-fit mode")
DIAG(files_no_wildcard_rules, DiagnosticEngine::Note,
     "Rule %0 in output section %1 does not have any sections specified")
DIAG(first_pt_load_doesnot_have_phdr, DiagnosticEngine::Error,
     "First PT_LOAD specified, does not have PHDRS/FILEHDR")
DIAG(not_enough_space_for_phdrs, DiagnosticEngine::Error,
     "Not enough space for program headers, please use SIZEOF_HEADERS")
DIAG(more_sections, DiagnosticEngine::CriticalWarning,
     "Link time performance may affected by input file %0")
DIAG(warn_loadable_section_in_none_segment, DiagnosticEngine::Warning,
     "Loadable section %0 defined in NONE segment")
DIAG(no_pt_tls_segment, DiagnosticEngine::Error,
     "PT_TLS segment not found, required by the image")
DIAG(linker_script_var_used_before_define, DiagnosticEngine::Warning,
     "Symbol %0 used before being defined")
DIAG(error_linker_script, DiagnosticEngine::Error, "%0")
DIAG(note_linker_script, DiagnosticEngine::Note, "%0")
DIAG(reading_dynamic_list, DiagnosticEngine::Verbose, "Dynamic List[%0] : %1")
DIAG(reading_extern_list, DiagnosticEngine::Verbose, "Extern List[%0] : %1")
DIAG(error_parsing_version_script, DiagnosticEngine::Error, "Error parsing version script %0")
DIAG(fatal_divide_by_zero, DiagnosticEngine::Fatal,
     "%0: division by zero in expression %1")
DIAG(fatal_modulo_by_zero, DiagnosticEngine::Fatal,
     "%0: modulo by zero in expression %1")
DIAG(error_experimental_not_supported, DiagnosticEngine::Error,
     "Linker has only experimental support for handling %0")
DIAG(error_memory_region_exceeded_limit, DiagnosticEngine::Error,
     "Memory region %0 exceeded limit while adding section %1")
DIAG(error_memory_region_empty, DiagnosticEngine::Error,
     "Missing name for memory region, name cannot be empty")
DIAG(error_duplicate_memory_region, DiagnosticEngine::Error,
     "Duplicate memory region %0")
DIAG(verbose_verified_add_memory_region, DiagnosticEngine::Verbose,
     "Successfully added memory region %0")
DIAG(warn_memory_region_zero_sized, DiagnosticEngine::Warning,
     "Memory region %0 is zero sized")
DIAG(error_region_not_found, DiagnosticEngine::Error,
     "Cannot find memory region %0")
DIAG(error_bad_memory_attributes, DiagnosticEngine::Error,
     "Bad memory attributes specified for memory region %0")
DIAG(error_no_memory_region, DiagnosticEngine::Error,
     "No memory region assigned to section %0")
DIAG(error_inverted_allowed_only_once, DiagnosticEngine::Error,
     "Error specifying inverted attributes in MEMORY command : %0")
DIAG(error_no_inverted_flags_present, DiagnosticEngine::Error,
     "Inverted memory attributes is empty for memory specification : %0")
DIAG(warn_dot_value_backward_movement, DiagnosticEngine::Warning,
     "unable to move \".\" symbol backwards for output section %0 with "
     "assignment expression %1")
DIAG(no_mapped_region_for_alias, DiagnosticEngine::Error,
     "Cannot find memory region %1 specified by alias %0")
DIAG(error_duplicate_memory_region_alias, DiagnosticEngine::Error,
     "Duplicate memory region alias for %0 in script %1")
DIAG(error_alias_already_is_memory_region, DiagnosticEngine::Error,
     "Alias %0 defined in linker script %1 is name of a memory region")
DIAG(warn_forward_reference, DiagnosticEngine::Warning,
     "%0: forward reference address expression to section %1")
DIAG(warn_output_data_truncated, DiagnosticEngine::Warning,
     "Output data value 0x%0 was truncated to %1 bits for type %2")
DIAG(error_section_overlap, DiagnosticEngine::Error,
     "section %0 %1 range overlaps with %2 \n\t >>> %3 range is %4 \n\t >>> %5 range is %6")
DIAG(error_exp_mem_region_not_found, DiagnosticEngine::Error,
     "Cannot find memory region %1 in script %0")
DIAG(warn_provide_sym_redecl, DiagnosticEngine::Warning,
     "%0: Ignoring provide symbol '%1' redeclaration")
DIAG(internal_err_tok_input_or_group, DiagnosticEngine::InternalError,
     "tok must be INPUT or GROUP!")
DIAG(error_dot_lhs_in_non_alloc, DiagnosticEngine::Error,
     "dot expression evaluation in %0 with permissions %1 is not supported")
DIAG(error_cannot_specify_lma_and_memory_region, DiagnosticEngine::Error,
     "cannot specify AT and LMA memory region for output section : %0 defined in script %1")
DIAG(warn_non_power_of_2_value_to_align_builtin, DiagnosticEngine::Warning,
     "%0: non-power-of-2 value 0x%1 passed to ALIGN builtin function")
DIAG(error_non_power_of_2_value_to_align_output_section, DiagnosticEngine::Error,
     "%0: non-power-of-2 value 0x%1 passed to ALIGN in '%2' output section description, value must "
     "be 0 or a power of 2")
DIAG(warn_linker_script, DiagnosticEngine::Warning, "%0")
