//===- LoadInfoReport.h---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_OBJECT_LOADINFOREPORT_H
#define ELD_OBJECT_LOADINFOREPORT_H

#include "eld/Object/ObjectLinker.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/JSON.h"
#include <unordered_map>
#include <vector>

namespace eld {

class ArchiveFile;
class DiagnosticEngine;
class Input;
class InputAction;
class LinkerScriptFile;

class LoadInfoReport {
public:
  LoadInfoReport(const ObjectLinker &ObjLinker,
                 const std::vector<InputAction *> &Actions,
                 DiagnosticEngine &DiagEngine);

  bool emitLoadInfoJson(llvm::StringRef Filename);

private:
  using ArchiveRecordsMapType =
      std::unordered_map<ArchiveFile *,
                         std::vector<ObjectLinker::ArchiveMemberReportRecord>>;

  ArchiveRecordsMapType buildArchiveRecordsMap();
  llvm::StringRef getTypeName(InputFile::InputFileKind Kind);
  llvm::StringRef getBehaviorOptionName(InputAction::InputActionKind Kind);
  bool isBehaviorOption(InputAction::InputActionKind Kind);
  llvm::json::Object emitInputEntry(Input *I, bool IncludeIndex);
  void emitArchiveMembers(llvm::json::Object &Entry, ArchiveFile *AF);
  void emitNestedInputs(llvm::json::Object &Entry, LinkerScriptFile *LSF);

  const ObjectLinker &ObjLinker;
  const std::vector<InputAction *> &Actions;
  DiagnosticEngine &DiagEngine;
  ArchiveRecordsMapType ArchiveRecordsMap;
};

} // namespace eld

#endif // ELD_OBJECT_LOADINFOREPORT_H
