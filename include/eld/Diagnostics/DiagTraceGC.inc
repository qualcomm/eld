//===- DiagTraceGC.inc-----------------------------------------------------===//
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

DIAG(trace_gc_header, DiagnosticEngine::Trace,
     "\nReachability map for mark phase of GC:\nReferring site%0Referred "
     "site(s)")
DIAG(symbols_referring, DiagnosticEngine::Trace, "Symbols referring %0 [%1]")
DIAG(referred_symbol, DiagnosticEngine::Trace, "\t%0\t%1\t[%2]")
DIAG(tracing_gc_phase, DiagnosticEngine::Trace,
     "Tracing references in %0 phase")
DIAG(refers_to, DiagnosticEngine::Trace, "Refers to = %0")
DIAG(referenced_input_file, DiagnosticEngine::Trace, "\t[%0]")
DIAG(referenced_bc_file, DiagnosticEngine::Trace, "\tBC : [%0]")
DIAG(referenced_by_root_symbol, DiagnosticEngine::Trace,
     "\tReferenced by = Root symbol")
DIAG(referenced_by, DiagnosticEngine::Trace, "\tReferenced by = %0")
DIAG(trace_gc_section, DiagnosticEngine::Trace, "GC : %0[%1]")
DIAG(trace_gc_symbol, DiagnosticEngine::Trace, "GC (Common Symbol): %0[%1]")