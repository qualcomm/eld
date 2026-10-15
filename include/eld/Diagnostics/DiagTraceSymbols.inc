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

TRACE_DIAG(apply_relocation, "Symbol %0, application site: 0x%1", TraceSymbol)
TRACE_DIAG(process_file, "Processing file: %0", TraceAllSymbols)
TRACE_DIAG(obj_symbol_created, "Object symbol created [%0]: (SHNDX %1) %2 ",
           TraceAllSymbols)
TRACE_DIAG(adding_new_sym, "Adding new symbol (%0): %1", TraceAllSymbols)
TRACE_DIAG(overriding_old_sym,
           "Overriding old symbol %0 (%1) with newly added symbol %2",
           TraceAllSymbols)
TRACE_DIAG(marking_sym, "Marking symbol %0 as %1", TraceAllSymbols)
TRACE_DIAG(override_dyn_sym, "Overriding the dynamic symbol %0 (%1) with %2",
           TraceAllSymbols)
TRACE_DIAG(override_common_sym,
           "Overriding common symbol %0 with largest size (%1)",
           TraceAllSymbols)
TRACE_DIAG(mark_common_sym, "Marking common symbol %0 with largest size (%1)",
           TraceAllSymbols)
