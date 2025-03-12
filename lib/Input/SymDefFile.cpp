//===- SymDefFile.cpp------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Input/SymDefFile.h"
#include "eld/Input/Input.h"
#include "eld/Input/InputFile.h"

using namespace eld;

SymDefFile::SymDefFile(Input *I, DiagnosticEngine *diagEngine)
    : InputFile(I, diagEngine, InputFile::ELFSymDefFileKind) {
  if (I->getSize())
    Contents = I->getFileContents();
}
