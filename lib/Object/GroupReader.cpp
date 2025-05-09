//===- GroupReader.cpp-----------------------------------------------------===//
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

#include "eld/Object/GroupReader.h"
#include "eld/Input/LinkerScriptFile.h"
#include "eld/Object/ObjectLinker.h"
#include "eld/Readers/ArchiveParser.h"
#include "eld/Support/RegisterTimer.h"

using namespace eld;

GroupReader::GroupReader(Module &PModule, ObjectLinker *O)
    : MModule(PModule), MObjLinker(O) {}

GroupReader::~GroupReader() {}

bool GroupReader::readGroup(InputBuilder::InputIteratorT &CurNode,
                            InputBuilder &PBuilder, LinkerConfig &PConfig,
                            bool IsPostLtoPhase) {
  LayoutInfo *layoutInfo = MModule.getLayoutInfo();

  Module::LibraryList &ArchiveLibraryList = MModule.getArchiveLibraryList();
  size_t ArchiveLibraryListSize = ArchiveLibraryList.size();

  if (layoutInfo) {
    layoutInfo->recordInputActions(LayoutInfo::StartGroup, nullptr);
    layoutInfo->recordGroup();
  }

  std::vector<ArchiveFile *> Archives;

  // first time read the sub-tree
  while ((*CurNode)->kind() != Node::GroupEnd) {

    FileNode *Node = llvm::dyn_cast<FileNode>(*CurNode);
    if (!Node) {
      ++CurNode;
      continue;
    }
    Input *Input = Node->getInput();
    // Resolve the path.
    if (!Input->resolvePath(PConfig))
      return false;

    if (!MObjLinker->readAndProcessInput(Input, IsPostLtoPhase))
      return false;

    if (Input->getInputFile()->getKind() == InputFile::GNULinkerScriptKind) {
      if (!MObjLinker->readInputs(
              llvm::dyn_cast<eld::LinkerScriptFile>(Input->getInputFile())
                  ->getNodes()))
        return false;
    }

    ++CurNode;
  }

  size_t CurNamePoolSize = 0;
  size_t NewSize = 0;
  // Traverse all archives in the group.
  do {
    CurNamePoolSize = MModule.getNamePool().getNumGlobalSize();
    for (Module::lib_iterator
             It = ArchiveLibraryList.begin() + ArchiveLibraryListSize,
             Ie = ArchiveLibraryList.end();
         It != Ie; ++It) {
      if ((*It)->getInput()->getAttribute().isWholeArchive())
        continue;
      eld::Expected<uint32_t> ExpNumObjects =
          MObjLinker->getArchiveParser()->parseFile(**It);
      if (!ExpNumObjects) {
        PConfig.raise(Diag::error_read_archive)
            << (*It)->getInput()->decoratedPath();
        PConfig.raiseDiagEntry(std::move(ExpNumObjects.error()));
        return false;
      }
    }
    NewSize = MModule.getNamePool().getNumGlobalSize();
    if (layoutInfo)
      layoutInfo->recordGroup();
  } while (CurNamePoolSize != NewSize);

  if (layoutInfo)
    layoutInfo->recordInputActions(LayoutInfo::EndGroup, nullptr);

  return true;
}
