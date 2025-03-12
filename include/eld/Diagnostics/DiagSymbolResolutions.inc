//===- DiagSymbolResolutions.inc-------------------------------------------===//
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

DIAG(note_has_no_symtab, DiagnosticEngine::Note,
     "input file `%0' has no symbol table `%2'\n  path of input file: %1")
DIAG(fatal_cannot_read_strtab, DiagnosticEngine::Fatal,
     "cannot read strtab for %2 in file `%0': %1")
DIAG(fail_sym_resolution, DiagnosticEngine::Unreachable,
     "Fails to resolve symbols [%0:%1]")
DIAG(mark_dynamic_defined, DiagnosticEngine::Ignore,
     "refer to dynamic symbol %0")
DIAG(multiple_definitions, DiagnosticEngine::Error,
     "multiple definition of symbol `%0' in file %1 and %2")
DIAG(multiple_absolute_definitions, DiagnosticEngine::Error,
     "inconsistent definitions of absolute symbol `%0': old(%1) -> new(%2)")
DIAG(add_new_symbol, DiagnosticEngine::Note,
     "Symbol `%0' from Input file `%1' with info `%2' being added to Namepool")
DIAG(resolve_new_symbol, DiagnosticEngine::Note,
     "Symbol `%0' from Input file "
     "`%1' with info `%2' being "
     "resolved from Namepool")
DIAG(symbol_undefined_by_user, DiagnosticEngine::Error,
     "Symbol %0 defined in %1 is undefined")
DIAG(symbol_not_found, DiagnosticEngine::Fatal,
     "Malformed image: Symbol `%0' not found")
DIAG(dynlist_symbol_undefined_by_user, DiagnosticEngine::Error,
     "Symbol %0 defined in Dynamic List  is undefined")
DIAG(tls_non_tls_symbol_mismatch, DiagnosticEngine::Fatal, "%0")
DIAG(undefined_symbol, DiagnosticEngine::Error,
     "Symbol %0 is undefined and referenced from %1")
DIAG(rename_undef_sym, DiagnosticEngine::Note,
     "Renaming undefined symbol `%1' to `%2' from %0 for wrapping")
DIAG(restore_def_binding, DiagnosticEngine::Note,
     "Restoring binding for symbol `%0'")
DIAG(insert_wrapper, DiagnosticEngine::Note,
     "Inserting wrapper `%0' for bitcode symbol")
DIAG(real_sym_reference, DiagnosticEngine::Note,
     "Found real symbol `%0' for wrapped symbol `%1'")
DIAG(cannot_find_real_symbol, DiagnosticEngine::Fatal,
     "Cannot find real symbol for symbol %0 referenced from %1")
DIAG(cannot_find_wrap_symbol, DiagnosticEngine::Fatal,
     "Cannot find wrap symbol for symbol %0 referenced from %1")
DIAG(comm_override_by_definition, DiagnosticEngine::Warning,
     "common symbol %0 in file %1 is overridden by definition from %2")
DIAG(common_symbol_doesnot_override_definition, DiagnosticEngine::Warning,
     "common symbol in %2 does not override defined symbol %0 in file %1")
DIAG(comm_override_by_comm, DiagnosticEngine::Warning,
     "common symbol %0 in file %1 is overridden by common symbol from %2")
DIAG(
    read_section, DiagnosticEngine::Note,
    "Input Section : %0 InputFile : %1 Alignment : 0x%2 Size : 0x%3 Flags : %4")
DIAG(read_bitcode_section, DiagnosticEngine::Note,
     "Input Section : %0 InputFile : %1")
DIAG(section_info, DiagnosticEngine::Note,
     "Output Section : %0 PMA : 0x%1 VMA : 0x%2 Offset : 0x%3 Segment : %4 "
     "SegmentType : %5")
DIAG(input_section_info, DiagnosticEngine::Note,
     "Input Section : %0 PMA : 0x%1 VMA : 0x%2 Offset : 0x%3 Size : 0x%4")
DIAG(internal_section_create, DiagnosticEngine::Note,
     "Internal Section : %0 Alignment : 0x%1 Size : 0x%2 Flags : %3")
DIAG(discarded_section_info, DiagnosticEngine::Note,
     "Section : %0 is being discarded. Section originated from input : %1")
DIAG(symbols_in_section_info, DiagnosticEngine::Note,
     "Input Section : %0 Symbol : %1 ")
DIAG(gc_section_info, DiagnosticEngine::Note,
     "Section : %0 is being garbage collected. Section originated from input : "
     "%1")
DIAG(section_mapping_info, DiagnosticEngine::Note,
     "Input Section : %0 Output Section : %1")
DIAG(target_specific_symbol, DiagnosticEngine::Note,
     "Target Specific Symbol %0 added")
DIAG(note_resolving_from_provide_symdef_file, DiagnosticEngine::Note,
     "Resolving symbol '%0' from provide style sym def file '%1'")
DIAG(note_read_from_symdef_file, DiagnosticEngine::Note,
     "Read symbol '%0' from sym def file '%1'")
DIAG(note_resolving_from_symdef_file, DiagnosticEngine::Note,
     "Resolving symbol '%0' from sym def file '%1'")
DIAG(error_referenced_discarded_section, DiagnosticEngine::Error,
     "`%0' referenced in section `%1' of %2: defined in discarded section")
DIAG(error_patchable_override, DiagnosticEngine::Error,
     "Patchable symbol '%0' in file '%1' is overridden by non-patchable symbol "
     "from file '%2'")
DIAG(error_patchable_script, DiagnosticEngine::Error,
     "Patchable symbol '%0' is overridden by linker script")
DIAG(error_patchable_alias_not_found, DiagnosticEngine::Error,
     "Patchable alias '%0' was not found or was garbage collected")
