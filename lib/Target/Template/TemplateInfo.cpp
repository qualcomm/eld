//===- TemplateInfo.cpp----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
#include "TemplateInfo.h"
#include "eld/Config/Config.h"
#include "eld/Support/MsgHandling.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSwitch.h"

using namespace eld;

#define UNKNOWN -1

const char *TemplateInfo::flagString(uint64_t flag) const { return ""; }

llvm::StringRef TemplateInfo::getOutputMCPU() const { return "Template"; }

//===----------------------------------------------------------------------===//
// TemplateInfo
//===----------------------------------------------------------------------===//
TemplateInfo::TemplateInfo(LinkerConfig &pConfig) : TargetInfo(pConfig) {
  m_OutputFlag = m_CmdLineFlag;
}

uint64_t TemplateInfo::translateFlag(uint64_t pFlag) const { return pFlag; }

bool TemplateInfo::checkFlags(uint64_t pFlag, const std::string &name) {
  // Choose the default architecture from the input files, only if mcpu option
  // is not specified on the command line.
  if ((m_CmdLineFlag == UNKNOWN) && (m_OutputFlag == UNKNOWN)) {
    if (m_OutputFlag < (int64_t)pFlag)
      m_OutputFlag = pFlag;
  }

  // FIXME: Check for compatibility about other versions.
  if (m_OutputFlag < (int64_t)pFlag)
    m_OutputFlag = pFlag;

  return true;
}

/// flags - the value of ElfXX_Ehdr::e_flags
uint64_t TemplateInfo::flags() const {
  // FIXME: Add proper code.
  return m_OutputFlag;
}

uint8_t TemplateInfo::OSABI() const { return llvm::ELF::ELFOSABI_NONE; }
