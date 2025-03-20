//===- Diagnostics.cpp-----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/PluginAPI/Diagnostics.h"
#include "eld/Diagnostics/DiagnosticEngine.h"

using namespace eld;

DLL_A_EXPORT plugin::DiagnosticEntry::DiagIDType
plugin::Diagnostic::error_file_does_not_exist() {
  return eld::Diag::error_file_does_not_exist;
}

DLL_A_EXPORT plugin::DiagnosticEntry::DiagIDType
plugin::Diagnostic::warn_no_section_overrides_found() {
  return eld::Diag::warn_no_section_overrides_found;
}

DLL_A_EXPORT plugin::DiagnosticEntry::DiagIDType
plugin::Diagnostic::error_failed_to_register_reloc() {
  return eld::Diag::error_failed_to_register_reloc;
}

DLL_A_EXPORT plugin::DiagnosticEntry::DiagIDType
plugin::Diagnostic::error_failed_to_add_sym_to_chunk() {
  return eld::Diag::error_failed_to_add_sym_to_chunk;
}

DLL_A_EXPORT plugin::DiagnosticEntry::DiagIDType
plugin::Diagnostic::error_failed_to_reset_symbol() {
  return eld::Diag::error_failed_to_reset_symbol;
}

DLL_A_EXPORT plugin::DiagnosticEntry::DiagIDType
plugin::Diagnostic::error_failed_to_insert_rule() {
  return eld::Diag::error_failed_to_insert_rule;
}

DLL_A_EXPORT plugin::DiagnosticEntry::DiagIDType
plugin::Diagnostic::error_invalid_use() {
  return eld::Diag::error_invalid_use;
}

DLL_A_EXPORT plugin::DiagnosticEntry::DiagIDType
plugin::Diagnostic::error_empty_data() {
  return eld::Diag::error_empty_data;
}

DLL_A_EXPORT plugin::DiagnosticEntry::DiagIDType
plugin::Diagnostic::error_invalid_symbol() {
  return eld::Diag::error_invalid_symbol;
}

DLL_A_EXPORT plugin::DiagnosticEntry::DiagIDType
plugin::Diagnostic::error_symbol_has_no_chunk() {
  return eld::Diag::error_symbol_has_no_chunk;
}

DLL_A_EXPORT plugin::DiagnosticEntry::DiagIDType
plugin::Diagnostic::error_chunk_is_bss() {
  return eld::Diag::error_chunk_is_bss;
}

DLL_A_EXPORT plugin::DiagnosticEntry::DiagIDType
plugin::Diagnostic::error_symbol_is_small() {
  return eld::Diag::error_symbol_is_small;
}

DLL_A_EXPORT plugin::DiagnosticEntry::DiagIDType
plugin::Diagnostic::error_write_file() {
  return eld::Diag::error_write_file;
}

DLL_A_EXPORT plugin::DiagnosticEntry::DiagIDType
plugin::Diagnostic::error_no_output_section() {
  return eld::Diag::error_no_output_section;
}

DLL_A_EXPORT plugin::DiagnosticEntry::DiagIDType
plugin::Diagnostic::error_output_section_not_found() {
  return eld::Diag::error_output_section_not_found;
}

DLL_A_EXPORT plugin::DiagnosticEntry::DiagIDType
plugin::Diagnostic::error_invalid_input_file() {
  return eld::Diag::error_invalid_input_file;
}

DLL_A_EXPORT plugin::DiagnosticEntry::DiagIDType
plugin::Diagnostic::error_dwarf_context_not_available() {
  return eld::Diag::error_dwarf_context_not_available;
}

DLL_A_EXPORT plugin::DiagnosticEntry::DiagIDType
plugin::Diagnostic::error_symbol_not_found() {
  return eld::Diag::error_symbol_not_found;
}
