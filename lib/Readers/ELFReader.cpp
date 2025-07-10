//===- ELFReader.cpp-------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Readers/ELFReader.h"
#include "eld/Config/GeneralOptions.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Core/Module.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Input/ELFFileBase.h"
#include "eld/Input/ELFObjectFile.h"
#include "eld/Input/InputFile.h"
#include "eld/Input/ObjectFile.h"
#include "eld/PluginAPI/DiagnosticEntry.h"
#include "eld/Readers/ELFSection.h"
#include "eld/SymbolResolver/IRBuilder.h"
#include "eld/SymbolResolver/LDSymbol.h"
#include "eld/SymbolResolver/ResolveInfo.h"
#include "eld/Target/GNULDBackend.h"
#include "eld/Target/LDFileFormat.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/Object/ELF.h"
#include "llvm/Object/ELFTypes.h"
#include "llvm/Support/Compression.h"
#include "llvm/Support/Error.h"
#include <string>

namespace eld {

template <class ELFT>
ELFReader<ELFT>::ELFReader(Module &module, InputFile &inputFile,
                           plugin::DiagnosticEntry &diagEntry)
    : ELFReaderBase(module, inputFile),
      m_LLVMELFFile(
          createLLVMELFFile(module.getConfig(), inputFile, diagEntry)) {}

template <class ELFT>
std::optional<llvm::object::ELFFile<ELFT>>
ELFReader<ELFT>::createLLVMELFFile(LinkerConfig &config, const InputFile &input,
                                   plugin::DiagnosticEntry &diagEntry) {
  llvm::Expected<llvm::object::ELFFile<ELFT>> exp =
      llvm::object::ELFFile<ELFT>::create(
          input.getInput()->getMemoryBufferRef().getBuffer());
  if (!exp) {
    diagEntry = config.getDiagEngine()->convertToDiagEntry(exp.takeError());
    return {};
  }
  return exp.get();
}

template <class ELFT>
eld::Expected<std::string>
ELFReader<ELFT>::getSectionName(Elf_Shdr rawSectHdr) {
  ASSERT(m_LLVMELFFile, "m_LLVMELFFile must be initialized!");

  llvm::Expected<llvm::StringRef> expSectName =
      m_LLVMELFFile->getSectionName(rawSectHdr);
  LLVMEXP_RETURN_DIAGENTRY_IF_ERROR(expSectName);
  llvm::StringRef sectionName = expSectName.get();
  return sectionName.str();
}

template <class ELFT>
LDFileFormat::Kind
ELFReader<ELFT>::getSectionKind(Elf_Shdr rawSectHdr,
                                llvm::StringRef sectionName) {
  LinkerConfig &config = m_Module.getConfig();
  LDFileFormat::Kind kind = LDFileFormat::getELFSectionKind(
      rawSectHdr.sh_flags, rawSectHdr.sh_addralign, rawSectHdr.sh_entsize,
      rawSectHdr.sh_type, sectionName, config);
  return kind;
}

template <class ELFT>
void ELFReader<ELFT>::setSectionAttributes(ELFSection *S, Elf_Shdr rawSectHdr) {
  S->setAddrAlign(rawSectHdr.sh_addralign);
  S->setSize(rawSectHdr.sh_size);
  S->setOffset(rawSectHdr.sh_offset);
  S->setInfo(rawSectHdr.sh_info);
  S->setAddr(rawSectHdr.sh_addr);
  S->setInputFile(&m_InputFile);
}

template <class ELFT>
void ELFReader<ELFT>::setSectionInInputFile(ELFSection *S,
                                            Elf_Shdr rawSectHdr) {
  ELFFileBase *EFileBase = llvm::cast<ELFFileBase>(&m_InputFile);
  bool isDynObj = m_InputFile.isDynamicLibrary();

  switch (rawSectHdr.sh_type) {
  case llvm::ELF::SHT_DYNAMIC:
    EFileBase->setDynamic(S);
    break;
  case llvm::ELF::SHT_DYNSYM:
    EFileBase->setSymbolTable(S);
    break;
  case llvm::ELF::SHT_SYMTAB:
    if (!isDynObj)
      EFileBase->setSymbolTable(S);
    break;
  case llvm::ELF::SHT_SYMTAB_SHNDX:
    EFileBase->setExtendedSymbolTable(S);
    break;
  case llvm::ELF::SHT_STRTAB:
    EFileBase->setStringTable(S);
    break;
  default:
    break;
  }

  if (!isDynObj && S->name() == ".llvmbc") {
    ELFObjectFile *EObj = llvm::cast<ELFObjectFile>(&m_InputFile);
    EObj->setLLVMBCSection(S);
  }

  EFileBase->addSection(S);
}

template <class ELFT> bool ELFReader<ELFT>::setLinkInfoAttributes() {
  ASSERT(m_LLVMELFFile.has_value(), "m_LLVMELFFile must be initialized!");
  ASSERT(m_RawSectHdrs.has_value(), "m_RawSectHdrs must be initialized!");

  ELFFileBase *EFileBase = llvm::cast<ELFFileBase>(&m_InputFile);
  for (std::size_t i = 0; i < m_RawSectHdrs->size(); ++i) {
    ELFSection *S = llvm::cast<ELFSection>(EFileBase->getSection(i));
    Elf_Shdr rawSectHdr = (*m_RawSectHdrs)[i];
    if (rawSectHdr.sh_link != 0x0 || rawSectHdr.sh_info != 0x0) {
      if (S->isRelocationKind())
        S->setLink(EFileBase->getELFSection(rawSectHdr.sh_info));
      else
        S->setLink(EFileBase->getELFSection(rawSectHdr.sh_link));
      if (S->isLinkOrder())
        EFileBase->getELFSection(rawSectHdr.sh_link)->addDependentSection(S);
    }
  }
  return true;
}

template <class ELFT>
void ELFReader<ELFT>::processAndReportSymbolAliases(
    const AliasMap &DynObjAliasSymbolMap,
    const GlobalSymbolMap &DynObjGlobalSymbolMap) const {
  if (DynObjAliasSymbolMap.empty())
    return;
  for (auto &alias : DynObjAliasSymbolMap) {
    auto globalSym = DynObjGlobalSymbolMap.find(alias.first);
    if (globalSym == DynObjGlobalSymbolMap.end())
      continue;
    std::vector<std::string> aliasNames;
    for (auto &aliasSyms : alias.second) {
      aliasNames.push_back(aliasSyms->resolveInfo()->name());
      aliasSyms->resolveInfo()->setAlias(globalSym->second->resolveInfo());
    }
    if (!aliasNames.empty())
      m_Module.getConfig().raise(Diag::verbose_symbol_has_aliases)
          << globalSym->second->resolveInfo()->getDecoratedName(
                 m_Module.getConfig().options().shouldDemangle())
          << globalSym->second->resolveInfo()
                 ->resolvedOrigin()
                 ->getInput()
                 ->decoratedPath()
          << llvm::join(aliasNames.begin(), aliasNames.end(), ", ");
  }
}

template <class ELFT>
bool ELFReader<ELFT>::verifyFile(DiagnosticEngine *diagEngine) {
  ELFFileBase *EFileBase = llvm::cast<ELFFileBase>(&m_InputFile);
  bool verify = true;
  for (const Section *S : EFileBase->getSections())
    verify = llvm::cast<ELFSection>(S)->verify(diagEngine) && verify;
  return verify;
}

template <class ELFT> eld::Expected<bool> ELFReader<ELFT>::checkFlags() const {
  ASSERT(m_LLVMELFFile, "m_LLVMELFFile must be initialized!");
  GNULDBackend &backend = *m_Module.getBackend();
  Elf_Ehdr elfHeader = m_LLVMELFFile->getHeader();
  return backend.getInfo().checkFlags(elfHeader.e_flags, &m_InputFile);
}

template <class ELFT>
const typename ELFReader<ELFT>::Elf_Shdr *
ELFReader<ELFT>::findSection(llvm::ArrayRef<Elf_Shdr> sections, uint32_t type) {
  for (const Elf_Shdr &section : sections) {
    if (section.sh_type == type)
      return &section;
  }
  return nullptr;
}

template <class ELFT>
eld::Expected<uint32_t> ELFReader<ELFT>::getExtendedSymTabIdx(Elf_Sym rawSym,
                                                              uint32_t symIdx) {
  ASSERT(m_LLVMELFFile, "m_LLVMELFFile must be initialized!");
  ASSERT(m_RawSectHdrs, "m_RawSectHdrs must be initialized!");
  ELFFileBase *EFileBase = llvm::cast<ELFFileBase>(&m_InputFile);

  ASSERT(EFileBase->getExtendedSymbolTable(),
         "Extended symbol table must be present!");

  std::size_t extendedSymTabSecIdx =
      EFileBase->getExtendedSymbolTable()->getIndex();
  const Elf_Shdr &rawExtendedSymTabSec = (*m_RawSectHdrs)[extendedSymTabSecIdx];
  llvm::Expected<llvm::ArrayRef<Elf_Word>> expExtendedSymTab =
      m_LLVMELFFile->template getSectionContentsAsArray<Elf_Word>(
          rawExtendedSymTabSec);
  LLVMEXP_RETURN_DIAGENTRY_IF_ERROR(expExtendedSymTab);
  llvm::ArrayRef<Elf_Word> extendedSymTab = expExtendedSymTab.get();
  llvm::Expected<uint32_t> expIdx =
      llvm::object::getExtendedSymbolTableIndex<ELFT>(rawSym, symIdx,
                                                      extendedSymTab);
  LLVMEXP_RETURN_DIAGENTRY_IF_ERROR(expIdx);
  return expIdx.get();
}

template <class ELFT>
eld::Expected<LDSymbol *>
ELFReader<ELFT>::createSymbol(llvm::StringRef stringTable, Elf_Sym rawSym,
                              std::size_t idx, bool isPatchable) {
  GNULDBackend &backend = *m_Module.getBackend();
  IRBuilder &builder = *m_Module.getIRBuilder();
  ELFFileBase *EFileBase = llvm::cast<ELFFileBase>(&m_InputFile);

  bool is_ordinary = true;
  // FIXME: Instead of checking this when creating each symbol (which can be
  // relatively expensive as there can be too many symbols), can we instead set
  // this using 'e_shnum'? Or perhaps, by seeing if there is any entry in
  // section of type SHT_SYMTAB_SHNDX?
  if (rawSym.st_shndx == llvm::ELF::SHN_XINDEX) {
    EFileBase->setHasHighSectionCount();
    is_ordinary = false;
  }

  eld::Expected<uint32_t> expShndx = computeShndx(rawSym, idx);
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expShndx);
  uint32_t st_shndx = expShndx.value();

  // get section
  ELFSection *section = EFileBase->getELFSection(st_shndx);
  // get ld_type
  ResolveInfo::Type ldType = getSymbolType(rawSym.st_info, st_shndx);
  // All File symbols are absolute symbols.
  if (ldType == ResolveInfo::File)
    section = nullptr;
  ResolveInfo::Desc ldDesc = getSymbolDesc(
      backend, st_shndx, rawSym.st_info >> 4, m_InputFile, is_ordinary);
  ResolveInfo::Binding ldBinding =
      getSymbolBinding((rawSym.st_info >> 4), st_shndx, *EFileBase);
  uint64_t ldValue = rawSym.st_value;
  ResolveInfo::Visibility ldVis =
      static_cast<ResolveInfo::Visibility>(rawSym.st_other);
  // get ld_name
  eld::Expected<llvm::StringRef> expSymName =
      computeSymbolName(rawSym, section, stringTable, st_shndx, ldType);
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expSymName);
  llvm::StringRef ldName = expSymName.value();

  if (isPatchable) {
    if (ldDesc != ResolveInfo::Define || ldBinding != ResolveInfo::Global) {
      return std::make_unique<plugin::DiagnosticEntry>(plugin::DiagnosticEntry(
          Diag::error_patch_invalid_symbol,
          {ldName.str(), m_InputFile.getInput()->decoratedPath()}));
    }
  }

  if (section && ldType != ResolveInfo::Section)
    section->setWanted(true);

  bool isPostLTOPhase = backend.getModule().isPostLTOPhase();
  LDSymbol *sym = builder.addSymbol(
      m_InputFile, ldName.str(), ldType, ldDesc, ldBinding, rawSym.st_size,
      ldValue, section, ldVis, isPostLTOPhase, st_shndx, idx, isPatchable);
  eld::Expected<bool> expVerifySym = verifySymbol(sym);
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expVerifySym);
  return sym;
}

template <class ELFT> eld::Expected<bool> ELFReader<ELFT>::readSymbols() {
  ASSERT(m_LLVMELFFile, "m_LLVMELFFile must be initialized!");
  if (!m_RawSectHdrs)
    LLVMEXP_EXTRACT_AND_CHECK(m_RawSectHdrs, m_LLVMELFFile->sections());
  ASSERT(m_RawSectHdrs, "m_RawSectHdrs must be initialized!");

  IRBuilder &builder = *m_Module.getIRBuilder();
  LinkerConfig &config = m_Module.getConfig();

  const Elf_Shdr *symTabSec = findSection(
      m_RawSectHdrs.value(),
      (m_InputFile.getKind() == InputFile::InputFileKind::ELFDynObjFileKind
           ? llvm::ELF::SHT_DYNSYM
           : llvm::ELF::SHT_SYMTAB));
  if (!symTabSec)
    return true;
  auto expElfSyms = m_LLVMELFFile->symbols(symTabSec);
  LLVMEXP_RETURN_DIAGENTRY_IF_ERROR(expElfSyms);
  llvm::ArrayRef<Elf_Sym> elfSyms = expElfSyms.get();

  llvm::Expected<llvm::StringRef> expStrTab =
      m_LLVMELFFile->getStringTableForSymtab(*symTabSec);
  LLVMEXP_RETURN_DIAGENTRY_IF_ERROR(expStrTab)
  llvm::StringRef strTab = expStrTab.get();

  if (builder.getModule().getPrinter()->traceSymbols())
    config.raise(Diag::process_file) << m_InputFile.getInput()->decoratedPath();

  switch (m_InputFile.getKind()) {
  case InputFile::ELFObjFileKind:
  case InputFile::ELFExecutableFileKind:
    break;
  default:
    if (m_Module.getConfig().options().isPatchEnable()) {
      return std::make_unique<plugin::DiagnosticEntry>(
          plugin::DiagnosticEntry(Diag::error_patch_dynamic_input,
                                  {m_InputFile.getInput()->decoratedPath()}));
    }
  }

  ELFFileBase *EFileBase = llvm::cast<ELFFileBase>(&m_InputFile);

  // skip the first nullptr symbol
  EFileBase->addSymbol(LDSymbol::null());

  auto IsPatchableAlias = [&](llvm::StringRef &Name) -> bool {
    // TODO: Check other symbol attributes e.g. type value binding etc.
    return Name.consume_front("__llvm_patchable_");
  };

  llvm::StringSet<> PatchableSymbols;
  // Read the patchable attribute when linking the base image or only from the
  // base image when linking the patch. Knowing the patchable attribute for all
  // symbols in the base image is obviously needed. When building the patch, we
  // also need to know which symbols were patchable because it will affect
  // symbol resolution. Also, we ignore patchable attributes from other files in
  // the patch link, which will include either the patched code itself or
  // libraries. In the patch code, the attribute is not needed. The case of
  // libraries is interesting but less certain: in case a library has patchable
  // symbols, the user may want to compile it once with patching enabled,
  // instead of compiling two versions, a patchable and unpatchable one. A
  // reason not to do so is that a patchable version will be less optimized. To
  // support this case, we should accept patchable object files in the patch
  // link but ignore the patchable attribute.
  if (m_Module.getConfig().options().isPatchEnable() ||
      m_InputFile.getInput()->getAttribute().isPatchBase()) {
    for (const Elf_Sym &rawSym : elfSyms) {
      llvm::Expected<llvm::StringRef> expName = rawSym.getName(strTab);
      LLVMEXP_RETURN_DIAGENTRY_IF_ERROR(expName);
      llvm::StringRef Name = *expName;
      if (IsPatchableAlias(Name))
        PatchableSymbols.insert(Name);
    }
  }

  for (size_t idx = 1; idx < elfSyms.size(); ++idx) {
    const Elf_Sym &rawSym = elfSyms[idx];
    llvm::Expected<llvm::StringRef> expLdName = rawSym.getName(strTab);
    LLVMEXP_RETURN_DIAGENTRY_IF_ERROR(expLdName);
    eld::Expected<LDSymbol *> expSym = createSymbol(
        strTab, rawSym, idx, PatchableSymbols.contains(*expLdName));
    ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expSym);
  }
  return true;
}

template <class ELFT> std::string ELFReader<ELFT>::getFlagString() const {
  ASSERT(m_LLVMELFFile, "m_LLVMELFFile must be initialized!");
  GNULDBackend &backend = *m_Module.getBackend();
  Elf_Ehdr elfHeader = m_LLVMELFFile->getHeader();
  return backend.getInfo().flagString(elfHeader.e_flags);
}

template <class ELFT> void ELFReader<ELFT>::recordInputActions() const {
  ASSERT(m_LLVMELFFile, "m_LLVMELFFile must be initialized!");
  LayoutInfo *layoutInfo = m_Module.getLayoutInfo();
  if (layoutInfo) {
    std::string flagStr = getFlagString();
    if (!flagStr.empty()) {
      flagStr = "[" + flagStr + "]";
      layoutInfo->recordInputActions(LayoutInfo::Load, m_InputFile.getInput(),
                                     flagStr);
    }
  }
}

template <class ELFT> bool ELFReader<ELFT>::checkMachine() const {
  ASSERT(m_LLVMELFFile, "m_LLVMELFFile must be initialized!");
  GNULDBackend &backend = *m_Module.getBackend();
  const Elf_Ehdr &elfHeader = m_LLVMELFFile->getHeader();
  return elfHeader.e_machine == backend.getInfo().machine();
}

template <class ELFT> bool ELFReader<ELFT>::checkClass() const {
  ASSERT(m_LLVMELFFile, "m_LLVMELFFile must be initialized!");
  GNULDBackend &backend = *m_Module.getBackend();
  const Elf_Ehdr &elfHeader = m_LLVMELFFile->getHeader();
  return elfHeader.e_ident[llvm::ELF::EI_CLASS] == backend.getInfo().ELFClass();
}

template <class ELFT>
eld::Expected<bool> ELFReader<ELFT>::isCompatible() const {
  const InputFile &inputFile = this->m_InputFile;
  LinkerConfig &config = this->m_Module.getConfig();

  if (!checkMachine())
    return std::make_unique<plugin::DiagnosticEntry>(plugin::DiagnosticEntry(
        Diag::err_unrecognized_input_file,
        {inputFile.getInput()->getResolvedPath().native(),
         config.targets().triple().str()}));

  eld::Expected<bool> expCheckFlags = checkFlags();
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expCheckFlags);
  if (!expCheckFlags.value())
    return false;

  if (!checkClass())
    return std::make_unique<plugin::DiagnosticEntry>(plugin::DiagnosticEntry(
        Diag::invalid_elf_class,
        {inputFile.getInput()->decoratedPath(), config.targets().getArch()}));

  return true;
}

template <class ELFT>
eld::Expected<uint32_t> ELFReader<ELFT>::computeShndx(const Elf_Sym &rawSym,
                                                      std::size_t idx) {
  ELFFileBase *EFileBase = llvm::cast<ELFFileBase>(&this->m_InputFile);
  uint32_t st_shndx = rawSym.st_shndx;
  // If the section should not be included, set the st_shndx to SHN_UNDEF
  // - A section in interrelated groups are not included.
  if (st_shndx < llvm::ELF::SHN_LORESERVE && st_shndx != llvm::ELF::SHN_UNDEF) {
    if (!EFileBase->getELFSection(st_shndx))
      st_shndx = llvm::ELF::SHN_UNDEF;
  }

  if (st_shndx == llvm::ELF::SHN_XINDEX) {
    eld::Expected<uint32_t> expShndx = getExtendedSymTabIdx(rawSym, idx);
    ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expShndx);
    return expShndx.value();
  }
  return st_shndx;
}

template <class ELFT>
eld::Expected<llvm::StringRef> ELFReader<ELFT>::computeSymbolName(
    const Elf_Sym &rawSym, const ELFSection *S, llvm::StringRef stringTable,
    uint32_t st_shndx, ResolveInfo::Type ldType) const {
  if (ldType == ResolveInfo::Section) {
    if (!S)
      return std::make_unique<plugin::DiagnosticEntry>(plugin::DiagnosticEntry(
          Diag::idx_sect_not_found,
          {std::to_string(st_shndx), m_InputFile.getInput()->decoratedPath()}));
    return S->name();
  } else {
    llvm::Expected<llvm::StringRef> expLdName = rawSym.getName(stringTable);
    LLVMEXP_RETURN_DIAGENTRY_IF_ERROR(expLdName);
    return expLdName.get();
  }
}

template <class ELFT>
eld::Expected<bool> ELFReader<ELFT>::verifySymbol(const LDSymbol *sym) {
  LinkerConfig &config = m_Module.getConfig();
  if (!config.showZeroSizedSectionsWarnings() || !sym || !sym->hasFragRef() ||
      !sym->size())
    return true;
  const FragmentRef *fragRef = sym->fragRef();
  const Fragment *frag = fragRef->frag();
  // Warn if fragment size is zero and the recorded symbol is non zero
  if (frag && !frag->size())
    config.raise(Diag::warning_zero_sized_fragment_for_non_zero_symbol)
        << frag->getOwningSection()->name() << sym->name()
        << sym->resolveInfo()->resolvedOrigin()->getInput()->getName();
  return true;
}

template <class ELFT>
void ELFReader<ELFT>::checkAndMayBeReportZeroSizedSection(
    const LDSymbol *sym) const {
  LinkerConfig &config = this->m_Module.getConfig();
  if (!config.showZeroSizedSectionsWarnings() || !sym || !sym->hasFragRef() ||
      !sym->size())
    return;
  const FragmentRef *fragRef = sym->fragRef();
  const Fragment *frag = fragRef->frag();
  // Warn if fragment size is zero and the recorded symbol is non zero
  if (frag && !frag->size())
    config.raise(Diag::warning_zero_sized_fragment_for_non_zero_symbol)
        << frag->getOwningSection()->name() << sym->name()
        << sym->resolveInfo()->resolvedOrigin()->getInput()->getName();
}

template <class ELFT>
eld::Expected<bool> ELFReader<ELFT>::readCompressedSection(ELFSection *S) {
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
  LayoutInfo *layoutInfo = this->m_Module.getLayoutInfo();
  if (layoutInfo)
    layoutInfo->recordFragment(&this->m_InputFile, S, frag);

  return true;
}

template <class ELFT>
eld::Expected<bool> ELFReader<ELFT>::readMergeStringSection(ELFSection *S) {
  LinkerConfig &config = this->m_Module.getConfig();
  llvm::StringRef Contents = S->getContents();
  if (Contents.empty())
    return true;
  MergeStringFragment *F = make<MergeStringFragment>(S);
  if (!F->readStrings(config))
    return false;
  S->addFragment(F);
  LayoutInfo *layoutInfo = this->m_Module.getLayoutInfo();
  if (layoutInfo)
    layoutInfo->recordFragment(&this->m_InputFile, S, F);
  return true;
}

template class ELFReader<llvm::object::ELF32LE>;
template class ELFReader<llvm::object::ELF64LE>;
} // namespace eld
