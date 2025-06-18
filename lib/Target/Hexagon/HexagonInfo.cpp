//===- HexagonInfo.cpp-----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "HexagonInfo.h"
#include "eld/Config/Config.h"
#include "eld/Core/Module.h"
#include "eld/Support/MsgHandling.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/BinaryFormat/ELF.h"

using namespace eld;

enum CompatibilityAction {
  NS = 1, // unsupported ISA
  WA = 2, // warn we are mixing two compatible ISA
  OK = 3, // ISA match, so nothing wrong
  ER = 4  // mixing erroneous ISA
};

// Compatibility array for flags in hexagon.
// Rows indicate the command line ISA provided and column
// indicate the object file ISA

// clang-format off
static const enum CompatibilityAction
    flag_action[eld::HexagonInfo::LAST_ISA][eld::HexagonInfo::LAST_ISA] =
{ //          V68 V69 V71 V71t V73 V75 V77 v79 v81 v83 v85
 /* V68   */ {OK, OK, OK, WA,   OK, OK, OK, OK, OK, OK, OK},
 /* V69   */ {OK, OK, OK, WA,   OK, OK, OK, OK, OK, OK, OK},
 /* V71   */ {OK, OK, OK, WA,   OK, OK, OK, OK, OK, OK, OK},
 /* V71t  */ {WA, WA, WA, OK,   WA, WA, WA, WA, WA, WA, WA},
 /* V73   */ {OK, OK, OK, WA,   OK, OK, OK, OK, OK, OK, OK},
 /* V75   */ {OK, OK, OK, WA,   OK, OK, OK, OK, OK, OK, OK},
 /* V77   */ {OK, OK, OK, WA,   OK, OK, OK, OK, OK, OK, OK},
 /* V79   */ {OK, OK, OK, WA,   OK, OK, OK, OK, OK, OK, OK},
 /* V81   */ {OK, OK, OK, WA,   OK, OK, OK, OK, OK, OK, OK},
 /* V83   */ {OK, OK, OK, WA,   OK, OK, OK, OK, OK, OK, OK},
 /* V85   */ {OK, OK, OK, WA,   OK, OK, OK, OK, OK, OK, OK},
 /* V87   */ {OK, OK, OK, WA,   OK, OK, OK, OK, OK, OK, OK},
 /* V89   */ {OK, OK, OK, WA,   OK, OK, OK, OK, OK, OK, OK},
};
// clang-format on

static const char *ISAs[eld::HexagonInfo::LAST_ISA] = {
    "v68", "v69", "v71", "v71t", "v73", "v75",
    "v77", "v79", "v81", "v83",  "v85", "v87", "v89"};

static const char *MCPUs[eld::HexagonInfo::LAST_ISA] = {
    "hexagonv68", "hexagonv69", "hexagonv71", "hexagonv71t",
    "hexagonv73", "hexagonv75", "hexagonv77", "hexagonv79",
    "hexagonv81", "hexagonv83", "hexagonv85", "hexagonv87",
    "hexagonv89",
};

static const uint32_t ISAsToEFlag[eld::HexagonInfo::LAST_ISA] = {
    llvm::ELF::EF_HEXAGON_MACH_V68, llvm::ELF::EF_HEXAGON_MACH_V69,
    llvm::ELF::EF_HEXAGON_MACH_V71, llvm::ELF::EF_HEXAGON_MACH_V71T,
    llvm::ELF::EF_HEXAGON_MACH_V73, llvm::ELF::EF_HEXAGON_MACH_V75,
    llvm::ELF::EF_HEXAGON_MACH_V77, llvm::ELF::EF_HEXAGON_MACH_V79,
    llvm::ELF::EF_HEXAGON_MACH_V81, llvm::ELF::EF_HEXAGON_MACH_V83,
    llvm::ELF::EF_HEXAGON_MACH_V85, llvm::ELF::EF_HEXAGON_MACH_V87,
    llvm::ELF::EF_HEXAGON_MACH_V89,
};

std::string HexagonInfo::flagString(uint64_t flag) const {
  return getArchStr(flag);
}

llvm::StringRef HexagonInfo::getOutputMCPU() const {
  return MCPUs[translateFlag(flags())];
}

//===----------------------------------------------------------------------===//
// HexagonInfo
//===----------------------------------------------------------------------===//
HexagonInfo::HexagonInfo(LinkerConfig &pConfig)
    : TargetInfo(pConfig), m_OutputFlag(LINK_UNKNOWN) {}

bool HexagonInfo::initialize() {
  m_CmdLineFlag = LINK_UNKNOWN;
  if (targetOptions().getTargetCPU().empty())
    return true;

  // No support for deprecated architectures in the command line.
  m_CmdLineFlag = llvm::StringSwitch<uint64_t>(targetOptions().getTargetCPU())
                      .Case("hexagonv68", llvm::ELF::EF_HEXAGON_MACH_V68)
                      .Case("hexagonv69", llvm::ELF::EF_HEXAGON_MACH_V69)
                      .Case("hexagonv71", llvm::ELF::EF_HEXAGON_MACH_V71)
                      .Case("hexagonv71t", llvm::ELF::EF_HEXAGON_MACH_V71T)
                      .Case("hexagonv73", llvm::ELF::EF_HEXAGON_MACH_V73)
                      .Case("hexagonv75", llvm::ELF::EF_HEXAGON_MACH_V75)
                      .Case("hexagonv77", llvm::ELF::EF_HEXAGON_MACH_V77)
                      .Case("hexagonv79", llvm::ELF::EF_HEXAGON_MACH_V79)
                      .Case("hexagonv81", llvm::ELF::EF_HEXAGON_MACH_V81)
                      .Case("hexagonv83", llvm::ELF::EF_HEXAGON_MACH_V83)
                      .Case("hexagonv85", llvm::ELF::EF_HEXAGON_MACH_V85)
                      .Case("hexagonv87", llvm::ELF::EF_HEXAGON_MACH_V87)
                      .Case("hexagonv89", llvm::ELF::EF_HEXAGON_MACH_V89)
                      .Default(LINK_UNKNOWN);
  if (m_CmdLineFlag == -1) {
    m_Config.raise(Diag::fatal_unsupported_emulation)
        << targetOptions().getTargetCPU();
    return false;
  }

  m_OutputFlag = m_CmdLineFlag;

  return true;
}

uint64_t HexagonInfo::translateFlag(uint64_t pFlag) const {
  // Only bits [11:0] are used for flags.
  pFlag &= 0xFFFF;
  std::string pflagStr = getArchStr(pFlag);
  switch (pFlag) {
  case LINK_V68:
  case llvm::ELF::EF_HEXAGON_MACH_V68:
    return LINK_V68;
  case LINK_V69:
  case llvm::ELF::EF_HEXAGON_MACH_V69:
    return LINK_V69;
  case LINK_V71:
  case llvm::ELF::EF_HEXAGON_MACH_V71:
    return LINK_V71;
  case LINK_V71T:
  case llvm::ELF::EF_HEXAGON_MACH_V71T:
    return LINK_V71T;
  case LINK_V73:
  case llvm::ELF::EF_HEXAGON_MACH_V73:
    return LINK_V73;
  case LINK_V75:
  case llvm::ELF::EF_HEXAGON_MACH_V75:
    return LINK_V75;
  case llvm::ELF::EF_HEXAGON_MACH_V77:
    return LINK_V77;
  case llvm::ELF::EF_HEXAGON_MACH_V79:
    return LINK_V79;
  case llvm::ELF::EF_HEXAGON_MACH_V81:
    return LINK_V81;
  case llvm::ELF::EF_HEXAGON_MACH_V83:
    return LINK_V83;
  case llvm::ELF::EF_HEXAGON_MACH_V85:
    return LINK_V85;
  case llvm::ELF::EF_HEXAGON_MACH_V87:
    return LINK_V87;
  case llvm::ELF::EF_HEXAGON_MACH_V89:
    return LINK_V89;
  default:
    ASSERT(0, llvm::Twine("Unknown flag " + pflagStr).str().c_str());
  }
}

std::string HexagonInfo::getArchStr(uint64_t pFlag) const {
  pFlag &= 0XFFFF;
  switch (pFlag) {
  case llvm::ELF::EF_HEXAGON_MACH_V2:
    return "hexagonv2";
  case llvm::ELF::EF_HEXAGON_MACH_V3:
    return "hexagonv3";
  case llvm::ELF::EF_HEXAGON_MACH_V4:
    return "hexagonv4";
  case llvm::ELF::EF_HEXAGON_MACH_V5:
    return "hexagonv5";
  case llvm::ELF::EF_HEXAGON_MACH_V55:
    return "hexagonv55";
  case llvm::ELF::EF_HEXAGON_MACH_V60:
    return "hexagonv60";
  case llvm::ELF::EF_HEXAGON_MACH_V61:
    return "hexagonv61";
  case llvm::ELF::EF_HEXAGON_MACH_V62:
    return "hexagonv62";
  case llvm::ELF::EF_HEXAGON_MACH_V65:
    return "hexagonv65";
  case llvm::ELF::EF_HEXAGON_MACH_V66:
    return "hexagonv66";
  case llvm::ELF::EF_HEXAGON_MACH_V67:
    return "hexagonv67";
  case llvm::ELF::EF_HEXAGON_MACH_V67T:
    return "hexagonv67t";
  case llvm::ELF::EF_HEXAGON_MACH_V68:
    return "hexagonv68";
  case llvm::ELF::EF_HEXAGON_MACH_V69:
    return "hexagonv69";
  case llvm::ELF::EF_HEXAGON_MACH_V71:
    return "hexagonv71";
  case llvm::ELF::EF_HEXAGON_MACH_V71T:
    return "hexagonv71t";
  case llvm::ELF::EF_HEXAGON_MACH_V73:
    return "hexagonv73";
  case llvm::ELF::EF_HEXAGON_MACH_V75:
    return "hexagonv75";
  case llvm::ELF::EF_HEXAGON_MACH_V77:
    return "hexagonv77";
  case llvm::ELF::EF_HEXAGON_MACH_V79:
    return "hexagonv79";
  case llvm::ELF::EF_HEXAGON_MACH_V81:
    return "hexagonv81";
  case llvm::ELF::EF_HEXAGON_MACH_V83:
    return "hexagonv83";
  case llvm::ELF::EF_HEXAGON_MACH_V85:
    return "hexagonv85";
  case llvm::ELF::EF_HEXAGON_MACH_V87:
    return "hexagonv87";
  case llvm::ELF::EF_HEXAGON_MACH_V89:
    return "hexagonv89";
  default:
    break;
  }
  return "";
}

// Is this a deprecated architecture and the linker removed support for it.
HexagonInfo::ArchSupport HexagonInfo::getArchSupport(uint64_t pFlag) const {
  pFlag &= 0XFFFF;
  switch (pFlag) {
  case llvm::ELF::EF_HEXAGON_MACH_V2:
  case llvm::ELF::EF_HEXAGON_MACH_V3:
  case llvm::ELF::EF_HEXAGON_MACH_V4:
  case llvm::ELF::EF_HEXAGON_MACH_V5:
  case llvm::ELF::EF_HEXAGON_MACH_V55:
    return HexagonInfo::ArchSupport::DeprecatedAndNoSupportExists;
  case llvm::ELF::EF_HEXAGON_MACH_V60:
  case llvm::ELF::EF_HEXAGON_MACH_V61:
  case llvm::ELF::EF_HEXAGON_MACH_V62:
  case llvm::ELF::EF_HEXAGON_MACH_V65:
  case llvm::ELF::EF_HEXAGON_MACH_V66:
  case llvm::ELF::EF_HEXAGON_MACH_V67:
  case llvm::ELF::EF_HEXAGON_MACH_V67T:
    return HexagonInfo::ArchSupport::Deprecated;
  case llvm::ELF::EF_HEXAGON_MACH_V68:
  case llvm::ELF::EF_HEXAGON_MACH_V69:
    return HexagonInfo::ArchSupport::Supported;
  case llvm::ELF::EF_HEXAGON_MACH_V71:
  case llvm::ELF::EF_HEXAGON_MACH_V71T:
  case llvm::ELF::EF_HEXAGON_MACH_V73:
  case llvm::ELF::EF_HEXAGON_MACH_V75:
  case llvm::ELF::EF_HEXAGON_MACH_V77:
  case llvm::ELF::EF_HEXAGON_MACH_V79:
  case llvm::ELF::EF_HEXAGON_MACH_V81:
  case llvm::ELF::EF_HEXAGON_MACH_V83:
  case llvm::ELF::EF_HEXAGON_MACH_V85:
  case llvm::ELF::EF_HEXAGON_MACH_V87:
  case llvm::ELF::EF_HEXAGON_MACH_V89:
    return HexagonInfo::ArchSupport::Supported;
  default:
    break;
  }
  return HexagonInfo::ArchSupport::NotSupported;
}

bool HexagonInfo::checkFlags(uint64_t pFlag,
                             const InputFile *pInputFile) const {
  if (!pFlag)
    return true;

  HexagonInfo::ArchSupport archSupport = getArchSupport(pFlag);

  if (archSupport == ArchSupport::NotSupported) {
    m_Config.raise(Diag::not_supported_isa)
        << getArchStr(pFlag) << pInputFile->getInput()->decoratedPath();
    return false;
  }

  if (archSupport == ArchSupport::DeprecatedAndNoSupportExists) {
    m_Config.raise(Diag::deprecated_and_no_support_exists)
        << getArchStr(pFlag) << pInputFile->getInput()->decoratedPath();
    return false;
  }

  if (archSupport == ArchSupport::Deprecated) {
    m_Config.raise(Diag::deprecated_isa)
        << getArchStr(pFlag) << pInputFile->getInput()->decoratedPath();
    // Reset Flag to the architecture that is supported by the toolchain.
    pFlag = getLowestSupportedArch();
  }

  if (m_OutputFlag == LINK_UNKNOWN) {
    if (m_OutputFlag < (int64_t)pFlag)
      m_OutputFlag = pFlag;
  }

  CompatibilityAction action =
      flag_action[translateFlag(m_OutputFlag)][translateFlag(pFlag)];
  switch (action) {
  case NS:
  case ER:
    m_Config.raise(Diag::fatal_unsupported_ISA)
        << pInputFile->getInput()->decoratedPath()
        << ISAs[translateFlag(pFlag)];
    return false;
  case WA:
    if (!m_Config.options().noWarnMismatch())
      m_Config.raise(Diag::incompatible_input_architecture)
          << pInputFile->getInput()->decoratedPath() << getArchStr(pFlag)
          << getArchStr(m_OutputFlag);
    LLVM_FALLTHROUGH;
  case OK:
    if (m_OutputFlag < (int64_t)pFlag)
      m_OutputFlag = pFlag;
    break;
  }

  return true;
}

/// flags - the value of ElfXX_Ehdr::e_flags
uint64_t HexagonInfo::flags() const {
  int32_t OutputFlag = m_OutputFlag;
  if (m_CmdLineFlag != LINK_UNKNOWN) {
    if (OutputFlag == LINK_UNKNOWN)
      OutputFlag = m_CmdLineFlag;
    CompatibilityAction action =
        flag_action[translateFlag(m_CmdLineFlag)][translateFlag(OutputFlag)];
    switch (action) {
    case NS:
    case ER:
      m_Config.raise(Diag::fatal_unsupported_ISA)
          << targetOptions().getTargetCPU() << ISAs[translateFlag(OutputFlag)];
      LLVM_FALLTHROUGH;
    case WA:
      if (!m_Config.options().noWarnMismatch())
        m_Config.raise(Diag::incompatible_architecture)
            << targetOptions().getTargetCPU();
      LLVM_FALLTHROUGH;
    case OK:
      if (OutputFlag < m_CmdLineFlag)
        OutputFlag = m_CmdLineFlag;
      break;
    }
  }
  if (OutputFlag != LINK_UNKNOWN)
    return ISAsToEFlag[translateFlag(OutputFlag)];
  return getLowestSupportedArch();
}

uint64_t HexagonInfo::getLowestSupportedArch() const {
  // Default target CPU == V68.
  return llvm::ELF::EF_HEXAGON_MACH_V68;
}

uint8_t HexagonInfo::OSABI() const { return llvm::ELF::ELFOSABI_NONE; }

uint64_t HexagonInfo::abiPageSize(bool linkerScriptHasSectionsCommand) const {
  if (targetOptions().triple().isOSLinux())
    return 0x10000;
  return TargetInfo::abiPageSize(linkerScriptHasSectionsCommand);
}

bool HexagonInfo::InitializeDefaultMappings(Module &pModule) {
  LinkerScript &pScript = pModule.getScript();
  if (pScript.linkerScriptHasSectionsCommand())
    return true;
  if (m_Config.codeGenType() == LinkerConfig::Object)
    return true;
  pScript.sectionMap().insert(".sdata.1", ".sdata");
  pScript.sectionMap().insert(".sdata.2", ".sdata");
  pScript.sectionMap().insert(".sdata.4", ".sdata");
  pScript.sectionMap().insert(".sdata.8", ".sdata");
  pScript.sectionMap().insert(".sdata*", ".sdata");
  pScript.sectionMap().insert(".sdata", ".sdata");
  pScript.sectionMap().insert(".sbss.1", ".sdata");
  pScript.sectionMap().insert(".sbss.2", ".sdata");
  pScript.sectionMap().insert(".sbss.4", ".sdata");
  pScript.sectionMap().insert(".sbss.8", ".sdata");
  pScript.sectionMap().insert(".sbss*", ".sdata");
  pScript.sectionMap().insert(".sbss", ".sdata");
  pScript.sectionMap().insert(".scommon.1", ".sdata");
  pScript.sectionMap().insert(".scommon.1.*", ".sdata");
  pScript.sectionMap().insert(".scommon.2", ".sdata");
  pScript.sectionMap().insert(".scommon.2.*", ".sdata");
  pScript.sectionMap().insert(".scommon.4", ".sdata");
  pScript.sectionMap().insert(".scommon.4.*", ".sdata");
  pScript.sectionMap().insert(".scommon.8", ".sdata");
  pScript.sectionMap().insert(".scommon.8.*", ".sdata");
  pScript.sectionMap().insert(".scommon*", ".sdata");
  pScript.sectionMap().insert(".lita", ".sdata");
  pScript.sectionMap().insert(".lit4", ".sdata");
  pScript.sectionMap().insert(".lit8", ".sdata");
  pScript.sectionMap().insert(".gnu.linkonce.s.*", ".sdata");
  pScript.sectionMap().insert(".gnu.linkonce.sb.*", ".sdata");
  pScript.sectionMap().insert(".gnu.linkonce.sb.*", ".sdata");
  pScript.sectionMap().insert(".gnu.linkonce.la*", ".sdata");
  pScript.sectionMap().insert(".gnu.linkonce.l4*", ".sdata");
  pScript.sectionMap().insert(".gnu.linkonce.l8*", ".sdata");
  pScript.sectionMap().insert(".hexagon.attributes*", ".hexagon.attributes");
  // These entries will take precedence over platform-independent ones defined
  // later in TargetInfo::InitializeDefaultMappings.
  if (m_Config.options().hasNow()) {
    pScript.sectionMap().insert(".got", ".got");
    pScript.sectionMap().insert(".got.plt", ".got");
  }
  pScript.sectionMap().insert(".gnu.linkonce.l8*", ".sdata");
  return TargetInfo::InitializeDefaultMappings(pModule);
}
