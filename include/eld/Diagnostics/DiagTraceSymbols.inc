//===- DiagTraceSymbols.inc------------------------------------------------===//
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

DIAG(apply_relocation, DiagnosticEngine::Trace,
     "Symbol %0, application site: 0x%1")
DIAG(process_file, DiagnosticEngine::Trace, "Processing file: %0")
DIAG(obj_symbol_created, DiagnosticEngine::Trace,
     "Object symbol created [%0]: (SHNDX %1) %2 ")
DIAG(dyn_obj_symbol_created, DiagnosticEngine::Trace,
     "DynObj symbol created [%0]: %1")
DIAG(adding_new_sym, DiagnosticEngine::Trace, "Adding new symbol (%0): %1")
DIAG(overriding_old_sym, DiagnosticEngine::Trace,
     "Overriding old symbol %0 (%1) with newly added symbol %2")
DIAG(marking_sym, DiagnosticEngine::Trace, "Marking symbol %0 as %1")
DIAG(override_dyn_sym, DiagnosticEngine::Trace,
     "Overriding the dynamic symbol %0 (%1) with %2")
DIAG(override_common_sym, DiagnosticEngine::Trace,
     "Overriding common symbol %0 with largest size (%1)")
DIAG(mark_common_sym, DiagnosticEngine::Trace,
     "Marking common symbol %0 with largest size (%1)")
