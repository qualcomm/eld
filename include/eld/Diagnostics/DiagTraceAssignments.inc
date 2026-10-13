//===- DiagTraceAssignments.inc--------------------------------------------===//
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

TRACE_DIAG(output_section_fill, "OUTPUT_SECTION(FILL) >> %0", TraceAssignments)
TRACE_DIAG(padding_map, "Start Offset = %0\nEnd Offset = %1", TraceAssignments)
TRACE_DIAG(output_section_eval, "OUTPUT_SECTION(EVALUATE) >>  %0",
           TraceAssignments)
TRACE_DIAG(trace_output_section_addr,
           "OUTPUT_SECTION(EVALUATE) >> %0 VMA : 0x%1 LMA : 0x%2",
           TraceAssignments)
