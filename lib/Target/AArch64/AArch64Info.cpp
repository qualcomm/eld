//===- AArch64Info.cpp-----------------------------------------------------===//
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
#include "AArch64Info.h"

using namespace eld;

AArch64Info::AArch64Info(LinkerConfig &Config) : TargetInfo(Config) {}

bool AArch64Info::needEhdr(Module &pModule, bool linkerScriptHasSectionsCmd,
                           bool isPhdr) {
  return pModule.getBackend()->hasEhFrameHdr();
}

bool AArch64Info::InitializeDefaultMappings(Module &pModule) {
  // For android 64bit, the loader is unable to read the eh frame header
  // information and is reverting to not doing a binary search.
  if (isAndroid())
    pModule.getBackend()->populateEhFrameHdrWithNoFDEInfo();

  LinkerScript &pScript = pModule.getScript();

  // These entries will take precedence over platform-independent ones defined
  // later in TargetInfo::InitializeDefaultMappings.
  if (m_Config.options().hasNow()) {
    pScript.sectionMap().insert(".got", ".got");
    pScript.sectionMap().insert(".got.plt", ".got");
  }

  TargetInfo::InitializeDefaultMappings(pModule);

  if (!pScript.linkerScriptHasSectionsCommand()) {
    m_Config.targets().addEntrySection(
        pScript, ".gnu.linkonce.d.rel.ro.local*personality*");
    m_Config.targets().addEntrySection(pScript,
                                       ".gnu.linkonce.d.rel.ro*personality*");
  }
  return true;
}

std::string AArch64Info::flagString(uint64_t flag) const { return "aarch64"; }
