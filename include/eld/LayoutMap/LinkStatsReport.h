//===- LinkStatsReport.h--------------------------------------------------===//
// Part of the eld Project, under the BSD License.
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_LAYOUTMAP_LINKSTATSREPORT_H
#define ELD_LAYOUTMAP_LINKSTATSREPORT_H

#include "llvm/ADT/StringRef.h"

namespace eld {

class DiagnosticEngine;
class Module;

/// Emits input file names and types as a JSON report.
/// Returns false if the report cannot be written.
bool emitLinkStatsReport(Module &M, llvm::StringRef Filename,
                         DiagnosticEngine *DiagEngine);

} // namespace eld

#endif // ELD_LAYOUTMAP_LINKSTATSREPORT_H
