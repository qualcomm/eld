//===- RISCVInfo.cpp-------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "RISCVInfo.h"
#include "eld/Config/Config.h"
#include "eld/Support/MsgHandling.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/BinaryFormat/ELF.h"

using namespace eld;

#define UNKNOWN -1

std::string RISCVInfo::flagString(uint64_t flag) const {
#define INTOEFLAGSTR(ns, enum, str)                                            \
  if ((flag & ns::enum) == ns::enum) {                                         \
    if (!flagStr.empty())                                                      \
      flagStr += "|";                                                          \
    flagStr += str;                                                            \
  }
  std::string flagStr;
  INTOEFLAGSTR(llvm::ELF, EF_RISCV_RVC, "RVC");
  INTOEFLAGSTR(llvm::ELF, EF_RISCV_FLOAT_ABI_SINGLE, "FloatABISingle");
  INTOEFLAGSTR(llvm::ELF, EF_RISCV_FLOAT_ABI_DOUBLE, "FloatABIDouble");
  INTOEFLAGSTR(llvm::ELF, EF_RISCV_FLOAT_ABI_QUAD, "FloatABIQuad");
  INTOEFLAGSTR(llvm::ELF, EF_RISCV_RVE, "RVE");
  return flagStr;
}

llvm::StringRef RISCVInfo::getOutputMCPU() const {
  return m_Config.targets().getTargetCPU();
}

//===----------------------------------------------------------------------===//
// RISCVInfo
//===----------------------------------------------------------------------===//
RISCVInfo::RISCVInfo(LinkerConfig &pConfig) : TargetInfo(pConfig) {
  m_CmdLineFlag = UNKNOWN;
  m_OutputFlag = m_CmdLineFlag;
}

uint64_t RISCVInfo::translateFlag(uint64_t pFlag) const { return pFlag; }

bool RISCVInfo::isABIFlagSet(uint64_t inputFlag, uint32_t ABIFlag) const {
  if ((inputFlag & ABIFlag) == ABIFlag)
    return true;
  return false;
}

bool RISCVInfo::isCompatible(uint64_t pFlag, const std::string &pFile) const {
  if (isABIFlagSet(pFlag, llvm::ELF::EF_RISCV_FLOAT_ABI_SOFT) ^
      isABIFlagSet(m_OutputFlag, llvm::ELF::EF_RISCV_FLOAT_ABI_SOFT)) {
    m_Config.raise(diag::incompatible_architecture_versions)
        << flagString(pFlag) << pFile << flagString(m_OutputFlag);
    return false;
  }
  if (isABIFlagSet(pFlag, llvm::ELF::EF_RISCV_FLOAT_ABI_SINGLE) ^
      isABIFlagSet(m_OutputFlag, llvm::ELF::EF_RISCV_FLOAT_ABI_SINGLE)) {
    m_Config.raise(diag::incompatible_architecture_versions)
        << flagString(pFlag) << pFile << flagString(m_OutputFlag);
    return false;
  }
  if (isABIFlagSet(pFlag, llvm::ELF::EF_RISCV_FLOAT_ABI_DOUBLE) ^
      isABIFlagSet(m_OutputFlag, llvm::ELF::EF_RISCV_FLOAT_ABI_DOUBLE)) {
    m_Config.raise(diag::incompatible_architecture_versions)
        << flagString(pFlag) << pFile << flagString(m_OutputFlag);
    return false;
  }
  if (isABIFlagSet(pFlag, llvm::ELF::EF_RISCV_FLOAT_ABI_QUAD) ^
      isABIFlagSet(m_OutputFlag, llvm::ELF::EF_RISCV_FLOAT_ABI_QUAD)) {
    m_Config.raise(diag::incompatible_architecture_versions)
        << flagString(pFlag) << pFile << flagString(m_OutputFlag);
    return false;
  }
  if ((pFlag & llvm::ELF::EF_RISCV_RVE) ^
      (m_OutputFlag & llvm::ELF::EF_RISCV_RVE)) {
    m_Config.raise(diag::incompatible_architecture_versions)
        << flagString(pFlag) << pFile << flagString(m_OutputFlag);
    return false;
  }
  return true;
}

bool RISCVInfo::checkFlags(uint64_t flag, const InputFile *pInputFile) const {
  // Choose the default architecture from the input files, only if mcpu option
  // is not specified on the command line.
  if ((m_CmdLineFlag == UNKNOWN) && (m_OutputFlag == UNKNOWN))
    m_OutputFlag = flag;

  if (!isCompatible(flag, pInputFile->getInput()->decoratedPath()))
    return false;

  m_OutputFlag |= flag;

  InputFlags[pInputFile] = flag;

  return true;
}

/// flags - the value of ElfXX_Ehdr::e_flags
uint64_t RISCVInfo::flags() const { return m_OutputFlag; }

uint8_t RISCVInfo::OSABI() const { return llvm::ELF::ELFOSABI_NONE; }

bool RISCVInfo::InitializeDefaultMappings(Module &pModule) {
  LinkerScript &pScript = pModule.getScript();

  if (pScript.linkerScriptHasSectionsCommand() ||
      m_Config.codeGenType() == LinkerConfig::Object)
    return true;
  // These entries will take precedence over platform-independent ones defined
  // later in TargetInfo::InitializeDefaultMappings.
  if (m_Config.options().hasNow() || m_Config.isCodeStatic()) {
    pScript.sectionMap().insert(".got", ".got");
    pScript.sectionMap().insert(".got.plt", ".got");
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
