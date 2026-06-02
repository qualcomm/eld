//===- LoadInfoReport.cpp-------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Object/LoadInfoReport.h"
#include "eld/Config/Version.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Input/ArchiveFile.h"
#include "eld/Input/ArchiveMemberInput.h"
#include "eld/Input/ELFDynObjectFile.h"
#include "eld/Input/InputAction.h"
#include "eld/Input/InputFile.h"
#include "eld/Input/InputTree.h"
#include "eld/Input/LinkerScriptFile.h"
#include "eld/SymbolResolver/LDSymbol.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/raw_ostream.h"

using namespace eld;

LoadInfoReport::LoadInfoReport(const ObjectLinker &ObjLinker,
                               const std::vector<InputAction *> &Actions,
                               DiagnosticEngine &DiagEngine)
    : ObjLinker(ObjLinker), Actions(Actions), DiagEngine(DiagEngine) {
  ArchiveRecordsMap = buildArchiveRecordsMap();
}

LoadInfoReport::ArchiveRecordsMapType LoadInfoReport::buildArchiveRecordsMap() {
  ArchiveRecordsMapType Result;
  for (const auto &Record : ObjLinker.getArchiveRecordsForReport()) {
    if (!Record.Origin)
      continue;
    if (auto *AMI = llvm::dyn_cast<ArchiveMemberInput>(Record.Origin)) {
      ArchiveFile *AF = AMI->getArchiveFile();
      if (AF)
        Result[AF].push_back(Record);
    }
  }
  return Result;
}

llvm::StringRef LoadInfoReport::getTypeName(InputFile::InputFileKind Kind) {
  switch (Kind) {
  case InputFile::ELFObjFileKind:
    return "Object";
  case InputFile::ELFDynObjFileKind:
    return "SharedLibrary";
  case InputFile::GNUArchiveFileKind:
    return "Archive";
  case InputFile::GNULinkerScriptKind:
    return "LinkerScript";
  case InputFile::BitcodeFileKind:
    return "BitcodeFile";
  case InputFile::BinaryFileKind:
    return "BinaryFile";
  case InputFile::ELFExecutableFileKind:
    return "Executable";
  default:
    return "Unknown";
  }
}

llvm::StringRef
LoadInfoReport::getBehaviorOptionName(InputAction::InputActionKind Kind) {
  switch (Kind) {
  case InputAction::AsNeeded:
    return "--as-needed";
  case InputAction::NoAsNeeded:
    return "--no-as-needed";
  case InputAction::WholeArchive:
    return "--whole-archive";
  case InputAction::NoWholeArchive:
    return "--no-whole-archive";
  case InputAction::StartGroup:
    return "--start-group";
  case InputAction::EndGroup:
    return "--end-group";
  case InputAction::StartLib:
    return "--start-lib";
  case InputAction::EndLib:
    return "--end-lib";
  case InputAction::PushState:
    return "--push-state";
  case InputAction::PopState:
    return "--pop-state";
  case InputAction::BStatic:
    return "-Bstatic";
  case InputAction::BDynamic:
    return "-Bdynamic";
  case InputAction::AddNeeded:
    return "--add-needed";
  case InputAction::NoAddNeeded:
    return "--no-add-needed";
  case InputAction::InputFormat:
    return "--format";
  default:
    return "";
  }
}

bool LoadInfoReport::isBehaviorOption(InputAction::InputActionKind Kind) {
  switch (Kind) {
  case InputAction::AsNeeded:
  case InputAction::NoAsNeeded:
  case InputAction::WholeArchive:
  case InputAction::NoWholeArchive:
  case InputAction::StartGroup:
  case InputAction::EndGroup:
  case InputAction::StartLib:
  case InputAction::EndLib:
  case InputAction::PushState:
  case InputAction::PopState:
  case InputAction::BStatic:
  case InputAction::BDynamic:
  case InputAction::AddNeeded:
  case InputAction::NoAddNeeded:
  case InputAction::InputFormat:
    return true;
  default:
    return false;
  }
}

void LoadInfoReport::emitArchiveMembers(llvm::json::Object &Entry,
                                        ArchiveFile *AF) {
  Entry["IncludedCount"] = static_cast<int64_t>(AF->getLoadedMemberCount());
  Entry["TotalCount"] = static_cast<int64_t>(AF->getTotalMemberCount());

  auto It = ArchiveRecordsMap.find(AF);
  if (It == ArchiveRecordsMap.end())
    return;

  llvm::json::Array Members;
  for (const auto &Record : It->second) {
    llvm::json::Object Member;
    if (auto *AMI = llvm::dyn_cast<ArchiveMemberInput>(Record.Origin))
      Member["Name"] = AMI->getMemberName().str();
    else
      Member["Name"] = Record.Origin->getFileName();

    if (Record.Referrer == nullptr && Record.Symbol == nullptr) {
      Member["Reason"] = "WholeArchive";
    } else if (Record.Symbol && Record.Referrer) {
      std::string Reason =
          llvm::formatv("Referred by {0}({1})",
                        Record.Referrer->getInput()->decoratedPath(false),
                        Record.Symbol->name())
              .str();
      Member["Reason"] = Reason;
    } else if (Record.Symbol) {
      Member["Reason"] = Record.Symbol->name();
    }
    Members.push_back(std::move(Member));
  }
  if (!Members.empty())
    Entry["Members"] = std::move(Members);
}

void LoadInfoReport::emitNestedInputs(llvm::json::Object &Entry,
                                      LinkerScriptFile *LSF) {
  llvm::json::Array NestedInputs;
  for (Node *N : LSF->getNodes()) {
    if (auto *FN = llvm::dyn_cast<FileNode>(N)) {
      Input *NestedI = FN->getInput();
      if (!NestedI)
        continue;
      NestedInputs.push_back(emitInputEntry(NestedI, /*IncludeIndex=*/false));
    }
  }
  Entry["NestedInputs"] = std::move(NestedInputs);
}

llvm::json::Object LoadInfoReport::emitInputEntry(Input *I, bool IncludeIndex) {
  llvm::json::Object Entry;
  if (IncludeIndex)
    Entry["Index"] = I->getInputOrdinal() - Input::getInternalInputOrderEnd();
  Entry["Name"] = I->getFileName();
  Entry["ResolvedPath"] = I->getResolvedPath().native();

  InputFile *IF = I->getInputFile();
  assert(IF && "InputFile should not be null for user inputs");

  Entry["Type"] = getTypeName(IF->getKind()).str();

  if (IF->isDynamicLibrary()) {
    auto *DynObj = llvm::cast<ELFDynObjectFile>(IF);
    Entry["SOName"] = DynObj->getSOName();
    Entry["Needed"] = DynObj->isELFNeeded();
  }

  if (IF->isArchive()) {
    auto *AF = llvm::cast<ArchiveFile>(IF);
    emitArchiveMembers(Entry, AF);
  }

  if (IF->isLinkerScript()) {
    auto *LSF = llvm::cast<LinkerScriptFile>(IF);
    emitNestedInputs(Entry, LSF);
  }

  return Entry;
}

bool LoadInfoReport::emitLoadInfoJson(llvm::StringRef Filename) {
  std::error_code EC;
  llvm::raw_fd_ostream OS(Filename, EC);
  if (EC) {
    DiagEngine.raise(Diag::unable_to_write_json_file)
        << Filename << EC.message();
    return false;
  }

  llvm::json::Array Inputs;

  for (InputAction *Action : Actions) {
    InputAction::InputActionKind Kind = Action->getInputActionKind();

    if (isBehaviorOption(Kind)) {
      llvm::StringRef OptName = getBehaviorOptionName(Kind);
      if (!OptName.empty()) {
        llvm::json::Object BehaviorEntry;
        BehaviorEntry["InputBehaviorOption"] = OptName.str();
        Inputs.push_back(std::move(BehaviorEntry));
      }
      continue;
    }

    Input *I = Action->getInput();
    // DefSymAction has no associated Input.
    if (!I)
      continue;

    InputFile *IF = I->getInputFile();
    assert(IF && "InputFile should not be null for user inputs");
    if (IF->isInternal())
      continue;

    Inputs.push_back(emitInputEntry(I, /*IncludeIndex=*/true));
  }

  llvm::json::Object Root;
  Root["LinkerRevision"] = eld::getELDRevision().str();
  Root["Inputs"] = std::move(Inputs);

  OS << llvm::formatv("{0:2}\n", llvm::json::Value(std::move(Root)));
  return true;
}
