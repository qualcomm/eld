//===- ELFDynObjectFile.cpp------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Input/ELFDynObjectFile.h"
#include "eld/Support/Memory.h"
#ifdef ELD_ENABLE_SYMBOL_VERSIONING
#include "eld/Readers/ELFSection.h"
#endif

using namespace eld;

ELFDynObjectFile::ELFDynObjectFile(Input *I, DiagnosticEngine *DiagEngine)
    : ELFFileBase(I, DiagEngine, InputFile::ELFDynObjFileKind) {
  if (I->getSize())
    Contents = I->getFileContents();
}

bool ELFDynObjectFile::isELFNeeded() {
  if (!getInput()->getAttribute().isAsNeeded())
    return true;
  return isUsed();
}

std::string ELFDynObjectFile::getFallbackSOName() const {
  Input *I = getInput();
  if (!getInput()->isNamespec())
    return I->getResolvedPath().native();
  const auto &filename = I->getFileName();
  if (filename[0] == ':')
    return filename.substr(1);
  return I->getResolvedPath().filename().native();
}

#ifdef ELD_ENABLE_SYMBOL_VERSIONING
llvm::StringRef ELFDynObjectFile::getDynStringTable() const {
  if (DynStrTabSection)
    return DynStrTabSection->getContents();
  return llvm::StringRef();
}
#endif