//===- ELFDynObjectFile.cpp------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Input/ELFDynObjectFile.h"
#include "eld/Support/Memory.h"

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
