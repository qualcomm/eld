//===- Diagnostics.h-------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_PLUGINAPI_DIAGNOSTICS_H
#define ELD_PLUGINAPI_DIAGNOSTICS_H

#include "DiagnosticEntry.h"

namespace eld::plugin {
/// This namespace provides common plugin diagnostics.
namespace Diagnostic {
DLL_A_EXPORT DiagnosticEntry::DiagIDType error_file_does_not_exist();
DLL_A_EXPORT DiagnosticEntry::DiagIDType warn_no_section_overrides_found();
DLL_A_EXPORT DiagnosticEntry::DiagIDType error_failed_to_register_reloc();
DLL_A_EXPORT DiagnosticEntry::DiagIDType error_failed_to_add_sym_to_chunk();
DLL_A_EXPORT DiagnosticEntry::DiagIDType error_failed_to_reset_symbol();
DLL_A_EXPORT DiagnosticEntry::DiagIDType error_failed_to_insert_rule();
DLL_A_EXPORT DiagnosticEntry::DiagIDType error_invalid_use();
DLL_A_EXPORT DiagnosticEntry::DiagIDType error_empty_data();
DLL_A_EXPORT DiagnosticEntry::DiagIDType error_invalid_symbol();
DLL_A_EXPORT DiagnosticEntry::DiagIDType error_symbol_has_no_chunk();
DLL_A_EXPORT DiagnosticEntry::DiagIDType error_chunk_is_bss();
DLL_A_EXPORT DiagnosticEntry::DiagIDType error_symbol_is_small();
DLL_A_EXPORT DiagnosticEntry::DiagIDType error_write_file();
DLL_A_EXPORT DiagnosticEntry::DiagIDType error_no_output_section();
DLL_A_EXPORT DiagnosticEntry::DiagIDType error_output_section_not_found();
DLL_A_EXPORT DiagnosticEntry::DiagIDType error_invalid_input_file();
DLL_A_EXPORT DiagnosticEntry::DiagIDType error_dwarf_context_not_available();
DLL_A_EXPORT DiagnosticEntry::DiagIDType error_symbol_not_found();
} // namespace Diagnostic
} // namespace eld::plugin
#endif
