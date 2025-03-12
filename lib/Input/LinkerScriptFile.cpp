//===- LinkerScriptFile.cpp------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Input/LinkerScriptFile.h"
#include "eld/Input/Input.h"

using namespace eld;

LinkerScriptFile::LinkerScriptFile(Input *I, DiagnosticEngine *diagEngine)
    : InputFile(I, diagEngine, InputFile::GNULinkerScriptKind) {
  if (I->getSize())
    Contents = I->getFileContents();
}
