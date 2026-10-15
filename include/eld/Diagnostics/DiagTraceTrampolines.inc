//===- DiagTraceTrampolines.inc--------------------------------------------===//
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

TRACE_DIAG(trampoline_symbol, "Symbol : %0", TraceTrampolines)
TRACE_DIAG(reuse_stub, "Reusing Stub %0", TraceTrampolines)
TRACE_DIAG(create_stub, "Creating Stub %0", TraceTrampolines)
TRACE_DIAG(set_call_from_stub, "From %0, Setting Call to %1", TraceTrampolines)
TRACE_DIAG(destination_stub, "at ~0x%0 to this stub", TraceTrampolines)
