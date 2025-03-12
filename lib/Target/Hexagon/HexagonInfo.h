//===- HexagonInfo.h-------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_TARGET_HEXAGON_GNU_INFO_H
#define ELD_TARGET_HEXAGON_GNU_INFO_H
#include "eld/Config/TargetOptions.h"
#include "eld/Target/TargetInfo.h"
#include "llvm/BinaryFormat/ELF.h"

#define HEXAGON_JUMP_INSTRUCTION 0x5800c000

namespace eld {

class HexagonInfo : public TargetInfo {
public:
  // This enumeration creates a mapping in the static array for every input
  // flag.
  enum TranslationCPU {
    LINK_UNKNOWN = -1,
    LINK_V68 = 0,
    LINK_V69,
    LINK_V71,
    LINK_V71T,
    LINK_V73,
    LINK_V75,
    LINK_V77,
    LINK_V79,
    LINK_V81,
    LINK_V83,
    LINK_V85,
    LAST_ISA
  };

  enum ArchSupport : uint8_t {
    DeprecatedAndNoSupportExists,
    Deprecated,
    Supported,
    NotSupported,
  };

  HexagonInfo(LinkerConfig &pConfig);

  uint32_t machine() const override { return llvm::ELF::EM_HEXAGON; }

  std::string getMachineStr() const override { return "Hexagon"; }

  /// flags - the value of ElfXX_Ehdr::e_flags
  uint64_t flags() const override;

  uint8_t OSABI() const override;

  bool checkFlags(uint64_t flag, const InputFile *pInputFile) const override;

  std::string flagString(uint64_t pFlag) const override;

  int32_t cmdLineFlag() const override { return m_CmdLineFlag; }

  int32_t outputFlag() const override { return m_OutputFlag; }

  uint64_t abiPageSize(bool linkerScriptHasSectionsCommand) const override;

  bool needEhdr(Module &pModule, bool linkerScriptHasSectionsCommand,
                bool isPhdr) override {
    return false & isPhdr;
  }

  bool processNoteGNUSTACK() override { return false; }

  llvm::StringRef getOutputMCPU() const override;

  TargetOptions &targetOptions() const { return m_Config.targets(); }

  bool InitializeDefaultMappings(Module &pModule) override;

  bool initialize() override;

private:
  uint64_t translateFlag(uint64_t pFlag) const;

  ArchSupport getArchSupport(uint64_t pFlag) const;

  uint64_t getLowestSupportedArch() const;

  std::string getArchStr(uint64_t pFlag) const;

private:
  int32_t m_CmdLineFlag;
  mutable int32_t m_OutputFlag;
};

} // namespace eld

#endif
