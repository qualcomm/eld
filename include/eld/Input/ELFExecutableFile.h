//===- ELFExecutableFile.h-------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_INPUT_ELFEXECUTABLEFILE_H
#define ELD_INPUT_ELFEXECUTABLEFILE_H

#include "eld/Input/ELFFileBase.h"

namespace eld {

/** \class ELFExecutableFile
 *  \brief ELFExecutableFile represents a static executable
 *  that the rest of the linker can work with.
 */
class ELFExecutableFile : public ELFFileBase {
public:
  ELFExecutableFile(Input *I, DiagnosticEngine *DiagEngine);

  /// Casting support.
  static bool classof(const InputFile *E) {
    return (E->getKind() == InputFile::ELFExecutableFileKind);
  }
};

} // namespace eld

#endif // ELD_INPUT_ELFEXECUTABLEFILE_H
