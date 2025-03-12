//===- DiagStats.inc-------------------------------------------------------===//
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

DIAG(linker_script_rule_matching_time, DiagnosticEngine::Note,
     "Linker script rule matching took %0 ms")
DIAG(linker_script_rule_matching_stats, DiagnosticEngine::Note,
     "Linker script rule %0 matched %1 (sections) and took %2 ms")
DIAG(layout_stats, DiagnosticEngine::Note, "Layout iteration %0 took %1 ms")
DIAG(threads_enabled, DiagnosticEngine::Note,
     "Threads Enabled %0, Number of threads = %1")
DIAG(threads_disabled, DiagnosticEngine::Note, "Threads Disabled : %0")
DIAG(linker_run_summary, DiagnosticEngine::Note,
     "Linker had errors (%0 Warnings, %1 Errors , %2 Fatal Errors)")