//===- DiagSymbolVersioning.inc--------------------------------------------===//
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
DIAG(trace_creating_symbol_versioning_section, DiagnosticEngine::Trace,
     "Creating symbol versioning section: %0")
DIAG(trace_creating_symbol_versioning_fragment, DiagnosticEngine::Trace,
     "Creating symbol versioning fragment: %0")
DIAG(trace_version_script_matched_scope, DiagnosticEngine::Trace,
     "Version script node matched %0 symbol '%1'")
DIAG(trace_clear_export_due_to_local_scope, DiagnosticEngine::Trace,
     "Clearing export for symbol '%0' due to local scope")
DIAG(error_missing_version_node, DiagnosticEngine::Error,
     "%0: symbol %1 has undefined version %2")
DIAG(trace_assign_output_version_ids, DiagnosticEngine::Trace,
     "Assigning version IDs to output symbols")