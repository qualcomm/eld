//===- x86_64Info.cpp------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
#include "x86_64Info.h"
#include "eld/Support/MsgHandling.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/BinaryFormat/ELF.h"

using namespace eld;

#define UNKNOWN -1

std::string x86_64Info::flagString(uint64_t flag) const { return "x86_64"; }

llvm::StringRef x86_64Info::getOutputMCPU() const {
  return m_Config.targets().getTargetCPU();
}

//===----------------------------------------------------------------------===//
// x86_64Info
//===----------------------------------------------------------------------===//
x86_64Info::x86_64Info(LinkerConfig &pConfig) : TargetInfo(pConfig) {
  m_CmdLineFlag = UNKNOWN;
  m_OutputFlag = m_CmdLineFlag;
}

uint64_t x86_64Info::translateFlag(uint64_t pFlag) const { return pFlag; }

bool x86_64Info::isABIFlagSet(uint64_t inputFlag, uint32_t ABIFlag) const {
  return (inputFlag & ABIFlag) == ABIFlag;
}

bool x86_64Info::checkFlags(uint64_t pFlag, const InputFile *pInputFile) const {
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
uint64_t x86_64Info::flags() const { return m_OutputFlag; }

uint8_t x86_64Info::OSABI() const { return llvm::ELF::ELFOSABI_NONE; }

bool x86_64Info::InitializeDefaultMappings(Module &pModule) {
  LinkerScript &pScript = pModule.getScript();
  // FIXME
  if (pScript.linkerScriptHasSectionsCommand() ||
      m_Config.codeGenType() == LinkerConfig::Object)
    return true;
  // These entries will take precedence over platform-independent ones defined
  // later in TargetInfo::InitializeDefaultMappings.
  if (m_Config.options().hasNow() || m_Config.isCodeStatic()) {
    pScript.sectionMap().insert(".got.plt", ".got");
    pScript.sectionMap().insert(".got", ".got");
  }
  TargetInfo::InitializeDefaultMappings(pModule);
  pScript.sectionMap().insert(".sdata.1", ".sdata");
  pScript.sectionMap().insert(".sdata.2", ".sdata");
  pScript.sectionMap().insert(".sdata.4", ".sdata");
  pScript.sectionMap().insert(".sdata.8", ".sdata");
  pScript.sectionMap().insert(".sdata*", ".sdata");
  pScript.sectionMap().insert(".sbss.1", ".sbss");
  pScript.sectionMap().insert(".sbss.2", ".sbss");
  pScript.sectionMap().insert(".sbss.4", ".sbss");
  pScript.sectionMap().insert(".sbss.8", ".sbss");
  pScript.sectionMap().insert(".sbss*", ".sbss");
  return true;
}
