//===- SymDefFile.h--------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
#ifndef ELD_INPUT_SYMDEFFILE_H
#define ELD_INPUT_SYMDEFFILE_H

#include "eld/Input/InputFile.h"

namespace eld {

class SymDefFile : public InputFile {
public:
  SymDefFile(Input *I, DiagnosticEngine *DiagEngine);

  /// Casting support.
  static bool classof(const InputFile *I) {
    return I->getKind() == ELFSymDefFileKind;
  }
};

} // namespace eld

#endif // ELD_INPUT_SYMDEFFILE_H
