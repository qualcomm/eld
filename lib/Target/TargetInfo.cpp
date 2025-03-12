//===- TargetInfo.cpp------------------------------------------------------===//
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
#include "eld/Target/TargetInfo.h"
#include "eld/Core/Module.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/ErrorHandling.h"

using namespace eld;

namespace {

struct NameMap {
  const char *from; ///< the prefix of the input string. (match FROM*)
  const char *to;   ///< the output string.
  InputSectDesc::Policy policy; /// mark whether the input is kept in GC
};

const NameMap map[] = {
    {".text.unlikely", ".text", InputSectDesc::NoKeep},
    {".text.unlikely.*", ".text", InputSectDesc::NoKeep},
    {".text.cold", ".text", InputSectDesc::NoKeep},
    {".text.cold.*", ".text", InputSectDesc::NoKeep},
    {".text.exit", ".text", InputSectDesc::NoKeep},
    {".text.exit.*", ".text", InputSectDesc::NoKeep},
    {".text.hot", ".text", InputSectDesc::NoKeep},
    {".text.hot.*", ".text", InputSectDesc::NoKeep},
    {".text*", ".text", InputSectDesc::NoKeep},
    {".rodata*", ".rodata", InputSectDesc::NoKeep},
    {".data.rel.ro.local*", ".data.rel.ro.local", InputSectDesc::NoKeep},
    {".data.rel.ro*", ".data.rel.ro", InputSectDesc::NoKeep},
    {".data.hot", ".data", InputSectDesc::NoKeep},
    {".data.hot.*", ".data", InputSectDesc::NoKeep},
    {".data.cold", ".data", InputSectDesc::NoKeep},
    {".data.cold.*", ".data", InputSectDesc::NoKeep},
    {".data*", ".data", InputSectDesc::NoKeep},
    {".bss*", ".bss", InputSectDesc::NoKeep},
    {".tdata*", ".tdata", InputSectDesc::NoKeep},
    {".tbss*", ".tbss", InputSectDesc::NoKeep},
    {".init", ".init", InputSectDesc::Keep},
    {".fini", ".fini", InputSectDesc::Keep},
    {".preinit_array*", ".preinit_array", InputSectDesc::Keep},
    {".init_array*", ".init_array", InputSectDesc::Keep},
    {".fini_array*", ".fini_array", InputSectDesc::Keep},
    // TODO: Support DT_INIT_ARRAY for all constructors?
    {".ctors*", ".ctors", InputSectDesc::Keep},
    {".dtors*", ".dtors", InputSectDesc::Keep},
    {".eh_frame", ".eh_frame", InputSectDesc::Keep},
    {".eh_frame_hdr", ".eh_frame_hdr", InputSectDesc::Keep},
    {".jcr", ".jcr", InputSectDesc::Keep},
    // FIXME: in GNU ld, if we are creating a shared object .sdata2 and .sbss2
    // sections would be handled differently.
    {".lrodata*", ".lrodata", InputSectDesc::NoKeep},
    {".ldata*", ".ldata", InputSectDesc::NoKeep},
    {".lbss*", ".lbss", InputSectDesc::NoKeep},
    {".gcc_except_table*", ".gcc_except_table", InputSectDesc::Keep},
    {".gnu.linkonce.d.rel.ro.local*", ".data.rel.ro.local",
     InputSectDesc::NoKeep},
    {".gnu.linkonce.d.rel.ro*", ".data.rel.ro", InputSectDesc::NoKeep},
    {".gnu.linkonce.r*", ".rodata", InputSectDesc::NoKeep},
    {".gnu.linkonce.d*", ".data", InputSectDesc::NoKeep},
    {".gnu.linkonce.b*", ".bss", InputSectDesc::NoKeep},
    {".gnu.linkonce.wi*", ".debug_info", InputSectDesc::NoKeep},
    {".debug_info*", ".debug_info", InputSectDesc::NoKeep},
    {".gnu.linkonce.td*", ".tdata", InputSectDesc::NoKeep},
    {".gnu.linkonce.tb*", ".tbss", InputSectDesc::NoKeep},
    {".gnu.linkonce.t*", ".text", InputSectDesc::NoKeep},
    {".gnu.linkonce.lr*", ".lrodata", InputSectDesc::NoKeep},
    {".gnu.linkonce.lb*", ".lbss", InputSectDesc::NoKeep},
    {".gnu.linkonce.l*", ".ldata", InputSectDesc::NoKeep},
    {"COMMON.*", ".bss", InputSectDesc::NoKeep},
};
} // namespace
//===----------------------------------------------------------------------===//
// TargetInfo
//===----------------------------------------------------------------------===//
TargetInfo::TargetInfo(LinkerConfig &pConfig) : m_Config(pConfig) {}

bool TargetInfo::checkFlags(uint64_t flag, const InputFile *pInputFile) const {
  return true;
}

std::string TargetInfo::flagString(uint64_t pFlag) const { return ""; }

uint8_t TargetInfo::OSABI() const {
  switch (m_Config.targets().triple().getOS()) {
  case llvm::Triple::Linux:
    return llvm::ELF::ELFOSABI_LINUX;
  default:
    return llvm::ELF::ELFOSABI_NONE;
  }
}

bool TargetInfo::InitializeDefaultMappings(Module &pModule) {
  LinkerScript &pScript = pModule.getScript();
  // set up section map
  if (!pScript.linkerScriptHasSectionsCommand() &&
      m_Config.codeGenType() != LinkerConfig::Object) {
    for (auto &elem : map) {
      std::pair<SectionMap::mapping, bool> res =
          pScript.sectionMap().insert(elem.from, elem.to, elem.policy);
      if (!res.second)
        return false;
    }
    m_Config.targets().addEntrySection(pScript, ".text*personality*");
    m_Config.targets().addEntrySection(pScript, ".data*personality*");
  }

  if (m_Config.codeGenType() != LinkerConfig::Object) {
    pScript.sectionMap().insert(".got", ".got");
    pScript.sectionMap().insert(".got.plt", ".got.plt");
  }

  return true;
}

llvm::StringRef TargetInfo::getOutputMCPU() const {
  return m_Config.targets().getTargetCPU();
}

uint8_t TargetInfo::ELFClass() const {
  if (m_Config.targets().is32Bits())
    return llvm::ELF::ELFCLASS32;
  if (m_Config.targets().is64Bits())
    return llvm::ELF::ELFCLASS64;
  llvm_unreachable("Invalid target architecture!");
}

TargetInfo::TargetRelocationType TargetInfo::getTargetRelocationType() const {
  return relocType;
}
