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

DIAG(trampoline_symbol, DiagnosticEngine::Trace, "Symbol : %0")
DIAG(reuse_stub, DiagnosticEngine::Trace, "Reusing Stub %0")
DIAG(create_stub, DiagnosticEngine::Trace, "Creating Stub %0")
DIAG(set_call_from_stub, DiagnosticEngine::Trace, "From %0, Setting Call to %1")
DIAG(destination_stub, DiagnosticEngine::Trace, "at ~0x%0 to this stub")
