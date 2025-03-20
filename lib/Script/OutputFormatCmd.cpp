//===- OutputFormatCmd.cpp-------------------------------------------------===//
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

#include "eld/Script/OutputFormatCmd.h"
#include "llvm/Support/raw_ostream.h"

using namespace eld;

//===----------------------------------------------------------------------===//
// OutputFormatCmd
//===----------------------------------------------------------------------===//
OutputFormatCmd::OutputFormatCmd(const std::string &PFormat)
    : ScriptCommand(ScriptCommand::OUTPUT_FORMAT) {
  OutputFormatList.push_back(PFormat);
}

OutputFormatCmd::OutputFormatCmd(const std::string &PDefault,
                                 const std::string &PBig,
                                 const std::string &PLittle)
    : ScriptCommand(ScriptCommand::OUTPUT_FORMAT) {
  OutputFormatList.push_back(PDefault);
  OutputFormatList.push_back(PBig);
  OutputFormatList.push_back(PLittle);
}

OutputFormatCmd::~OutputFormatCmd() {}

void OutputFormatCmd::dump(llvm::raw_ostream &Outs) const {
  Outs << "OUTPUT_FORMAT(";
  for (size_t I = 0; I < OutputFormatList.size(); ++I) {
    if (I != 0)
      Outs << ", ";
    Outs << "\"" << OutputFormatList[I] << "\"";
  }
  Outs << ")\n";
}

eld::Expected<void> OutputFormatCmd::activate(Module &CurModule) {
  return eld::Expected<void>();
}
