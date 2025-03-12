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
OutputArchCmd::OutputArchCmd(const std::string &pArch)
    : ScriptCommand(ScriptCommand::OUTPUT_ARCH), m_Arch(pArch) {}

OutputArchCmd::~OutputArchCmd() {}

void OutputArchCmd::dump(llvm::raw_ostream &outs) const {
  outs << "OUTPUT_ARCH(" << m_Arch << ")\n";
}

eld::Expected<void> OutputArchCmd::activate(Module &pModule) {
  return eld::Expected<void>();
}
