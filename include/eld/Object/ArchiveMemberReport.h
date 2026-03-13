//===- ArchiveMemberReport.h-----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_OBJECT_ARCHIVEMEMBERREPORT_H
#define ELD_OBJECT_ARCHIVEMEMBERREPORT_H

#include "llvm/ADT/StringRef.h"

namespace eld {

class DiagnosticEngine;
class ObjectLinker;

/// Emits a JSON report describing archive members pulled in to satisfy
/// symbol references.
/// \returns true on success, false on failure. Emits a diagnostic on failure.
bool emitArchiveMemberReport(const ObjectLinker &ObjLinker,
                             llvm::StringRef Filename,
                             DiagnosticEngine *DiagEngine);

} // namespace eld

#endif // ELD_OBJECT_ARCHIVEMEMBERREPORT_H
