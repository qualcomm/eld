//===- BinaryFile.cpp------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Input/BinaryFile.h"
#include "eld/Input/Input.h"

using namespace eld;

BinaryFile::BinaryFile(Input *I, DiagnosticEngine *diagEngine)
    : ObjectFile(I, InputFile::Kind::BinaryFileKind, diagEngine) {
  if (I->getSize())
    Contents = I->getFileContents();
}

bool BinaryFile::classof(const InputFile *I) { return I->isBinaryFile(); }