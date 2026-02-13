//===- LibReader.cpp-------------------------------------------------------===//
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

#include "eld/Object/LibReader.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Input/Input.h"
#include "eld/Input/InputTree.h"
#include "eld/Object/ObjectLinker.h"
#include "eld/Support/MemoryArea.h"
#include "eld/Support/RegisterTimer.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Object/Archive.h"
#include "llvm/Object/ArchiveWriter.h"

using namespace eld;

LibReader::LibReader(Module &M, ObjectLinker *ObjLinker)
    : MModule(M), MObjLinker(ObjLinker) {}

LibReader::~LibReader() {}

static std::string getStartLibArchiveName(bool IsThin, uint64_t Id) {
  llvm::StringRef Prefix = IsThin ? "<start-lib-thin:" : "<start-lib:";
  return (llvm::Twine(Prefix) + llvm::Twine(Id) + ">").str();
}

bool LibReader::readLib(InputBuilder::InputIteratorT &CurNode,
                        InputBuilder &Builder, LinkerConfig &Config,
                        bool IsPostLtoPhase) {
  LayoutInfo *layoutInfo = MModule.getLayoutInfo();
  Attribute LibAttr;
  bool IsThin = false;
  uint64_t LibId = 0;
  bool ReuseArchiveOnly = false;
  MemoryArea *CachedArchiveArea = nullptr;

  if (layoutInfo)
    layoutInfo->recordInputActions(LayoutInfo::StartLib, nullptr);

  std::vector<llvm::NewArchiveMember> Members;
  llvm::SmallVector<std::string, 0> MemberNames;

  // Build archive members from the in-between inputs.
  while ((*CurNode)->kind() != Node::LibEnd) {
    if (const auto *Start = llvm::dyn_cast<LibStart>(*CurNode)) {
      LibAttr = Start->getAttribute();
      IsThin = Start->isThin();
      LibId = Start->getId();
      if (IsPostLtoPhase) {
        std::string ArchiveName = getStartLibArchiveName(IsThin, LibId);
        CachedArchiveArea =
            Input::getMemoryAreaForPath(ArchiveName, Config.getDiagEngine());
        if (CachedArchiveArea)
          ReuseArchiveOnly = true;
      }
      ++CurNode;
      continue;
    }

    FileNode *Node = llvm::dyn_cast<FileNode>(*CurNode);
    if (!Node) {
      ++CurNode;
      continue;
    }

    if (ReuseArchiveOnly) {
      ++CurNode;
      continue;
    }

    Input *Input = Node->getInput();
    if (!Input->resolvePath(Config))
      return false;

    if (IsThin)
      MemberNames.push_back(Input->getResolvedPath().getFullPath());
    else
      MemberNames.push_back(Input->getResolvedPath().filename().native());
    llvm::NewArchiveMember NM(Input->getMemoryBufferRef());
    NM.MemberName = MemberNames.back();
    Members.push_back(std::move(NM));

    ++CurNode;
  }

  if (layoutInfo)
    layoutInfo->recordInputActions(LayoutInfo::EndLib, nullptr);

  std::string ArchiveName = getStartLibArchiveName(IsThin, LibId);
  MemoryArea *MemArea = CachedArchiveArea;
  if (!MemArea) {
    eld::RegisterTimer T("Build --start-lib archive", "Read all Input files",
                         Config.options().printTimingStats());
    llvm::Expected<std::unique_ptr<llvm::MemoryBuffer>> ExpArchiveBuf =
        llvm::writeArchiveToBuffer(Members,
                                   llvm::SymtabWritingMode::NormalSymtab,
                                   llvm::object::Archive::K_GNU,
                                   /*Deterministic=*/true, /*Thin=*/IsThin);
    if (!ExpArchiveBuf) {
      plugin::DiagnosticEntry DiagEntry =
          Config.getDiagEngine()->convertToDiagEntry(ExpArchiveBuf.takeError());
      Config.raiseDiagEntry(
          std::make_unique<plugin::DiagnosticEntry>(std::move(DiagEntry)));
      return false;
    }
    MemArea = make<MemoryArea>(std::move(*ExpArchiveBuf));
    Input::cacheMemoryAreaForPath(ArchiveName, MemArea);
  }

  auto *ArchiveInput =
      make<Input>(ArchiveName, LibAttr, Config.getDiagEngine(), Input::Default);
  ArchiveInput->setResolvedPath(ArchiveName);
  ArchiveInput->setResolvedPathHash(Input::computeFilePathHash(ArchiveName));
  ArchiveInput->setMemberNameHash(Input::computeFilePathHash(ArchiveName));
  ArchiveInput->setMemArea(MemArea);

  return MObjLinker->readAndProcessInput(ArchiveInput, IsPostLtoPhase);
}
