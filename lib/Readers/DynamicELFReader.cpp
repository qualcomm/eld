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
#ifdef ELD_ENABLE_SYMBOL_VERSIONING
#include "llvm/BinaryFormat/ELF.h"
#endif

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
  // FIXME: We probably do not need to create section headers objects
  // for sections in shared libraries.
  for (const typename ELFReader<ELFT>::Elf_Shdr &rawSectHdr :
       this->m_RawSectHdrs.value()) {
    eld::Expected<ELFSection *> expSection = this->createSection(rawSectHdr);
    ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expSection);
    ELFSection *S = expSection.value();
    this->setSectionInInputFile(S, rawSectHdr);
    this->setSectionAttributes(S, rawSectHdr);
  }

  this->setLinkInfoAttributes();

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

#ifdef ELD_ENABLE_SYMBOL_VERSIONING
template <class ELFT>
void DynamicELFReader<ELFT>::setSectionInInputFile(
    ELFSection *S, typename ELFReader<ELFT>::Elf_Shdr RawSectHdr) {
  ELFReader<ELFT>::setSectionInInputFile(S, RawSectHdr);
  ELFDynObjectFile *DynObjFile =
      llvm::cast<ELFDynObjectFile>(&this->m_InputFile);

  switch (RawSectHdr.sh_type) {
  case llvm::ELF::SHT_GNU_verdef:
    DynObjFile->setVerDefSection(S);
    break;
  case llvm::ELF::SHT_GNU_verneed:
    DynObjFile->setVerNeedSection(S);
    break;
  case llvm::ELF::SHT_GNU_versym:
    DynObjFile->setVerSymSection(S);
    break;
  case llvm::ELF::SHT_STRTAB:
    if (S->name() == ".dynstr")
      DynObjFile->setDynStrTabSection(S);
  }
}

template <class ELFT>
eld::Expected<void> DynamicELFReader<ELFT>::readSections() {
  ELFDynObjectFile *DynObjFile =
      llvm::cast<ELFDynObjectFile>(&this->m_InputFile);
  if (DynObjFile->getVerDefSection()) {
    auto expReadVerDef = readVerDefSection();
    ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expReadVerDef);
  }
  if (DynObjFile->getVerNeedSection()) {
    auto expReadVerNeed = readVerNeedSection();
    ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expReadVerNeed);
  }
  if (DynObjFile->getVerSymSection()) {
    auto expReadVerSym = readVerSymSection();
    ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expReadVerSym);
  }
  return {};
}

template <class ELFT>
eld::Expected<void> DynamicELFReader<ELFT>::readVerDefSection() {
  ELFDynObjectFile *DynObjFile =
      llvm::cast<ELFDynObjectFile>(&this->m_InputFile);
  const ELFSection *S = DynObjFile->getVerDefSection();
  if (!S)
    return {};
  ASSERT(S->getType() == llvm::ELF::SHT_GNU_verdef,
         "S must be SHT_GNU_verdef section");
  std::vector<const void *> Verdefs;
  const uint8_t *Verdef =
      reinterpret_cast<const uint8_t *>(S->getContents().data());
  for (unsigned i = 0, e = S->getInfo(); i < e; ++i) {
    auto *CurVerDef = reinterpret_cast<const typename ELFT::Verdef *>(Verdef);
    Verdef += CurVerDef->vd_next;
    unsigned CurVerDefIndex = CurVerDef->vd_ndx;
    if (CurVerDefIndex >= Verdefs.size())
      Verdefs.resize(CurVerDefIndex + 1);
    Verdefs[CurVerDefIndex] = CurVerDef;
  }
  DynObjFile->setVerDefs(std::move(Verdefs));
  return {};
}

template <class ELFT>
eld::Expected<void> DynamicELFReader<ELFT>::readVerNeedSection() {
  ELFDynObjectFile *DynObjFile =
      llvm::cast<ELFDynObjectFile>(&this->m_InputFile);
  const ELFSection *S = DynObjFile->getVerNeedSection();
  if (!S)
    return {};
  ASSERT(S->getType() == llvm::ELF::SHT_GNU_verneed,
         "S must be SHT_GNU_verneed section");
  std::vector<uint32_t> VerNeeds;
  const uint8_t *VerNeedBuf =
      reinterpret_cast<const uint8_t *>(S->getContents().data());
  for (unsigned i = 0, e = S->getInfo(); i < e; ++i) {
    // FIXME: Perhaps add the below error check?
    // if (verneedBuf + sizeof(typename ELFT::Verneed) > data.end()) {
    //   Err(ctx) << this << " has an invalid Verneed";
    //   break;
    // }
    auto *CurVerNeed =
        reinterpret_cast<const typename ELFT::Verneed *>(VerNeedBuf);
    const uint8_t *VernAuxBuf = VerNeedBuf + CurVerNeed->vn_aux;
    for (unsigned j = 0; j != CurVerNeed->vn_cnt; ++j) {
      auto *CurVernAux =
          reinterpret_cast<const typename ELFT::Vernaux *>(VernAuxBuf);
      uint16_t VersionIdentifier =
          CurVernAux->vna_other & llvm::ELF::VERSYM_VERSION;
      if (VersionIdentifier > VerNeeds.size())
        VerNeeds.resize(VersionIdentifier + 1);
      VerNeeds[VersionIdentifier] = CurVernAux->vna_name;
      VernAuxBuf += CurVernAux->vna_next;
    }
    VerNeedBuf += CurVerNeed->vn_next;
  }
  DynObjFile->setVerNeeds(std::move(VerNeeds));
  return {};
}

template <class ELFT>
eld::Expected<void> DynamicELFReader<ELFT>::readVerSymSection() {
  ELFDynObjectFile *DynObjFile =
      llvm::cast<ELFDynObjectFile>(&this->m_InputFile);
  const ELFSection *S = DynObjFile->getVerSymSection();
  if (!S)
    return {};
  ASSERT(S->getType() == llvm::ELF::SHT_GNU_versym,
         "S must be SHT_GNU_versym section");
  std::vector<uint16_t> VerSyms;
  typename ELFReader<ELFT>::Elf_Shdr RawSectHdr =
      this->m_RawSectHdrs.value()[S->getIndex()];
  llvm::Expected<llvm::ArrayRef<typename ELFReader<ELFT>::Elf_Versym>>
      ExpRawVerSym = this->m_LLVMELFFile->template getSectionContentsAsArray<
          typename ELFReader<ELFT>::Elf_Versym>(RawSectHdr);
  LLVMEXP_RETURN_DIAGENTRY_IF_ERROR(ExpRawVerSym);
  llvm::ArrayRef<typename ELFReader<ELFT>::Elf_Versym> RawVerSym =
      std::move(ExpRawVerSym.get());
  VerSyms.resize(RawVerSym.size());
  for (size_t i = 0; i < RawVerSym.size(); ++i)
    VerSyms[i] = RawVerSym[i].vs_index;
  DynObjFile->setVerSyms(std::move(VerSyms));
  return {};
}
#endif

template class DynamicELFReader<llvm::object::ELF32LE>;
template class DynamicELFReader<llvm::object::ELF64LE>;
} // namespace eld
