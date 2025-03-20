//===- OutputArchCmd.cpp---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Script/OutputArchCmd.h"
#include "llvm/Support/raw_ostream.h"

using namespace eld;

//===----------------------------------------------------------------------===//
// OutputArchCmd
//===----------------------------------------------------------------------===//
OutputArchCmd::OutputArchCmd(const std::string &PArch)
    : ScriptCommand(ScriptCommand::OUTPUT_ARCH), OutputArch(PArch) {}

OutputArchCmd::~OutputArchCmd() {}

void OutputArchCmd::dump(llvm::raw_ostream &Outs) const {
  Outs << "OUTPUT_ARCH(" << OutputArch << ")\n";
}

eld::Expected<void> OutputArchCmd::activate(Module &CurModule) {
  return eld::Expected<void>();
}
