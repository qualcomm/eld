//===- RISCVInfo.h---------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_TARGET_RISCV_GNU_INFO_H
#define ELD_TARGET_RISCV_GNU_INFO_H
#include "eld/Config/TargetOptions.h"
#include "eld/Core/Module.h"
#include "eld/Target/TargetInfo.h"
#include "llvm/BinaryFormat/ELF.h"

namespace eld {

class RISCVInfo : public TargetInfo {
public:
  RISCVInfo(LinkerConfig &m_Config);

  uint32_t machine() const override { return llvm::ELF::EM_RISCV; }

  std::string getMachineStr() const override { return "RISC-V"; }

  /// flags - the value of ElfXX_Ehdr::e_flags
  uint64_t flags() const override;

  uint8_t OSABI() const override;

  bool checkFlags(uint64_t flag, const InputFile *pInputFile) const override;

  std::string flagString(uint64_t pFlag) const override;

  int32_t cmdLineFlag() const override { return m_CmdLineFlag; }

  int32_t outputFlag() const override { return m_OutputFlag; }

  bool needEhdr(Module &pModule, bool linkerScriptHasSectionsCmd,
                bool isPhdr) override {
    if (pModule.getScript().linkerScriptHasSectionsCommand())
      return false;
    return true;
  }

  bool processNoteGNUSTACK() override { return false; }

  llvm::StringRef getOutputMCPU() const override;

  bool InitializeDefaultMappings(Module &pModule) override;

  bool isCompatible(uint64_t pFlag, const std::string &pFile) const;

  bool recordEFlag(InputFile *, uint32_t Flag);

private:
  bool isABIFlagSet(uint64_t inputFlag, uint32_t ABIFlag) const;

private:
  uint64_t translateFlag(uint64_t pFlag) const;
  int32_t m_CmdLineFlag;
  mutable int64_t m_OutputFlag;
  mutable llvm::DenseMap<const InputFile *, uint64_t> InputFlags;
};

} // namespace eld

#endif
