//===- InternalInputFile.h-------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
#ifndef ELD_INPUT_INTERNALINPUTFILE_H
#define ELD_INPUT_INTERNALINPUTFILE_H

#include "eld/Input/ObjectFile.h"

namespace eld {

class InternalInputFile : public ObjectFile {
public:
  InternalInputFile(Input *I, DiagnosticEngine *diagEngine)
      : ObjectFile(I, InputFile::InternalInputKind, diagEngine) {
    setUsed(true);
  }

  /// Casting support.
  static bool classof(const InputFile *I) {
    return I->getKind() == InputFile::InternalInputKind;
  }
};

} // namespace eld

#endif // ELD_INPUT_INTERNALINPUTFILE_H
