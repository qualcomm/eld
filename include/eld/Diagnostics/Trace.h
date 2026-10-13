//===- Trace.h-------------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
#ifndef ELD_DIAGNOSTICS_TRACE_H
#define ELD_DIAGNOSTICS_TRACE_H

#include <cstdint>

namespace eld {

enum TraceCategory : uint32_t {
  TraceNone = 0,
  TraceFiles = 0x1,
  TraceTrampolines = 0x2,
  TraceAllSymbols = 0x4,
  TraceCommandLine = 0x8,
  TraceGC = 0x10,
  TraceSymbol = 0x20,
  TraceLTO = 0x40,
  TraceAssignments = 0x80,
  TraceReloc = 0x100,
  TraceThreads = 0x200,
  TracePlugin = 0x400,
  TraceGCLive = 0x800,
  TraceWrap = 0x1000,
  TraceSection = 0x2000,
  TraceDynamicLinking = 0x4000,
  TraceMergeStrings = 0x8000,
  TraceLinkerScript = 0x10000,
  TraceUntar = 0x20000,
  TraceRelax = 0x40000,
  TraceMergeConstants = 0x80000,
  TraceSymDef = 0x100000,
  TraceSymbolVersioning = 0x200000
};

using TraceMask = uint32_t;

} // namespace eld

#endif
