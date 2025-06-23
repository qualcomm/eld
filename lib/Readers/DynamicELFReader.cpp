//===- DynamicELFReader.cpp------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Readers/DynamicELFReader.h"
#include "eld/Core/Module.h"
#include "eld/Diagnostics/MsgHandler.h"
#include "eld/Input/ELFDynObjectFile.h"
#include "eld/Readers/ELFSection.h"
#include "eld/SymbolResolver/LDSymbol.h"
#include "eld/SymbolResolver/ResolveInfo.h"

namespace eld {
template <class ELFT>
DynamicELFReader<ELFT>::DynamicELFReader(Module &module, InputFile &inputFile,
                                         plugin::DiagnosticEntry &diagEntry)
    : ELFReader<ELFT>(module, inputFile, diagEntry) {}

template <class ELFT>
eld::Expected<std::unique_ptr<DynamicELFReader<ELFT>>>
DynamicELFReader<ELFT>::Create(Module &module, InputFile &inputFile) {
  plugin::DiagnosticEntry diagEntry;
  DynamicELFReader<ELFT> reader =
      DynamicELFReader<ELFT>(module, inputFile, diagEntry);
  if (diagEntry)
    return std::make_unique<plugin::DiagnosticEntry>(diagEntry);
  return std::make_unique<DynamicELFReader<ELFT>>(reader);
}

template <class ELFT> void DynamicELFReader<ELFT>::setSymbolsAliasInfo() {
  ASSERT(this->m_InputFile.isDynamicLibrary(),
         "'setSymbolsAliasInfo' must only be called for dynamic libraries!");
  ELFDynObjectFile *EDynObjFile =
      llvm::cast<ELFDynObjectFile>(&(this->m_InputFile));
  llvm::DenseMap<uint64_t, std::vector<LDSymbol *>> dynObjAliasSymbolMap;
  llvm::DenseMap<uint64_t, LDSymbol *> dynObjGlobalSymbolMap;
  for (LDSymbol *sym : EDynObjFile->getSymbols()) {
    // Skip symbols that are not defined.
    if (sym->desc() != ResolveInfo::Define)
      continue;
    // Skip local symbols and Absolute symbols.
    if (sym->binding() == ResolveInfo::Local ||
        sym->binding() == ResolveInfo::Absolute)
      continue;
    // Check if the symbol is an alias symbol of some other symbol.The alias
    // symbols are usually defined as OBJECT|WEAK
    bool mayAlias = false;
    if ((sym->binding() == ResolveInfo::Weak) &&
        (sym->type() == ResolveInfo::Object))
      mayAlias = true;
    if (mayAlias)
      dynObjAliasSymbolMap[sym->value()].push_back(sym);
    else
      dynObjGlobalSymbolMap[sym->value()] = sym;
  }
  ELFReader<ELFT>::processAndReportSymbolAliases(dynObjAliasSymbolMap,
                                                 dynObjGlobalSymbolMap);
}

template <class ELFT>
eld::Expected<bool> DynamicELFReader<ELFT>::readSymbols() {
  auto expReadSymbols = ELFReader<ELFT>::readSymbols();
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expReadSymbols);
  // setSymbolsAliasInfo();
  return true;
}

template <class ELFT>
eld::Expected<bool> DynamicELFReader<ELFT>::readDynamic() {
  ASSERT(this->m_LLVMELFFile, "m_LLVMELFFile must be initialized!");
  ASSERT(this->m_RawSectHdrs, "m_RawSectHdrs must be initialized!");

  ELFDynObjectFile *dynObjFile =
      llvm::cast<ELFDynObjectFile>(&(this->m_InputFile));
  /// FIXME: unneeded cast?
  ELFSection *dynamicSect =
      llvm::dyn_cast<ELFSection>(dynObjFile->getDynamic());
  if (!dynamicSect) {
    return std::make_unique<plugin::DiagnosticEntry>(
        plugin::DiagnosticEntry(Diag::err_cannot_read_section, {".dynamic"}));
  }
  const typename ELFReader<ELFT>::Elf_Shdr &rawDynSectHdr =
      (*(this->m_RawSectHdrs))[dynamicSect->getIndex()];
  llvm::Expected<llvm::StringRef> expDynStrTab =
      this->m_LLVMELFFile->getLinkAsStrtab(rawDynSectHdr);
  LLVMEXP_RETURN_DIAGENTRY_IF_ERROR(expDynStrTab);

  llvm::StringRef dynStrTab = expDynStrTab.get();

  llvm::Expected<typename ELFReader<ELFT>::Elf_Dyn_Range> expDynamicEntries =
      this->m_LLVMELFFile->dynamicEntries();
  LLVMEXP_RETURN_DIAGENTRY_IF_ERROR(expDynamicEntries);

  bool hasSOName = false;

  for (typename ELFReader<ELFT>::Elf_Dyn dynamic : expDynamicEntries.get()) {
    typename ELFReader<ELFT>::intX_t dTag{dynamic.getTag()};
    typename ELFReader<ELFT>::uintX_t dVal{dynamic.getVal()};
    switch (dTag) {
    case llvm::ELF::DT_SONAME:
      ASSERT(dVal < dynStrTab.size(), "invalid tag value!");
      dynObjFile->setSOName(
          sys::fs::Path(dynStrTab.data() + dVal).filename().native());
      hasSOName = true;
      break;
    case llvm::ELF::DT_NEEDED:
      // TODO:
      break;
    case llvm::ELF::DT_NULL:
    default:
      break;
    }
  }

  // if there is no SONAME in .dynamic, then set it from input path
  if (!hasSOName)
    dynObjFile->setSOName(dynObjFile->getFallbackSOName());
  return true;
}

template <class ELFT>
eld::Expected<bool> DynamicELFReader<ELFT>::readSectionHeaders() {
  if (!this->m_RawSectHdrs)
    LLVMEXP_EXTRACT_AND_CHECK(this->m_RawSectHdrs,
                              this->m_LLVMELFFile->sections());
  if (this->m_RawSectHdrs->empty())
    return true;

  /// Create all sections, including the first null section.
  for (const typename ELFReader<ELFT>::Elf_Shdr &rawSectHdr :
       this->m_RawSectHdrs.value()) {
    eld::Expected<ELFSection *> expSection = this->createSection(rawSectHdr);
    ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expSection);
    ELFSection *S = expSection.value();
    this->setSectionInInputFile(S, rawSectHdr);
    this->setSectionAttributes(S, rawSectHdr);
  }

  this->setLinkInfoAttributes();
  this->verifyFile(this->m_Module.getConfig().getDiagEngine());

  auto expReadDyn = readDynamic();

  if (!expReadDyn)
    return std::move(expReadDyn);

  auto &PM = this->m_Module.getPluginManager();
  if (!PM.callVisitSectionsHook(this->m_InputFile))
    return false;

  return true;
}

template <class ELFT>
eld::Expected<ELFSection *> DynamicELFReader<ELFT>::createSection(
    typename ELFReader<ELFT>::Elf_Shdr rawSectHdr) {
  eld::Expected<std::string> expSectionName = this->getSectionName(rawSectHdr);
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expSectionName);
  std::string sectionName = expSectionName.value();

  // Setup all section properties.
  LDFileFormat::Kind kind = this->getSectionKind(rawSectHdr, sectionName);

  // FIXME: Emit some diagnostic here.
  if (kind == LDFileFormat::Error)
    return static_cast<ELFSection *>(nullptr);

  ELFSection *section =
      this->m_Module.getScript().sectionMap().createELFSection(
          sectionName, kind, rawSectHdr.sh_type, rawSectHdr.sh_flags,
          rawSectHdr.sh_entsize);

  return section;
}

template class DynamicELFReader<llvm::object::ELF32LE>;
template class DynamicELFReader<llvm::object::ELF64LE>;
} // namespace eld
