//===- DiagGOTPLT.inc------------------------------------------------------===//
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

DIAG(create_got_entry, DiagnosticEngine::Note,
     "Created GOT Entry for Symbol %0")
DIAG(create_plt_entry, DiagnosticEngine::Note,
     "Created PLT Entry for Symbol %0")
