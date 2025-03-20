//===- SymDefWriter.h------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
//
//                     The MCLinker Project
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#ifndef ELD_WRITERS_SYMDEFWRITER_H
#define ELD_WRITERS_SYMDEFWRITER_H

#include "eld/Config/LinkerConfig.h"
#include "llvm/Support/FileOutputBuffer.h"
#include <system_error>

namespace eld {

class Module;
class LinkerConfig;

class SymDefWriter {
public:
  SymDefWriter(LinkerConfig &Config);

  eld::Expected<void> init();

  ~SymDefWriter();

  llvm::raw_ostream &outputStream() const;

  std::error_code writeSymDef(Module &CurModule);

private:
  void addHeader();

  LinkerConfig &Config;
  llvm::raw_fd_ostream *SymDefFile;
};

} // namespace eld

#endif
