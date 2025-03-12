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

using namespace eld;

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
