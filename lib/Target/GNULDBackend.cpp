//===- GNULDBackend.cpp----------------------------------------------------===//
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
#include "eld/Target/GNULDBackend.h"
#include "eld/BranchIsland/BranchIslandFactory.h"
#include "eld/BranchIsland/StubFactory.h"
#include "eld/Config/Config.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Config/Version.h"
#include "eld/Core/LinkerScript.h"
#include "eld/Core/Module.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Diagnostics/DiagnosticInfos.h"
#include "eld/Fragment/BuildIDFragment.h"
#include "eld/Fragment/FillFragment.h"
#include "eld/Fragment/GNUHashFragment.h"
#include "eld/Fragment/RegionFragmentEx.h"
#include "eld/Fragment/StringFragment.h"
#include "eld/Fragment/SysVHashFragment.h"
#include "eld/Fragment/TimingFragment.h"
#include "eld/Input/ELFDynObjectFile.h"
#include "eld/Input/ELFObjectFile.h"
#include "eld/Input/InputTree.h"
#include "eld/LayoutMap/LayoutInfo.h"
#include "eld/LayoutMap/TextLayoutPrinter.h"
#include "eld/LayoutMap/YamlLayoutPrinter.h"
#include "eld/Object/ObjectBuilder.h"
#include "eld/Object/ObjectLinker.h"
#include "eld/Object/ScriptMemoryRegion.h"
#include "eld/Object/SectionMap.h"
#include "eld/PluginAPI/ControlFileSizePlugin.h"
#include "eld/PluginAPI/ControlMemorySizePlugin.h"
#include "eld/PluginAPI/PluginADT.h"
#include "eld/PluginAPI/PluginBase.h"
#include "eld/Readers/ArchiveParser.h"
#include "eld/Readers/BinaryFileParser.h"
#include "eld/Readers/BitcodeReader.h"
#include "eld/Readers/CommonELFSection.h"
#include "eld/Readers/ELFDynObjParser.h"
#include "eld/Readers/ELFExecObjParser.h"
#include "eld/Readers/ELFRelocObjParser.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Readers/EhFrameHdrSection.h"
#include "eld/Script/MemoryCmd.h"
#include "eld/Script/OutputSectDesc.h"
#include "eld/Script/StrToken.h"
#include "eld/Script/StringList.h"
#include "eld/Support/DynamicLibrary.h"
#include "eld/Support/Memory.h"
#include "eld/Support/MsgHandling.h"
#include "eld/Support/RegisterTimer.h"
#include "eld/Support/StringRefUtils.h"
#include "eld/Support/Utils.h"
#include "eld/SymbolResolver/IRBuilder.h"
#include "eld/SymbolResolver/LDSymbol.h"
#include "eld/SymbolResolver/ResolveInfo.h"
#include "eld/Target/ELFDynamic.h"
#include "eld/Target/ELFFileFormat.h"
#include "eld/Target/ELFSegment.h"
#include "eld/Target/ELFSegmentFactory.h"
#include "eld/Target/LDFileFormat.h"
#include "eld/Target/Relocator.h"
#include "eld/Target/TargetInfo.h"
#include "eld/Writers/SymDefWriter.h"
#include "llvm/ADT/Hashing.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/BinaryStreamWriter.h"
#include "llvm/Support/FileOutputBuffer.h"
#include "llvm/Support/Memory.h"
#include "llvm/Support/ThreadPool.h"
#include <algorithm>
#include <chrono>
#include <climits>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {
uint64_t alignToELFSpec(uint64_t vma, uint64_t offset, uint64_t segAlign) {
  // Align according to ELF spec.
  if ((vma & (segAlign - 1)) != (offset & (segAlign - 1))) {
    int64_t padding = (vma & (segAlign - 1)) - (offset & (segAlign - 1));
    if (padding < 0)
      padding = segAlign + padding;
    offset += padding;
  }
  return offset;
}

} // anonymous namespace
using namespace eld;

// Initialize the TLS Base size.
uint64_t GNULDBackend::m_TLSBaseSize = 0;

//===----------------------------------------------------------------------===//
// GNULDBackend
//===----------------------------------------------------------------------===//
GNULDBackend::GNULDBackend(Module &pModule, TargetInfo *pInfo)
    : m_Module(pModule), m_pInfo(pInfo) {
  m_pELFSegmentTable = make<ELFSegmentFactory>();
  IsSectionTracingRequested = config().options().isSectionTracingRequested();
}

GNULDBackend::~GNULDBackend() {}

size_t GNULDBackend::sectionStartOffset() const {
  if (LinkerConfig::Binary == config().codeGenType())
    return 0x0;

  switch (config().targets().bitclass()) {
  case 32u:
    return sizeof(llvm::ELF::Elf32_Ehdr) +
           (elfSegmentTable().size() + numReservedSegments()) *
               sizeof(llvm::ELF::Elf32_Phdr);
  case 64u:
    return sizeof(llvm::ELF::Elf64_Ehdr) +
           (elfSegmentTable().size() + numReservedSegments()) *
               sizeof(llvm::ELF::Elf64_Phdr);
  default:
    config().raise(Diag::unsupported_bitclass)
        << config().targets().triple().str() << config().targets().bitclass();
    return 0;
  }
}

bool GNULDBackend::handleOrphanSection(const ELFSection *elem) const {
  if (!elem->size())
    return false;
  if (m_Module.getConfig().options().getOrphanMode() ==
      GeneralOptions::OrphanMode::Error) {
    m_Module.getConfig().raise(Diag::err_found_orphan_section)
        << elem->getDecoratedName(config().options());
    return true;
  } else if (m_Module.getConfig().options().getOrphanMode() ==
             GeneralOptions::OrphanMode::Warn) {
    m_Module.getConfig().raise(Diag::warn_found_orphan_section)
        << elem->getDecoratedName(config().options());
  }
  return false;
}

bool GNULDBackend::isSymbolStringTableSection(const ELFSection *elem) const {
  if (elem->getType() == llvm::ELF::SHT_SYMTAB ||
      elem->getType() == llvm::ELF::SHT_STRTAB)
    return true;
  return false;
}

bool GNULDBackend::isNonDymSymbolStringTableSection(
    const ELFSection *elem) const {
  if (isSymbolStringTableSection(elem) && !(elem->isAlloc()))
    return true;
  return false;
}

uint64_t GNULDBackend::getSectLink(const ELFSection *S) const {
  auto *O = S->getOutputSection();
  if (PluginLinkedSections.contains(O))
    return S->getLink()->getIndex();
  return llvm::ELF::SHN_UNDEF;
}

bool GNULDBackend::shouldskipAssert(const Assignment *Assign) const {
  return (!Assign->hasDot() && Assign->type() == Assignment::ASSERT);
}

LinkerConfig &GNULDBackend::config() const { return m_Module.getConfig(); }

void GNULDBackend::insertTimingFragmentStub() {
  llvm::StringRef moduleName = Saver.save(config().options().outputFileName());
  TimingFragment *F = make<TimingFragment>(0, 0, moduleName, m_pTiming);
  m_pTiming->addFragmentAndUpdateSize(F);
  LayoutInfo *layoutInfo = m_Module.getLayoutInfo();
  if (layoutInfo)
    layoutInfo->recordFragment(F->getOwningSection()->getInputFile(),
                      F->getOwningSection(), F);
  m_pTimingFragment = F;
}

eld::Expected<void> GNULDBackend::initStdSections() {
  eld::RegisterTimer T("Initialize ELF default sections", "Link Summary",
                       m_Module.getConfig().options().printTimingStats());
  m_pFileFormat = make<ELFFileFormat>();

  m_pFileFormat->initStdSections(m_Module, config().targets().bitclass());

  ELFSection *interp = nullptr;

  if (config().options().getInsertTimingStats()) {
    m_pTiming = m_Module.createInternalSection(
        Module::InternalInputType::Timing, LDFileFormat::Timing,
        ".note.qc.timing", llvm::ELF::SHT_NOTE, 0, 1);

    insertTimingFragmentStub();
  }

  // Interp section.
  if (!config().isCodeStatic() || config().options().isPIE() ||
      config().options().forceDynamic()) {
    if ((config().codeGenType() != LinkerConfig::DynObj) ||
        (config().options().isPIE())) {

      if (config().options().hasDynamicLinker()) {
        interp = m_Module.createInternalSection(
            Module::InternalInputType::DynamicSections, LDFileFormat::Note,
            ".interp", llvm::ELF::SHT_PROGBITS, llvm::ELF::SHF_ALLOC, 1);
      }
    }
    if ((GeneralOptions::SystemV == config().options().getHashStyle() ||
         (GeneralOptions::Both == config().options().getHashStyle()))) {
      m_pSysVHash = m_Module.createInternalSection(
          Module::InternalInputType::DynamicSections, LDFileFormat::Regular,
          ".hash", llvm::ELF::SHT_HASH, llvm::ELF::SHF_ALLOC, 4);
    }
    if ((GeneralOptions::GNU == config().options().getHashStyle() ||
         (GeneralOptions::Both == config().options().getHashStyle()))) {
      m_pGNUHash = m_Module.createInternalSection(
          Module::InternalInputType::DynamicSections, LDFileFormat::Regular,
          ".gnu.hash", llvm::ELF::SHT_GNU_HASH, llvm::ELF::SHF_ALLOC, 4);
    }
  }

  // Create a .comment section.
  if (LinkerConfig::Object != config().codeGenType()) {
    if (config().options().hasEhFrameHdr()) {
      m_pEhFrameHdrSection = m_Module.createEhFrameHdrSection(
          Module::InternalInputType::EhFrameHdr, ".eh_frame_hdr",
          llvm::ELF::SHT_PROGBITS, llvm::ELF::SHF_ALLOC,
          config().targets().is32Bits() ? 4 : 8);
    }
    m_pEhFrameFillerSection = m_Module.createInternalSection(
        Module::InternalInputType::EhFrameFiller, LDFileFormat::Regular,
        ".eh_frame", llvm::ELF::SHT_PROGBITS, llvm::ELF::SHF_ALLOC, 4);
  }

  m_pComment = m_Module.createInternalSection(
      Module::InternalInputType::LinkerVersion, LDFileFormat::Regular,
      ".comment", llvm::ELF::SHT_PROGBITS,
      llvm::ELF::SHF_MERGE | llvm::ELF::SHF_STRINGS, 1, 1);
  makeVersionString();

  if (config().options().isBuildIDEnabled()) {
    m_pBuildIDSection = m_Module.createInternalSection(
        Module::InternalInputType::GNUBuildID, LDFileFormat::Note,
        ".note.gnu.build-id", llvm::ELF::SHT_NOTE, llvm::ELF::SHF_ALLOC, 4);
    m_pBuildIDFragment = make<BuildIDFragment>(m_pBuildIDSection);
    eld::Expected<void> E = m_pBuildIDFragment->setBuildIDStyle(config());
    ELDEXP_RETURN_DIAGENTRY_IF_ERROR(E);
    m_pBuildIDSection->addFragmentAndUpdateSize(m_pBuildIDFragment);
  }

  if (!interp)
    return eld::Expected<void>();

  Fragment *F = nullptr;
  if (config().options().hasDyld())
    F = make<StringFragment>(config().options().dyld(), interp);
  else
    F = make<StringFragment>(m_pInfo->dyld(), interp);
  interp->addFragmentAndUpdateSize(F);

  LayoutInfo *layoutInfo = m_Module.getLayoutInfo();

  if (layoutInfo)
    layoutInfo->recordFragment(F->getOwningSection()->getInputFile(),
                      F->getOwningSection(), F);

  return eld::Expected<void>();
}

/// initStandardSymbols - define and initialize standard symbols.
/// This function is called after section merging but before read relocations.
bool GNULDBackend::initStandardSymbols() {
  eld::RegisterTimer T("Define and Initialize Standard Symbols",
                       "Add Default Standard Symbols",
                       m_Module.getConfig().options().printTimingStats());
  if (LinkerConfig::Object == config().codeGenType() ||
      m_Module.getScript().linkerScriptHasSectionsCommand())
    return true;

  auto MayDefineStandardSym = [&](llvm::StringRef Symbol) {
    m_ProvideStandardSymbols[Symbol] = nullptr;
  };

  auto InitStandardSym = [&](llvm::StringRef A, llvm::StringRef B,
                             ELFSection *O) {
    LDSymbol *SymA = nullptr;
    LDSymbol *SymB = nullptr;
    if (!A.empty()) {
      SymA = m_Module.getNamePool().findSymbol(A.str());
      if (SymA && SymA->isDyn())
        SymA = nullptr;
      if (SymA) {
        SymA->resolveInfo()->setDesc(ResolveInfo::Define);
        SymA->resolveInfo()->setBinding(ResolveInfo::Absolute);
        SymA->setShouldIgnore(false);
      }
    }
    if (!B.empty()) {
      SymB = m_Module.getNamePool().findSymbol(B.str());
      if (SymB && SymB->isDyn())
        SymB = nullptr;
      if (SymB) {
        SymB->resolveInfo()->setDesc(ResolveInfo::Define);
        SymB->resolveInfo()->setBinding(ResolveInfo::Absolute);
        SymB->setShouldIgnore(false);
      }
    }
  };

  InitStandardSym("__eh_frame_start", "__eh_frame_end",
                  m_Module.getSection(".eh_frame"));
  InitStandardSym("__eh_frame_hdr_start", "__eh_frame_hdr_end",
                  m_Module.getSection(".eh_frame_hdr"));
  InitStandardSym("__preinit_array_start", "__preinit_array_end",
                  m_Module.getSection(".preinit_array"));
  InitStandardSym("__init_array_start", "__init_array_end",
                  m_Module.getSection(".init_array"));
  InitStandardSym("__fini_array_start", "__fini_array_end",
                  m_Module.getSection(".fini_array"));
  InitStandardSym("_DYNAMIC", "", m_Module.getSection(".dynamic"));
  InitStandardSym("__executable_start", "", m_Module.getSection(".text"));
  InitStandardSym("", "etext", m_Module.getSection(".text"));
  InitStandardSym("", "_etext", m_Module.getSection(".text"));
  InitStandardSym("", "__etext", m_Module.getSection(".text"));
  InitStandardSym("", "edata", m_Module.getSection(".data"));
  InitStandardSym("", "_edata", m_Module.getSection(".data"));
  InitStandardSym("__bss_start", "end", m_Module.getSection(".bss"));
  InitStandardSym("", "_end", m_Module.getSection(".bss"));
  MayDefineStandardSym("__ehdr_start");
  MayDefineStandardSym("__dso_handle");
  return true;
}

void GNULDBackend::DefineStandardSymFromSegment(
    llvm::StringRef A, llvm::StringRef B, uint32_t IncludePermissions,
    uint32_t ExcludePermissions, int SymBAlign, bool isBSS, bool isBackWards,
    uint32_t SegType) {
  eld::RegisterTimer T("Define Standard Symbols from Segments",
                       "Perform Layout",
                       m_Module.getConfig().options().printTimingStats());
  LDSymbol *SymA = nullptr;
  LDSymbol *SymB = nullptr;
  ELFSegment *Seg = nullptr;
  if (isBackWards)
    Seg = elfSegmentTable().findr(SegType, IncludePermissions,
                                  ExcludePermissions);
  else
    Seg =
        elfSegmentTable().find(SegType, IncludePermissions, ExcludePermissions);
  if (!Seg)
    return;
  if (!A.empty()) {
    SymA = m_Module.getNamePool().findSymbol(A.str());
    if (SymA && SymA->isDyn())
      SymA = nullptr;
    if (SymA) {
      SymA->resolveInfo()->setBinding(ResolveInfo::Global);
      if (isBSS)
        SymA->setValue(Seg->vaddr() + Seg->filesz());
      else
        SymA->setValue(Seg->vaddr());
    }
  }
  if (!B.empty()) {
    SymB = m_Module.getNamePool().findSymbol(B.str());
    if (SymB && SymB->isDyn())
      SymB = nullptr;
    if (SymB) {
      SymB->resolveInfo()->setBinding(ResolveInfo::Global);
      SymB->setValue(Seg->vaddr() + Seg->memsz());
      if (SymBAlign)
        SymB->setValue((SymB->value() + SymBAlign - 1) & ~(SymBAlign - 1));
    }
  }
}

int64_t GNULDBackend::getPLTAddr(ResolveInfo *pInfo) const { return 0; }

bool GNULDBackend::finalizeStandardSymbols() {
  eld::RegisterTimer T("Finalize Standard Symbols", "Perform Layout",
                       m_Module.getConfig().options().printTimingStats());
  if (LinkerConfig::Object == config().codeGenType() ||
      m_Module.getScript().linkerScriptHasSectionsCommand())
    return true;

  auto DefineStandardSym = [&](llvm::StringRef A, llvm::StringRef B,
                               ELFSection *O) {
    LDSymbol *SymA = nullptr;
    LDSymbol *SymB = nullptr;
    if (!A.empty()) {
      SymA = m_Module.getNamePool().findSymbol(A.str());
      if (SymA && SymA->isDyn())
        SymA = nullptr;
      if (O && SymA) {
        SymA->resolveInfo()->setBinding(ResolveInfo::Global);
        SymA->setValue(O->addr());
      }
    }
    if (!B.empty()) {
      SymB = m_Module.getNamePool().findSymbol(B.str());
      if (SymB && SymB->isDyn())
        SymB = nullptr;
      if (O && SymB) {
        SymB->resolveInfo()->setBinding(ResolveInfo::Global);
        SymB->setValue(O->addr() + O->size());
      }
    }
  };

  DefineStandardSym("__eh_frame_start", "__eh_frame_end",
                    m_Module.getSection(".eh_frame"));
  DefineStandardSym("__eh_frame_hdr_start", "__eh_frame_hdr_end",
                    m_Module.getSection(".eh_frame_hdr"));
  DefineStandardSym("__preinit_array_start", "__preinit_array_end",
                    m_Module.getSection(".preinit_array"));
  DefineStandardSym("__init_array_start", "__init_array_end",
                    m_Module.getSection(".init_array"));
  DefineStandardSym("__fini_array_start", "__fini_array_end",
                    m_Module.getSection(".fini_array"));
  DefineStandardSym("_DYNAMIC", "", m_Module.getSection(".dynamic"));
  DefineStandardSymFromSegment("__executable_start", "", llvm::ELF::PF_X,
                               llvm::ELF::PF_W);
  DefineStandardSymFromSegment("", "etext", llvm::ELF::PF_X, llvm::ELF::PF_W);
  DefineStandardSymFromSegment("", "_etext", llvm::ELF::PF_X, llvm::ELF::PF_W);
  DefineStandardSymFromSegment("", "__etext", llvm::ELF::PF_X, llvm::ELF::PF_W);
  DefineStandardSymFromSegment("", "edata", llvm::ELF::PF_W, llvm::ELF::PF_X);
  DefineStandardSymFromSegment("", "_edata", llvm::ELF::PF_W, llvm::ELF::PF_X);
  DefineStandardSymFromSegment("__bss_start", "end", llvm::ELF::PF_W,
                               llvm::ELF::PF_X, 8, true, true);
  DefineStandardSymFromSegment("", "_end", llvm::ELF::PF_W, llvm::ELF::PF_X, 8,
                               true, true);
  return true;
}

bool GNULDBackend::isStandardSymbol(llvm::StringRef Symbol) const {
  return (m_ProvideStandardSymbols.find(Symbol) !=
          m_ProvideStandardSymbols.end());
}

void GNULDBackend::setStandardSymbol(llvm::StringRef Symbol, ResolveInfo *R) {
  ASSERT(m_ProvideStandardSymbols[Symbol] == nullptr,
         "Setting standard symbol " + Symbol.str() + "again!");
  m_ProvideStandardSymbols[Symbol] = R;
}

ResolveInfo *GNULDBackend::getStandardSymbol(llvm::StringRef Symbol) {
  auto P = m_ProvideStandardSymbols.find(Symbol);
  if (P == m_ProvideStandardSymbols.end())
    return nullptr;
  return P->second;
}

llvm::StringRef
GNULDBackend::parseSectionMagicSymbol(llvm::StringRef SymbolName) {
  llvm::StringRef SectionName;
  if (SymbolName.starts_with("__start_")) {
    SectionName = SymbolName.drop_front(8);
  } else if (SymbolName.starts_with("__stop_")) {
    SectionName = SymbolName.drop_front(7);
  } else
    return {};
  if (!string::isValidCIdentifier(SectionName))
    return {};
  return SectionName;
}

bool GNULDBackend::isSectionMagicSymbol(llvm::StringRef SymbolName) {
  llvm::StringRef SectionName = parseSectionMagicSymbol(SymbolName);
  return !SectionName.empty();
}

void GNULDBackend::defineStandardAndSectionMagicSymbol(const ResolveInfo &R) {
  // all references to __start_SECTION and __stop_SECTION must be
  // defined if they are referred.
  assert(R.isUndef());
  llvm::StringRef SymbolName = R.getName();
  llvm::StringRef SectionName = parseSectionMagicSymbol(SymbolName);

  // isStandardSymbol does not need to be called in a critical section because
  // the map is prepopulated with NULL pointers for each standard symbol.
  if (SectionName.empty()) {
    if (isStandardSymbol(SymbolName)) {
      LDSymbol *magic_sym =
          m_Module.getIRBuilder()
              ->addSymbol<IRBuilder::AsReferred, IRBuilder::Resolve>(
                  m_Module.getInternalInput(Module::Script), SymbolName.str(),
                  ResolveInfo::NoType, ResolveInfo::Define, ResolveInfo::Global,
                  0x0,                 // size
                  0x0,                 // value
                  FragmentRef::null(), // FragRef
                  ResolveInfo::Default);
      if (magic_sym)
        magic_sym->setShouldIgnore(false);
      setStandardSymbol(SymbolName, magic_sym->resolveInfo());
    }
    return;
  }

  // Sections that are zero sized may not be emitted in the output. Its
  // desirable to search the sections that were placed in the sectionMap to
  // make the start and stop symbols point to them appropriately. These
  // symbols that are added do have a fragment but the section is not in the
  // output as they are zero sized, making those symbols ABSOLUTE symbols.
  ELFSection *section =
      m_Module.getScript().sectionMap().find(SectionName.str());
  if (section != nullptr && section->getOutputSection()->getFirstFrag()) {
    FragmentRef *magic_fragref = nullptr;
    Fragment *F = section->getOutputSection()->getFirstFrag();
    if (SymbolName.starts_with("__start_"))
      magic_fragref = make<FragmentRef>(*F, 0x0);
    else
      magic_fragref = make<FragmentRef>(*F, section->size());

    ResolveInfo info;
    info.override(*m_Module.getNamePool().findInfo(SymbolName.str()));
    LDSymbol *magic_sym =
        m_Module.getIRBuilder()
            ->addSymbol<IRBuilder::AsReferred, IRBuilder::Resolve>(
                m_Module.getInternalInput(Module::Script), SymbolName.str(),
                ResolveInfo::NoType, ResolveInfo::Define, ResolveInfo::Global,
                0x0,           // size
                0x0,           // value
                magic_fragref, // FragRef
                ResolveInfo::Default);
    if (magic_sym)
      magic_sym->setShouldIgnore(false);
    // all symbols are already finalized, hence define the symbols address
    if (magic_sym)
      magic_sym->setValue(section->addr() +
                          magic_fragref->getOutputOffset(m_Module));
  }
}

bool GNULDBackend::defineStandardAndSectionMagicSymbols() {
  for (auto &G : m_Module.getNamePool().getGlobals()) {
    ResolveInfo *R = G.getValue();
    if (R->isUndef())
      defineStandardAndSectionMagicSymbol(*R);
    // If the symbol is still undefined, check
    // if linker script/target can provide definition
    if (R->isUndef())
      canProvideSymbol(R);
  }
  return true;
}

void GNULDBackend::provideSymbols() {
  // PROVIDE symbols which are directly referenced by the user
  std::vector<ResolveInfo *> directlyReferencedProvideSyms;
  // PROVIDE symbols upon which directly referenced PROVIDE symbols
  // depend on.
  std::unordered_set<std::string> indirectlyReferencedProvideSyms;
  for (auto &G : m_Module.getNamePool().getGlobals()) {
    ResolveInfo *R = G.getValue();
    // Check if linker script/target can provide definition
    if (R->isUndef()) {
      LDSymbol *sym = canProvideSymbol(R);
      if (sym && ProvideMap.find(R->name()) != ProvideMap.end())
        directlyReferencedProvideSyms.push_back(R);
    }
  }
  for (const ResolveInfo *R : directlyReferencedProvideSyms)
    findIndirectlyReferencedProvideSyms(R->name(),
                                        indirectlyReferencedProvideSyms);
  for (const std::string &symName : indirectlyReferencedProvideSyms)
    canProvideSymbol(symName);
}

void GNULDBackend::findIndirectlyReferencedProvideSyms(
    llvm::StringRef symName,
    std::unordered_set<std::string> &indirectReferenceProvideSyms) {
  const NamePool &NP = m_Module.getNamePool();
  auto assign = m_Module.getAssignmentForSymbol(symName);
  // This happens when an undefined symbol is used in the PROVIDE command.
  // PROVIDE(sym = UNDEF_SYM)
  // UNDEF_SYM does not have any corresponding assignment expression.
  if (!assign)
    return;
  std::unordered_set<std::string> referencedSymNames;
  for (const std::string &name : assign->getSymbolNames()) {
    if (!indirectReferenceProvideSyms.count(name) && !NP.findInfo(name)) {
      indirectReferenceProvideSyms.insert(name);
      findIndirectlyReferencedProvideSyms(name, indirectReferenceProvideSyms);
    }
  }
}

uint64_t GNULDBackend::finalizeTLSSymbol(LDSymbol *pSymbol) {
  // Dont finalize TLS symbols with Partial Link.
  if (LinkerConfig::Object == config().codeGenType()) {
    if (!pSymbol->hasFragRef())
      return pSymbol->value();
    return pSymbol->fragRef()->getOutputOffset(m_Module);
  }

  // ignore if symbol has no fragRef
  // FIXME: This is probably wrong. If a symbol does not have a fragment ref
  // that implies the symbol is undefined, in that case, the symbol index should
  // be 0 instead of 1 (true).
  if (!pSymbol->hasFragRef())
    return true;

  // the value of a TLS symbol is the offset to the TLS segment
  std::vector<ELFSegment *> tls_segs =
      elfSegmentTable().getSegments(llvm::ELF::PT_TLS);
  if (!tls_segs.size()) {
    config().raise(Diag::no_pt_tls_segment);
    return false;
  }
  ELFSegment *tls_seg = nullptr;
  for (auto &seg : tls_segs) {
    if (seg->size()) {
      tls_seg = seg;
      break;
    }
  }
  if (!tls_seg) {
    config().raise(Diag::error_empty_pt_tls_segment);
    return false;
  }

  uint64_t value = pSymbol->fragRef()->getOutputOffset(m_Module);
  uint64_t addr = pSymbol->fragRef()->getOutputELFSection()->addr();
  return value + addr - tls_seg->vaddr();
}

ELFFileFormat *GNULDBackend::getOutputFormat() const { return m_pFileFormat; }

/// sizeShstrtab - compute the size of .shstrtab
void GNULDBackend::sizeShstrtab() {
  eld::RegisterTimer T("Compute the size of .shstrtab", "Perform Layout",
                       m_Module.getConfig().options().printTimingStats());
  size_t shstrtab = 0;
  // compute the size of .shstrtab section.
  Module::const_iterator sect, sectEnd = m_Module.end();
  for (sect = m_Module.begin(); sect != sectEnd; ++sect) {
    shstrtab += (*sect)->name().size() + 1;
  } // end of for
  getOutputFormat()->getShStrTab()->setSize(shstrtab);
}

bool GNULDBackend::canSkipSymbolFromExport(ResolveInfo *R, bool isEntry) const {
  const auto &found = SymbolScopes.find(R);
  if (found != SymbolScopes.end() && !(found->second->isGlobal()))
    return true;
  if (R->isUndef() && R->resolvedOrigin() && R->resolvedOrigin()->isInternal())
    return true;
  if (R->exportToDyn())
    return false;
  // For PIE, only symbols that really need to be exported are the only ones
  // that can be exported. Dynamic List will control this as well.
  if (config().options().isPIE() && !isEntry)
    return true;
  if (R->isAbsolute())
    return true;
  if (R->isLocal() || R->isDyn() || R->isHidden())
    return true;
  if (R->isCommon())
    return false;
  return false;
}

bool GNULDBackend::applyVersionScriptScopes() {
  eld::RegisterTimer T("Apply Version Script Scopes", "Output Symbols",
                       m_Module.getConfig().options().printTimingStats());
  if (!config().options().hasVersionScript())
    return true;
  for (auto &I : m_Module.getNamePool().getGlobals()) {
    ResolveInfo *R = I.getValue();
    const auto &found = SymbolScopes.find(R);
    if (found != SymbolScopes.end() && !(found->second->isGlobal()) &&
        !R->isUndef() && !R->isDyn())
      R->clearExportToDyn();
  }
  return true;
}

bool GNULDBackend::SetSymbolsToBeExported() {
  eld::RegisterTimer T("Set Symbols to be Exported", "Output Symbols",
                       m_Module.getConfig().options().printTimingStats());
  // If we are not building a forced dynamic executable, dont build any dynamic
  // namepools.
  if (config().isCodeStatic() && !config().options().forceDynamic())
    return false;

  applyVersionScriptScopes();

  // The entry symbol "main" if found, needs to be exported too!
  if (config().options().isPIE() || config().options().forceDynamic()) {
    LDSymbol *entry_sym = m_Module.getNamePool().findSymbol("main");
    if ((entry_sym != nullptr) && (entry_sym->hasFragRef())) {
      if (!canSkipSymbolFromExport(entry_sym->resolveInfo(), true))
        entry_sym->resolveInfo()->setExportToDyn();
    }
  }

  // Symbols specified with --export-dynamic-symbol must be exported if they
  // are defined
  auto &DynExpSymbols = config().options().getExportDynSymList();
  for (const auto &S : DynExpSymbols) {
    LDSymbol *sym = m_Module.getNamePool().findSymbol(S->name());
    assert(sym != nullptr);
    ResolveInfo *rInfo = sym->resolveInfo();
    if (!rInfo->isUndef() && rInfo->binding() != ResolveInfo::Local &&
        rInfo->visibility() == ResolveInfo::Default && !sym->shouldIgnore())
      rInfo->setExportToDyn();
  }

  // The default set of symbols that can be exported are symbols that have a
  // dynamic relocation.
  ELFSection *RelDyn = getRelaDyn();
  for (auto &R : RelDyn->getRelocations()) {
    if (hasSymInfo(R))
      R->symInfo()->setExportToDyn();
  }

  for (auto &P : getRelaPLT()->getRelocations()) {
    if (hasSymInfo(P))
      P->symInfo()->setExportToDyn();
  }

  // The linker will export all globals only if exportDynamic or
  bool ExportAllGlobals = (config().options().exportDynamic() ||
                           (config().codeGenType() == LinkerConfig::DynObj));
  if (!ExportAllGlobals)
    return true;
  for (auto &R : m_Module.getSymbols()) {
    if (canSkipSymbolFromExport(R))
      continue;
    R->setExportToDyn();
  }
  return true;
}

void GNULDBackend::sizeDynNamePools() {
  if (!SetSymbolsToBeExported())
    return;

  typedef std::vector<ResolveInfo *>::iterator RIter;

  std::vector<ResolveInfo *> &RVect = m_Module.getSymbols();

  // Partition the symbols so that all symbols that need to be exported are to
  // the end of the table.
  RIter PartitionBegin;
  {
    eld::RegisterTimer T("Partition Symbols for Export", "Output Symbols",
                         m_Module.getConfig().options().printTimingStats());
    PartitionBegin =
        std::stable_partition(RVect.begin(), RVect.end(),
                              [](ResolveInfo *R) { return !R->exportToDyn(); });

    DynamicSymbols.push_back(LDSymbol::null()->resolveInfo());

    // Move all the DynamicSymbols.
    std::move(PartitionBegin, RVect.end(), std::back_inserter(DynamicSymbols));
  }

  {
    eld::RegisterTimer T("Create System V Fragments", "Output Symbols",
                         m_Module.getConfig().options().printTimingStats());
    // Create a SysV Fragment.
    if (m_pSysVHash) {
      Fragment *F = nullptr;
      if (config().targets().is32Bits())
        F = make<SysVHashFragment<llvm::object::ELF32LE>>(m_pSysVHash,
                                                          DynamicSymbols);
      else
        F = make<SysVHashFragment<llvm::object::ELF64LE>>(m_pSysVHash,
                                                          DynamicSymbols);
      m_pSysVHash->addFragmentAndUpdateSize(F);
    }
  }

  {
    eld::RegisterTimer T("GNU Hash Fragments", "Output Symbols",
                         m_Module.getConfig().options().printTimingStats());
    // Create a GNU Hash Fragment.
    if (m_pGNUHash) {
      Fragment *F = nullptr;
      if (config().targets().is32Bits())
        F = make<GNUHashFragment<llvm::object::ELF32LE>>(m_pGNUHash,
                                                         DynamicSymbols);
      else
        F = make<GNUHashFragment<llvm::object::ELF64LE>>(m_pGNUHash,
                                                         DynamicSymbols);
      m_pGNUHash->addFragmentAndUpdateSize(F);
    }
  }
}

void GNULDBackend::createEhFrameFillerAndHdrSection() {
  eld::RegisterTimer T("Create EhFrame Hdr Output Section", "Perform Layout",
                       m_Module.getConfig().options().printTimingStats());

  static Fragment *m_pEhFrameFillerFragment = nullptr;
  // The LSB standard does not allow a .eh_frame section with zero
  // Call Frame Information records. glibc unwind-dw2-fde.c
  // classify_object_over_fdes expects there is a CIE record length 0 as a
  // terminator. Thus we add one unconditionally.
  if (!m_pEhFrameFillerFragment && m_pEhFrameFillerSection) {
    if (m_Module.getPrinter()->isVerbose())
      config().raise(Diag::verbose_ehframe_log) << "Creating EhFrame Filler";

    m_pEhFrameFillerFragment =
        make<FillFragment>(getModule(), 0, 4, m_pEhFrameFillerSection);
    m_pEhFrameFillerSection->addFragmentAndUpdateSize(m_pEhFrameFillerFragment);
  }

  if (!m_pEhFrameHdrFragment && m_pEhFrameHdrSection) {
    if (m_Module.getPrinter()->isVerbose())
      config().raise(Diag::verbose_ehframe_log) << "Creating EhFrame Hdr";
    m_pEhFrameHdrFragment = make<EhFrameHdrFragment>(
        m_pEhFrameHdrSection, m_EhFrameHdrContainsTable,
        config().targets().is64Bits());
    m_pEhFrameHdrSection->addFragmentAndUpdateSize(m_pEhFrameHdrFragment);
  }
}

void GNULDBackend::sizeDynamic() {
  eld::RegisterTimer T("Create Dynamic Output Section", "Perform Layout",
                       m_Module.getConfig().options().printTimingStats());
  if (config().isCodeStatic() && !config().options().forceDynamic()) {
    return;
  }
  ELFFileFormat *FileFormat = getOutputFormat();
  ASSERT(FileFormat, "Must not be null!");
  size_t symIdx = 0;
  for (auto &DynSym : DynamicSymbols) {
    m_pDynSymIndexMap[DynSym->outSymbol()] = symIdx;
    FileFormat->addStringToDynStrTab(std::string(DynSym->name()));
    ++symIdx;
  }
  if (config().codeGenType() == LinkerConfig::DynObj) {
    if (!config().options().soname().empty())
      FileFormat->addStringToDynStrTab(config().options().soname());
  }

  // add DT_NEEDED
  std::unordered_set<MemoryArea *> addedLibs;
  for (auto &lib : m_Module.getDynLibraryList()) {
    if (llvm::dyn_cast<ELFFileBase>(lib)->isELFNeeded()) {
      const ELFDynObjectFile *dynObjFile = llvm::cast<ELFDynObjectFile>(lib);
      if (addedLibs.count(dynObjFile->getInput()->getMemArea()))
          continue;
      addedLibs.insert(dynObjFile->getInput()->getMemArea());
      std::size_t SONameOffset = FileFormat->addStringToDynStrTab(
          dynObjFile->getSOName());
      auto DTEntry = dynamic()->reserveNeedEntry();
      DTEntry->setValue(llvm::ELF::DT_NEEDED, SONameOffset);
    }
  }

  // add DT_RUNPATH
  if (!config().options().getRpathList().empty()) {
    auto DTEntry = dynamic()->reserveNeedEntry();
    std::string RunPath;
    GeneralOptions::const_rpath_iterator rpath,
        rpathEnd = config().options().rpathEnd();
    for (rpath = config().options().rpathBegin(); rpath != rpathEnd; ++rpath) {
      RunPath += *rpath;
      if (rpath + 1 != rpathEnd)
        RunPath += ":";
    }
    std::size_t RunPathOffset = FileFormat->addStringToDynStrTab(RunPath);
    DTEntry->setValue(llvm::ELF::DT_RUNPATH, RunPathOffset);
  }
  // set size
  if (config().targets().is32Bits()) {
    getOutputFormat()->getDynSymTab()->setSize((DynamicSymbols.size()) *
                                               sizeof(llvm::ELF::Elf32_Sym));
  } else {
    getOutputFormat()->getDynSymTab()->setSize((DynamicSymbols.size()) *
                                               sizeof(llvm::ELF::Elf64_Sym));
  }
  dynamic()->reserveEntries(*getOutputFormat(), m_Module);
  getOutputFormat()->getDynStrTab()->setSize(FileFormat->getDynStrTabSize());
  getOutputFormat()->getDynamic()->setSize(dynamic()->numOfBytes());
}

void GNULDBackend::initSymTab() {
  eld::RegisterTimer T("Initialize Symbol Table", "Perform Layout",
                       m_Module.getConfig().options().printTimingStats());
  getOutputFormat()->getShStrTab()->setSize(0x1);

  if (config().options().getStripSymbolMode() ==
      GeneralOptions::StripAllSymbols)
    return;

  getOutputFormat()->getStrTab()->setSize(1);
  if (config().targets().is32Bits())
    getOutputFormat()->getSymTab()->setSize(sizeof(llvm::ELF::Elf32_Sym));
  else
    getOutputFormat()->getSymTab()->setSize(sizeof(llvm::ELF::Elf64_Sym));
  if (m_Module.size() >= llvm::ELF::SHN_LORESERVE)
    getOutputFormat()->getSymTabShndxr()->setSize(4);
}

void GNULDBackend::sizeSymTab() {
  eld::RegisterTimer T("Create Symbol Table", "Perform Layout",
                       m_Module.getConfig().options().printTimingStats());

  GeneralOptions::StripSymbolMode S = config().options().getStripSymbolMode();

  if (S == GeneralOptions::StripAllSymbols)
    return;

  bool isStripLocal = (S == GeneralOptions::StripLocals);

  size_t strtab = 1;
  std::vector<ResolveInfo *> &RVect = m_Module.getSymbols();
  uint64_t NumSymbols = 0;
  for (auto &Sym : RVect) {
    if ((isStripLocal && NumSymbols && Sym->isLocal()) ||
        m_SymbolsToRemove.count(Sym)) {
      continue;
    }
    strtab += (std::string(Sym->name()).size() + 1);
    ++NumSymbols;
  }
  getOutputFormat()->getStrTab()->setSize(strtab);
  if (config().targets().is32Bits())
    getOutputFormat()->getSymTab()->setSize(++NumSymbols *
                                            sizeof(llvm::ELF::Elf32_Sym));
  else
    getOutputFormat()->getSymTab()->setSize(++NumSymbols *
                                            sizeof(llvm::ELF::Elf64_Sym));
  if (getOutputFormat()->getSymTabShndxr()->size()) {
    getOutputFormat()->getSymTabShndxr()->setSize(NumSymbols * 4);
  } else {
    assert(m_Module.size() < llvm::ELF::SHN_LORESERVE &&
           "Didn't reserve extended symbol section");
  }
}

bool GNULDBackend::readSection(InputFile &pInput, ELFSection *S) {
  Fragment *F = nullptr;
  static LayoutInfo *layoutInfo = m_Module.getLayoutInfo();
  if (!S->size() || S->isNoBits())
    F = make<FillFragment>(getModule(), 0x0, S->size(), S, S->getAddrAlign());
  else {
    llvm::StringRef R = pInput.getSlice(S->offset(), S->size());
    F = make<RegionFragment>(R, S, Fragment::Type::Region, S->getAddrAlign());
  }
  S->addFragment(F);
  if (layoutInfo)
    layoutInfo->recordFragment(&pInput, S, F);
  return F;
}

// emit section data.
eld::Expected<uint64_t> GNULDBackend::emitSection(ELFSection *pSection,
                                                  MemoryRegion &pRegion) const {
  if (!pSection->hasSectionData())
    return pRegion.size();

  for (auto &frag : pSection->getFragmentList()) {
    eld::Expected<void> expEmit = frag->emit(pRegion, getModule());
    ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expEmit);
  }

  return pRegion.size();
}

void GNULDBackend::markSymbolForRemoval(const ResolveInfo *S) {
  m_SymbolsToRemove.insert(S);
}

/// emitSymbol32 - emit an ELF32 symbol
void GNULDBackend::emitSymbol32(llvm::ELF::Elf32_Sym &pSym, LDSymbol *pSymbol,
                                char *pStrtab, size_t pStrtabsize,
                                size_t pSymtabIdx, bool IsDynSymTab) {
  if (pSymbol->type() == ResolveInfo::Section)
    pSym.st_name = 0;
  else {
    if (IsDynSymTab) {
      auto optSymNameOffset =
          getOutputFormat()->getOffsetInDynStrTab(std::string(pSymbol->name()));
      ASSERT(optSymNameOffset.has_value(),
             "Symbol name must be present in .dynstr!");
      pSym.st_name = optSymNameOffset.value();
    } else {
      pSym.st_name = pStrtabsize;
      strcpy((pStrtab + pStrtabsize), pSymbol->name());
    }
  }
  if ((pSymbol->resolveInfo()->isUndef()) || (pSymbol->isDyn()))
    pSym.st_value = 0;
  else
    pSym.st_value = pSymbol->value();
  pSym.st_size = getSymbolSize(pSymbol);
  pSym.st_info = getSymbolInfo(pSymbol);
  pSym.st_other = pSymbol->visibility();
  pSym.st_shndx = getSymbolShndx(pSymbol).first;
}

/// emitSymbol64 - emit an ELF64 symbol
void GNULDBackend::emitSymbol64(llvm::ELF::Elf64_Sym &pSym, LDSymbol *pSymbol,
                                char *pStrtab, size_t pStrtabsize,
                                size_t pSymtabIdx, bool IsDynSymTab) {
  if (pSymbol->type() == ResolveInfo::Section)
    pSym.st_name = 0;
  else {
    if (IsDynSymTab) {
      auto optSymNameOffset =
          getOutputFormat()->getOffsetInDynStrTab(std::string(pSymbol->name()));
      ASSERT(optSymNameOffset.has_value(), "Symbol name (" +
                                               std::string(pSymbol->name()) +
                                               ") must be present in .dynstr!");
      pSym.st_name = optSymNameOffset.value();
    } else {
      pSym.st_name = pStrtabsize;
      strcpy((pStrtab + pStrtabsize), pSymbol->name());
    }
  }
  if ((pSymbol->resolveInfo()->isUndef()) || (pSymbol->isDyn()))
    pSym.st_value = 0;
  else
    pSym.st_value = pSymbol->value();
  pSym.st_size = getSymbolSize(pSymbol);
  pSym.st_info = getSymbolInfo(pSymbol);
  pSym.st_other = pSymbol->visibility();
  pSym.st_shndx = getSymbolShndx(pSymbol).first;
}

/// emitRegNamePools - emit regular name pools - .symtab, .strtab
///
/// the size of these tables should be computed before layout
/// layout should computes the start offset of these tables
eld::Expected<void>
GNULDBackend::emitRegNamePools(llvm::FileOutputBuffer &pOutput) {
  {
    eld::RegisterTimer T("Sort Symbols", "Emit Output File",
                         m_Module.getConfig().options().printTimingStats());
    m_Module.sortSymbols();
  }
  // Emit SymDef Information.
  {
    eld::RegisterTimer T("Emit SymDef Information", "Emit Output File",
                         m_Module.getConfig().options().printTimingStats());
    if (config().options().symDef()) {
      std::unique_ptr<SymDefWriter> symDefWriter(new SymDefWriter(config()));
      eld::Expected<void> E = symDefWriter->init();
      if (!E)
        return E;
      symDefWriter->writeSymDef(m_Module);
    }
  }

  GeneralOptions::StripSymbolMode S = config().options().getStripSymbolMode();

  if (S == GeneralOptions::StripAllSymbols)
    return {};

  bool isStripLocal = (S == GeneralOptions::StripLocals);

  ELFFileFormat *file_format = getOutputFormat();
  if (!file_format->hasSymTab())
    return {};

  ELFSection &symtab_sect = *file_format->getSymTab();
  ELFSection &strtab_sect = *file_format->getStrTab();

  MemoryRegion symtab_region =
      getFileOutputRegion(pOutput, symtab_sect.offset(), symtab_sect.size());
  MemoryRegion strtab_region =
      getFileOutputRegion(pOutput, strtab_sect.offset(), strtab_sect.size());

  // set up symtab_region
  llvm::ELF::Elf32_Sym *symtab32 = nullptr;
  llvm::ELF::Elf64_Sym *symtab64 = nullptr;
  if (config().targets().is32Bits())
    symtab32 = (llvm::ELF::Elf32_Sym *)symtab_region.begin();
  else if (config().targets().is64Bits())
    symtab64 = (llvm::ELF::Elf64_Sym *)symtab_region.begin();
  else {
    config().raise(Diag::unsupported_bitclass)
        << config().targets().triple().str() << config().targets().bitclass();
  }
  char *strtab = (char *)strtab_region.begin();

  // emit the first ELF symbol
  if (config().targets().is32Bits())
    emitSymbol32(symtab32[0], LDSymbol::null(), strtab, 0, 0, /*IsDynSymTab=*/false);
  else
    emitSymbol64(symtab64[0], LDSymbol::null(), strtab, 0, 0, /*IsDynSymTab=*/false);

  m_pSymIndexMap[LDSymbol::null()] = 0;

  size_t symIdx = 1;
  size_t strtabsize = 1;

  std::vector<ResolveInfo *> &symbols = m_Module.getSymbols();
  std::optional<size_t> firstNonLocal;
  for (auto &S : symbols) {
    m_pSymIndexMap[S->outSymbol()] = symIdx;
    // Null symbol is already emitted.
    if ((isStripLocal && S->isLocal()) || m_SymbolsToRemove.count(S)) {
      config().raise(Diag::stripping_symbol) << S->name();
      continue;
    }
    if (config().targets().is32Bits())
      emitSymbol32(symtab32[symIdx], S->outSymbol(), strtab, strtabsize,
                   symIdx, /*IsDynSymTab=*/false);
    else
      emitSymbol64(symtab64[symIdx], S->outSymbol(), strtab, strtabsize,
                   symIdx, /*IsDynSymTab=*/false);
    if ((S->isGlobal() || S->isWeak()) && !firstNonLocal)
      firstNonLocal = symIdx;
    strtabsize += std::string(S->name()).size() + 1;
    ++symIdx;
  }
  if (firstNonLocal)
    symtab_sect.setInfo(*firstNonLocal);

  ELFSection &symtab_shndxr_sect = *file_format->getSymTabShndxr();
  if (symtab_shndxr_sect.size()) {
    MemoryRegion symtab_shndxr_region = getFileOutputRegion(
        pOutput, symtab_shndxr_sect.offset(), symtab_shndxr_sect.size());
    unsigned *symtab_shndxr =
        reinterpret_cast<unsigned *>(symtab_shndxr_region.begin());
    symtab_shndxr[0] = 0;
    size_t symIdx = 1;
    for (auto &S : symbols) {
      symtab_shndxr[symIdx] = getSymbolShndx(S->outSymbol()).second;
      ++symIdx;
    }
  }
  return {};
}

/// emitDynNamePools - emit dynamic name pools - .dyntab, .dynstr, .hash
///
/// the size of these tables should be computed before layout
/// layout should computes the start offset of these tables
bool GNULDBackend::emitDynNamePools(llvm::FileOutputBuffer &pOutput) {
  ELFSection *symtab_sect = m_Module.getSection(".dynsym");
  ELFSection *strtab_sect = m_Module.getSection(".dynstr");
  ELFSection *dyn_sect = m_Module.getSection(".dynamic");

  bool BuildDynSym = false;

  if (!dyn_sect || (dyn_sect && !dyn_sect->size()))
    return true;

  // Build dynsym only if the dynsym section has some size reserved.
  if (symtab_sect && !symtab_sect->isIgnore() && !symtab_sect->isDiscard() &&
      symtab_sect->size())
    BuildDynSym = true;

  // Reset strtab if the strtab section is set to be Ignored or there is nothing
  // that reserved size for it.
  if (!(strtab_sect && !strtab_sect->isIgnore() && !strtab_sect->isDiscard() &&
        strtab_sect->size()))
    strtab_sect = nullptr;

  llvm::ELF::Elf32_Sym *symtab32 = nullptr;
  llvm::ELF::Elf64_Sym *symtab64 = nullptr;
  if (BuildDynSym) {
    MemoryRegion symtab_region = getFileOutputRegion(
        pOutput, symtab_sect->offset(), symtab_sect->size());
    // set up symtab_region
    if (config().targets().is32Bits()) {
      symtab32 = (llvm::ELF::Elf32_Sym *)symtab_region.begin();
    } else if (config().targets().is64Bits()) {
      symtab64 = (llvm::ELF::Elf64_Sym *)symtab_region.begin();
    } else {
      config().raise(Diag::unsupported_bitclass)
          << config().targets().triple().str() << config().targets().bitclass();
    }
  }

  // set up strtab_region
  char *strtab = nullptr;
  if (BuildDynSym) {
    // Lets make sure that strtab section is not ignored.
    if (!strtab_sect ||
        (strtab_sect && strtab_sect->getKind() == LDFileFormat::Ignore)) {
      config().raise(Diag::section_ignored) << ".dynstr";
      return false;
    }
    MemoryRegion strtab_region = getFileOutputRegion(
        pOutput, strtab_sect->offset(), strtab_sect->size());
    strtab = (char *)strtab_region.begin();
  }

  if (strtab) {
    ELFFileFormat *FileFormat = getOutputFormat();
    ASSERT(FileFormat, "Must not be null!");
    const std::string &DynStrTabContents = FileFormat->getDynStrTabContents();
    ASSERT(strtab_sect->size() == DynStrTabContents.size(),
           "Size must be same!");
    memcpy(strtab, DynStrTabContents.c_str(), DynStrTabContents.size());
  }
  size_t symIdx = 0;
  size_t strtabsize = 0;

  std::optional<size_t> firstNonLocal;

  if (BuildDynSym) {
    for (auto &D : DynamicSymbols) {
      if (config().targets().is32Bits())
        emitSymbol32(symtab32[symIdx], D->outSymbol(), strtab, strtabsize,
                     symIdx, /*IsDynSymTab=*/true);
      else
        emitSymbol64(symtab64[symIdx], D->outSymbol(), strtab, strtabsize,
                     symIdx, /*IsDynSymTab=*/true);
      if ((D->isGlobal() || D->isWeak()) && !firstNonLocal)
        firstNonLocal = symIdx;
      symIdx++;
      strtabsize += D->nameSize() + 1;
    }
  }

  if (firstNonLocal)
    symtab_sect->setInfo(*firstNonLocal);

  dynamic()->applyEntries(*getOutputFormat(), m_Module);

  if (dyn_sect->getKind() != LDFileFormat::Null) {
    MemoryRegion dyn_region =
        getFileOutputRegion(pOutput, dyn_sect->offset(), dyn_sect->size());
    dynamic()->emit(*dyn_sect, dyn_region);
  }

  return true;
}

/// getSectionOrder
unsigned int GNULDBackend::getSectionOrder(const ELFSection &pSectHdr) const {
  bool linkerScriptHasSectionsCommand =
      m_Module.getScript().linkerScriptHasSectionsCommand();
  ELFFileFormat *file_format = getOutputFormat();
  llvm::StringRef sectionName = pSectHdr.name();

  // nullptr section should be the "1st" section
  if (LDFileFormat::Null == pSectHdr.getKind())
    return SHO_nullptr;

  if (&pSectHdr == file_format->getShStrTab())
    return SHO_SHSTRTAB;

  if (&pSectHdr == file_format->getSymTab())
    return SHO_SYMTAB;

  if (&pSectHdr == file_format->getSymTabShndxr())
    return SHO_SYMTAB_SHNDX;

  if (&pSectHdr == file_format->getStrTab())
    return SHO_STRTAB;

  if (pSectHdr.isGroupKind())
    return SHO_GROUP;

  if (pSectHdr.name() == ".start")
    return SHO_START;

  if (pSectHdr.name() == ".hash")
    return SHO_NAMEPOOL;

  if (pSectHdr.name() == ".gnu.hash")
    return SHO_NAMEPOOL;

  // if the section is not ALLOC, lay it out until the last possible moment
  if (0 == (pSectHdr.getFlags() & llvm::ELF::SHF_ALLOC)) {
    return SHO_UNDEFINED;
  }

  bool is_write = (pSectHdr.getFlags() & llvm::ELF::SHF_WRITE) != 0;
  bool is_exec = (pSectHdr.getFlags() & llvm::ELF::SHF_EXECINSTR) != 0;
  // TODO: need to take care other possible output sections
  switch (pSectHdr.getKind()) {
  case LDFileFormat::Common:
  case LDFileFormat::Internal:
  case LDFileFormat::OutputSectData:
  case LDFileFormat::Regular:
    if (is_exec) {
      if (!linkerScriptHasSectionsCommand && sectionName == ".init")
        return SHO_INIT;
      if (!linkerScriptHasSectionsCommand && sectionName == ".fini")
        return SHO_FINI;
      if (sectionName == ".plt")
        return SHO_PLT;
      return SHO_TEXT;
    } else {
      if (config().options().hasRelro()) {
        if (sectionName == ".data.rel.ro.local")
          return SHO_RELRO_LOCAL;

        // We can use the same function isRELRO for both cases, but we need to
        // also determine the case of Partial RELRO versus Full RELRO. This code
        // is mainly duplicated to assume that.
        if (sectionName == ".ctors" || sectionName == ".data.rel.ro.local" ||
            sectionName == ".data.rel.ro" || sectionName == ".dtors" ||
            sectionName == ".fini_array" || sectionName == ".init_array" ||
            sectionName == ".tdata" || sectionName == ".tbss" ||
            sectionName == ".preinit_array" || sectionName == ".jcr" ||
            sectionName == ".dynamic")
          return SHO_RELRO;

        if (sectionName == ".got" || sectionName == ".got.plt")
          return SHO_RELRO_LAST;
      }

      if (!is_write)
        return SHO_RO;

      if (pSectHdr.isNoBits()) {
        if (pSectHdr.isTLS())
          return SHO_TLS_BSS;
        return SHO_BSS;
      }
      if (pSectHdr.isTLS()) {
        return SHO_TLS_DATA;
      }
      return SHO_DATA;
    }

  case LDFileFormat::MergeStr:
    if (pSectHdr.isAlloc())
      return SHO_RO;
    return SHO_UNDEFINED;

  case LDFileFormat::NamePool: {
    if (&pSectHdr == file_format->getDynamic())
      return SHO_RELRO;
    return SHO_NAMEPOOL;
  }
  case LDFileFormat::Relocation:
  case LDFileFormat::DynamicRelocation:
    if (sectionName == ".rel.plt" || sectionName == ".rela.plt")
      return SHO_REL_PLT;
    return SHO_RELOCATION;

  // get the order from target for target specific sections
  case LDFileFormat::Target:
    return getTargetSectionOrder(pSectHdr);

  // handle .interp and .note.* sections
  case LDFileFormat::Note:
  case LDFileFormat::StackNote: {
    if (pSectHdr.name() == ".interp")
      return SHO_INTERP;
    else if (is_write)
      return SHO_RW_NOTE;
    return SHO_RO_NOTE;
  }

  case LDFileFormat::EhFrame:
    // set writable .eh_frame as relro
    if (is_write)
      return SHO_RELRO;
    LLVM_FALLTHROUGH;
  case LDFileFormat::EhFrameHdr:
  case LDFileFormat::GCCExceptTable:
    return SHO_EXCEPTION;

  case LDFileFormat::MetaData:
  case LDFileFormat::Debug:
  default:
    return SHO_UNDEFINED;
  }
}

bool GNULDBackend::isRelROSection(const ELFSection *section) const {
  llvm::StringRef sectionName = section->name();
  if (!config().options().hasRelro())
    return false;
  // If GOT and GOTPLT sections are merged to form a single output section, then
  // the section is not part of a RELRO section if -z relro is specified.
  if (isGOTAndGOTPLTMerged() && sectionName == ".got" &&
      !config().options().hasNow())
    return false;
  if (sectionName == ".ctors" || sectionName == ".data.rel.ro.local" ||
      sectionName == ".data.rel.ro" || sectionName == ".dtors" ||
      sectionName == ".fini_array" || sectionName == ".got" ||
      sectionName == ".init_array" || sectionName == ".tdata" ||
      sectionName == ".tbss" || sectionName == ".preinit_array" ||
      sectionName == ".jcr" || sectionName == ".dynamic")
    return true;
  if (config().options().hasNow() && sectionName == ".got.plt")
    return true;
  return false;
}

bool GNULDBackend::isGOTAndGOTPLTMerged() const {
  ELFSection *GOTPLT = getGOTPLT();
  ELFSection *GOT = getGOT();
  return (GOTPLT && GOT &&
          (GOTPLT->getOutputSection() == GOT->getOutputSection()));
}

void GNULDBackend::createInternalInputs() {
  // Create a special global input file for dynamic section headers, e.g.
  // GOT0, PLT0. This input file must be inserted before real inputs.
  m_DynamicSectionHeadersInputFile =
      llvm::dyn_cast<ELFObjectFile>(m_Module.createInternalInputFile(
          make<Input>("Dynamic section headers", config().getDiagEngine()),
          true));
}

/// getSymbolSize
uint64_t GNULDBackend::getSymbolSize(LDSymbol *pSymbol) const {
  // undefined and dynamic symbols should have zero size.
  if (pSymbol->isDyn() || pSymbol->desc() == ResolveInfo::Undefined)
    return 0x0;
  return pSymbol->resolveInfo()->size();
}

Relocation::Type GNULDBackend::getCopyRelType() const {
  return m_pInfo->getTargetRelocationType().CopyRelocType;
}

/// getSymbolInfo
uint64_t GNULDBackend::getSymbolInfo(LDSymbol *pSymbol) const {
  // set binding
  uint8_t bind = 0x0;
  if (pSymbol->resolveInfo()->isLocal())
    bind = llvm::ELF::STB_LOCAL;
  else if (pSymbol->resolveInfo()->isGlobal())
    bind = llvm::ELF::STB_GLOBAL;
  else if (pSymbol->resolveInfo()->isWeak())
    bind = llvm::ELF::STB_WEAK;
  else if (pSymbol->resolveInfo()->isAbsolute()) {
    // (Luba) Is a absolute but not global (weak or local) symbol meaningful?
    bind = llvm::ELF::STB_GLOBAL;
  }

  if (config().codeGenType() != LinkerConfig::Object &&
      pSymbol->visibility() == llvm::ELF::STV_INTERNAL)
    bind = llvm::ELF::STB_LOCAL;

  uint32_t type = pSymbol->resolveInfo()->type();
  // if the IndirectFunc symbol (i.e., STT_GNU_IFUNC) is from dynobj, change
  // its type to Function
  if (type == ResolveInfo::IndirectFunc && pSymbol->isDyn())
    type = ResolveInfo::Function;
  return (type | (bind << 4));
}

/// getSymbolValue - this function is called after layout()
uint64_t GNULDBackend::getSymbolValue(LDSymbol *pSymbol) const {
  if (pSymbol->isDyn())
    return 0x0;

  return pSymbol->value();
}

ELFSection *GNULDBackend::getGOT() const {
  return m_DynamicSectionHeadersInputFile->getGOT();
}

ELFSection *GNULDBackend::getGOTPLT() const {
  return m_DynamicSectionHeadersInputFile->getGOTPLT();
}

ELFSection *GNULDBackend::getPLT() const {
  return m_DynamicSectionHeadersInputFile->getPLT();
}

ELFSection *GNULDBackend::getRelaDyn() const {
  return m_DynamicSectionHeadersInputFile->getRelaDyn();
}

ELFSection *GNULDBackend::getRelaPLT() const {
  return m_DynamicSectionHeadersInputFile->getRelaPLT();
}

// Patching sections.
ELFSection *GNULDBackend::getGOTPatch() const {
  return m_DynamicSectionHeadersInputFile->getGOTPatch();
}

ELFSection *GNULDBackend::getRelaPatch() const {
  return m_DynamicSectionHeadersInputFile->getRelaPatch();
}

// Record an absolute PLT entry, which is used in the patch image for symbol
// resolution to PLTs located in the base image.
void GNULDBackend::recordAbsolutePLT(ResolveInfo *I, const ResolveInfo *P) {
  m_AbsolutePLTMap[I] = P;
}

// Find an entry in the PLT
const ResolveInfo *GNULDBackend::findAbsolutePLT(ResolveInfo *I) const {
  auto Entry = m_AbsolutePLTMap.find(I);
  if (Entry == m_AbsolutePLTMap.end())
    return nullptr;
  return Entry->second;
}

/// getSymbolShndx - this function is called after layout()
std::pair<uint16_t, uint32_t>
GNULDBackend::getSymbolShndx(LDSymbol *pSymbol) const {
  if (pSymbol->resolveInfo()->isAbsolute())
    return {llvm::ELF::SHN_ABS, 0};
  if (pSymbol->resolveInfo()->isCommon() && !pSymbol->hasFragRef())
    return {llvm::ELF::SHN_COMMON, 0};
  if (pSymbol->resolveInfo()->isUndef() || pSymbol->isDyn())
    return {llvm::ELF::SHN_UNDEF, 0};

  ELFSection *sectionInfo = getSectionInfo(pSymbol);
  if (sectionInfo) {
    if (getSectionIdx(sectionInfo) == -1)
      return {llvm::ELF::SHN_ABS, 0};
    uint64_t index = sectionInfo->getIndex();
    if (index >= llvm::ELF::SHN_LORESERVE)
      return {llvm::ELF::SHN_XINDEX, index};
    return {index, 0};
  }

  if (pSymbol->resolveInfo()->isDefine() && !pSymbol->hasFragRef())
    return {llvm::ELF::SHN_ABS, 0};

  ELFSection *section = pSymbol->fragRef()->frag()->getOwningSection();
  if (section->getOutputSection())
    section = section->getOutputSection()->getSection();

  if (getSectionIdx(section) == -1)
    return {llvm::ELF::SHN_ABS, 0};
  uint64_t index = section->getIndex();
  if (index >= llvm::ELF::SHN_LORESERVE)
    return {llvm::ELF::SHN_XINDEX, index};
  return {index, 0};
}

ELFSection *GNULDBackend::getSectionInfo(LDSymbol *symbol) const {
  auto symToSection = m_SymbolToSection.find(symbol);
  if (symToSection == m_SymbolToSection.end())
    return nullptr;
  return symToSection->second;
}

void GNULDBackend::addSectionInfo(LDSymbol *symbol, ELFSection *section) {
  m_SymbolToSection[symbol] = section;
}

int64_t GNULDBackend::getSectionIdx(ELFSection *S) const {
  auto entry = m_OutputSectionMap.find(S);
  if (entry == m_OutputSectionMap.end())
    return -1;
  return entry->second->getIndex();
}

/// getSymbolIdx - called by emitRelocation to get the output symbol table index
size_t GNULDBackend::getSymbolIdx(LDSymbol *pSymbol, bool IgnoreUnknown) const {
  // TODO: IgnoreUnknown was added as a kludge before QTOOL-102747 is resolved.
  // Given that this function dereferenced the past the end iterator, this error
  // probably never legitimately occurs and the error can be replaced with an
  // assertion. However, the callers that pass IgnoreUnknown = true should first
  // figure that the symbol is invalid and do not try to get its index.
  auto Entry = m_pSymIndexMap.find(pSymbol);
  if (Entry == m_pSymIndexMap.end()) {
    if (!IgnoreUnknown)
      config().raise(Diag::symbol_not_found) << pSymbol->name();
    return 0;
  }
  return Entry->second;
}

/// getDynSymbolIdx - get the symbol index of output dynamic symbol table
size_t GNULDBackend::getDynSymbolIdx(LDSymbol *pSymbol) const {
  auto Entry = m_pDynSymIndexMap.find(pSymbol);
  if (Entry == m_pDynSymIndexMap.end()) {
    config().raise(Diag::symbol_not_found) << pSymbol->name();
    return 0;
  }
  return Entry->second;
}

/// allocateCommonSymbols - allocate common symbols in the corresponding
/// sections. This is executed at pre-layout stage.
bool GNULDBackend::allocateCommonSymbols() {
  if (m_Module.getCommonSymbols().empty())
    return true;

  for (auto &com_sym : m_Module.getCommonSymbols()) {
    // For common symbols, symbol values holds the alignment.
    ELFSection *section = m_Module.createCommonELFSection(
        std::string("COMMON.") + com_sym->name(), com_sym->value(),
        com_sym->resolvedOrigin());
    Fragment *frag =
        make<FillFragment>(getModule(), 0x0, com_sym->outSymbol()->size(),
                           section, com_sym->outSymbol()->value());
    section->addFragmentAndUpdateSize(frag);
    com_sym->outSymbol()->setFragmentRef(make<FragmentRef>(*frag, 0));
  }
  return true;
}

/// readRelocation - read ELF32_Rel entry
bool GNULDBackend::readRelocation(const llvm::ELF::Elf32_Rel &pRel,
                                  Relocation::Type &pType, uint32_t &pSymIdx,
                                  uint32_t &pOffset) const {
  uint32_t r_info = 0x0;
  pOffset = pRel.r_offset;
  r_info = pRel.r_info;
  // FIXME: Why cast to unsigned char? pType is uint32 value!
  pType = static_cast<unsigned char>(r_info);
  pSymIdx = (r_info >> 8);
  return true;
}

/// readRelocation - read ELF32_Rela entry
bool GNULDBackend::readRelocation(const llvm::ELF::Elf32_Rela &pRel,
                                  Relocation::Type &pType, uint32_t &pSymIdx,
                                  uint32_t &pOffset, int32_t &pAddend) const {
  uint32_t r_info = 0x0;
  pOffset = pRel.r_offset;
  r_info = pRel.r_info;
  pAddend = pRel.r_addend;

  // FIXME: Why cast to unsigned char? pType is uint32 value!
  pType = static_cast<unsigned char>(r_info);
  pSymIdx = (r_info >> 8);
  return true;
}

/// readRelocation - read ELF64_Rel entry
bool GNULDBackend::readRelocation(const llvm::ELF::Elf64_Rel &pRel,
                                  Relocation::Type &pType, uint32_t &pSymIdx,
                                  uint64_t &pOffset) const {
  uint64_t r_info = 0x0;
  pOffset = pRel.r_offset;
  r_info = pRel.r_info;

  pType = static_cast<uint32_t>(r_info);
  pSymIdx = (r_info >> 32);
  return true;
}

/// readRel - read ELF64_Rela entry
bool GNULDBackend::readRelocation(const llvm::ELF::Elf64_Rela &pRel,
                                  Relocation::Type &pType, uint32_t &pSymIdx,
                                  uint64_t &pOffset, int64_t &pAddend) const {
  uint64_t r_info = 0x0;
  pOffset = pRel.r_offset;
  r_info = pRel.r_info;
  pAddend = pRel.r_addend;

  pType = static_cast<uint32_t>(r_info);
  pSymIdx = (r_info >> 32);
  return true;
}

// As these are considerably long functions, lets separate them into multiple
// files so that functionality is easily distinguished.
#include "CreateProgramHeaders.hpp"
#include "CreateScriptProgramHeaders.hpp"

void GNULDBackend::changeSymbolsFromAbsoluteToGlobal(OutputSectionEntry *out) {
  eld::RegisterTimer T("Change Symbol to Global", "Setup Output Section Offset",
                       m_Module.getConfig().options().printTimingStats());
  for (OutputSectionEntry::iterator in = out->begin(), inEnd = out->end();
       in != inEnd; ++in) {
    for (RuleContainer::sym_iterator it = (*in)->symBegin(),
                                     ie = (*in)->symEnd();
         it != ie; ++it) {
      LDSymbol *symbol = (*it)->symbol();
      if (symbol && symbol->resolveInfo()->isAbsolute()) {
        symbol->resolveInfo()->setBinding(ResolveInfo::Global);
        addSectionInfo(symbol, (*out).getSection());
      }
    }
  }
}

bool GNULDBackend::InsertAtSectionToEnd(ELFSection *OutSection,
                                        uint64_t &Offset,
                                        RuleContainer *CurRule,
                                        RuleContainer *NextRule,
                                        Expression *Fill, uint32_t atIndex) {
  std::vector<ELFSection *> &atTable = m_Module.getAtTable();

  if (OutSection->getKind() == LDFileFormat::Null)
    return false;

  if (!atTable.size())
    return false;
  if (atIndex >= atTable.size())
    return false;

  if (!NextRule)
    return false;

  eld::RegisterTimer T("Place AT Section to End", "Evaluate Assignments",
                       m_Module.getConfig().options().printTimingStats());

  // Create empty entry
  OutputSectionEntry *out = OutSection->getOutputSection();

  if (OutputSectionToFrags.find(out) == OutputSectionToFrags.end())
    OutputSectionToFrags[out];

  auto &OutputSectionFragVector = OutputSectionToFrags[out];

  uint64_t NewOffset = Offset;
  LDSymbol *dotSymbol = m_Module.getDotSymbol();
  if (OutSection->isAlloc())
    dotSymbol->setValue(OutSection->addr() + NewOffset);
  for (RuleContainer::sym_iterator it = NextRule->symBegin(),
                                   ie = NextRule->symEnd();
       it != ie; ++it) {
    Assignment *assign = (*it);
    if (shouldskipAssert(assign))
      continue;
    (*it)->assign(m_Module, OutSection);
    if (OutSection->isAlloc())
      NewOffset = dotSymbol->value() - OutSection->addr();
  }
  uint64_t atSectionAddress = 0;
  uint64_t atSectionOffset = 0;

  ELFSection *atSection = atTable[atIndex];
  if (atSection) {
    atSectionAddress = atSection->addr();
    atSectionOffset = atSectionAddress - OutSection->addr();
  }
  if (NewOffset >= atSectionOffset) {
    atSection = atSection->getOutputSection()->getLastRule()->getSection();
    typedef llvm::SmallVectorImpl<Fragment *>::iterator FragIter;
    FragIter EndIter = CurRule->getSection()->getFragmentList().end();
    for (auto &Frag : atSection->getFragmentList()) {
      Frag->getOwningSection()->setFixedAddr();
      Frag->getOwningSection()->setAddr(atSectionAddress);
      Frag->getOwningSection()->setMatchedLinkerScriptRule(CurRule);
      Frag->getOwningSection()->setOutputSection(
          OutSection->getOutputSection());
    }
    OutputSectionFragVector.insert(OutputSectionFragVector.end(),
                                   atSection->getFragmentList().begin(),
                                   atSection->getFragmentList().end());
    CurRule->getSection()->splice(EndIter, atSection->getFragmentList());
  } else {
    return false;
  }

  Offset = CurRule->getSection()->offset();
  if (OutSection->isAlloc())
    dotSymbol->setValue(OutSection->addr() + Offset);
  for (auto &F : CurRule->getSection()->getFragmentList()) {
    if (F->getOwningSection()->isFixedAddr())
      Offset = Offset + (F->getOwningSection()->addr() - dotSymbol->value());
    F->setOffset(Offset);
    if (Fill)
      getModule().setFragmentPaddingValue(F, Fill->result());
    Offset = F->getOffset(config().getDiagEngine()) + F->size();
    if (OutSection->isAlloc()) {
      dotSymbol->setValue(OutSection->addr() + Offset);
      Offset = dotSymbol->value() - OutSection->addr();
    }
  }
  return true;
}

bool GNULDBackend::TryToPlaceAtSection(RuleContainer *In, Fragment *Frag,
                                       ELFSection *Section, uint32_t atIndex) {
  std::vector<ELFSection *> &atTable = m_Module.getAtTable();

  if (Section->getKind() == LDFileFormat::Null)
    return false;

  if (!atTable.size())
    return false;

  if (atIndex >= atTable.size())
    return false;

  eld::RegisterTimer T("Try to Place AT Section", "Evaluate Assignments",
                       m_Module.getConfig().options().printTimingStats());
  // Create empty entry
  OutputSectionEntry *out = Section->getOutputSection();
  if (OutputSectionToFrags.find(out) == OutputSectionToFrags.end())
    OutputSectionToFrags[out];

  auto &OutputSectionFragVector = OutputSectionToFrags[out];

  ELFSection *atSection = atTable[atIndex];
  LDSymbol *dotSymbol = m_Module.getDotSymbol();

  uint64_t atSectionAddress = 0;
  uint64_t atSectionOffset = 0;
  llvm::StringRef atSectionName = atSection->name();

  if (atSection) {
    atSectionAddress = atSection->addr();
    atSectionOffset = atSectionAddress - Section->addr();
  }
  atSection = atSection->getOutputSection()->getLastRule()->getSection();
  Fragment *First = atSection->getFragmentList().front();
  typedef llvm::SmallVectorImpl<Fragment *>::iterator FragIter;
  FragIter EndIter = In->getSection()->getFragmentList().end();
  FragIter Iter = Frag ? Frag->getIterator() : EndIter;

  bool InsertedAtSection = false;

  uint64_t Offset = dotSymbol->value() - Section->addr();

  // Check if the next fragment can be inserted in the list of fragments
  // that is being iterated on.
  if (atSectionOffset == Offset ||
      (Frag && (Offset < atSectionOffset) &&
       ((First->getNewOffset(Offset) >= atSectionOffset) ||
        ((First->getNewOffset(Offset) + First->size()) >= atSectionOffset)))) {
    InsertedAtSection = true;
    Offset = atSectionAddress - Offset;
    for (auto &Frag : atSection->getFragmentList()) {
      Frag->getOwningSection()->setFixedAddr();
      Frag->getOwningSection()->setAddr(atSectionAddress);
      Frag->getOwningSection()->setMatchedLinkerScriptRule(In);
      Frag->getOwningSection()->setOutputSection(Section->getOutputSection());
    }
    OutputSectionFragVector.insert(OutputSectionFragVector.end(),
                                   atSection->getFragmentList().begin(),
                                   atSection->getFragmentList().end());
    if (m_Module.getPrinter()->isVerbose())
      config().raise(Diag::verbose_inserting_section_at_fixed_addr)
          << atSectionName << utility::toHex(atSectionAddress)
          << Section->getOutputSection()->name();

    In->getSection()->splice(Iter, atSection->getFragmentList());
  }
  return InsertedAtSection;
}

static void checkFragOffset(const Fragment *F, DiagnosticEngine *DiagEngine) {
  if (!F->hasOffset()) {
    auto *I = F->getOwningSection()->getMatchedLinkerScriptRule();
    std::string RuleStr;
    if (I) {
      llvm::raw_string_ostream S(RuleStr);
      if ((reinterpret_cast<RuleContainer *>(I))->desc())
        (reinterpret_cast<RuleContainer *>(I))
            ->desc()
            ->dumpMap(S, false, false);
    }
    if (F->getOwningSection()->isDiscard()) {
      DiagEngine->raise(Diag::offset_not_assigned)
          << F->getOwningSection()->name() << "Discarded"
          << F->getOwningSection()->getInputFile()->getInput()->decoratedPath();
    } else if (!I)
      DiagEngine->raise(Diag::offset_not_assigned)
          << F->getOwningSection()->name() << F->getOutputELFSection()->name()
          << F->getOwningSection()->getInputFile()->getInput()->decoratedPath();
    else
      DiagEngine->raise(Diag::offset_not_assigned_with_rule)
          << F->getOwningSection()->name()
          << F->getOwningSection()->getInputFile()->getInput()->decoratedPath()
          << RuleStr << F->getOutputELFSection()->name();
  }
}

// Study this function! Use this function to emit diagnostics of
// getOffset(config().getDiagEngine()).
void GNULDBackend::evaluateAssignments(OutputSectionEntry *out,
                                       uint32_t &atIndex) {

  eld::RegisterTimer T("Evaluate Expressions", "Establish Layout",
                       m_Module.getConfig().options().printTimingStats());

  ELFSection *OutSection = out->getSection();

  LDSymbol *dotSymbol = m_Module.getDotSymbol();

  // Initial dot symbol value.
  dotSymbol->setValue(OutSection->addr());
  LDSymbol::ValueType InitialDotValue = OutSection->addr();

  if (m_Module.getPrinter()->traceAssignments())
    config().raise(Diag::trace_output_section_addr)
        << (out->name().empty() ? "<nullptr>" : out->name())
        << utility::toHex(static_cast<uint64_t>(OutSection->addr()))
        << utility::toHex(static_cast<uint64_t>(OutSection->pAddr()));

  uint64_t offset = 0;

  // Clear all the fill maps.
  if (m_PaddingMap.find(OutSection) != m_PaddingMap.end())
    m_PaddingMap[OutSection].clear();

  PaddingT padding;

  Expression *fillExpression = out->epilog().fillExp();
  if (fillExpression)
    fillExpression->evaluateAndRaiseError();

  padding.Exp = fillExpression;

  bool hasAssignmentsOrFragments = false;

  // Create empty entry
  if (OutputSectionToFrags.find(out) == OutputSectionToFrags.end())
    OutputSectionToFrags[out];

  auto &OutputSectionFragVector = OutputSectionToFrags[out];
  OutputSectionFragVector.clear();

  for (OutputSectionEntry::iterator in = out->begin(), inEnd = out->end();
       in != inEnd; ++in) {

    RuleContainer *CurRule = (*in);
    ELFSection *InSection = CurRule->getSection();
    // Evaluate all assignments at the beginning of input section.
    for (RuleContainer::sym_iterator it = (*in)->symBegin(),
                                     ie = (*in)->symEnd();
         it != ie; ++it) {
      Assignment *assign = (*it);
      if (shouldskipAssert(assign))
        continue;
      // We do not need to evaluate PROVIDE expressions for PROVIDE
      if (assign->isProvideOrProvideHidden() && !isProvideSymBeingUsed(assign))
        continue;
      if (padding.startOffset == -1)
        padding.startOffset = offset;
      uint64_t previousDotValue = dotSymbol->value();
      (*it)->assign(m_Module, OutSection);
      offset = dotSymbol->value() - OutSection->addr();
      // Check for backward movement of dot symbol in the current output section
      if ((dotSymbol->value() < previousDotValue) &&
          m_Module.getConfig().showBadDotAssignmentWarnings()) {
        std::string expressionStr;
        llvm::raw_string_ostream SS(expressionStr);
        assign->dumpMap(SS, false, false);
        config().raise(Diag::warn_dot_value_backward_movement)
            << OutSection->name() << SS.str();
      }
      if (!OutSection->hasNoFragments())
        OutSection->setSize(offset);
      hasAssignmentsOrFragments = true;
      if ((*it)->getExpression()->type() == Expression::FILL) {
        if (padding.startOffset != -1 && (offset - padding.startOffset)) {
          padding.endOffset = offset;
          m_PaddingMap[OutSection].push_back(padding);
        }
        padding.startOffset = offset;
        padding.endOffset = -1;
        padding.Exp = (*it)->getExpression();
        fillExpression = padding.Exp;
      }
    }
    if (padding.startOffset != -1 && (offset - padding.startOffset)) {
      padding.endOffset = offset;
      m_PaddingMap[OutSection].push_back(padding);
      padding.Exp = fillExpression;
      padding.startOffset = -1;
      padding.endOffset = -1;
    }
    if (fillExpression) {
      fillExpression->evaluateAndRaiseError();
    }
    bool placedAtSection = false;
    OutputSectionEntry::iterator NextRuleIter = in + 1;
    if (!InSection->getFragmentList().size()) {
      if (!TryToPlaceAtSection(*in, nullptr, OutSection, atIndex))
        continue;
      placedAtSection = true;
      ++atIndex;
    }
    typedef llvm::SmallVectorImpl<Fragment *>::iterator FragIter;
    FragIter Begin, End;
    Begin = InSection->getFragmentList().begin();
    End = InSection->getFragmentList().end();
    OutputSectionFragVector.insert(OutputSectionFragVector.end(), Begin, End);
    InSection->setOffset(offset);
    while (Begin != End) {
      Fragment *F = *Begin;
      if (F->isNull()) {
        ++Begin;
        continue;
      }
      if (!placedAtSection &&
          TryToPlaceAtSection(*in, F, OutSection, atIndex)) {
        // Reset begin/end.
        Begin = InSection->getFragmentList().begin();
        End = InSection->getFragmentList().end();
        offset = InSection->offset();
        if (OutSection->isAlloc())
          dotSymbol->setValue(OutSection->addr() + offset);
        ++atIndex;
        continue;
      }
      auto OwningSection = F->getOwningSection();
      if (OwningSection->isFixedAddr())
        offset = offset + (OwningSection->addr() - dotSymbol->value());
      hasAssignmentsOrFragments = true;
      F->setOffset(offset);
      checkFragOffset(F, config().getDiagEngine());
      if (IsSectionTracingRequested &&
          config().options().traceSection(OwningSection)) {
        auto FragOffset = F->getOffset(config().getDiagEngine());
        config().raise(Diag::input_section_info)
            << OwningSection->getDecoratedName(config().options())
            << utility::toHex(static_cast<uint64_t>(
                   F->getOutputELFSection()->pAddr() + FragOffset))
            << utility::toHex(static_cast<uint64_t>(
                   F->getOutputELFSection()->addr() + FragOffset))
            << utility::toHex(FragOffset) << utility::toHex(F->size());
      }
      if (fillExpression)
        getModule().setFragmentPaddingValue(F, fillExpression->result());
      offset = F->getOffset(config().getDiagEngine()) + F->size();
      dotSymbol->setValue(OutSection->addr() + offset);
      offset = dotSymbol->value() - OutSection->addr();
      ++Begin;
    }
    RuleContainer *NextRule =
        (NextRuleIter == inEnd) ? nullptr : *(NextRuleIter);
    if (NextRule && NextRule->hasAssignments()) {
      if (InsertAtSectionToEnd(OutSection, offset, CurRule, NextRule,
                               fillExpression, atIndex))
        atIndex++;
      else
        dotSymbol->setValue(OutSection->addr() + offset);
    }
    CurRule->getSection()->setSize(offset);
    padding.startOffset = offset;
  }
  if (hasAssignmentsOrFragments && !OutSection->hasNoFragments())
    OutSection->setSize(offset);
  if (padding.startOffset != -1) {
    padding.endOffset = OutSection->size();
    m_PaddingMap[OutSection].push_back(padding);
  }

  if (OutSection->isAlloc()) {
    if (OutSection->isTBSS())
      dotSymbol->setValue(OutSection->addr());
    else
      dotSymbol->setValue(OutSection->addr() + OutSection->size());
  }

  // Restore dot-value if OutSection is non-alloc section.
  // dot counter (VMA) should not change in a non-alloc section.
  if (!OutSection->isAlloc())
    dotSymbol->setValue(InitialDotValue);

  if (!(m_Module.getPrinter()->traceAssignments()))
    return;

  config().raise(Diag::output_section_fill)
      << (out->name().empty() ? "<nullptr>" : out->name()) << "\n";
  if (m_PaddingMap.find(OutSection) != m_PaddingMap.end()) {
    auto &fvector = m_PaddingMap[OutSection];
    for (auto &f : fvector) {
      if (f.Exp)
        f.Exp->dump(llvm::outs(), true);
      config().raise(Diag::padding_map)
          << llvm::utostr(f.startOffset) << llvm::utostr(f.endOffset);
    }
  }
}

void GNULDBackend::evaluateAssignmentsAtEndOfOutputSection(
    OutputSectionEntry *out) {
  eld::RegisterTimer T("Evaluate Expressions", "Establish Layout",
                       m_Module.getConfig().options().printTimingStats());
  if (m_Module.getPrinter()->traceAssignments())
    config().raise(Diag::output_section_eval)
        << (out->name().empty() ? "<nullptr>" : out->name()) << "\n";
  // Evaluate all assignments at the end of the output section.
  for (OutputSectionEntry::sym_iterator it = out->sectionendsymBegin(),
                                        ie = out->sectionendsymEnd();
       it != ie; ++it) {
    Assignment *assign = (*it);
    // We do not need to evaluate PROVIDE expressions for PROVIDE
    // symbols that are not being used in the link.
    if (assign->isProvideOrProvideHidden() && !isProvideSymBeingUsed(assign))
      continue;
    if (shouldskipAssert(assign)) {
      if (m_Module.getPrinter()->isVerbose()) {
        std::string expressionStr;
        llvm::raw_string_ostream SS(expressionStr);
        assign->dumpMap(SS, false, false);
        config().raise(Diag::executing_assert_after_layout_is_complete)
            << SS.str();
      }
      continue;
    }
    (*it)->assign(m_Module, nullptr);
  }
}

/// createSegmentsFromLinkerScript - Create program headers mentioned in Linker
/// Script
bool GNULDBackend::createSegmentsFromLinkerScript() {
  eld::RegisterTimer T("Create Segments From PHDRS", "Perform Layout",
                       m_Module.getConfig().options().printTimingStats());

  LinkerScript &ldscript = m_Module.getScript();
  LinkerScript::PhdrSpecList::iterator phdr = ldscript.phdrList().begin(),
                                       phdrEnd = ldscript.phdrList().end();
  ELFSegment *noteGNUStackSegment = nullptr;

  for (; phdr != phdrEnd; ++phdr) {
    uint64_t Flags = 0;
    if ((*phdr)->spec().flags()) {
      (*phdr)->spec().flags()->evaluateAndRaiseError();
      Flags = (*phdr)->spec().flags()->result();
    }
    ELFSegment *segment =
        make<ELFSegment>((*phdr)->spec().type(), Flags,
                         (*phdr)->spec().atAddress(), (*phdr)->getSpec());
    const std::string name = ((*phdr)->spec().name());
    _segments[name] = segment;
    segment->setName(name);
    elfSegmentTable().addSegment(segment);
    // If the linker script requests for a GNU stack segment to be
    // created, the linker should create one by using the properties
    // of the .note.GNU-stack section.
    if (segment->type() == llvm::ELF::PT_GNU_STACK)
      noteGNUStackSegment = segment;
  }

  addPhdrsIfNeeded();

  SectionMap::iterator out, outBegin = ldscript.sectionMap().begin(),
                            outEnd = ldscript.sectionMap().end();
  OutputSectionEntry *prev = nullptr;
  OutputSectionEntry *cur = nullptr;

  StringList *curPhdrList = nullptr;
  StringList *prevPhdrList = nullptr;
  std::vector<OutputSectionEntry *> sections;

  for (out = outBegin; out != outEnd; ++out) {
    cur = (*out);

    bool isCurNoLoad = (cur->prolog().type() == OutputSectDesc::NOLOAD);

    if (cur->getSection()->isFixedAddr())
      continue;

    if (cur->getSection() == m_phdr || cur->getSection() == m_ehdr)
      continue;

    // Attach the Note GNU stack segment to the .note.GNU-stack
    // section.
    if (noteGNUStackSegment && cur->getSection()->isNoteGNUStack()) {
      cur->getSection()->setWantedInOutput(true);
      _segmentsForSection[cur].push_back(noteGNUStackSegment);
      continue;
    }

    bool isCurAllocSection =
        (cur->getSection()->getFlags() & llvm::ELF::SHF_ALLOC);
    curPhdrList = nullptr;

    // All orphan sections that dont have a segment assigned, are assigned to
    // the first segment.
    if (prevPhdrList && !sections.empty()) {
      for (auto &sec : sections)
        _segmentsForSection[sec] = _segmentsForSection[prev];
      sections.clear();
    }

    if (isCurAllocSection) {
      if (cur->epilog().hasPhdrs()) {
        curPhdrList = cur->epilog().phdrs();
        ELFSegment *seg = nullptr;
        for (StringList::const_iterator it = curPhdrList->begin(),
                                        ie = curPhdrList->end();
             it != ie; ++it) {
          std::string phdrName = ((*it)->name());
          auto iter = _segments.find(phdrName);
          if (llvm::StringRef(phdrName).lower() == "none") {
            if (!m_pNONESegment) {
              m_pNONESegment = make<ELFSegment>(llvm::ELF::PT_NULL);
              m_pNONESegment->setName("NONE");
            }
            seg = m_pNONESegment;
          } else if (iter == _segments.end()) {
            // This message needs a context because it's also raised.
            // TODO: each phdr name can be associated with a context.
            config().raise(Diag::fatal_segment_not_defined_ldscript)
                << "(layout)" << phdrName;
            return false;
          } else {
            seg = (*iter).getValue();
          }
          _segmentsForSection[cur].push_back(seg);
        }
      } else if (!isCurNoLoad) {
        if (prev->prolog().type() == OutputSectDesc::NOLOAD)
          config().raise(Diag::warn_section_no_segment) << cur->name();
        else if (prevPhdrList) {
          curPhdrList = prevPhdrList;
          // Construct a bucket for the current section before copying it.
          _segmentsForSection[cur];
          // Segments for orphan sections need to be set appropriately.
          _segmentsForSection[cur] = _segmentsForSection[prev];
        } else {
          sections.push_back(cur);
        }
      }
    }
    bool curHasLMA = cur->prolog().hasLMA();
    // If current section has LMA and previous also has LMA, and both belong to
    // the same segment, error out.
    if (prev && curPhdrList && prevPhdrList && prev->prolog().hasLMA() &&
        curHasLMA && (prevPhdrList == curPhdrList)) {
      std::string phdrName;
      for (auto &s : *curPhdrList) {
        phdrName += s->name();
        phdrName += ",";
      }
      config().raise(Diag::fatal_paddr_ignored)
          << (*out)->getSection()->name() << phdrName;
      return false;
    }
    prevPhdrList = curPhdrList;
    prev = *(out);
  }
  return false;
}

void GNULDBackend::assignOffsetsToSkippedSections() {
  eld::RegisterTimer T("Assign Offsets to Skipped Sections", "Establish Layout",
                       m_Module.getConfig().options().printTimingStats());
  ELFSection *prev = nullptr;
  uint64_t offset = 0;
  for (auto &out : m_Module.getScript().sectionMap()) {
    ELFSection *cur = out->getSection();
    if (cur->isNullType()) {
      prev = cur;
      continue;
    }
    if (!cur->hasOffset() && prev) {
      if (prev->isNoBits())
        offset = prev->offset();
      else
        offset = prev->offset() + prev->size();
      cur->setOffset(offset);
    }
  }
}

std::pair<int64_t, ELFSection *>
GNULDBackend::setupSegmentOffset(ELFSegment *Seg, ELFSection *P,
                                 int64_t BeginOffset, bool Check) {
  eld::RegisterTimer T("Setup Segment Offsets", "Establish Layout",
                       m_Module.getConfig().options().printTimingStats());
  ELFSection *prev = P;
  // If offsets are already assigned, dont change the offsets.
  if (m_OffsetsAssigned)
    return std::make_pair(BeginOffset, prev);

  if ((Seg->type() != llvm::ELF::PT_LOAD) &&
      (Seg->type() != llvm::ELF::PT_NULL))
    return std::make_pair(BeginOffset, prev);

  ELFSection *prevProgBitsInSegment = nullptr;

  bool isBeginningOfSection = true;

  // Handle PT_TLS segments. These are pretty special that sections part of
  // PT_TLS segment are already part of a PT_LOAD segment (by design), so we
  // cannot assign offsets again.
  bool isTLSSegment = (Seg->type() == llvm::ELF::PT_TLS);

  int64_t offset = BeginOffset;
  // Sort the sections in the segment. This is to allow decreasing addresses
  // in the same segment and making sure file offset is sane and doesn't
  // become negative. If we are doing a compact representation of the layout
  // this is just not needed, since the fileoffsets are incremented in the
  // order the sections are seen.
  if (!config().options().isCompact())
    Seg->sortSections();

  ELFSegment::iterator iterB = Seg->begin(), iterE = Seg->end(), iterNext;
  auto isSectionNoLoad = [](const ELFSection *S) -> bool {
    OutputSectionEntry *Sec = S->getOutputSection();
    if (!Sec)
      return false;
    return (Sec->prolog().type() == OutputSectDesc::NOLOAD);
  };

  if (config().options().allowBssConversion()) {
    std::vector<ELFSection *> NoBitsSections;
    for (; iterB != iterE; ++iterB) {
      OutputSectionEntry *outSection = (*iterB);
      ELFSection *cur = outSection->getSection();
      if (cur->hasOffset() || !cur->size() || isSectionNoLoad(cur))
        continue;
      iterNext = iterB + 1;
      ELFSection *next = nullptr;
      if (iterNext != iterE)
        next = (*iterNext)->getSection();
      if (cur->isNoBits() && !cur->isTLS()) {
        NoBitsSections.push_back(cur);
        if (next && !next->isNoBits()) {
          // reset all nobits sections to section type PROGBITS
          for (auto &S : NoBitsSections) {
            config().raise(Diag::promoting_bss_to_progbits)
                << S->name()
                << ELFSection::getELFTypeStr(S->name(), S->getType())
                << ELFSection::getELFTypeStr(next->name(), next->getType())
                << next->name();
            S->setType(llvm::ELF::SHT_PROGBITS);
          }
          NoBitsSections.clear();
        } // next
      } // NoBits/TLS check
    } // Iterate over all sections
  } // Check and allow BSS conversion

  iterB = Seg->begin(), iterE = Seg->end();
  for (; iterB != iterE; ++iterB) {
    OutputSectionEntry *outSection = (*iterB);
    ELFSection *cur = outSection->getSection();
    if (cur->hasOffset()) {
      // This can only happen for multiple sections when they
      // get loaded in multiple segments.
      if (offset < (int64_t)(cur->offset() + cur->size()))
        prev = cur;
      continue;
    }
    if (!cur->isWanted()) {
      cur->setOffset(offset);
      continue;
    }
    if (prev) {
      bool isPrevTBSS = prev->isTBSS();
      // If the linker has not seen a PROGBITS section in the segment
      // treat the new section as though its the start of the segment
      if (isPrevTBSS && !prevProgBitsInSegment)
        isBeginningOfSection = true;
      if (isPrevTBSS && prevProgBitsInSegment &&
          (cur->getType() != llvm::ELF::SHT_NOBITS))
        offset = prevProgBitsInSegment->offset() +
                 (cur->addr() - prevProgBitsInSegment->addr());
      else if (isSectionNoLoad(prev)) {
        if (cur->isNoBits())
          offset = prev->offset();
        else if (prevProgBitsInSegment)
          offset = prevProgBitsInSegment->offset() +
                   (cur->addr() - prevProgBitsInSegment->addr());
        else // PROGBITS
          offset = prev->offset() + (cur->addr() - Seg->front()->addr());
      } else if (prev->isNoBits()) {
        offset = prev->offset();
      } else if ((isBeginningOfSection) || (config().options().isCompact()) ||
                 (cur->addr() <= prev->addr()) || (cur->isNoBits()))
        offset = prev->offset() + prev->size();
      else
        offset = prev->offset() + (cur->addr() - prev->addr());
    }
    if (!cur->isNoBits())
      prevProgBitsInSegment = cur;
    if (config().options().isCompact() || !cur->isBSS()) {
      if (!outSection->prolog().hasVMA())
        alignAddress((uint64_t &)offset, cur->getAddrAlign());
    }
    if (isBeginningOfSection && !isTLSSegment) {
      int64_t segMaxAlign = Seg->getMaxSectionAlign();
      int64_t segAlign = Seg->align();
      segAlign = segMaxAlign > segAlign ? segMaxAlign : segAlign;
      int64_t new_offset = alignToELFSpec(cur->addr(), offset, segAlign);
      // Check for signed integer overflow, as the address is unsigned.
      if ((cur->addr() > (uint64_t)offset) && (new_offset > segAlign) &&
          ((uint64_t)new_offset > cur->addr()))
        offset = cur->addr();
      else
        offset = new_offset;
      if (config().options().isCompact() &&
          ((uint64_t)offset % segAlign != Seg->front()->pAddr() % segAlign))
        config().raise(Diag::physical_address_and_offset_are_not_congruent)
            << Seg->front()->name();
      isBeginningOfSection = false;
      BeginOffset = offset;
    }
    cur->setOffset(offset);
    if (IsSectionTracingRequested && config().options().traceSection(cur))
      config().raise(Diag::section_info)
          << cur->getDecoratedName(config().options())
          << utility::toHex(static_cast<uint64_t>(cur->pAddr()))
          << utility::toHex(static_cast<uint64_t>(cur->addr()))
          << utility::toHex(static_cast<uint64_t>(offset)) << Seg->name()
          << Seg->type();

    if (Check) {
      if (config().options().isCompact() && cur->isAlloc() &&
          (uint64_t)(offset - BeginOffset + Seg->front()->pAddr()) !=
              cur->pAddr())
        config().raise(Diag::physical_address_not_in_syn_with_offset)
            << cur->name();
    }
    prev = cur;
  }
  return std::make_pair(BeginOffset, isTLSSegment ? P : prev);
}

bool GNULDBackend::setupSegment(ELFSegment *E) {
  uint64_t lower = ULLONG_MAX;
  uint64_t upper = 0;
  uint64_t lower_offset = ULLONG_MAX;
  uint64_t upper_offset = 0;
  uint64_t segAlign = E->align();
  bool canMergePrevious = true;

  eld::RegisterTimer T("Setup Segment", "Perform Layout",
                       m_Module.getConfig().options().printTimingStats());
  for (auto &O : *E) {
    bool isCurSectionNoLoad = (O->prolog().type() == OutputSectDesc::NOLOAD);
    // If the section is not bss and the size is more than 0
    ELFSection *cur = O->getSection();
    bool isTLS = cur->isTLS();
    bool isCurBSS = cur->isNoBits() || isCurSectionNoLoad;

    // Skip .tbss for PT_LOAD and GNU_RELRO as it is not really a section.
    if ((isTLS && isCurBSS) && (E->type() == llvm::ELF::PT_LOAD ||
                                E->type() == llvm::ELF::PT_GNU_RELRO))
      continue;

    // Dont count empty sections.
    if (!cur->size())
      continue;

    // All PT_LOAD segments should be compliant to ELF standard.
    bool isELFCompliant =
        (E->type() == llvm::ELF::PT_LOAD) &&
        ((cur->offset() % segAlign) == (cur->addr() % segAlign));

    if (canMergePrevious && (cur->size() || cur->isWanted()) &&
        isELFCompliant) {
      E->setOffset(cur->offset());
      E->setVaddr(cur->addr());
      E->setPaddr(cur->pAddr());
      canMergePrevious = false;
      lower = cur->addr();
      upper = cur->addr() + cur->size();
      lower_offset = cur->offset();
      if (!isCurBSS)
        upper_offset = cur->offset() + cur->size();
      else
        upper_offset = cur->offset();
    }

    if (cur->addr() < lower)
      lower = cur->addr();
    if (cur->offset() < lower_offset)
      lower_offset = cur->offset();

    if ((cur->addr() + cur->size()) > upper)
      upper = cur->addr() + cur->size();

    if (!isCurBSS && (cur->offset() + cur->size()) > upper_offset)
      upper_offset = cur->offset() + cur->size();
    else if (isCurBSS && cur->offset() > upper_offset)
      upper_offset = cur->offset();
  }
  size_t filesz = 0;
  size_t memsz = 0;
  if (lower_offset != ULLONG_MAX)
    filesz = upper_offset - lower_offset;
  if (lower != ULLONG_MAX)
    memsz = upper - lower;
  E->setFilesz(filesz);
  E->setMemsz(memsz);

  if (E->type() != llvm::ELF::PT_LOAD && !filesz && !memsz) {
    E->setVaddr(0);
    E->setPaddr(0);
  }
  return true;
}

void GNULDBackend::clearSegmentOffset(ELFSegment *S) {
  eld::RegisterTimer T("Clear Segment Offsets", "Evaluate Expressions",
                       m_Module.getConfig().options().printTimingStats());
  if (m_OffsetsAssigned)
    return;
  for (auto &OS : S->sections())
    OS->getSection()->setNoOffset();
}

/// setupProgramHdrs - set up the attributes of segments as per PHDRS
bool GNULDBackend::setupProgramHdrs() {
  {
    eld::RegisterTimer X("Setup Program Headers", "Establish Layout",
                         m_Module.getConfig().options().printTimingStats());
    int64_t segmentOffset = 0;
    // update segment info
    for (auto &Seg : elfSegmentTable()) {

      // bypass if there is no section in this segment (e.g., PT_GNU_STACK)
      if (Seg->size() == 0) {
        if (Seg->type() == llvm::ELF::PT_LOAD)
          Seg->setOffset(segmentOffset);
        continue;
      }

      Seg->setOffset(Seg->front()->offset());
      Seg->setVaddr(Seg->front()->addr());
      Seg->setPaddr(Seg->front()->pAddr());

      segmentOffset = Seg->front()->offset();

      if (!setupSegment(Seg))
        return false;

      segmentOffset += Seg->filesz();
    }
  }

  // Setup the TLS parameters.
  {
    eld::RegisterTimer X("Setup TLS parameters", "Establish Layout",
                         m_Module.getConfig().options().printTimingStats());
    ELFSegment *T =
        elfSegmentTable().find(llvm::ELF::PT_TLS, llvm::ELF::PF_R, 0);
    if (T)
      setTLSTemplateSize(T->memsz());
  }
  return true;
}

/// getSegmentFlag - give a section flag and return the corresponding segment
/// flag
uint32_t GNULDBackend::getSegmentFlag(const uint32_t pSectionFlag) {
  uint32_t flag = 0x0;
  if ((pSectionFlag & llvm::ELF::SHF_ALLOC) != 0x0)
    flag |= llvm::ELF::PF_R;
  if ((pSectionFlag & llvm::ELF::SHF_WRITE) != 0x0)
    flag |= llvm::ELF::PF_W;
  if ((pSectionFlag & llvm::ELF::SHF_EXECINSTR) != 0x0)
    flag |= llvm::ELF::PF_X;
  if (config().options().hasExecuteOnlySegments() && (flag & llvm::ELF::PF_X))
    flag &= ~llvm::ELF::PF_R;
  return flag;
}

/// setOutputSectionOffset - helper function to set output sections' offset.
bool GNULDBackend::setOutputSectionOffset() {
  LinkerScript &script = m_Module.getScript();
  uint64_t offset = 0x0;
  ELFSection *prev = nullptr;
  SectionMap::iterator out, outBegin, outEnd;
  out = outBegin = script.sectionMap().begin();
  outEnd = script.sectionMap().end();
  uint32_t atIndex = 0;

  uint64_t one_ehdr_size = 0;

  eld::RegisterTimer T("Setup Output Section Offset", "Establish Layout",
                       m_Module.getConfig().options().printTimingStats());

  if (config().targets().is32Bits())
    one_ehdr_size = sizeof(llvm::ELF::Elf32_Ehdr);
  else
    one_ehdr_size = sizeof(llvm::ELF::Elf64_Ehdr);

  // Set initial dot symbol value.
  LDSymbol *dotSymbol = m_Module.getNamePool().findSymbol(".");

  while (out != outEnd) {
    ELFSection *cur = (*out)->getSection();

    bool isCurAlloc = cur->isAlloc();

    // Initialize value of dot symbol. For partial linking there is no real VMA.
    dotSymbol->setValue(0);

    // Ignore Null sections and non allocatable sections
    if ((cur->isNullKind()) && (prev == nullptr)) {
      offset = one_ehdr_size;
      cur->setOffset(one_ehdr_size);
    }
    evaluateAssignments(*out, atIndex);
    if (!config().getDiagEngine()->diagnose()) {
      if (m_Module.getPrinter()->isVerbose())
        config().raise(Diag::function_has_error) << __PRETTY_FUNCTION__;
      return false;
    }
    if (prev) {
      if (prev->isNoBits())
        offset = prev->offset();
      else
        offset = prev->offset() + prev->size();
    }
    if (!cur->isNoBits())
      alignAddress(offset, cur->getAddrAlign());
    cur->setOffset(offset);
    if (isCurAlloc && cur->size())
      changeSymbolsFromAbsoluteToGlobal(*out);
    prev = cur;
    ++out;
  }
  return true;
}

void GNULDBackend::checkCrossReferencesHelper(InputFile *input) {
  ELFObjectFile *ObjFile = llvm::dyn_cast<ELFObjectFile>(input);
  if (!ObjFile)
    return;
  for (auto &rs : ObjFile->getRelocationSections()) {
    if (rs->isIgnore())
      continue;
    if (rs->isDiscard())
      continue;

    for (auto &relocation : rs->getLink()->getRelocations()) {
      // bypass the reloc if the symbol is in the discarded input section
      ResolveInfo *info = relocation->symInfo();

      if (!info->outSymbol()->hasFragRef() &&
          ResolveInfo::Section == info->type() &&
          ResolveInfo::Undefined == info->desc())
        continue;

      ELFSection *target_sect =
          relocation->targetRef()->frag()->getOwningSection();
      // bypass the reloc if the section where it sits will be discarded.
      if (target_sect->isIgnore())
        continue;

      if (target_sect->isDiscard())
        continue;

      // Symbols ignored should not be looked into.
      if (info->outSymbol()->shouldIgnore())
        continue;

      // scan relocation
      getRelocator()->checkCrossReferences(*relocation, *input, *target_sect);
    } // for all relocations
  } // for all relocation section
}

bool GNULDBackend::checkCrossReferences() {
  eld::RegisterTimer T("Evaluate NOCROSSREFS", "Establish Layout",
                       m_Module.getConfig().options().printTimingStats());
  if (config().options().numThreads() <= 1 ||
      !config().isCheckCrossRefsMultiThreaded()) {
    if (m_Module.getPrinter()->traceThreads())
      config().raise(Diag::threads_disabled) << "CheckCrossRefs";
    for (auto &input : m_Module.getObjectList()) {
      checkCrossReferencesHelper(input);
    }
  } else {
    if (m_Module.getPrinter()->traceThreads())
      config().raise(Diag::threads_enabled)
          << "CheckCrossRefs" << config().options().numThreads();
    llvm::ThreadPoolInterface *Pool = m_Module.getThreadPool();
    for (auto &input : m_Module.getObjectList()) {
      Pool->async([&] { checkCrossReferencesHelper(input); });
    }
    Pool->wait();
  }
  if (!config().getDiagEngine()->diagnose()) {
    if (m_Module.getPrinter()->isVerbose())
      config().raise(Diag::function_has_error) << __PRETTY_FUNCTION__;
    return false;
  }
  return true;
}

/// placeOutputSections - place output sections based on SectionMap
bool GNULDBackend::placeOutputSections() {
  typedef std::vector<ELFSection *> Orphans;
  bool isPartialLink = (LinkerConfig::Object == config().codeGenType());
  Orphans orphans;
  bool isError = false;
  LinkerScript &script = m_Module.getScript();
  SectionMap &sectionMap = script.sectionMap();
  LayoutInfo *layoutInfo = m_Module.getLayoutInfo();
  {
    eld::RegisterTimer T("Select Sections Needed in Output",
                         "Place Output Sections",
                         m_Module.getConfig().options().printTimingStats());
    for (auto &elem : m_Module) {
      bool wanted = false;
      if (elem->isDiscard() || elem->isIgnore()) {
        if (IsSectionTracingRequested && config().options().traceSection(elem))
          config().raise(Diag::discarded_section_info)
              << elem->getDecoratedName(config().options());
        continue;
      }

      switch ((elem)->getKind()) {
      // take nullptr and StackNote directly
      case LDFileFormat::Null:
      case LDFileFormat::StackNote:
        wanted = true;
        break;
      // ignore if section size is 0
      case LDFileFormat::EhFrame:
        if ((elem)->size() || config().codeGenType() == LinkerConfig::Object)
          wanted = true;
        break;
      case LDFileFormat::Relocation:
        if (elem->size() || ((elem)->hasRelocData() &&
                             config().codeGenType() == LinkerConfig::Object))
          wanted = true;
        break;
      case LDFileFormat::Common:
      case LDFileFormat::DynamicRelocation:
      case LDFileFormat::Internal:
        if (elem->size())
          wanted = true;
        break;
      case LDFileFormat::Regular:
      case LDFileFormat::Target:
      case LDFileFormat::MetaData:
      case LDFileFormat::Debug:
      case LDFileFormat::GCCExceptTable:
      case LDFileFormat::Note:
      case LDFileFormat::EhFrameHdr:
      case LDFileFormat::MergeStr:
      case LDFileFormat::NamePool:
        // Place the section in the proper place as per the section permissions.
        if ((elem->size() || string::isValidCIdentifier(elem->name())) ||
            ((elem->hasSectionData() || elem->isWanted()) &&
             config().codeGenType() == LinkerConfig::Object))
          wanted = true;
        break;
      case LDFileFormat::Group:
        if (LinkerConfig::Object == config().codeGenType()) {
          wanted = true;
        }
        break;
      case LDFileFormat::Timing:
        if (elem->size() || elem->hasSectionData())
          wanted = true;
        break;
      case LDFileFormat::Version:
        if ((elem)->hasSectionData() || elem->size()) {
          wanted = true;
          config().raise(Diag::warn_unsupported_symbolic_versioning)
              << (elem)->name();
        }
        break;
      case LDFileFormat::GNUProperty:
        break;
      default:
        if ((elem)->size())
          config().raise(Diag::err_unsupported_section)
              << (elem)->name() << (elem)->getKind();
        break;
      } // end of switch

      if ((LinkerConfig::Object == config().codeGenType()) &&
          (wanted || (elem)->isWanted()))
        (elem)->setWanted(wanted);

      // If the section is not really needed, because of its size constraint,
      // let
      // the orphan section deal with it.
      if (!wanted && elem->isWanted()) {
        if (!isNonDymSymbolStringTableSection(elem))
          isError |= handleOrphanSection(elem);
        orphans.push_back(elem);
      }

      OutputSectionEntry *unwantedSection = elem->getOutputSection();
      if (unwantedSection && !unwantedSection->hasOrder())
        unwantedSection->setOrder(getSectionOrder(*elem));

      if (wanted) {
        SectionMap::iterator out, outBegin, outEnd, outMatched;
        outBegin = sectionMap.begin();
        outEnd = sectionMap.end();
        outMatched = sectionMap.end();
        if (!m_Module.getConfig().options().shouldEmitUniqueOutputSections()) {
          for (out = outBegin; out != outEnd; ++out) {
            bool matched = false;
            if ((elem)->name().compare((*out)->name()) == 0) {
              switch ((*out)->prolog().constraint()) {
              case OutputSectDesc::NO_CONSTRAINT:
                matched = true;
                break;
              case OutputSectDesc::ONLY_IF_RO:
                matched = !elem->isWritable();
                break;
              case OutputSectDesc::ONLY_IF_RW:
                matched = elem->isWritable();
                break;
              } // end of switch

              // Must assign orders for all matched output sections but use
              // the first seen to do the work.
              if (matched) {
                if (!elem->isInGroup() || !isPartialLink) {
                  (*out)->setOrder(getSectionOrder(*elem));
                  if (outMatched == sectionMap.end())
                    outMatched = out;
                }
              }
            }
          } // for each output section description
        }

        out = outMatched;
        if (out != outEnd) {
          // force output alignment from ldscript if any
          uint64_t out_align = 0x0;
          if ((*out)->prolog().hasAlign()) {
            (*out)->prolog().align().evaluateAndRaiseError();
            out_align = (*out)->prolog().align().result();
            if (out_align > elem->getAddrAlign())
              elem->setAddrAlign(out_align);
          }

          // set up the section
          (*out)->setSection(elem);
          elem->setOutputSection(*out);
          if (elem->hasSectionData()) {
            for (auto &f : elem->getFragmentList())
              f->getOwningSection()->setOutputSection(*out);
          }
        } else {
          SectionMap::mapping pair;
          pair.first = elem->getOutputSection();
          if (pair.first && pair.first->isDiscard())
            (elem)->setKind(LDFileFormat::Null);
          else {
            if (!isNonDymSymbolStringTableSection(elem))
              isError |= handleOrphanSection(elem);
            orphans.push_back(elem);
          }
        }
      }
    } // for each section in Module
  }

  // set up sections in SectionMap but do not exist at all.
  uint32_t flag = 0x0;
  bool linkerScriptHasSectionsCommand =
      m_Module.getScript().linkerScriptHasSectionsCommand();
  unsigned int order = 0;
  bool debugSectionSeen = false;
  OutputSectDesc::Type type = OutputSectDesc::LOAD;
  {
    eld::RegisterTimer T("Set Section Permissions", "Place Output Sections",
                         m_Module.getConfig().options().printTimingStats());
    for (auto &elem : sectionMap) {
      ELFSection *cur = (elem)->getSection();
      if (cur->isNullKind() || cur->isNoteGNUStack() || cur->isNoteKind()) {
        (elem)->setOrder(getSectionOrder(*cur));
        (elem)->prolog().setType(type);
        flag = cur->getFlags();
      } else if (cur->isDebugKind()) {
        order = getSectionOrder(*cur);
        elem->setOrder(order);
        flag = cur->getFlags();
        debugSectionSeen = true;
      } else if (!cur->getFlags() &&
                 (cur->isWanted() || linkerScriptHasSectionsCommand)) {
        elem->setOrder(order);
        if (LinkerConfig::Object != config().codeGenType()) {
          if (debugSectionSeen && !cur->size())
            cur->setFlags(flag);
          llvm::StringRef name = cur->name();
          if (name.starts_with(".debug") || name.starts_with(".zdebug") ||
              name.starts_with(".line") || name.starts_with(".stab")) {
            if (!cur->isIgnore() && !cur->isDiscard())
              cur->setKind(LDFileFormat::Debug);
            cur->setType(llvm::ELF::SHT_PROGBITS);
            cur->setFlags(cur->getFlags());
            debugSectionSeen = true;
            order = getSectionOrder(*cur);
            elem->setOrder(order);
          } else {
            if (!cur->size())
              cur->setFlags(llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE);
            if (cur->getKind() != LDFileFormat::Target)
              cur->setKind(LDFileFormat::Regular);
          }
          if (!cur->getType())
            cur->setType(llvm::ELF::SHT_NOBITS);
          if (!cur->getAddrAlign())
            cur->setAddrAlign(1);
          flag = cur->getFlags();
        }
      } else {
        flag = cur->getFlags();
        if (!linkerScriptHasSectionsCommand) {
          order = getSectionOrder(*cur);
          (elem)->setOrder(order);
        } else {
          order = (elem)->order();
        }
        type = (elem)->prolog().type();
      }
    } // for each output section description
  }

  // sort output section orders if there is no default ldscript
  if (!linkerScriptHasSectionsCommand) {
    eld::RegisterTimer T("Sort Output Section Order", "Place Output Sections",
                         m_Module.getConfig().options().printTimingStats());
    std::stable_sort(
        sectionMap.begin(), sectionMap.end(),
        [](const OutputSectionEntry *LHS, const OutputSectionEntry *RHS) {
          return LHS->order() < RHS->order();
        });
  }

  // place orphan sections
  {
    eld::RegisterTimer T("Place Orphan Sections", "Place Output Sections",
                         m_Module.getConfig().options().printTimingStats());
    for (auto &orphan : orphans) {
      if (layoutInfo)
        layoutInfo->recordOrphanSection();
      // If the section has an @ command, the section remains as an orphan
      // section
      // permanently, When the output sections are created, it assigns the right
      // place for these sections.
      size_t atpos = orphan->name().find("@");
      if (atpos != llvm::StringRef::npos) {
        uint64_t addr;
        orphan->name()
            .slice(atpos + 1, orphan->name().size())
            .getAsInteger(0, addr);
        orphan->setAddr(addr);
        orphan->setFixedAddr();
        if ((addr % orphan->getAddrAlign()) != 0) {
          config().raise(Diag::at_section_not_aligned) << orphan->name();
          alignAddress(addr, orphan->getAddrAlign());
          orphan->setAddr(addr);
        }
        if (!orphan->isRelocationKind())
          m_Module.getAtTable().push_back(orphan);
        continue;
      }
      size_t order = getSectionOrder(*orphan);
      SectionMap::iterator out, outBegin, outEnd;
      outBegin = sectionMap.begin();
      outEnd = sectionMap.end();

      if ((orphan)->getKind() == LDFileFormat::Null)
        out = sectionMap.insert(outBegin, orphan);
      else {
        // Orphan placement Doesn't matter so much for partial link steps.
        // Skip the first discard section if any. We cannot skip the discard
        // later
        // though. The value is initialized to -1, to skip the Null section
        // inserted.
        int numSections = -1;
        for (out = outBegin; out != outEnd; ++out) {
          if (numSections && (*out)->isDiscard() && (order <= SHO_UNDEFINED))
            break;
          if ((*out)->order() > order) {
            break;
          }
          ++numSections;
        }
        if (orphan->getOutputSection())
          out = sectionMap.insert(out, orphan->getOutputSection());
        else {
          out = sectionMap.insert(out, orphan);
          if (orphan->hasSectionData()) {
            for (auto &f : orphan->getFragmentList())
              f->getOwningSection()->setOutputSection(*out);
          }
        }
      }
      orphan->setOutputSection(*out);
      (*out)->setOrder(order);
      if (linkerScriptHasSectionsCommand &&
          (orphan->getFlags() & llvm::ELF::SHF_ALLOC) == llvm::ELF::SHF_ALLOC) {
        SectionMap::iterator prev = --out;
        SectionMap::iterator cur = ++out;
        // Move section assignments only if the orphan section and the previous
        // section prior to the orphan section have the same permissions.
        // Behavior seen from GNU.
        if ((out != sectionMap.begin() && ((*cur)->getSection()->getFlags() ==
                                           (*prev)->getSection()->getFlags())))
          (*cur)->moveSectionAssignments(*prev);
      }
    }
  }
  // Set the right properties for sections.
  {
    eld::RegisterTimer T("Override Output Section Properties",
                         "Place Output Sections",
                         m_Module.getConfig().options().printTimingStats());
    for (SectionMap::iterator out = sectionMap.begin(),
                              outEnd = sectionMap.end();
         out != outEnd; ++out) {
      ELFSection *cur = (*out)->getSection();
      uint32_t type = (*out)->prolog().type();
      if (type == OutputSectDesc::DEFAULT_TYPE)
        (*out)->prolog().setType(OutputSectDesc::LOAD);
      else if (type == OutputSectDesc::PROGBITS) {
        if (!cur->isProgBits()) {
          config().raise(Diag::change_section_type)
              << ELFSection::getELFTypeStr("", llvm::ELF::SHT_PROGBITS)
              << ELFSection::getELFTypeStr(cur->name(), cur->getType())
              << cur->name();
          cur->setType(llvm::ELF::SHT_PROGBITS);
        }
        (*out)->prolog().setType(OutputSectDesc::LOAD);
      }
      // If the prolog doesn't have a flag specified, then move on.
      if (!(*out)->prolog().hasFlag())
        continue;
      uint32_t flag = (*out)->prolog().flag();
      if (flag != OutputSectDesc::DEFAULT_PERMISSIONS) {
        uint32_t old_flag = cur->getFlags();
        uint32_t new_flag = flag | old_flag;
        // If its an empty section, use user specified flag
        // as the default flag.
        if (!cur->size())
          new_flag = flag;
        config().raise(Diag::change_section_perm)
            << ELFSection::getELFPermissionsStr(new_flag)
            << ELFSection::getELFPermissionsStr(old_flag) << cur->name();
        cur->setFlags(new_flag);
      }
    }
  }

  // Sort the At Table by the address, so its easy to place them.
  std::vector<ELFSection *> &atTable = m_Module.getAtTable();
  std::stable_sort(
      atTable.begin(), atTable.end(),
      [](ELFSection *A, ELFSection *B) { return A->addr() < B->addr(); });
  return !isError;
}

/// layout - layout method
bool GNULDBackend::layout() {
  bool isPartialLink = (LinkerConfig::Object == config().codeGenType());

  // Place output sections and orphan sections properly.
  if (!placeOutputSections())
    return false;

  if (!config().getDiagEngine()->diagnose()) {
    if (m_Module.getPrinter()->isVerbose())
      config().raise(Diag::function_has_error) << __PRETTY_FUNCTION__;
    return false;
  }

  // Check for any prohibited cross references
  if (m_Module.getNonRefSections().size()) {
    checkCrossReferences();
    if (!config().getDiagEngine()->diagnose()) {
      if (m_Module.getPrinter()->isVerbose())
        config().raise(Diag::function_has_error) << __PRETTY_FUNCTION__;
      return false;
    }
  }

  // Clear the section table so that real sections can be inserted properly.
  m_Module.clearOutputSections();

  // Evaluate assignments that are before the SECTIONS command.
  {
    eld::RegisterTimer T("Evaluate Script Assignments", "Establish Layout",
                         m_Module.getConfig().options().printTimingStats());
    evaluateScriptAssignments(/*afterLayout=*/false);
  }

  // If partial link, we only set offsets, no addresses.
  if (isPartialLink) {
    if (!setOutputSectionOffset()) {
      m_Module.setFailure(true);
      return false;
    }
  } else {
    if (!assignMemoryRegions())
      return false;
    relax();
  }
  if (!config().getDiagEngine()->diagnose()) {
    if (m_Module.getPrinter()->isVerbose())
      config().raise(Diag::function_has_error) << __PRETTY_FUNCTION__;
    return false;
  }

  // Evaluate assignments that are after the SECTIONS command. Or in case,
  // there is no SECTIONS command, then evaluate BEFORE_SECTIONS assignments
  // again.
  {
    eld::RegisterTimer T("Evaluate Script Assignments",
                         "Establish Layout",
                         m_Module.getConfig().options().printTimingStats());
    evaluateScriptAssignments(/*afterLayout=*/true);
  }

  if (!config().getDiagEngine()->diagnose()) {
    if (m_Module.getPrinter()->isVerbose())
      config().raise(Diag::function_has_error) << __PRETTY_FUNCTION__;
    return false;
  }

  {
    eld::RegisterTimer T("Define Magic Symbols", "Establish Layout",
                         m_Module.getConfig().options().printTimingStats());
    if (!defineStandardAndSectionMagicSymbols())
      return false;
  }

  // Set up program headers.
  if (!isPartialLink) {
    if (!setupProgramHdrs()) {
      m_Module.setFailure(true);
      return false;
    }
  }

  // Insert all sections that are needed in the section table
  {
    eld::RegisterTimer T("Do Post Layout", "Establish Layout",
                         m_Module.getConfig().options().printTimingStats());
    doPostLayout();
  }

  if (m_Module.getScript().phdrsSpecified() && !config().isLinkPartial())
    checkForLinkerScriptPhdrErrors();

  // Check for segment information and emit warnings if any
  verifySegments();

  if (!config().getDiagEngine()->diagnose()) {
    if (m_Module.getPrinter()->isVerbose())
      config().raise(Diag::function_has_error) << __PRETTY_FUNCTION__;
    return false;
  }

  // Create program headers.
  if (!isPartialLink) {
    if (!isEhdrInLayout() ||
        (m_ehdr->getOutputSection() &&
         !isOutputSectionInLoadSegment(m_ehdr->getOutputSection())))
      m_StartOffset = m_ehdr->size();

    if (!isPhdrInLayout() ||
        (m_phdr->getOutputSection() &&
         !isOutputSectionInLoadSegment(m_phdr->getOutputSection())))
      m_StartOffset += m_phdr->size();

    // Assign Offsets.
    if (!assignOffsets(m_StartOffset)) {
      m_Module.setFailure(true);
      return false;
    }
    if (!setupProgramHdrs()) {
      m_Module.setFailure(true);
      return false;
    }
  }

  // Evaluate all asserts.
  evaluateAsserts();

  return config().getDiagEngine()->diagnose();
}

// Create or return an already created relocation output section for partial
// linking.
ELFSection *GNULDBackend::getOutputRelocationSection(ELFSection *S,
                                                     ELFSection *rs) {
  // Find if there is a relocation section for the Output Section
  auto R = m_RelocationSectionForOutputSection.find(S);
  if (R == m_RelocationSectionForOutputSection.end()) {
    std::string outputName = getOutputRelocSectName(
        std::string(S->name()), getRelocator()->relocType());
    ELFSection *output_sect =
        m_Module.createOutputSection(outputName, rs->getKind(), rs->getType(),
                                     rs->getFlags(), rs->getAddrAlign());
    output_sect->setEntSize(rs->getEntSize());
    m_RelocationSectionForOutputSection[S] = output_sect;
    return output_sect;
  }
  return R->second;
}

/// preLayout - Backend can do any needed modification before layout
void GNULDBackend::preLayout() {
  // prelayout target first
  {
    eld::RegisterTimer T("Do Target Pre-Layout", "Pre-Layout",
                         m_Module.getConfig().options().printTimingStats());
    doPreLayout();
  }

  // To merge input's relocation sections into output's relocation sections.
  //
  // If we are generating relocatables (-r), move input relocation sections
  // to corresponding output relocation sections.
  if (LinkerConfig::Object == config().codeGenType()) {
    eld::RegisterTimer T("Copy Input Relocations to Output", "Perform Layout",
                         m_Module.getConfig().options().printTimingStats());
    Module::obj_iterator input, inEnd = m_Module.objEnd();
    for (input = m_Module.objBegin(); input != inEnd; ++input) {
      ELFObjectFile *ObjFile = llvm::dyn_cast<eld::ELFObjectFile>(*input);
      if (!ObjFile)
        continue;
      for (auto &rs : ObjFile->getRelocationSections()) {

        if (rs->isIgnore())
          continue;
        if (rs->isDiscard())
          continue;

        // get the output relocation ELFSection with name in accordance with
        // linker script rule for the section where relocations are patched
        ELFSection *section = rs->getLink();
        if (section->getOutputSection())
          section = section->getOutputSection()->getSection();

        ELFSection *output_sect = getOutputRelocationSection(section, rs);

        // set output relocation section link
        const ELFSection *input_link = rs->getLink();
        assert(nullptr != input_link && "Illegal input relocation section.");

        // get the linked output section
        ELFSection *output_link = nullptr;

        OutputSectionEntry *outputSection = input_link->getOutputSection();
        if (outputSection)
          output_link = outputSection->getSection();
        else
          output_link = m_Module.getSection(input_link->name().str());

        rs->setOutputSection(output_sect->getOutputSection());

        assert(nullptr != output_link);

        output_sect->setLink(output_link);

        for (auto &reloc : rs->getLink()->getRelocations())
          output_sect->addRelocation(reloc);

        // size output
        if (llvm::ELF::SHT_REL == output_sect->getType())
          output_sect->setSize(output_sect->getRelocations().size() *
                               getRelEntrySize());
        else if (output_sect->isRela())
          output_sect->setSize(output_sect->getRelocations().size() *
                               getRelaEntrySize());
        else {
          config().raise(Diag::unknown_reloc_section_type)
              << output_sect->getType() << output_sect->name();
        }
      } // end of for each relocation section
    } // end of for each input
  } // end of if
}

/// postLayout - Backend can do any needed modification after layout
bool GNULDBackend::postLayout() {
  eld::RegisterTimer T("Do Post Layout", "Post-Layout",
                       m_Module.getConfig().options().printTimingStats());
  size_t sectionIdx = 0;
  LayoutInfo *layoutInfo = m_Module.getLayoutInfo();
  std::vector<ELFSection *> outputSections;
  for (auto &out : m_Module.getScript().sectionMap()) {
    ELFSection *cur = out->getSection();
    if (cur->isWanted() || cur->size() ||
        cur->getKind() == LDFileFormat::Null) {
      if (cur->isNullType() && !cur->name().empty())
        continue;
      if (layoutInfo)
        layoutInfo->recordOutputSection();
      m_OutputSectionMap[cur] = cur;
      m_Module.addOutputSection(cur);
      cur->setIndex(sectionIdx++);
      if (cur->size())
        outputSections.push_back(cur);
    }
  }
  if (!config().getDiagEngine()->diagnose()) {
    if (m_Module.getPrinter()->isVerbose())
      config().raise(Diag::function_has_error) << __PRETTY_FUNCTION__;
    return false;
  }

  if (!config().options().doCheckOverlaps())
    return true;

  // Check for overlapping file offsets. In this case we need to skip any
  // section marked as SHT_NOBITS. These sections don't actually occupy space in
  // the file so Sec->Offset + Sec->Size can overlap with others. If --oformat
  // binary is specified only add SHF_ALLOC sections are added to the output
  // file so we skip any non-allocated sections in that case.
  std::vector<SectionOffset> fileOffs;
  for (ELFSection *sec : outputSections) {
    if (!sec->isNoBits())
      fileOffs.push_back({sec, sec->offset()});
  }
  checkOverlap("file", fileOffs, false);

  // When linking with -r there is no need to check for overlapping virtual/load
  // addresses since those addresses will only be assigned when the final
  // executable/shared object is created.
  if (config().isLinkPartial())
    return true;

  // Checking for overlapping virtual and load addresses only needs to take
  // into account SHF_ALLOC sections since others will not be loaded.
  // Furthermore, we also need to skip SHF_TLS sections since these will be
  // mapped to other addresses at runtime and can therefore have overlapping
  // ranges in the file.
  std::vector<SectionOffset> vmas;
  for (ELFSection *sec : outputSections) {
    if (sec->size() > 0 && (sec->isAlloc() && !sec->isTLS()))
      vmas.push_back({sec, sec->addr()});
  }
  checkOverlap("virtual address", vmas, true);

  // Finally, check that the load addresses don't overlap. This will usually be
  // the same as the virtual addresses but can be different when using a linker
  // script with AT().
  std::vector<SectionOffset> lmas;
  for (ELFSection *sec : outputSections) {
    if (sec->size() > 0 && (sec->isAlloc() && !sec->isTLS()))
      lmas.push_back({sec, sec->pAddr()});
  }
  checkOverlap("load address", lmas, false);
  return true;
}

std::string GNULDBackend::rangeToString(uint64_t addr, uint64_t len) {
  return "[0x" + llvm::utohexstr(addr) + ", 0x" +
         llvm::utohexstr(addr + len - 1) + "]";
}

// Check whether sections overlap for a specific address range (file offsets,
// load and virtual addresses).
void GNULDBackend::checkOverlap(llvm::StringRef name,
                                std::vector<SectionOffset> &sections,
                                bool isVirtualAddr) {
  llvm::sort(sections, [=](const SectionOffset &a, const SectionOffset &b) {
    return a.offset < b.offset;
  });

  // Finding overlap is easy given a vector is sorted by start position.
  // If an element starts before the end of the previous element, they overlap.
  for (size_t i = 1, end = sections.size(); i < end; ++i) {
    SectionOffset a = sections[i - 1];
    SectionOffset b = sections[i];
    if (b.offset >= a.offset + a.sec->size())
      continue;
    config().raise(Diag::error_section_overlap)
        << a.sec->name() << std::string(name) << b.sec->name() << a.sec->name()
        << rangeToString(a.offset, a.sec->size()) << b.sec->name()
        << rangeToString(b.offset, b.sec->size());
  }
}

void GNULDBackend::finalizeBeforeWrite() {
  SectionMap::iterator out, outBegin, outEnd;
  LinkerScript &script = m_Module.getScript();
  outBegin = script.sectionMap().begin();
  outEnd = script.sectionMap().end();

  bool recalculateOffsets = false;

  ELFSection *prev = nullptr;

  ELFSection *shstrtab = getOutputFormat()->getShStrTab();

  ELFSection *symtab = getOutputFormat()->getSymTab();

  eld::RegisterTimer T("Set Offset of SymTab", "Perform Layout",
                       m_Module.getConfig().options().printTimingStats());

  for (out = outBegin; out != outEnd; ++out) {
    ELFSection *cur = (*out)->getSection();
    if (recalculateOffsets) {
      uint64_t offset = prev->offset() + prev->size();
      alignAddress(offset, cur->getAddrAlign());
      cur->setOffset(offset);
    }
    if (cur == symtab || cur == shstrtab) {
      recalculateOffsets = true;
      if (cur == symtab)
        sizeSymTab();
      if (cur == shstrtab)
        sizeShstrtab();
    }
    prev = cur;
  }
}

bool GNULDBackend::printLayout() {
  eld::RegisterTimer T("Emit Map File", "Diagnostics",
                       m_Module.getConfig().options().printTimingStats());
  // Emit Map
  TextLayoutPrinter *printer = m_Module.getTextMapPrinter();
  if (printer) {
    printer->printMapFile(m_Module);
  }
  // print Cross Reference table, either on stdout
  // or in map file
  if (config().options().cref())
    printCref(true);

  // Emit YAML Map
  YamlLayoutPrinter *YAMLMapPrinter = m_Module.getYAMLMapPrinter();
  if (YAMLMapPrinter) {
    YAMLMapPrinter->printLayout(m_Module, *this);
  }
  return true;
}

std::vector<GNULDBackend::PaddingT>
GNULDBackend::getPaddingBetweenFragments(ELFSection *section,
                                         const Fragment *StartFrag,
                                         const Fragment *EndFrag) const {

  std::vector<GNULDBackend::PaddingT> Padding;

  if (m_PaddingMap.find(section) == m_PaddingMap.end())
    return Padding;

  int64_t FirstFragEnd =
      StartFrag
          ? StartFrag->getOffset(config().getDiagEngine()) + StartFrag->size()
          : 0;

  int64_t LastFragStart =
      EndFrag ? EndFrag->getOffset(config().getDiagEngine()) : 0;

  for (const PaddingT &padding : m_PaddingMap.lookup(section)) {

    if (StartFrag && padding.startOffset < FirstFragEnd)
      continue;

    if (EndFrag && padding.endOffset > LastFragStart)
      return Padding;

    if (padding.startOffset < padding.endOffset)
      Padding.push_back(padding);
  }
  return Padding;
}

void GNULDBackend::printCref(bool pIsPostLTO) const {
  llvm::raw_ostream *stream;
  TextLayoutPrinter *printer = m_Module.getTextMapPrinter();
  if (printer)
    stream = &(printer->outputStream());
  else
    stream = &(llvm::outs());

  eld::RegisterTimer T("Emit CRef", "Diagnostics",
                       m_Module.getConfig().options().printTimingStats());

  const GeneralOptions::CrefTableType &table = config().options().crefTable();
  std::vector<const ResolveInfo *> symVector;
  GeneralOptions::CrefTableType::const_iterator itr = table.begin(),
                                                ite = table.end();
  for (; itr != ite; itr++)
    symVector.push_back(itr->first);

  std::sort(symVector.begin(), symVector.end(),
            [](const ResolveInfo *A, const ResolveInfo *B) {
              return (std::string(A->name()) < std::string(B->name()));
            });

  // GNU linker uses 54 as the column size for the report.
  size_t CRefColumnSize = 54;
  *stream << "\nCross Reference Table";
  if (!pIsPostLTO)
    *stream << " - Pre LTO phase";
  *stream << "\n\n";
  *stream << "Symbol";
  stream->indent(CRefColumnSize - strlen("Symbol"));
  *stream << "File\n";

  for (auto i : symVector) {
    std::string DecoratedName = m_Module.getNamePool().getDecoratedName(
        i, config().options().shouldDemangle());

    const std::vector<std::pair<const InputFile *, bool>> refSites =
        table.find(i)->second;

    if (pIsPostLTO) {
      LDSymbol *sym = m_Module.getNamePool().findSymbol(i->name());
      if (sym == nullptr || sym->resolveInfo()->isBitCode() ||
          !sym->hasFragRef() ||
          sym->fragRef()->frag()->getOwningSection()->isIgnore() ||
          sym->fragRef()->frag()->getOwningSection()->isDiscard())
        continue;
    }
    size_t column = CRefColumnSize;
    if (DecoratedName.size() > (column - 4))
      column = DecoratedName.size() + strlen("Symbol") + 1;

    *stream << DecoratedName;

    bool first = true;
    for (auto j : refSites) {
      if (pIsPostLTO && j.second)
        continue;

      if (first) {
        stream->indent(column - DecoratedName.size());
      } else {
        stream->indent(column);
      }
      first = false;

      *stream << j.first->getInput()->decoratedPath() << "\n";
    }
  }
  stream->flush();
}

void GNULDBackend::maybeFillRegion(const OutputSectionEntry *O,
                                   MemoryRegion R) const {
  auto Fill = m_PaddingMap.find(O->getSection());
  if (Fill == m_PaddingMap.end())
    return;
  fillRegion(R, Fill->getSecond());
}

void GNULDBackend::fillRegion(MemoryRegion &Region,
                              const std::vector<PaddingT> &FillV) const {
  for (auto &FV : FillV) {
    Expression *Fill = FV.Exp;
    if (!Fill)
      continue;
    uint64_t FillValue = Fill->result();
    uint64_t FillValueSize =
        FillValue > 0xFFFFFFFF
            ? 8
            : (FillValue > 0xFFFF ? 4 : (FillValue > 0xFF ? 2 : 1));
    std::array<uint8_t, sizeof(uint64_t)> Buf;
    llvm::support::endian::write64be(Buf.data(), FillValue);
    uint32_t StartIdx = 8 - FillValueSize;
    for (uint32_t I = 0; I < FV.endOffset - FV.startOffset; ++I)
      std::memcpy(Region.begin() + I + FV.startOffset,
                  Buf.data() + I % FillValueSize + StartIdx, 1);
  }
}

void GNULDBackend::fillValuesFromUser(llvm::FileOutputBuffer &pOutput) {
  if (!m_PaddingMap.size())
    return;

  eld::RegisterTimer T("Apply Fill Patterns", "Post Processing",
                       m_Module.getConfig().options().printTimingStats());

  for (auto &fsections : m_PaddingMap) {
    ELFSection *sec = fsections.first;
    MemoryRegion region =
        getFileOutputRegion(pOutput, sec->offset(), sec->size());
    if (sec->isNoBits())
      continue;
    fillRegion(region, fsections.second);
  }
}

eld::Expected<void>
GNULDBackend::postProcessing(llvm::FileOutputBuffer &pOutput) {
  fillValuesFromUser(pOutput);

  if (m_pEhFrameHdrFragment) {
    eld::RegisterTimer T("EhFrameHdr Emit", "Post Processing",
                         m_Module.getConfig().options().printTimingStats());
    MemoryRegion region =
        getFileOutputRegion(pOutput, 0, pOutput.getBufferSize());
    eld::Expected<void> expEmit =
        m_pEhFrameHdrFragment->emit(region, getModule());
    ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expEmit);
  }
  {
    eld::RegisterTimer T(
        "Replace Fragments from Plugin", "Post Processing",
        m_Module.getConfig().options().printTimingStats("Plugin"));
    for (auto &V : m_Module.getReplaceFrags()) {
      FragmentRef *F = V.first;
      MemoryArea *M = V.second;
      FragmentRef::Offset Off = F->getOutputOffset(m_Module);
      size_t out_offset = F->getOutputELFSection()->offset() + Off;
      uint8_t *target_addr = pOutput.getBufferStart() + out_offset;
      llvm::StringRef Contents = M->getContents();
      std::memcpy(target_addr, Contents.data(), Contents.size());
    }
  }
  return {};
}

/// elfSegmentTable - return the reference of the elf segment table
ELFSegmentFactory &GNULDBackend::elfSegmentTable() const {
  assert(m_pELFSegmentTable != nullptr && "Do not create ELFSegmentTable!");
  return *m_pELFSegmentTable;
}

/// commonPageSize - the common page size of the target machine.
uint64_t GNULDBackend::commonPageSize() const {
  if (config().options().hasCommPageSize())
    return std::min(config().options().commPageSize(), abiPageSize());
  else
    return std::min(m_pInfo->commonPageSize(), abiPageSize());
}

/// abiPageSize - the abi page size of the target machine.
uint64_t GNULDBackend::abiPageSize() const {
  if (config().options().hasMaxPageSize())
    return config().options().maxPageSize();
  else
    return m_pInfo->abiPageSize(
        m_Module.getScript().linkerScriptHasSectionsCommand());
}

/// isSymbolPreemtible - whether the symbol can be preemted by other
/// link unit
bool GNULDBackend::isSymbolPreemptible(const ResolveInfo &pSym) const {
  if (pSym.isDefine() && pSym.binding() == ResolveInfo::Local)
    return false;

  if (pSym.visibility() != ResolveInfo::Default && pSym.isUndef())
    return false;

  // For ELD, Weak undefined symbols are treated a bit differently from GNU
  // linker.
  //
  // PIC Code
  // ----------
  // With PIC code, global symbols are accessed via the GOT. So we do the
  // below. So when building shared objects/PIE executables we allow weak
  // undefined symbols to be preempted.  Preemption allows a GOT slot to be
  // created allowing preemption. For shared objects, the dynamic linker is
  // going to deposit the right value in the GOT/GOTPLT slot.
  //
  // Non PIC Code
  // -------------
  // The slot is never going to get filled in and will have the value 0.
  // For static/dynamic executables, we never allow weak undefined symbols to
  // be preemptible.
  if (pSym.isWeakUndef()) {
    if (config().options().isPIE())
      return true;
    if (!config().isCodeStatic())
      return true;
    if (LinkerConfig::DynObj == config().codeGenType())
      return true;
    return false;
  }

  if (pSym.isDyn() || pSym.isUndef()) {
    if (config().options().isPIE())
      return true;
    if (config().isCodeStatic())
      return false;
    return true;
  }

  if (pSym.other() != ResolveInfo::Default)
    return false;

  // This is because the codeGenType of pie is DynObj. And gold linker check
  // the "shared" option instead.
  if (config().options().isPIE())
    return false;

  if (LinkerConfig::DynObj != config().codeGenType())
    return false;

  if (config().options().bsymbolic())
    return false;

  if (config().options().bsymbolicFunctions() && pSym.isDefine() &&
      pSym.type() == ResolveInfo::Function)
    return false;

  if (pSym.isHidden())
    return false;

  // An IndirectFunc symbol (i.e., STT_GNU_IFUNC) always needs a plt entry
  if (pSym.type() == ResolveInfo::IndirectFunc)
    return true;

  const auto VersionInfo = SymbolScopes.find(&pSym);
  if (VersionInfo != SymbolScopes.end() && !(VersionInfo->second->isGlobal()))
    return false;

  return true;
}

/// canIssueUndef- check if we can issue an undefined reference
bool GNULDBackend::canIssueUndef(const ResolveInfo *pSym) {
  // Expect all non-default visibility symbols in shared objects to
  // be defined within
  bool MagicSym =
      isSectionMagicSymbol(pSym->name()) || isStandardSymbol(pSym->name());

  // Visibility trumps --unresolved-symbols behavior. Dont check Unresolved
  // symbol policy here.
  if (!MagicSym && pSym->isUndef() &&
      pSym->visibility() != ResolveInfo::Default &&
      LinkerConfig::DynObj == config().codeGenType())
    return true;

  if (pSym->isBitCode())
    return true;

  // Normal undefined symbol not found anywhere
  if (!MagicSym && pSym->isUndef() && !pSym->isDyn() && !pSym->isWeak() &&
      !pSym->isNull()) {
    bool IgnoreUndef =
        m_Module.getLinker()->shouldIgnoreUndefine(pSym->isDyn());
    // If we are ignoring undefines, but the symbol visibility is not default
    // the linker still reports it which is insane.
    if (IgnoreUndef && pSym->visibility() != ResolveInfo::Default)
      return true;
    if (!IgnoreUndef || config().options().isNoUndefined())
      return true;
  }

  // Reference to a discarded section is not allowed
  if (pSym->outSymbol()->hasFragRef()) {
    ELFSection *S = pSym->getOwningSection();

    // If the section has been marked DISCARD by a plugin, report the undef.
    if (S->isDiscard())
      return true;

    OutputSectionEntry *OutputDesc = S->getOutputSection();
    // Its a real bug if a linker script drives the link and the symbol is in a
    // DISCARD section. Dont check for Unresolved symbol policy here too.
    if ((OutputDesc && OutputDesc->isDiscard() &&
         OutputDesc->getSection()->isAlloc()))
      return true;
  }
  return false;
}
/// symbolNeedsDynRel - return whether the symbol needs a dynamic relocation
bool GNULDBackend::symbolNeedsDynRel(const ResolveInfo &pSym, bool pSymHasPLT,
                                     bool isAbsReloc) const {
  // an undefined reference in the executables should be statically
  // resolved to 0 and no need a dynamic relocation
  if (pSym.isUndef() && !pSym.isDyn() &&
      (LinkerConfig::Exec == config().codeGenType() ||
       LinkerConfig::Binary == config().codeGenType()))
    return false;

  // An absolute symbol can be resolved directly if it is either local
  // or we are linking statically. Otherwise it can still be overridden
  // at runtime.
  if (pSym.isAbsolute() &&
      (pSym.binding() == ResolveInfo::Local || config().isCodeStatic()))
    return false;
  if (config().isCodeIndep() && isAbsReloc)
    return true;
  if (config().isCodeIndep() && !isAbsReloc &&
      pSym.visibility() == ResolveInfo::Hidden)
    return false;
  if (pSymHasPLT && ResolveInfo::Function == pSym.type())
    return false;
  if (!config().isCodeIndep() && pSymHasPLT)
    return false;
  if (pSym.isDyn() || pSym.isUndef() || isSymbolPreemptible(pSym))
    return true;

  return false;
}

/// symbolNeedsCopyReloc - return whether the symbol needs a copy relocation
bool GNULDBackend::symbolNeedsCopyReloc(const Relocation &pReloc,
                                        const ResolveInfo &pSym) const {
  // only the reference from dynamic executable to non-function symbol in
  // the dynamic objects may need copy relocation
  if (config().isCodeIndep() || !pSym.isDyn() ||
      pSym.type() == ResolveInfo::Function || pSym.size() == 0)
    return false;

  // TODO: Is this check necessary?
  // if relocation target place is readonly, a copy relocation is needed
  uint32_t flag = pReloc.targetRef()->getOutputELFSection()->getFlags();
  if (0 == (flag & llvm::ELF::SHF_WRITE))
    return true;

  return false;
}

uint64_t GNULDBackend::getImageBase(bool HasInterp, bool LoadEHdr) const {
  if (auto ImageBase = config().options().imageBase())
    return *ImageBase;
  return m_pInfo->startAddr(
      m_Module.getScript().linkerScriptHasSectionsCommand(), HasInterp,
      LoadEHdr);
}

llvm::StringRef GNULDBackend::getEntry() const {
  if (config().options().hasEntry())
    return config().options().entry();
  return getInfo().entry();
}

const LDSymbol *GNULDBackend::getEntrySymbol() const {
  llvm::StringRef entryName = getEntry();
  if (!string::isValidCIdentifier(entryName))
    return nullptr;
  const LDSymbol *entrySymbol =
      getModule().getNamePool().findSymbol(entryName.str());
  if (entrySymbol && entrySymbol->resolveInfo() &&
      !entrySymbol->resolveInfo()->isDefine())
    return nullptr;
  return entrySymbol;
}

void GNULDBackend::checkAndSetHasTextRel(const ELFSection &pSection) {
  if (m_bHasTextRel)
    return;

  // if the target section of the dynamic relocation is ALLOCATE but is not
  // writable, than we should set DF_TEXTREL
  const uint32_t flag = pSection.getFlags();
  if (0 == (flag & llvm::ELF::SHF_WRITE) && (flag & llvm::ELF::SHF_ALLOC))
    m_bHasTextRel = true;

  return;
}

/// sortRelocation - sort the dynamic relocations to let dynamic linker
/// process relocations more efficiently
void GNULDBackend::sortRelocation(ELFSection &pSection) {
  if (!config().options().hasCombReloc())
    return;

  if (pSection.getKind() != LDFileFormat::DynamicRelocation)
    return;

  if ((pSection.name() != ".rel.dyn") && (pSection.name() != ".rela.dyn"))
    return;

  std::sort(pSection.getRelocations().begin(), pSection.getRelocations().end(),
            [this](Relocation *X, Relocation *Y) {
              // 1. compare if relocation is relative
              if (!hasSymInfo(X)) {
                if (hasSymInfo(Y))
                  return true;
              } else if (!hasSymInfo(Y)) {
                return false;
              } else {
                // 2. compare the symbol index
                size_t symIdxX = getDynSymbolIdx(X->symInfo()->outSymbol());
                size_t symIdxY = getDynSymbolIdx(Y->symInfo()->outSymbol());
                if (symIdxX < symIdxY)
                  return true;
                if (symIdxX > symIdxY)
                  return false;
              }

              // 3. compare the relocation address
              if (X->place(m_Module) < Y->place(m_Module))
                return true;
              if (X->place(m_Module) > Y->place(m_Module))
                return false;

              // 4. compare the relocation type
              if (X->type() < Y->type())
                return true;
              if (X->type() > Y->type())
                return false;

              // 5. compare the addend
              if (X->addend() < Y->addend())
                return true;
              if (X->addend() > Y->addend())
                return false;

              return false;
            });
}

namespace {

bool shouldIgnoreDiscardedRelocationsFromSection(const ELFSection &Section) {
  return (!Section.isAlloc() && Section.name().starts_with(".debug")) ||
         Section.isEXIDX() ||
         (Section.isProgBits() && Section.name() == ".eh_frame");
}

} // namespace

bool GNULDBackend::maySkipRelocProcessing(Relocation *pReloc) const {
  auto canSkip = [&](Relocation *relocation) -> bool {
    const ResolveInfo *info = relocation->symInfo();
    // bypass the reloc if the section where it sits will be discarded.
    const auto *OwningSection =
        relocation->targetRef()->frag()->getOwningSection();
    if (OwningSection->isIgnore())
      return true;

    // bypass the reloc if the section where it sits will be discarded.
    if (OwningSection->isDiscard())
      return true;

    // Symbols ignored should not be looked into.
    const auto *OutSym = info->outSymbol();
    if (OutSym->shouldIgnore())
      return true;

    if (const FragmentRef *OutFrag = OutSym->fragRef())
      if (OutFrag->isDiscard() ||
          (OutSym->hasFragRef() &&
           (OutFrag->frag()->getOwningSection()->isIgnore() ||
            OutFrag->frag()->getOwningSection()->isDiscard()))) {
        if (!shouldIgnoreDiscardedRelocationsFromSection(*OwningSection)) {
          // ld also reports the name and file of the discarded section. We
          // don't have this information because such sections are not loaded,
          // there are no fragments, and resolve info points to a discard
          // fragment instead.
          config().raise(Diag::error_referenced_discarded_section)
              << info->getDecoratedName(config().options().shouldDemangle())
              << OwningSection->name()
              << OwningSection->getInputFile()->getInput()->decoratedPath();
        }
        return true;
      }
    return false;
  };
  return canSkip(pReloc);
}

bool GNULDBackend::relax() {
  bool finished = false;
  int iteration = 0;

  // Create file header and program header
  createFileHeader();
  createProgramHeader();

  while (!finished) {
    auto start = std::chrono::steady_clock::now();
    {
      eld::RegisterTimer T("Assign Address", "Establish Layout",
                           m_Module.getConfig().options().printTimingStats());
      bool hasError = createProgramHdrs();
      if (hasError)
        m_Module.setFailure(true);
      if (updateTargetSections()) {
        bool hasError = createProgramHdrs();
        if (hasError)
          m_Module.setFailure(true);
      }
    }

    if (!config().getDiagEngine()->diagnose()) {
      if (m_Module.getPrinter()->isVerbose())
        config().raise(Diag::function_has_error) << __PRETTY_FUNCTION__;
      return false;
    }
    {
      eld::RegisterTimer T("Create Trampolines", "Establish Layout",
                           m_Module.getConfig().options().printTimingStats());
      mayBeRelax(iteration, finished);
    }

    if (!config().getDiagEngine()->diagnose()) {
      if (m_Module.getPrinter()->isVerbose())
        config().raise(Diag::function_has_error) << __PRETTY_FUNCTION__;
      return false;
    }
    auto end = std::chrono::steady_clock::now();
    if (m_Module.getPrinter()->allStats())
      config().raise(Diag::layout_stats)
          << iteration
          << (int)std::chrono::duration<double, std::milli>(end - start)
                 .count();
    iteration++;
  }

  // Print memory regions
  printMemoryRegionsUsage();

  // Verify memory regions
  verifyMemoryRegions();

  return config().getDiagEngine()->diagnose();
}

MemoryRegion GNULDBackend::getFileOutputRegion(llvm::FileOutputBuffer &pBuffer,
                                               size_t pOffset, size_t pLength) {
  eld::RegisterTimer T("Create File Output Region", "Post Processing",
                       m_Module.getConfig().options().printTimingStats());
  if (pOffset > pBuffer.getBufferSize() ||
      (pOffset + pLength > pBuffer.getBufferSize()))
    return MemoryRegion();
  return MemoryRegion(pBuffer.getBufferStart() + pOffset, pLength);
}

void GNULDBackend::evaluateScriptAssignments(bool afterLayout) {
  auto &LS = m_Module.getScript();
  for (auto &assign : m_Module.getScript().assignments()) {
    if (LS.linkerScriptHasSectionsCommand()) {
      Assignment::Level AllowedLevel =
          (afterLayout ? Assignment::Level::AFTER_SECTIONS
                       : Assignment::Level::BEFORE_SECTIONS);
      if (assign->level() != AllowedLevel)
        continue;
    }

    if (shouldskipAssert(assign)) {
      if (m_Module.getPrinter()->isVerbose()) {
        std::string expressionStr;
        llvm::raw_string_ostream SS(expressionStr);
        assign->dumpMap(SS, false, false);
        config().raise(Diag::executing_assert_after_layout_is_complete)
            << SS.str();
      }
      continue;
    }

    if (assign->isProvideOrProvideHidden() && !isProvideSymBeingUsed(assign))
      continue;
    assign->assign(m_Module, nullptr);
  }
}

void GNULDBackend::evaluateAsserts() {
  for (auto &a : m_Module.getScript().assignments()) {
    if (a->type() != Assignment::ASSERT)
      continue;
    if (a->hasDot()) {
      if (m_Module.getPrinter()->isVerbose()) {
        std::string expressionStr;
        llvm::raw_string_ostream SS(expressionStr);
        a->dumpMap(SS, false, false);
        config().raise(Diag::skipping_executed_asserts) << SS.str();
      }
      continue;
    }
    if (m_Module.getPrinter()->isVerbose()) {
      std::string expressionStr;
      llvm::raw_string_ostream SS(expressionStr);
      a->dumpMap(SS, false, false);
      config().raise(Diag::executing_assert_after_layout) << SS.str();
    }
    a->assign(m_Module, nullptr);
  }
}

void GNULDBackend::mayWarnSection(ELFSection *sect) const {
  llvm::StringRef sectionName = sect->name();
  // If the section has some sort of flag, no need to worry.
  if (sect->getFlags() != 0)
    return;
  bool raiseWarn = llvm::StringSwitch<bool>(sectionName)
                       .StartsWith(".text", true)
                       .StartsWith(".rodata", true)
                       .StartsWith(".data", true)
                       .StartsWith(".bss", true)
                       .Default(false);
  if (!raiseWarn)
    return;
  config().raise(Diag::section_does_not_have_proper_permissions)
      << sectionName << sect->getInputFile()->getInput()->decoratedPath();
}

/// defineSymbolforCopyReloc
/// For a symbol needing copy relocation, define a copy symbol in the BSS
/// section and all other reference to this symbol should refer to this
/// copy.
/// @note This is executed at `scan relocation' stage.
LDSymbol &GNULDBackend::defineSymbolforCopyReloc(eld::IRBuilder &pBuilder,
                                                 ResolveInfo *pSym,
                                                 ResolveInfo *origSym) {

  LayoutInfo *layoutInfo = m_Module.getLayoutInfo();
  ResolveInfo *curSym = pSym;
  ResolveInfo *aliasSym = pSym->alias();

  eld::RegisterTimer T("Define Symbol for COPY Reloc", "Perform Layout",
                       m_Module.getConfig().options().printTimingStats());
  // If the alias symbol and the aliasee are from different input files,
  // they are not really aliases.
  if (aliasSym && (aliasSym->resolvedOrigin() != curSym->resolvedOrigin())) {
    aliasSym = nullptr;
    pSym->setAlias(nullptr);
  }

  // If the alias has a fragment already, just use the fragment from the alias
  // symbol.
  if (aliasSym && aliasSym->outSymbol() &&
      aliasSym->outSymbol()->hasFragRef()) {
    LDSymbol *cpy_sym =
        pBuilder.addSymbol<IRBuilder::Force, IRBuilder::Resolve>(
            pSym->resolvedOrigin(), pSym->name(),
            (ResolveInfo::Type)pSym->type(), ResolveInfo::Define,
            ResolveInfo::Global,
            pSym->size(), // size
            0x0,          // value
            aliasSym->outSymbol()->fragRef(),
            (ResolveInfo::Visibility)pSym->other());
    cpy_sym->setShouldIgnore(false);
    return *cpy_sym;
  }

  // If the alias symbol doesn't have a fragment, first create a fragment for
  // the alias.
  if (aliasSym && aliasSym->outSymbol() && !aliasSym->outSymbol()->hasFragRef())
    pSym = aliasSym;

  // Create a unique section for each copy relocation symbol.
  ELFSection *copyRelocSect = m_Module.createInternalSection(
      Module::InternalInputType::CopyRelocSymbols, LDFileFormat::Kind::Internal,
      ".dynbss." + origSym->getName().str(), llvm::ELF::SHT_NOBITS,
      llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE, 1);

  uint32_t SecAlign =
      llvm::dyn_cast<eld::ELFFileBase>(origSym->resolvedOrigin())
          ->getELFSection(origSym->outSymbol()->sectionIndex())
          ->getAddrAlign();
  int TrailingZeros =
      std::min(llvm::countr_zero(SecAlign),
               llvm::countr_zero(origSym->outSymbol()->value()));
  uint32_t addralign = 1U << TrailingZeros;

  // allocate space in BSS for the copy symbol
  Fragment *frag = make<FillFragment>(getModule(), 0x0, pSym->size(),
                                      copyRelocSect, addralign);
  copyRelocSect->addFragmentAndUpdateSize(frag);
  if (layoutInfo)
    layoutInfo->recordFragment(copyRelocSect->getInputFile(), copyRelocSect,
                                  frag);

  // change symbol binding to Global if it's a weak symbol
  ResolveInfo::Binding binding = (ResolveInfo::Binding)pSym->binding();
  if (binding == ResolveInfo::Weak)
    binding = ResolveInfo::Global;

  // Define the copy symbol in the bss section and resolve it
  LDSymbol *cpy_sym = pBuilder.addSymbol<IRBuilder::Force, IRBuilder::Resolve>(
      pSym->resolvedOrigin(), pSym->name(), (ResolveInfo::Type)pSym->type(),
      ResolveInfo::Define, binding,
      pSym->size(), // size
      0x0,          // value
      make<FragmentRef>(*frag, 0x0), (ResolveInfo::Visibility)pSym->other());

  cpy_sym->setShouldIgnore(false);

  if ((curSym != pSym) && aliasSym && aliasSym->outSymbol() &&
      aliasSym->outSymbol()->hasFragRef()) {
    LDSymbol *new_cur_sym =
        pBuilder.addSymbol<IRBuilder::Force, IRBuilder::Resolve>(
            curSym->resolvedOrigin(), curSym->name(),
            (ResolveInfo::Type)curSym->type(), ResolveInfo::Define,
            ResolveInfo::Global,
            curSym->size(), // size
            0x0,            // value
            aliasSym->outSymbol()->fragRef(),
            (ResolveInfo::Visibility)curSym->other());
    new_cur_sym->setShouldIgnore(false);
    return *new_cur_sym;
  }
  return *cpy_sym;
}

bool GNULDBackend::checkBssMixing(const ELFSegment &Seg) const {
  const ELFSection *bssSect = nullptr;
  for (const OutputSectionEntry *O : Seg) {
    const ELFSection *cur = O->getSection();
    bool isCurSectionNoLoad = O->prolog().type() == OutputSectDesc::NOLOAD;
    bool isCurBSS = cur->isNoBits() || isCurSectionNoLoad;
    bool isTLS = cur->isTLS();
    // Skip .tbss for PT_LOAD and GNU_RELRO as it is not really a section.
    if (isTLS && isCurBSS &&
        (Seg.type() == llvm::ELF::PT_LOAD ||
         Seg.type() == llvm::ELF::PT_GNU_RELRO))
      continue;
    // If the section is not bss and the size is more than 0
    if (!cur->size())
      continue;
    if (bssSect) {
      if (!isCurSectionNoLoad && !Seg.isNoneSegment() &&
          handleBSS(bssSect, cur)) {
        config().raise(Diag::err_mix_bss_section)
            << cur->name() << bssSect->name();
        return false;
      }
    } else if (isCurBSS) {
      if (isCurSectionNoLoad)
        break;
      bssSect = cur;
    }
  }
  return true;
}

// Assign offsets.
bool GNULDBackend::assignOffsets(uint64_t offset) {
  std::vector<OutputSectionEntry *> NonAllocSections;
  std::vector<ELFSegment *> segments = elfSegmentTable().segments();

  ELFSection *prev = nullptr;

  LinkerScript &script = m_Module.getScript();

  if (!allocateHeaders()) {
    config().raise(Diag::not_enough_space_for_phdrs);
    return false;
  }

  SectionMap::iterator out = script.sectionMap().begin(),
                       outEnd = script.sectionMap().end();

  SectionMap::OutputSectionEntryDescList OList =
      m_Module.getScript()
          .sectionMap()
          .getOutputSectionEntrySectionsForPluginType(
              plugin::Plugin::Type::ControlFileSize);

  // Iterate through all sections to get to the debug sections
  // for assigning offsets to them.
  {
    eld::RegisterTimer T("Get Debug Sections", "Establish Layout",
                         m_Module.getConfig().options().printTimingStats());
    while (out != outEnd) {
      ELFSection *cur = (*out)->getSection();
      if (!cur->isAlloc() && !cur->isNullKind())
        NonAllocSections.push_back(*out);
      ++out;
    }
  }

  {
    eld::RegisterTimer T("Run File Size Plugin", "Establish Layout",
                         m_Module.getConfig().options().printTimingStats());

    if (!RunPluginsAndProcessHelper(OList)) {
      m_Module.setFailure(true);
      return false;
    }
  }

  int64_t BeginOffset = offset;

  // All segments have not been assigned offsets.
  m_OffsetsAssigned = false;
  // Add NONE segment.
  if (m_pNONESegment)
    segments.push_back(m_pNONESegment);

  // int segment_counter = 0;
  for (auto &Seg : segments) {
    /* llvm::outs() << "Segment ##" << segment_counter << " :";
    for (auto &Sec : *Seg) {
      llvm::outs() << "   " << Sec->name();
    }
    llvm::outs() << "\n";
    segment_counter++;
    */
    std::pair<int64_t, ELFSection *> SF =
        setupSegmentOffset(Seg, prev, BeginOffset, true);
    BeginOffset = SF.first;
    prev = SF.second;
    // raise an error if there is a BSS section and the following
    // section is non BSS.
    if (!config().options().allowBssMixing() && !checkBssMixing(*Seg))
      return false;
  }
  // All segments have offsets assigned.
  m_OffsetsAssigned = true;

  for (auto &nonAllocSect : NonAllocSections) {
    ELFSection *cur = nonAllocSect->getSection();
    if (prev) {
      if (prev->isNoBits())
        offset = prev->offset();
      else
        offset = prev->offset() + prev->size();
    }
    alignAddress((uint64_t &)offset, cur->getAddrAlign());
    cur->setOffset(offset);
    if (cur->size())
      cur->setWanted(true);
    if (!config().getDiagEngine()->diagnose()) {
      if (m_Module.getPrinter()->isVerbose())
        config().raise(Diag::function_has_error) << __PRETTY_FUNCTION__;
      return false;
    }
    prev = cur;
  }

  // Set the appropriate section information for NOLOAD sections.
  // NOLOAD sections are turned into NOBITS sections as per GNU linker.
  // May be there is some reason behind it. TODO : print a diagnostic note.
  for (auto &sec : noLoadSections) {
    auto &Segments = getSegmentsForSection(sec->getOutputSection());
    // Only set the offset to NOLOAD sections mapped to NOLOAD segments to 0.
    bool isLoadSegment = true;
    for (auto &S : Segments) {
      if (!S->isLoadSegment())
        isLoadSegment = false;
    }
    if (!isLoadSegment)
      sec->setOffset(0);
  }
  assignOffsetsToSkippedSections();

  return true;
}

bool GNULDBackend::CleanupPlugins(SectionMap::OutputSectionEntryDescList &O) {
  eld::RegisterTimer T(
      "Cleanup", "Plugins",
      m_Module.getConfig().options().printTimingStats("Plugin"));
  for (auto &P : O) {
    if (!P->prolog().getPlugin()->destroy())
      return false;
  }
  return true;
}

bool GNULDBackend::RunPluginsAndProcessHelper(
    SectionMap::OutputSectionEntryDescList &OList, bool MatchSections) {
  {
    eld::RegisterTimer T(
        "VA/File Size Plugin and Process Helper", "Establish Layout",
        m_Module.getConfig().options().printTimingStats("Plugin"));

    if (!OList.size())
      return true;

    std::vector<ELFSection *> PluginSections;
    LayoutInfo *layoutInfo = m_Module.getLayoutInfo();
    ObjectBuilder builder(config(), m_Module);
    ELFObjectWriter *Writer =
        m_Module.getLinker()->getObjectLinker()->getWriter();

    // Build the memory blocks.
    for (auto &O : OList) {
      plugin::PluginBase *N = O->prolog().getPlugin()->getLinkerPlugin();
      plugin::ControlFileSizePlugin *FOP =
          llvm::dyn_cast<plugin::ControlFileSizePlugin>(N);
      plugin::ControlMemorySizePlugin *VAP =
          llvm::dyn_cast<plugin::ControlMemorySizePlugin>(N);

      ELFSection *S = O->getSection();

      if (layoutInfo)
        layoutInfo->recordPlugin(S, O->prolog().getPlugin());

      // Reserve memory region.
      std::error_code ec;
      llvm::sys::MemoryBlock MB = llvm::sys::Memory::allocateMappedMemory(
          S->size(), nullptr,
          llvm::sys::Memory::MF_READ | llvm::sys::Memory::MF_WRITE, ec);
      if (ec != std::error_code()) {
        config().raise(Diag::unable_to_reserve_memory) << S->size();
        return false;
      }

      if (m_Module.getPrinter()->tracePlugins())
        config().raise(Diag::allocating_memory) << S->size() << S->name();

      MemoryRegion mr(reinterpret_cast<uint8_t *>(MB.base()),
                      MB.allocatedSize());
      for (auto &Cur : *O) {
        eld::Expected<void> expWrite =
            Writer->writeRegion(Cur->getSection(), mr);
        if (!expWrite) {
          config().raiseDiagEntry(std::move(expWrite.error()));
          return false;
        }
      }
      if (m_Module.getPrinter()->tracePlugins())
        config().raise(Diag::applying_relocations) << S->name();
      m_Module.getLinker()->getObjLinker()->relocation(false);
      if (m_Module.getPrinter()->tracePlugins())
        config().raise(Diag::syncing_relocations) << S->name();
      m_Module.getLinker()->getObjLinker()->syncRelocations(
          reinterpret_cast<uint8_t *>(MB.base()));

      // Clear all the fragments.
      if (!MatchSections) {
        for (auto &Cur : *O) {
          Cur->getSection()->getFragmentList().clear();
          Cur->getSection()->getRelocations().clear();
        }
      }

      // Initialize the plugin.
      if (!O->prolog().getPlugin()->init(m_Module.getOutputTarWriter())) {
        m_Module.setFailure(true);
        return false;
      }

      plugin::Block B;
      B.Data = reinterpret_cast<uint8_t *>(MB.base());
      B.Size = S->size();
      B.Address = S->addr();
      B.Name = std::string(S->name());

      // Add the Memory Block.
      if (m_Module.getPrinter()->tracePlugins())
        config().raise(Diag::adding_memory_blocks) << S->name();

      // Add Memory Blocks.
      if (MatchSections)
        VAP->AddBlocks(std::move(B));
      else
        FOP->AddBlocks(std::move(B));

      if (m_Module.getPrinter()->tracePlugins())
        config().raise(Diag::calling_handler) << S->name();

      // Run the algorithm.
      if (!O->prolog().getPlugin()->run(
              m_Module.getScript().getPluginRunList())) {
        m_Module.setFailure(true);
        return false;
      }

      auto RB = MatchSections ? VAP->GetBlocks() : FOP->GetBlocks();

      if (m_Module.getPrinter()->tracePlugins())
        config().raise(Diag::plugin_returned_blocks) << RB.size() << S->name();

      ELFSection *OutputSection = S;

      uint64_t Offset = 0;
      for (auto &M : RB) {
        ELFSection *N = m_Module.getScript().sectionMap().createELFSection(
            M.Name, LDFileFormat::Regular, llvm::ELF::SHT_PROGBITS,
            llvm::ELF::SHF_ALLOC, /*EntSize=*/0);
        PluginSections.push_back(N);
        Fragment *frag = make<RegionFragment>(
            llvm::StringRef(reinterpret_cast<const char *>(M.Data), M.Size),
            OutputSection, Fragment::Type::Region);
        N->addFragmentAndUpdateSize(frag);
        frag->setOwningSection(N);
        frag->setAlignment(M.Alignment);
        frag->setOffset(Offset);
        Offset = frag->getOffset(config().getDiagEngine()) + frag->size();
      }
      if (!MatchSections && (Offset > S->size()))
        config().raise(Diag::plugin_returned_blocks_more_than_input_size)
            << N->GetName() << Offset << S->size();
      OutputSection->setSize(Offset);

      if (!MatchSections) {
        ELFSection *DefaultRuleSection = O->getLastRule()->getSection();
        for (auto &P : PluginSections)
          builder.moveSection(P, DefaultRuleSection);
      }
    }

    CleanupPlugins(OList);

    if (!MatchSections) {
      return true;
    }

    std::set<OutputSectionEntry *> OutputSectionSet;

    InputFile *input =
        m_Module.getInternalInput(Module::InternalInputType::Plugin);

    bool hasError = false;

    for (auto &sec : PluginSections) {
      // Hash of all the required things for Match.
      uint64_t inputFileHash = input->getInput()->getResolvedPathHash();
      uint64_t nameHash = input->getInput()->getArchiveMemberNameHash();
      uint64_t inputSectionHash = sec->sectionNameHash();
      SectionMap::mapping pair = m_Module.getScript().sectionMap().findIn(
          m_Module.getScript().sectionMap().begin(),
          input->getInput()->getResolvedPath().native(), *sec, false,
          input->getInput()->getName(), inputSectionHash, inputFileHash,
          nameHash,
          (config().options().getScriptOption() == GeneralOptions::MatchGNU));
      if (!pair.first) {
        config().raise(Diag::cannot_find_output_section_for_input)
            << sec->name();
        hasError = true;
        continue;
      }
      sec->setOutputSection(pair.first);
      if (IsSectionTracingRequested && config().options().traceSection(sec))
        config().raise(Diag::section_mapping_info)
            << sec->getDecoratedName(config().options())
            << pair.first->getSection()->getDecoratedName(config().options());
      sec->setMatchedLinkerScriptRule(pair.second);
      builder.moveSection(sec, pair.second->getSection());
      OutputSectionSet.insert(pair.first);
    }

    // If there have been no sections that matched
    if (hasError)
      return false;

    for (auto &O : OutputSectionSet) {
      O->getSection()->setType(llvm::ELF::SHT_PROGBITS);
      m_Module.getLinker()->getObjLinker()->createOutputSection(builder, O,
                                                                true);
    }
  }

  return true;
}

bool GNULDBackend::RunVAPluginsAndProcess(
    SectionMap::OutputSectionEntryDescList &OList) {
  return (RunPluginsAndProcessHelper(OList, true));
}

void GNULDBackend::pluginLinkSections(OutputSectionEntry *A,
                                      OutputSectionEntry *B) {
  A->getSection()->setLink(B->getSection());
  PluginLinkedSections.insert(A);
  if (m_Module.getPrinter()->isVerbose())
    config().raise(Diag::chaining_sections) << A->name() << B->name();
}

void GNULDBackend::resolveTargetDefinedSymbols() {
  // Resolve target specific standard symbols.
  ResolveInfo *Info = getStandardSymbol("__ehdr_start");
  if (Info)
    Info->setValue(m_ehdr->addr(), true);
  ResolveInfo *InfoDSOHandle = getStandardSymbol("__dso_handle");
  if (InfoDSOHandle)
    InfoDSOHandle->setValue(m_ehdr->addr(), true);
}

void GNULDBackend::doPostLayout() {
  resolveTargetDefinedSymbols();

  m_Module.setLinkState(LinkState::CreatingSegments);
  if (!m_Module.getLinker()
           ->getObjectLinker()
           ->runOutputSectionIteratorPlugin()) {
    m_Module.setFailure(true);
    return;
  }

  // Update all relative relocations to point to the right fragment.
  {
    eld::RegisterTimer T("Update Relative Relocations", "Do Post Layout",
                         m_Module.getConfig().options().printTimingStats());
    for (auto &r : m_RelativeRelocMap) {
      Relocation *R = r.first;
      const Relocation *N = r.second;
      R->modifyRelocationFragmentRef(N->targetFragRef());
      R->setAddend(N->addend());
    }
  }

  {
    eld::RegisterTimer T("Run VA Plugin", "Establish Layout",
                         m_Module.getConfig().options().printTimingStats());
    SectionMap::OutputSectionEntryDescList OList =
        m_Module.getScript()
            .sectionMap()
            .getOutputSectionEntrySectionsForPluginType(
                plugin::Plugin::Type::ControlMemorySize);
    if (!OList.size())
      return;
    if (!config().getDiagEngine()->diagnose()) {
      if (m_Module.getPrinter()->isVerbose())
        config().raise(Diag::function_has_error) << __PRETTY_FUNCTION__;
      return;
    }
    if (!RunVAPluginsAndProcess(OList)) {
      m_Module.setFailure(true);
      return;
    }
  }

  {
    eld::RegisterTimer T("Create Script Program Headers", "Establish Layout",
                         m_Module.getConfig().options().printTimingStats());
    createScriptProgramHdrs();
  }
  if (!config().getDiagEngine()->diagnose()) {
    if (m_Module.getPrinter()->isVerbose())
      config().raise(Diag::function_has_error) << __PRETTY_FUNCTION__;
    return;
  }
}

LDSymbol *GNULDBackend::canProvideSymbol(ResolveInfo *rsym) {
  if (rsym->isBitCode())
    return nullptr;
  return canProvideSymbol(rsym->name());
}

LDSymbol *GNULDBackend::canProvideSymbol(llvm::StringRef symName) {
  uint64_t symVal = 0x0;
  ResolveInfo::Type resolverType = ResolveInfo::NoType;
  ResolveInfo::Visibility V = ResolveInfo::Default;
  InputFile *file = nullptr;
  auto P = ProvideMap.find(symName.str());
  auto PSymDef = m_SymDefProvideMap.find(symName);
  bool isPSymDef = PSymDef != m_SymDefProvideMap.end();
  bool Patchable = false;
  if (P != ProvideMap.end()) {
    if (P->second->isProvideHidden())
      V = ResolveInfo::Hidden;
    resolverType = ResolveInfo::NoType;
    file = P->second->getInputFileInContext();
    // FIXME: We ideally should not need this. It is added so that the link
    // does not fail if the provide command does not have input file context due
    // to any corner case.
    if (!file)
      file = m_Module.getInternalInput(Module::Script);
    P->second->setUsed(true);
  } else if (isPSymDef) {
    resolverType = std::get<0>(PSymDef->second);
    symVal = std::get<1>(PSymDef->second);
    file = std::get<2>(PSymDef->second);
    Patchable = std::get<3>(PSymDef->second);
  } else
    return nullptr;

  LDSymbol *provided_sym =
      m_Module.getIRBuilder()->addSymbol<IRBuilder::Force, IRBuilder::Resolve>(
          file, symName.str(), resolverType, ResolveInfo::Define,
          ResolveInfo::Absolute,
          0x0,                 // size
          symVal,              // value
          FragmentRef::null(), // FragRef
          V, /* isPostLTOPhase */ false, /* isBitCode */ false, Patchable);
  if (provided_sym != nullptr) {
    provided_sym->setShouldIgnore(false);
    provided_sym->setScriptDefined();
    if (isPSymDef &&
        ((config().options().isSymbolTracingRequested() &&
          config().options().traceSymbol(*(provided_sym->resolveInfo()))) ||
         m_Module.getPrinter()->traceSymDef()))
      config().raise(Diag::note_resolving_from_provide_symdef_file)
          << symName << file->getInput()->decoratedPath();
  }
  return provided_sym;
}

void GNULDBackend::makeVersionString() {
  if (!m_pComment)
    return;
  std::string VersionString = std::string(eld::getVendorName()) + " Linker ";
  VersionString += eld::getELDVersion();
  VersionString += " (" + eld::getELDRevision().str() + ")";
  if (m_Module.needLTOToBeInvoked() || config().options().hasLTO())
    VersionString += " LTO Enabled ";
  Fragment *F = make<StringFragment>(VersionString, m_pComment);
  m_pComment->addFragmentAndUpdateSize(F);
  LayoutInfo *layoutInfo = m_Module.getLayoutInfo();
  if (layoutInfo)
    layoutInfo->recordFragment(F->getOwningSection()->getInputFile(),
                      F->getOwningSection(), F);
  // Add LLVM revision information as well if available.
  // This check is required because eld do not necessarily have access
  // to LLVM revision information.
  if (!eld::getLLVMRevision().empty() && !eld::getLLVMVersion().empty()) {
    std::string LLVMRevisionInfo = std::string(eld::getVendorName()) + " LLVM ";
    LLVMRevisionInfo += eld::getLLVMVersion();
    LLVMRevisionInfo += " (" + eld::getLLVMRevision().str() + ") ";
    Fragment *LLVMRevisionF =
        make<StringFragment>(LLVMRevisionInfo, m_pComment);
    m_pComment->addFragmentAndUpdateSize(LLVMRevisionF);
    if (layoutInfo)
      layoutInfo->recordFragment(LLVMRevisionF->getOwningSection()->getInputFile(),
                        LLVMRevisionF->getOwningSection(), LLVMRevisionF);
  }
  if ((LinkerConfig::Object != config().codeGenType()) &&
      (LinkerConfig::DynObj != config().codeGenType()) &&
      !config().options().isPIE())
    return;
  // Add the command line information to the link
  std::string CommandLine = "Command:";
  std::string Separator = "";
  for (auto arg : config().options().args()) {
    if (!arg)
      continue;
    CommandLine.append(Separator);
    Separator = " ";
    CommandLine.append(std::string(arg));
  }
  Fragment *CmdLineFragment = make<StringFragment>(CommandLine, m_pComment);
  m_pComment->addFragmentAndUpdateSize(CmdLineFragment);
  if (layoutInfo)
    layoutInfo->recordFragment(CmdLineFragment->getOwningSection()->getInputFile(),
                      CmdLineFragment->getOwningSection(), CmdLineFragment);
}

bool GNULDBackend::addPhdrsIfNeeded(void) {
  eld::RegisterTimer T("Add Phdrs", "Perform Layout",
                       m_Module.getConfig().options().printTimingStats());
  bool hasFileHdrOrPhdr = false;
  std::optional<PhdrSpec> firstPTLOAD;

  // Check if the PHDR command lists FILEHDR/PHDR to be
  for (auto &Phdr : m_Module.getScript().phdrList()) {
    if (Phdr->spec().hasFileHdr() || Phdr->spec().hasPhdr())
      hasFileHdrOrPhdr = true;
    if (Phdr->spec().type() == llvm::ELF::PT_PHDR)
      continue;
    // If the PHDR or FILEHDR is part of the following PT_LOAD and not the first
    // PT_LOAD, its an error.
    if ((Phdr->spec().type() == llvm::ELF::PT_LOAD) &&
        (Phdr->spec().hasFileHdr() || Phdr->spec().hasPhdr())) {
      if (firstPTLOAD &&
          (!firstPTLOAD->hasFileHdr() || !firstPTLOAD->hasPhdr())) {
        config().raise(Diag::first_pt_load_doesnot_have_phdr);
        return false;
      }
    }
    if (!firstPTLOAD && Phdr->spec().type() == llvm::ELF::PT_LOAD)
      firstPTLOAD = Phdr->spec();
  }

  if (!hasFileHdrOrPhdr)
    return true;

  //
  // Check if file header or program header is requested
  //
  bool hasFileHdr = false;
  bool hasPhdr = false;

  for (auto &Phdr : m_Module.getScript().phdrList()) {
    if (Phdr->spec().hasFileHdr())
      hasFileHdr = true;
    if (Phdr->spec().hasPhdr())
      hasPhdr = true;
  }

  // If FILEHDR is requested, lets add the file hader.
  if (hasFileHdr)
    setNeedEhdr();

  // Add the program header even if only file header is requested. If we dont do
  // this, sections would start right from end of the file header, which is
  // totally wrong.
  if (hasFileHdr || hasPhdr)
    setNeedPhdr();

  // SuccessFul.
  return true;
}

uint32_t GNULDBackend::getOneEhdrSize() const {
  if (config().targets().is32Bits())
    return sizeof(llvm::ELF::Elf32_Ehdr);
  else
    return sizeof(llvm::ELF::Elf64_Ehdr);
}

uint32_t GNULDBackend::getOnePhdrSize() const {
  if (config().targets().is32Bits())
    return sizeof(llvm::ELF::Elf32_Phdr);
  else
    return sizeof(llvm::ELF::Elf64_Phdr);
}

void GNULDBackend::addTargetSpecificSegments() { return; }

bool GNULDBackend::hasFatalError() const {
  if (!config().getDiagEngine()->diagnose()) {
    if (m_Module.getPrinter()->isVerbose())
      config().raise(Diag::function_has_error) << __PRETTY_FUNCTION__;
    return true;
  }
  return false;
}

const std::vector<ELFSegment *>
GNULDBackend::getSegmentsForSection(const OutputSectionEntry *O) const {
  std::vector<ELFSegment *> Segments;
  if (!m_Module.getScript().phdrsSpecified())
    return Segments;
  const auto &SectionsIter = _segmentsForSection.find(O);
  if (SectionsIter != _segmentsForSection.end())
    for (auto &S : SectionsIter->second)
      Segments.push_back(S);
  return Segments;
}

size_t GNULDBackend::getGOTSymbolAddr() const {
  ResolveInfo *info = m_pGOTSymbol->resolveInfo();
  if (!info->outSymbol()->hasFragRef())
    return info->outSymbol()->value();
  FragmentRef *fragRef = info->outSymbol()->fragRef();
  ELFSection *section = fragRef->getOutputELFSection();
  return (fragRef->getOutputOffset(m_Module) + section->addr());
}

std::string
GNULDBackend::getCommonSymbolName(const CommonELFSection *commonSection) const {
  llvm::StringRef sectionName = commonSection->name();
  if (sectionName.starts_with("COMMON."))
    return sectionName.substr(llvm::StringRef("COMMON.").size()).str();
  std::vector<std::string> smallSizes = {"1", "2", "4", "8"};
  for (const std::string &size : smallSizes) {
    std::string commonPrefix = std::string(".scommon.") + size + ".";
    llvm::StringRef commonPrefixRef(commonPrefix);
    if (sectionName.starts_with(commonPrefixRef))
      return sectionName.substr(commonPrefixRef.size()).str();
  }
  llvm_unreachable("Invalid CommonELFSection name!");
}

LDSymbol *
GNULDBackend::getCommonSymbol(const CommonELFSection *commonSection) const {
  std::string commonSymbolName = getCommonSymbolName(commonSection);
  LDSymbol *commonSymbol = m_Module.getNamePool().findSymbol(commonSymbolName);
  ASSERT(commonSymbol,
         "Missing common symbol!!. Each CommonELFSection should have a "
         "corresponding common symbol.");
  return commonSymbol;
}

uint64_t GNULDBackend::getImageStartVMA() const {
  assert(m_ImageStartVMA.has_value());
  return m_ImageStartVMA.value();
}

bool GNULDBackend::checkForLinkerScriptPhdrErrors() const {
  LinkerScript &ldscript = m_Module.getScript();
  SectionMap::iterator out, outBegin = ldscript.sectionMap().begin(),
                            outEnd = ldscript.sectionMap().end();
  bool hasError = false;
  for (out = outBegin; out != outEnd; ++out) {
    OutputSectionEntry *cur = (*out);
    bool isCurAllocSection =
        (cur->getSection()->getFlags() & llvm::ELF::SHF_ALLOC);
    if (!isCurAllocSection)
      continue;
    if (cur->getSection() == m_phdr || cur->getSection() == m_ehdr)
      continue;
    // TBSS sections may not be occupied with a PT_LOAD segment.
    if (cur->getSection()->isTBSS())
      continue;
    // The Note GNU stack is allocatable, if present. We should skip this
    // section from being queried for if the linker script did the right thing.
    if (cur->getSection()->isNoteGNUStack())
      continue;
    if ((*out)->prolog().type() == OutputSectDesc::NOLOAD)
      continue;
    if ((*out)->prolog().hasPlugin())
      continue;
    bool found = false;
    std::vector<ELFSegment *> segments;
    if (_segmentsForSection.find(cur) != _segmentsForSection.end())
      segments = _segmentsForSection.at(cur);
    for (auto &seg : segments) {
      if (seg->type() == llvm::ELF::PT_LOAD) {
        found = true;
        break;
      }
      if (seg->isNoneSegment() && cur->getSection()->size()) {
        config().raise(Diag::warn_loadable_section_in_none_segment)
            << cur->name();
        found = true;
      }
    }
    if (!found && cur->getSection() && cur->getSection()->size()) {
      hasError = true;
      config().raise(Diag::loadable_section_not_in_load_segment) << cur->name();
    }
  }

  hasError = hasError || !config().getDiagEngine()->diagnose();
  return hasError;
}

void GNULDBackend::clearMemoryRegions() {
  for (auto &M : m_Module.getLinkerScript().getMemoryRegions())
    M->clearMemoryRegion();
}

void GNULDBackend::verifyMemoryRegions() {
  for (auto &M : m_Module.getLinkerScript().getMemoryRegions())
    M->verifyMemoryUsage(config());
}

bool GNULDBackend::assignMemoryRegions() {
  LinkerScript &script = m_Module.getScript();
  bool linkerScriptHasMemoryCommand = m_Module.getScript().hasMemoryCommand();

  if (!linkerScriptHasMemoryCommand)
    return true;

  for (auto &out : script.sectionMap()) {
    for (auto &region : script.getMemoryRegions()) {
      if (region->checkCompatibilityAndAssignMemorySpecToOutputSection(out))
        break;
    }
  }
  bool status = true;
  for (auto &out : script.sectionMap()) {
    if (!out->getSection()->isAlloc())
      continue;
    if (out->isDiscard())
      continue;
    if (!out->epilog().hasRegion()) {
      status = false;
      config().raise(eld::Diag::error_no_memory_region) << out->name();
    }
  }
  return status;
}

bool GNULDBackend::isProvideSymBeingUsed(const Assignment *provideCmd) const {
  return provideCmd->isUsed();
}

eld::Expected<void> GNULDBackend::printMemoryRegionsUsage() {
  if (!config().options().shouldPrintMemoryUsage())
    return eld::Expected<void>();
  if (!m_Module.getLinkerScript().getMemoryRegions().size())
    return eld::Expected<void>();
  ScriptMemoryRegion::printHeaderForMemoryUsage(llvm::outs());
  for (auto &M : m_Module.getLinkerScript().getMemoryRegions()) {
    eld::Expected<void> E = M->printMemoryUsage(llvm::outs());
    ELDEXP_RETURN_DIAGENTRY_IF_ERROR(E);
  }
  return eld::Expected<void>();
}

void GNULDBackend::setDefaultConfigs() {
  if (!config().options().threadsEnabled() &&
      !config().isGlobalThreadingEnabled()) {
    config().disableThreadOptions(LinkerConfig::EnableThreadsOpt::AllThreads);
    return;
  }

  if (config().options().threadsEnabled() &&
      !config().isGlobalThreadingEnabled()) {
    if (config().options().emitRelocs())
      config().disableThreadOptions(LinkerConfig::ApplyRelocations);
  }
}

eld::Expected<void>
GNULDBackend::finalizeAndEmitBuildID(llvm::FileOutputBuffer &buffer) {
  if (!m_pBuildIDFragment)
    return eld::Expected<void>();
  auto E = m_pBuildIDFragment->finalizeBuildID(buffer.getBufferStart(),
                                               buffer.getBufferSize());
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(E);
  auto region = getFileOutputRegion(
      buffer, m_pBuildIDSection->getOutputELFSection()->offset(),
      m_pBuildIDSection->getOutputELFSection()->size());
  if (!region.size())
    return eld::Expected<void>();
  m_pBuildIDFragment->emit(region, getModule());
  return eld::Expected<void>();
}

void GNULDBackend::resetNewSectionsAddedToLayout() {
  m_NewSectionsAddedToLayout = false;
}

bool GNULDBackend::isNewSectionsAddedToLayout() const {
  return m_NewSectionsAddedToLayout;
}

void GNULDBackend::setNewSectionsAddToLayout() {
  m_NewSectionsAddedToLayout = true;
}

void GNULDBackend::createFileHeader() {
  if (m_ehdr)
    return;
  eld::RegisterTimer T("Add File Header", "Perform Layout",
                       m_Module.getConfig().options().printTimingStats());
  LinkerScript &script = m_Module.getScript();
  m_ehdr = script.sectionMap().createELFSection(
      "__ehdr__", LDFileFormat::Regular, /*Type=*/0, /*Flags=*/0,
      /*EntSize=*/0);
  if (config().options().hasExecuteOnlySegments())
    m_ehdr->setFlags(llvm::ELF::SHF_ALLOC);
  else
    m_ehdr->setFlags(llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_EXECINSTR);
  m_ehdr->setSize(getOneEhdrSize());
  m_ehdr->setHasNoFragments();
  m_ehdr->setAddrAlign(config().targets().is32Bits() ? 4 : 8);
}

void GNULDBackend::addFileHeaderToLayout() {
  if (!isEhdrNeeded())
    return;
  LinkerScript &script = m_Module.getScript();
  SectionMap::iterator out = script.sectionMap().begin();
  SectionMap::iterator Iter = script.sectionMap().insert(out + 1, m_ehdr);
  m_ehdr->setOffset(0);
  m_ehdr->setSize(getOneEhdrSize());
  m_ehdr->setOutputSection(*Iter);
  setNewSectionsAddToLayout();
  setEhdrInLayout();
}

void GNULDBackend::createProgramHeader() {
  if (m_phdr)
    return;
  eld::RegisterTimer T("Add Program Header", "Perform Layout",
                       m_Module.getConfig().options().printTimingStats());
  LinkerScript &script = m_Module.getScript();
  m_phdr = script.sectionMap().createELFSection(
      "__pHdr__", LDFileFormat::Regular, /*Type=*/0, /*Flags=*/0,
      /*EntSize=*/0);
  m_phdr->setAddrAlign(config().targets().is32Bits() ? 4 : 8);
  if (config().options().hasExecuteOnlySegments())
    m_phdr->setFlags(llvm::ELF::SHF_ALLOC);
  else
    m_phdr->setFlags(llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_EXECINSTR);
  m_phdr->setHasNoFragments();
}

void GNULDBackend::addProgramHeaderToLayout() {
  ELFSegment *phdrSeg =
      elfSegmentTable().find(llvm::ELF::PT_PHDR, llvm::ELF::PF_R, 0x0);
  if (!isPhdrNeeded()) {
    if (phdrSeg) {
      OutputSectionEntry *PhdrOutputSection = eld::make<OutputSectionEntry>(
          &m_Module.getScript().sectionMap(), m_phdr);
      phdrSeg->append(PhdrOutputSection);
      phdrSeg->updateFlag(getSegmentFlag(m_phdr->getFlags()));
      phdrSeg->setAlign(m_phdr->getAddrAlign());
      m_phdr->setOffset(getOneEhdrSize());
    }
    return;
  }
  LinkerScript &script = m_Module.getScript();

  SectionMap::iterator out = script.sectionMap().begin();
  SectionMap::iterator Iter;
  if (isEhdrInLayout())
    Iter = script.sectionMap().insert(out + 2, m_phdr);
  else
    Iter = script.sectionMap().insert(out + 1, m_phdr);
  m_phdr->setOutputSection(*Iter);
  m_phdr->setOffset(getOneEhdrSize());
  if (phdrSeg) {
    phdrSeg->append(m_phdr->getOutputSection());
    phdrSeg->updateFlag(getSegmentFlag(m_phdr->getFlags()));
    phdrSeg->setAlign(m_phdr->getAddrAlign());
  }
  setNewSectionsAddToLayout();
  setPhdrInLayout();
}

bool GNULDBackend::isOutputSectionInLoadSegment(OutputSectionEntry *OSE) const {
  if (!getLoadSegmentForOutputSection(OSE))
    return false;
  return true;
}

const ELFSegment *
GNULDBackend::getLoadSegmentForOutputSection(OutputSectionEntry *OSE) const {
  auto S = _segmentsForSection.find(OSE);
  if (S == _segmentsForSection.end())
    return nullptr;
  for (auto &Seg : S->second) {
    if (Seg->isLoadSegment())
      return Seg;
  }
  return nullptr;
}

bool GNULDBackend::allocateHeaders() {
  if (!isEhdrInLayout() && !isPhdrInLayout())
    return true;

  bool isEhdrInLoadSegment =
      (m_ehdr->getOutputSection() &&
       isOutputSectionInLoadSegment(m_ehdr->getOutputSection()));
  bool isPhdrInLoadSegment =
      (m_phdr->getOutputSection() &&
       isOutputSectionInLoadSegment(m_phdr->getOutputSection()));

  if (!isEhdrInLoadSegment && !isPhdrInLoadSegment)
    return true;

  std::optional<uint64_t> VAddr, PAddr;
  for (auto *Seg : elfSegmentTable()) {
    if (!Seg->isLoadSegment())
      continue;
    for (auto &S : *Seg) {
      if (S->getSection()->getType() == llvm::ELF::PT_NULL)
        continue;
      VAddr = std::min<uint64_t>(VAddr ? VAddr.value()
                                       : std::numeric_limits<uint64_t>::max(),
                                 S->getSection()->addr());
      PAddr = std::min<uint64_t>(PAddr ? PAddr.value()
                                       : std::numeric_limits<uint64_t>::max(),
                                 S->getSection()->pAddr());
    }
    if (VAddr)
      break;
  }

  if (!VAddr)
    return true;

  uint64_t headerSize = m_ehdr->size() + m_phdr->size();

  if ((VAddr.value() - headerSize) == 0)
    return true;

  if (((int64_t)(VAddr.value() - headerSize)) > 0) {
    uint64_t ModifiedAddr =
        llvm::alignDown(VAddr.value() - headerSize, abiPageSize());
    uint64_t ModifiedPAddr = PAddr.value() + (ModifiedAddr - VAddr.value());
    if (m_ehdr) {
      m_ehdr->setAddr(ModifiedAddr);
      m_ehdr->setPaddr(ModifiedPAddr);
    }
    m_phdr->setAddr(ModifiedAddr + getOneEhdrSize());
    m_phdr->setPaddr(ModifiedPAddr + getOneEhdrSize());
    return true;
  }
  return false;
}

bool GNULDBackend::verifySegments() const {
  const auto &SegTable = elfSegmentTable();
  for (const auto &S : SegTable.segments()) {
    if (S->empty() && config().showLinkerScriptWarnings())
      config().raise(Diag::warn_empty_segment) << S->name();
  }
  return true;
}

bool GNULDBackend::setupTLS() {
  ELFSection *firstTLS = nullptr;
  bool seenTLS = false;
  bool lastSectTLS = false;
  uint32_t MaxAlignment = 1;
  SectionMap::iterator out, outBegin, outEnd;
  outBegin = m_Module.getScript().sectionMap().begin();
  outEnd = m_Module.getScript().sectionMap().end();
  out = outBegin;
  while (out != outEnd) {
    auto sec = (*out)->getSection();
    if (!sec->size()) {
      out++;
      continue;
    }
    if (sec->isTLS()) {
      if (seenTLS && !lastSectTLS) {
        config().raise(Diag::non_contiguous_TLS)
            << firstTLS->name() << sec->name();
      }
      if (!firstTLS)
        firstTLS = sec;
      lastSectTLS = true;
      seenTLS = true;
    } else {
      lastSectTLS = false;
    }
    if (lastSectTLS && MaxAlignment < sec->getAddrAlign())
      MaxAlignment = sec->getAddrAlign();
    out++;
  }
  if (firstTLS)
    firstTLS->setAddrAlign(MaxAlignment);
  return seenTLS;
}
