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

TRACE_DIAG(trace_gc_header,
           "\nReachability map for mark phase of GC:\nReferring site%0Referred "
           "site(s)",
           TraceGC)
TRACE_DIAG(symbols_referring, "Symbols referring %0 [%1]", TraceGC)
TRACE_DIAG(referred_symbol, "\t%0\t%1\t[%2]", TraceGC)
TRACE_DIAG(tracing_gc_phase, "Tracing references in %0 phase", TraceGCLive)
TRACE_DIAG(refers_to, "Refers to = %0", TraceGCLive)
TRACE_DIAG(referenced_input_file, "\t[%0]", TraceGCLive)
TRACE_DIAG(referenced_bc_file, "\tBC : [%0]", TraceGCLive)
TRACE_DIAG(referenced_by_root_symbol, "\tReferenced by = Root symbol",
           TraceGCLive)
TRACE_DIAG(referenced_by, "\tReferenced by = %0", TraceGCLive)
TRACE_DIAG(trace_gc_section, "GC : %0[%1]", TraceGC)
TRACE_DIAG(trace_gc_symbol, "GC (Common Symbol): %0[%1]", TraceGC)
