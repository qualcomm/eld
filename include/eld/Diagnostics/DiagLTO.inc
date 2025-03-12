//===- DiagLTO.inc---------------------------------------------------------===//
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

DIAG(note_preserve_symbol, DiagnosticEngine::Note, "Preserving symbol %0")
DIAG(note_lto_phase, DiagnosticEngine::Note, "Beginning LTO Phase %0")
DIAG(note_insert_object, DiagnosticEngine::Note,
     "Inserting generated LTO object %0 into input tree")
DIAG(note_temp_lto_object, DiagnosticEngine::Note,
     "LTO generated native object: %0")
DIAG(note_temp_lto_bitcode, DiagnosticEngine::Note, "LTO generated bitcode: %0")
DIAG(note_lto_threads, DiagnosticEngine::Note,
     "Using %0 threads for LTO code generation.")
DIAG(note_lto_cache, DiagnosticEngine::Note,
     "Using ThinLTO cache directory: %0")
DIAG(fatal_lto_cache_error, DiagnosticEngine::Fatal,
     "Error using ThinLTO cache directory (%0): %1")
DIAG(process_launch, DiagnosticEngine::Note, "Running %0")
DIAG(lto_resolution, DiagnosticEngine::Note,
     "LTO resolution for symbol %0: Prevailing=%1, VisibleToRegularObj=%2, "
     "FinalDefinitionInLinkageUnit=%3, LinkerRedefined=%5. Resolved Origin: "
     "%4.")
DIAG(adding_module, DiagnosticEngine::Note, "Adding LTO module %0")
DIAG(codegen_options, DiagnosticEngine::Note,
     "Setting Codegen options (triple %0): %1")
DIAG(note_preserving_common, DiagnosticEngine::Note,
     "Preserving common symbol %0 defined in file %1")
DIAG(note_not_preserving_symbol, DiagnosticEngine::Note,
     "Not preserving symbol %0 as there are no external references")
DIAG(fatal_lto_merge_error, DiagnosticEngine::Fatal,
     "Could not merge LTO modules: `%0'")
DIAG(using_lto_asm_file, DiagnosticEngine::Warning,
     "Using LTO assembly file : %0")
DIAG(using_lto_output_file, DiagnosticEngine::Warning,
     "Using LTO output file : %0")
DIAG(set_codegen_mcpu, DiagnosticEngine::Note, "Setting codegen MCPU to %0")
DIAG(lto_code_model, DiagnosticEngine::Note, "LTO Set codemodel to %0")
DIAG(lto_codegen_note, DiagnosticEngine::Note, "LTO: %0")
DIAG(lto_codegen_warning, DiagnosticEngine::Warning, "LTO: %0")
DIAG(lto_codegen_error, DiagnosticEngine::Fatal, "LTO: %0")
DIAG(lto_codegen_remark, DiagnosticEngine::Remark, "LTO: %0")
DIAG(lto_deleting_temp_file, DiagnosticEngine::Note, "LTO: Deleting temporary file %0")
DIAG(lto_cannot_save_temps, DiagnosticEngine::Fatal, "LTO: Unable to enable save-temps: %0")
DIAG(preserve_dyn_list_sym, DiagnosticEngine::Note,
     "Marking symbol %0 to be preserved (found in dynamic list).")
DIAG(preserve_wrap, DiagnosticEngine::Note,
     "Marking wrap symbol %0 to be preserved.")
DIAG(preserve_wrapper_def, DiagnosticEngine::Note,
     "Preserving wrapper definition %0.")
