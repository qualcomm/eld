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

GroupReader::GroupReader(Module &pModule, ObjectLinker *O)
    : m_Module(pModule), m_ObjLinker(O) {}

GroupReader::~GroupReader() {}

bool GroupReader::readGroup(InputBuilder::InputIteratorT &curNode,
                            InputBuilder &pBuilder, LinkerConfig &pConfig,
                            bool isPostLTOPhase) {
  LayoutPrinter *printer = m_Module.getLayoutPrinter();

  Module::LibraryList &ArchiveLibraryList = m_Module.getArchiveLibraryList();
  size_t ArchiveLibraryListSize = ArchiveLibraryList.size();

  if (printer) {
    printer->recordInputActions(LayoutPrinter::StartGroup, nullptr);
    printer->recordGroup();
  }

  std::vector<ArchiveFile *> archives;

  // first time read the sub-tree
  while ((*curNode)->kind() != Node::GroupEnd) {

    FileNode *node = llvm::dyn_cast<FileNode>(*curNode);
    if (!node) {
      ++curNode;
      continue;
    }
    Input *input = node->getInput();
    // Resolve the path.
    if (!input->resolvePath(pConfig))
      return false;

    if (!m_ObjLinker->readAndProcessInput(input, isPostLTOPhase))
      return false;

    if (input->getInputFile()->getKind() == InputFile::GNULinkerScriptKind) {
      if (!m_ObjLinker->readInputs(
              llvm::dyn_cast<eld::LinkerScriptFile>(input->getInputFile())
                  ->getNodes()))
        return false;
    }

    ++curNode;
  }

  size_t cur_name_pool_size = 0;
  size_t new_size = 0;
  // Traverse all archives in the group.
  do {
    cur_name_pool_size = m_Module.getNamePool().getNumGlobalSize();
    for (Module::lib_iterator
             It = ArchiveLibraryList.begin() + ArchiveLibraryListSize,
             Ie = ArchiveLibraryList.end();
         It != Ie; ++It) {
      if ((*It)->getInput()->getAttribute().isWholeArchive())
        continue;
      eld::Expected<uint32_t> expNumObjects =
          m_ObjLinker->getArchiveParser()->parseFile(**It);
      if (!expNumObjects) {
        pConfig.raise(diag::error_read_archive)
            << (*It)->getInput()->decoratedPath();
        pConfig.raiseDiagEntry(std::move(expNumObjects.error()));
        return false;
      }
    }
    new_size = m_Module.getNamePool().getNumGlobalSize();
    if (printer)
      printer->recordGroup();
  } while (cur_name_pool_size != new_size);

  if (printer)
    printer->recordInputActions(LayoutPrinter::EndGroup, nullptr);

  return true;
}
