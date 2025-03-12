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
OutputFormatCmd::OutputFormatCmd(const std::string &pFormat)
    : ScriptCommand(ScriptCommand::OUTPUT_FORMAT) {
  m_FormatList.push_back(pFormat);
}

OutputFormatCmd::OutputFormatCmd(const std::string &pDefault,
                                 const std::string &pBig,
                                 const std::string &pLittle)
    : ScriptCommand(ScriptCommand::OUTPUT_FORMAT) {
  m_FormatList.push_back(pDefault);
  m_FormatList.push_back(pBig);
  m_FormatList.push_back(pLittle);
}

OutputFormatCmd::~OutputFormatCmd() {}

void OutputFormatCmd::dump(llvm::raw_ostream &outs) const {
  outs << "OUTPUT_FORMAT(";
  for (size_t i = 0; i < m_FormatList.size(); ++i) {
    if (i != 0)
      outs << ", ";
    outs << "\"" << m_FormatList[i] << "\"";
  }
  outs << ")\n";
}

eld::Expected<void> OutputFormatCmd::activate(Module &pModule) {
  return eld::Expected<void>();
}
