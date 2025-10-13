//===- ELFFileBase.cpp-----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Input/ELFFileBase.h"
#include "eld/Readers/ELFSection.h"

using namespace eld;

ELFSection *ELFFileBase::getELFSection(uint32_t Index) {
  Section *S = getSection(Index);
  if (!S)
    return nullptr;
  return llvm::dyn_cast<eld::ELFSection>(S);
}

void ELFFileBase::addSection(ELFSection *S) {
  if (S->isRelocationKind())
    RelocationSections.push_back(S);
  S->setIndex(MSectionTable.size());
  ObjectFile::addSection(S);
}

llvm::StringRef ELFFileBase::getStringTableData() const {
  if (StringTable)
    return StringTable->getContents();
  return llvm::StringRef();
}