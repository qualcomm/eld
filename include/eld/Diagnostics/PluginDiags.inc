//===- PluginDiags.inc-----------------------------------------------------===//
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

DIAG(error_file_does_not_exist, DiagnosticEngine::Error,
     "File '%0' does not exist")
DIAG(warn_no_section_overrides_found, DiagnosticEngine::Warning,
     "'LinkerWrapper::finishAssignOutputSections' is called but no "
     "section overrides are found.")
DIAG(error_failed_to_register_reloc, DiagnosticEngine::Error,
     "Failed to register relocation '%1'")
DIAG(error_failed_to_add_sym_to_chunk, DiagnosticEngine::Error,
     "Failed to add symbol '%0' to chunk")
DIAG(error_failed_to_reset_symbol, DiagnosticEngine::Error,
     "Failed to reset symbol '%0'")
DIAG(error_failed_to_insert_rule, DiagnosticEngine::Error,
     "Failed to insert rule")
DIAG(error_invalid_use, DiagnosticEngine::Error, "Invalid Use")
DIAG(error_empty_data, DiagnosticEngine::Error,
     "Data '%0' is empty")
DIAG(error_invalid_symbol, DiagnosticEngine::Error, "Invalid Symbol")
DIAG(error_symbol_has_no_chunk, DiagnosticEngine::Error,
     "Symbol '%0' content cannot be replaced because the "
     "symbol has no chunk")
DIAG(error_chunk_is_bss, DiagnosticEngine::Error,
     "Symbol '%0' content cannot be replaced because the symbol belongs "
     "to bss")
DIAG(error_symbol_is_small, DiagnosticEngine::Error,
     "Symbol '%0' content cannot be replaced because the new content is "
     "larger (by %1 bytes)")
DIAG(error_write_file, DiagnosticEngine::Error,
     "Failed to write to '%0': %1")
DIAG(error_no_output_section, DiagnosticEngine::Error,
     "Output section not found for the input section '%0'")
DIAG(error_output_section_not_found, DiagnosticEngine::Error,
     "Output section '%0' not found")
DIAG(error_invalid_input_file, DiagnosticEngine::Error,
     "Invalid input file %0")
DIAG(error_dwarf_context_not_available, DiagnosticEngine::Error,
     "Dwarf context is not available for %0")
DIAG(error_symbol_not_found, DiagnosticEngine::Error,
     "Symbol '%0' not found")
DIAG(error_invalid_link_state, DiagnosticEngine::Error,
     "Link state '%0' is invalid for the API '%1'. Valid link states: [%2]")
DIAG(error_nobits_unsupported, DiagnosticEngine::Error,
     "Cannot retrive content of NOBITS section %0")
DIAG(error_plugin_opt_prefix, DiagnosticEngine::Error,
     "Invalid option '%0'. Plugin options must begin with double-dash ('--')")
DIAG(error_plugin_opt_empty, DiagnosticEngine::Error,
     "Invalid option '%0'. Option cannot be just double-dash")
DIAG(error_empty_input_file, DiagnosticEngine::Error,
     "Empty eld::plugin::InputFile object")
DIAG(error_invalid_input_file_for_api, DiagnosticEngine::Error,
     "Invalid input file '%0' for the API '%1'")
DIAG(error_RM_sect_name_map_already_set, DiagnosticEngine::Error,
     "Section name map is already set for the input file '%0' by "
     "the plugin '%1'")
DIAG(error_aux_sym_name_map_already_set, DiagnosticEngine::Error,
     "Auxiliary symbol name map is already set for the input file '%0' by "
     "the plugin '%1'")
DIAG(error_empty_rule, DiagnosticEngine::Error,
     "Empty eld::plugin::LinkerScriptRule object")
DIAG(error_empty_section, DiagnosticEngine::Error,
     "Empty eld::plugin::Section object")
