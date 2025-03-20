//===- ELFExecutableFile.cpp-----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Input/ELFExecutableFile.h"
#include "eld/Input/Input.h"
#include "eld/Input/InputFile.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Support/Memory.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/Support/MemoryBuffer.h"

using namespace eld;

ELFExecutableFile::ELFExecutableFile(Input *I, DiagnosticEngine *DiagEngine)
    : ELFFileBase(I, DiagEngine, InputFile::ELFExecutableFileKind) {
  if (I->getSize())
    Contents = I->getFileContents();
}
