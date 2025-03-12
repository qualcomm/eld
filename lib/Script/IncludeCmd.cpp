//===- IncludeCmd.cpp------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Script/IncludeCmd.h"
#include "llvm/Support/Casting.h"

using namespace eld;

//===----------------------------------------------------------------------===//
// IncludeCmd
//===----------------------------------------------------------------------===//
IncludeCmd::IncludeCmd(const std::string FileName, bool isOptional)
    : ScriptCommand(ScriptCommand::INCLUDE), FileName(FileName),
      m_isOptional(isOptional) {}

void IncludeCmd::dump(llvm::raw_ostream &outs) const {
  if (m_isOptional)
    outs << "INCLUDE_OPTIONAL";
  else
    outs << "INCLUDE";
  outs << " ";
  outs << FileName;
  outs << "\n";
}

void IncludeCmd::dumpOnlyThis(llvm::raw_ostream &outs) const {
  if (m_isOptional)
    outs << "INCLUDE_OPTIONAL";
  else
    outs << "INCLUDE";
  outs << " ";
  outs << FileName;
}

eld::Expected<void> IncludeCmd::activate(Module &pModule) {
  return eld::Expected<void>();
}
