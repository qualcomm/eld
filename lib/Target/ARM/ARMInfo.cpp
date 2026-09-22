//===- ARMInfo.cpp---------------------------------------------------------===//
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

#include "ARMInfo.h"
#include "eld/Support/MsgHandling.h"

using namespace eld;

namespace {

uint64_t getEABIVersion(uint64_t Flags) {
  return Flags & llvm::ELF::EF_ARM_EABIMASK;
}

bool areEABIVersionsCompatible(uint64_t InputFlags, uint64_t OutputFlags) {
  uint64_t InputVersion = getEABIVersion(InputFlags);
  uint64_t OutputVersion = getEABIVersion(OutputFlags);

  if (InputVersion == OutputVersion ||
      OutputVersion == llvm::ELF::EF_ARM_EABI_UNKNOWN)
    return true;

  return (InputVersion == llvm::ELF::EF_ARM_EABI_VER4 &&
          OutputVersion == llvm::ELF::EF_ARM_EABI_VER5) ||
         (InputVersion == llvm::ELF::EF_ARM_EABI_VER5 &&
          OutputVersion == llvm::ELF::EF_ARM_EABI_VER4);
}

std::string getEABIVersionString(uint64_t Flags) {
  return "EABI" + std::to_string(getEABIVersion(Flags) >> 24);
}

} // namespace

bool ARMInfo::InitializeDefaultMappings(Module &pModule) {
  LinkerScript &pScript = pModule.getScript();

  if (m_Config.codeGenType() != LinkerConfig::Object) {
    // These entries will take precedence over platform-independent ones defined
    // later in TargetInfo::InitializeDefaultMappings.
    if (isAndroid()) {
      // Merge .got.plt and .got sections into a .got sections respectively.
      pScript.sectionMap().insert(".got.plt", ".got");
      pScript.sectionMap().insert(".got", ".got");
    } else if (m_Config.options().hasNow()) {
      pScript.sectionMap().insert(".got", ".got");
      pScript.sectionMap().insert(".got.plt", ".got");
    }
  }

  TargetInfo::InitializeDefaultMappings(pModule);

  // set up section map
  if (m_Config.codeGenType() != LinkerConfig::Object) {
    pScript.sectionMap().insert(".ARM.exidx.text.unlikely", "ARM.exidx");
    pScript.sectionMap().insert(".ARM.exidx.text.unlikely.*", "ARM.exidx");
    pScript.sectionMap().insert(".ARM.exidx.text.cold", "ARM.exidx");
    pScript.sectionMap().insert(".ARM.exidx.text.cold.*", "ARM.exidx");
    pScript.sectionMap().insert(".ARM.exidx.text.exit", "ARM.exidx");
    pScript.sectionMap().insert(".ARM.exidx.text.exit.*", "ARM.exidx");
    pScript.sectionMap().insert(".ARM.exidx.text.hot", "ARM.exidx");
    pScript.sectionMap().insert(".ARM.exidx.text.hot.*", "ARM.exidx");
    pScript.sectionMap().insert(".ARM.exidx*", ".ARM.exidx");
    pScript.sectionMap().insert(".ARM.extab.text.unlikely", "ARM.extab");
    pScript.sectionMap().insert(".ARM.extab.text.unlikely.*", "ARM.extab");
    pScript.sectionMap().insert(".ARM.extab.text.cold", "ARM.extab");
    pScript.sectionMap().insert(".ARM.extab.text.cold.*", "ARM.extab");
    pScript.sectionMap().insert(".ARM.extab.text.exit", "ARM.extab");
    pScript.sectionMap().insert(".ARM.extab.text.exit.*", "ARM.extab");
    pScript.sectionMap().insert(".ARM.extab.text.hot", "ARM.extab");
    pScript.sectionMap().insert(".ARM.extab.text.hot.*", "ARM.extab");
    pScript.sectionMap().insert(".ARM.extab*", ".ARM.extab");
    pScript.sectionMap().insert(".ARM.attributes*", ".ARM.attributes");
    if (!pScript.linkerScriptHasSectionsCommand()) {
      m_Config.targets().addEntrySection(
          pScript, ".gnu.linkonce.d.rel.ro.local*personality*");
      m_Config.targets().addEntrySection(pScript,
                                         ".gnu.linkonce.d.rel.ro*personality*");
      m_Config.targets().addEntrySection(pScript, ".ARM.attributes*");
    }
  }
  return true;
}

std::string ARMInfo::flagString(uint64_t flag) const { return "arm"; }

uint64_t ARMInfo::flags() const {
  // checkFlags() was never called. This means the linker was given a lone empty
  // .o file or a lone symdef file, etc. In either case, we want the result to
  // have flags.
  if (!OutputFlags)
    return llvm::ELF::EF_ARM_EABI_VER5;
  return *OutputFlags;
}

bool ARMInfo::checkFlags(uint64_t Flags, const InputFile *I,
                         bool hasExecutableSections) {
  // Binary inputs do not carry ARM EABI information.
  if (I->isBinaryFile()) {
    if (!OutputFlags)
      OutputFlags = llvm::ELF::EF_ARM_EABI_UNKNOWN;
    return true;
  }

  // The first object establishes the output EABI version. Other ARM e_flags
  // are output properties and must not be inherited implicitly from inputs.
  if (!OutputFlags) {
    OutputFlags = getEABIVersion(Flags);
    return true;
  }

  // Once output flags have been established, data-only relocatable objects do
  // not participate in ARM EABI compatibility checking. Dynamic objects still
  // participate, matching ld.bfd behavior.
  if (!hasExecutableSections && !I->isDynamicLibrary())
    return true;

  if (!areEABIVersionsCompatible(Flags, *OutputFlags)) {
    m_Config.raise(Diag::incompatible_architecture_versions)
        << getEABIVersionString(Flags) << I->getInput()->decoratedPath()
        << getEABIVersionString(*OutputFlags);

    if (m_Config.options().warnMismatch())
      return false;
  }

  // UNKNOWN may be superseded by the first known EABI version.
  if (getEABIVersion(*OutputFlags) == llvm::ELF::EF_ARM_EABI_UNKNOWN &&
      getEABIVersion(Flags) != llvm::ELF::EF_ARM_EABI_UNKNOWN)
    OutputFlags = getEABIVersion(Flags);

  return true;
}
