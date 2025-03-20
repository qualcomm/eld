//===- RelocELFReader.cpp--------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Readers/RelocELFReader.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Core/Module.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Fragment/MergeStringFragment.h"
#include "eld/Input/ELFObjectFile.h"
#include "eld/Input/InputFile.h"
#include "eld/Object/SectionMap.h"
#include "eld/PluginAPI/DiagnosticEntry.h"
#include "eld/Readers/Relocation.h"
#include "eld/SymbolResolver/LDSymbol.h"
#include "eld/SymbolResolver/Resolver.h"
#include "eld/Target/GNULDBackend.h"
#include "eld/Target/LDFileFormat.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/Compression.h"
#include <cstdint>
#include <numeric>
#include <type_traits>

namespace eld {
template <class ELFT>
RelocELFReader<ELFT>::RelocELFReader(Module &module, InputFile &inputFile,
                                     plugin::DiagnosticEntry &diagEntry)
    : ELFReader<ELFT>(module, inputFile, diagEntry) {}

template <class ELFT>
eld::Expected<std::unique_ptr<RelocELFReader<ELFT>>>
RelocELFReader<ELFT>::Create(Module &module, InputFile &inputFile) {
  ASSERT(inputFile.isObjectFile(),
         "RelocELFReader must only be used for relocatable object files.");
  plugin::DiagnosticEntry diagEntry;
  RelocELFReader<ELFT> reader =
      RelocELFReader<ELFT>(module, inputFile, diagEntry);
  if (diagEntry)
    return std::make_unique<plugin::DiagnosticEntry>(diagEntry);
  return std::make_unique<RelocELFReader<ELFT>>(reader);
}

template <class ELFT>
eld::Expected<ELFSection *> RelocELFReader<ELFT>::createSection(
    typename ELFReader<ELFT>::Elf_Shdr rawSectHdr) {
  Module &module = this->m_Module;
  eld::Expected<std::string> expSectName = this->getSectionName(rawSectHdr);
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expSectName);
  std::string sectName = expSectName.value();

  // Setup all section properties.
  // FIXME: sectName can be extracted from rawSectHdr.
  LDFileFormat::Kind kind = this->getSectionKind(rawSectHdr, sectName);

  // FIXME: Emit some diagnostic here.
  if (kind == LDFileFormat::Error)
    return static_cast<ELFSection *>(nullptr);

  bool LLVMBCSectionIsIgnore = false;
  // Embedded bitcode sections must be not regarded in linking. However they
  // must be added to context since the sh_info field of relocation sections
  // that follow these sections will need to have proper index in section
  // header table. We thus read them and then set to ignore to have no effect
  // on linking.
  if (sectName == ".llvmbc" || sectName == ".llvmcmd")
    LLVMBCSectionIsIgnore = true;

  ELFSection *section = nullptr;
  if (kind == LDFileFormat::EhFrame)
    section = module.getScript().sectionMap().createEhFrameSection(
        sectName, rawSectHdr.sh_type, rawSectHdr.sh_flags,
        rawSectHdr.sh_entsize);

  if (!section) {
    section = module.getScript().sectionMap().createELFSection(
        sectName, (LLVMBCSectionIsIgnore ? LDFileFormat::Discard : kind),
        rawSectHdr.sh_type, rawSectHdr.sh_flags, rawSectHdr.sh_entsize);
  }

  return section;
}

template <class ELFT>
eld::Expected<bool> RelocELFReader<ELFT>::readSectionHeaders() {
  ASSERT(this->m_LLVMELFFile, "m_LLVMELFFile must be initialized!");
  if (!this->m_RawSectHdrs)
    LLVMEXP_EXTRACT_AND_CHECK(this->m_RawSectHdrs,
                              this->m_LLVMELFFile->sections());
  ASSERT(this->m_RawSectHdrs, "m_RawSectHdrs must be initialized!");

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
  // FIXME: This doesn't return the result of verifyFile to maintain
  // compatibility with the old readers.
  this->verifyFile(this->m_Module.getConfig().getDiagEngine());

  auto &PM = this->m_Module.getPluginManager();
  if (!PM.callVisitSectionsHook(this->m_InputFile))
    return false;

  return true;
}

template <class ELFT>
eld::Expected<ResolveInfo *>
RelocELFReader<ELFT>::computeGroupSignature(const ELFSection *S) const {
  ASSERT(this->m_LLVMELFFile, "m_LLVMELFFile must be initialized!");
  ASSERT(this->m_RawSectHdrs, "m_RawSectHdrs must be initialized!");
  ASSERT(S->isGroupKind(), "S must be a group section!");
  ASSERT(S->getLink() && S->getInfo(),
         "link and info attributes of " + S->name().str() + "(" +
             S->getInputFile()->getInput()->decoratedPath() +
             ") must not be null!");

  const typename ELFReader<ELFT>::Elf_Shdr &rawS =
      (*this->m_RawSectHdrs)[S->getIndex()];
  typename ELFReader<ELFT>::Elf_Word symIdx = rawS.sh_info;
  typename ELFReader<ELFT>::Elf_Word symTabIdx = rawS.sh_link;
  const typename ELFReader<ELFT>::Elf_Shdr &rawSymTab =
      (*this->m_RawSectHdrs)[symTabIdx];
  llvm::Expected<llvm::StringRef> expStrTab =
      this->m_LLVMELFFile->getStringTableForSymtab(rawSymTab);
  LLVMEXP_RETURN_DIAGENTRY_IF_ERROR(expStrTab);
  llvm::StringRef strTab = expStrTab.get();
  llvm::Expected<const typename ELFReader<ELFT>::Elf_Sym *> expRawSym =
      this->m_LLVMELFFile->getSymbol(&rawSymTab, symIdx);
  LLVMEXP_RETURN_DIAGENTRY_IF_ERROR(expRawSym)
  const typename ELFReader<ELFT>::Elf_Sym *rawSym = expRawSym.get();
  llvm::Expected<llvm::StringRef> expSymName = rawSym->getName(strTab);
  LLVMEXP_RETURN_DIAGENTRY_IF_ERROR(expSymName);

  // Other symbol values do not matter for group signature.
  ResolveInfo *R = make<ResolveInfo>(expSymName.get());
  R->setResolvedOrigin(&this->m_InputFile);
  return R;
}

template <class ELFT>
eld::Expected<std::vector<uint32_t>>
RelocELFReader<ELFT>::getGroupMemberIndices(const ELFSection *S) const {
  ASSERT(this->m_RawSectHdrs, "m_RawSectHdrs must be initialized!");
  const typename ELFReader<ELFT>::Elf_Shdr &rawSectHdr =
      (*this->m_RawSectHdrs)[S->getIndex()];
  llvm::Expected<llvm::ArrayRef<typename ELFReader<ELFT>::Elf_Word>>
      expEntries = this->m_LLVMELFFile->template getSectionContentsAsArray<
          typename ELFReader<ELFT>::Elf_Word>(rawSectHdr);
  LLVMEXP_RETURN_DIAGENTRY_IF_ERROR(expEntries);
  llvm::ArrayRef<typename ELFReader<ELFT>::Elf_Word> entries = expEntries.get();

  std::vector<uint32_t> indices;

  // first entry is group flag.
  for (const typename ELFReader<ELFT>::Elf_Word &entry : entries.slice(1))
    indices.push_back(entry);
  return indices;
}

template <class ELFT>
eld::Expected<uint32_t>
RelocELFReader<ELFT>::getGroupFlag(const ELFSection *S) const {
  ASSERT(this->m_RawSectHdrs, "m_RawSectHdrs must be initialized!");
  const typename ELFReader<ELFT>::Elf_Shdr &rawSectHdr =
      (*this->m_RawSectHdrs)[S->getIndex()];
  llvm::Expected<llvm::ArrayRef<typename ELFReader<ELFT>::Elf_Word>>
      expEntries = this->m_LLVMELFFile->template getSectionContentsAsArray<
          typename ELFReader<ELFT>::Elf_Word>(rawSectHdr);
  LLVMEXP_RETURN_DIAGENTRY_IF_ERROR(expEntries);
  llvm::ArrayRef<typename ELFReader<ELFT>::Elf_Word> entries = expEntries.get();
  // first entry is the group flag.
  return entries.front();
}

template <class ELFT>
eld::Expected<bool> RelocELFReader<ELFT>::readCompressedSection(ELFSection *S) {
  LinkerConfig &config = this->m_Module.getConfig();

  config.raise(Diag::reading_compressed_section)
      << S->name() << S->originalInput()->getInput()->decoratedPath();
  if (!S->size())
    return true;
  llvm::StringRef rawData = S->getContents();

  // FIXME: Return error here.
  if (rawData.size() < sizeof(typename ELFReader<ELFT>::Elf_Chdr))
    return false;

  const typename ELFReader<ELFT>::Elf_Chdr *hdr =
      reinterpret_cast<const typename ELFReader<ELFT>::Elf_Chdr *>(
          rawData.data());
  // FIXME: Return error here.
  if (hdr->ch_type != llvm::ELF::ELFCOMPRESS_ZLIB)
    return false;

  size_t uncompressedSize = hdr->ch_size;
  typename ELFReader<ELFT>::uintX_t alignment =
      std::max<typename ELFReader<ELFT>::uintX_t>(hdr->ch_addralign, 1);
  rawData = rawData.slice(sizeof(*hdr), sizeof(*hdr) + uncompressedSize);

  char *uncompressedBuf = this->m_Module.getUninitBuffer(uncompressedSize);
  if (llvm::Error e = llvm::compression::zlib::decompress(
          llvm::arrayRefFromStringRef(rawData), (uint8_t *)uncompressedBuf,
          uncompressedSize)) {
    plugin::DiagnosticEntry de =
        config.getDiagEngine()->convertToDiagEntry(std::move(e));
    return std::make_unique<plugin::DiagnosticEntry>(de);
  }

  // Reset the section flag, linker is going to write the data uncompressed
  S->setFlags(S->getFlags() & ~llvm::ELF::SHF_COMPRESSED);

  RegionFragment *frag =
      make<RegionFragment>(llvm::StringRef(uncompressedBuf, uncompressedSize),
                           S, Fragment::Type::Region, alignment);
  S->addFragment(frag);

  // Record stuff in the map file
  LayoutPrinter *printer = this->m_Module.getLayoutPrinter();
  if (printer)
    printer->recordFragment(&this->m_InputFile, S, frag);

  return true;
}

template <class ELFT>
eld::Expected<bool>
RelocELFReader<ELFT>::readMergeStringSection(ELFSection *S) {
  LinkerConfig &config = this->m_Module.getConfig();
  llvm::StringRef Contents = S->getContents();
  if (Contents.empty())
    return true;
  MergeStringFragment *F = make<MergeStringFragment>(S);
  if (!F->readStrings(config))
    return false;
  S->addFragment(F);
  LayoutPrinter *printer = this->m_Module.getLayoutPrinter();
  if (printer)
    printer->recordFragment(&this->m_InputFile, S, F);
  return true;
}

template <class ELFT>
eld::Expected<llvm::StringRef> RelocELFReader<ELFT>::computeSymbolName(
    const typename ELFReader<ELFT>::Elf_Sym &rawSym, const ELFSection *S,
    llvm::StringRef stringTable, uint32_t st_shndx,
    ResolveInfo::Type ldType) const {
  eld::Expected<llvm::StringRef> expSymName =
      ELFReader<ELFT>::computeSymbolName(rawSym, S, stringTable, st_shndx,
                                         ldType);
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expSymName);
  return expSymName.value();
}

template <class ELFT>
void RelocELFReader<ELFT>::setPostLTOCommonSymbolsOrigin() const {
  ASSERT(this->m_Module.isPostLTOPhase(),
         "This function must only be called in post LTO phase!");
  if (!this->m_Module.getScript().linkerScriptHasSectionsCommand())
    return;
  ELFFileBase *EFileBase = llvm::cast<ELFFileBase>(&this->m_InputFile);
  for (LDSymbol *sym : EFileBase->getSymbols()) {
    ASSERT(sym, "Input files should not contain nullptr in symbols array.");
    if (sym->desc() != ResolveInfo::Common)
      continue;
    InputFile *I = this->m_Module.findCommon(sym->name());
    // FIXME: Can we remove the 'if' from here? Is there any valid case
    // when common symbols do not have an associated input file after name
    // resolution?
    if (I)
      sym->resolveInfo()->setResolvedOrigin(I);
  }
}

template <class ELFT>
eld::Expected<bool> RelocELFReader<ELFT>::readOneGroup(ELFSection *S) {
  ELFObjectFile *EObj = llvm::cast<ELFObjectFile>(&this->m_InputFile);
  LDSymbol *signatureSymbol = EObj->getSymbol(S->getInfo());
  // FIXME: Return an error instead!
  if (!signatureSymbol)
    return false;
  S->setSymbol(signatureSymbol->resolveInfo()->outSymbol());
  return true;
}

template <class ELFT>
eld::Expected<bool>
RelocELFReader<ELFT>::readRelocationSection(ELFSection *RS) {
  ASSERT(RS->isRelocationSection(), "RS must be a relocation section!");
  if (RS->isIgnore() || RS->isDiscard())
    return true;
  eld::Expected<bool> expReadRelocSect =
      (RS->isRela() ? readRelocationSection</*isRela=*/true>(RS)
                    : readRelocationSection</*isRela=*/false>(RS));
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expReadRelocSect);
  return expReadRelocSect.value();
}

template <class ELFT>
template <bool isRela>
eld::Expected<bool>
RelocELFReader<ELFT>::readRelocationSection(ELFSection *RS) {
  ASSERT(this->m_LLVMELFFile.has_value(), "m_LLVMELFFile must be initialized!");
  if (!this->m_RawSectHdrs)
    LLVMEXP_EXTRACT_AND_CHECK(this->m_RawSectHdrs,
                              this->m_LLVMELFFile->sections());
  ASSERT(this->m_RawSectHdrs, "m_RawSectHdrs must be initialized!");

  // using RelType =
  //     std::conditional_t<isRela, typename ELFReader<ELFT>::Elf_Rela,
  //                        typename ELFReader<ELFT>::Elf_Rel>;
  // using RelRangeType =
  //     std::conditional_t<isRela, typename ELFReader<ELFT>::Elf_Rela_Range,
  //                        typename ELFReader<ELFT>::Elf_Rel_Range>;

  const typename ELFReader<ELFT>::Elf_Shdr &rawS =
      (*this->m_RawSectHdrs)[RS->getIndex()];

  auto expRelRange = ELFReader<ELFT>::template getRelocations<isRela>(rawS);
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expRelRange);
  auto relRange = std::move(expRelRange.value());

  GNULDBackend &backend = *(this->m_Module.getBackend());
  InputFile *inputFile = this->getInputFile();
  ELFObjectFile *EObj = llvm::cast<ELFObjectFile>(inputFile);

  for (const auto &R : relRange) {
    uint32_t rSym = R.getSymbol(/*isMips=*/false);
    LDSymbol *symbol = EObj->getSymbol(rSym);
    if (!symbol) {
      return std::make_unique<plugin::DiagnosticEntry>(plugin::DiagnosticEntry(
          Diag::err_cannot_read_symbol,
          {std::to_string(rSym),
           inputFile->getInput()->getResolvedPath().getFullPath()}));
    }
    if (EObj && EObj->isLTOObject())
      symbol = fixWrapSyms(symbol);

    // If the symbol is a section symbol (or) if the symbol is a local symbol
    // and that symbol belongs to a merge string section, create the relocation
    // as part of the section itself.
    ResolveInfo *rInfo = symbol->resolveInfo();
    if (rInfo->type() == ResolveInfo::Section) {
      ELFSection *section = EObj->getELFSection(symbol->sectionIndex());
      if (section)
        section->setWanted(true);
    }

    auto *linkSect = llvm::dyn_cast_or_null<ELFSection>(RS->getLink());
    Relocation::Type rType =
        ELFReader<ELFT>::template getRelocationType<isRela>(R);
    typename ELFReader<ELFT>::intX_t rAddend = ELFReader<ELFT>::getAddend(R);

    if (backend.handleRelocation(linkSect, rType, *symbol, R.r_offset, rAddend))
      continue;

    Relocation *relocation = eld::IRBuilder::addRelocation(
        backend.getRelocator(), linkSect, rType, *symbol, R.r_offset, rAddend);
    if (relocation)
      linkSect->addRelocation(relocation);
  }
  return backend.handlePendingRelocations(RS->getLink());
}

template <class ELFT>
LDSymbol *RelocELFReader<ELFT>::fixWrapSyms(LDSymbol *sym) {
  llvm::StringRef name(sym->name());
  if (!this->m_Module.hasWrapReference(name))
    return sym;

  std::string realName;
  if (name.starts_with("__real_"))
    realName = name.drop_front(7).str();
  else
    realName = std::string("__wrap_") + sym->name();

  LDSymbol *realSym = this->m_Module.getNamePool().findSymbol(realName);
  if (!realSym)
    realSym = createUndefReference(realName);
  LinkerConfig &config = this->m_Module.getConfig();
  if (this->m_Module.getPrinter()->traceWrapSymbols())
    config.raise(Diag::real_sym_reference) << realName << sym->name();
  return realSym;
}

template <class ELFT>
LDSymbol *RelocELFReader<ELFT>::createUndefReference(llvm::StringRef symName) {
  Resolver::Result result;
  InputFile *inputFile = this->getInputFile();
  this->m_Module.getNamePool().insertSymbol(
      inputFile, symName.str(), false, eld::ResolveInfo::NoType,
      eld::ResolveInfo::Undefined, eld::ResolveInfo::Global, 0, 0,
      eld::ResolveInfo::Default, nullptr, result, false, false, 0,
      false /* isPatchable */, this->m_Module.getPrinter());

  LDSymbol *sym = make<LDSymbol>(result.Info, false);
  result.Info->setOutSymbol(sym);
  result.Info->setResolvedOrigin(inputFile);
  return sym;
}

template class RelocELFReader<llvm::object::ELF32LE>;
template class RelocELFReader<llvm::object::ELF64LE>;
} // namespace eld
