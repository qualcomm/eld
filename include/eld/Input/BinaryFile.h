//===- BinaryFile.h--------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
#ifndef ELD_INPUT_BINARYFILE_H
#define ELD_INPUT_BINARYFILE_H

#include "eld/Input/ObjectFile.h"

namespace eld {

/// Input files that are read by the linker in binary input format are stored as
/// BinaryFile. Binary input format here means that linker considers the
/// binary format input file as a block of data that is put verbatim into the
/// output file.
class BinaryFile : public ObjectFile {
public:
  BinaryFile(Input *I, DiagnosticEngine *diagEngine);

  /// Casting support.
  static bool classof(const InputFile *I);
};

} // namespace eld

#endif // ELD_INPUT_BINARYFILE_H
