//===- Module.cpp----------------------------------------------------------===//
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
#include "eld/Core/Module.h"
#include "eld/Fragment/FragmentRef.h"
#include "eld/Input/BitcodeFile.h"
#include "eld/Input/ELFObjectFile.h"
#include "eld/Input/InternalInputFile.h"
#include "eld/LayoutMap/TextLayoutPrinter.h"
#include "eld/LayoutMap/YamlLayoutPrinter.h"
#include "eld/Object/ObjectLinker.h"
#include "eld/Plugin/PluginData.h"
#include "eld/PluginAPI/PluginConfig.h"
#include "eld/Readers/CommonELFSection.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Readers/EhFrameHdrSection.h"
#include "eld/Support/Memory.h"
#include "eld/Support/MsgHandling.h"
#include "eld/Support/RegisterTimer.h"
#include "eld/Support/Utils.h"
#include "eld/SymbolResolver/LDSymbol.h"
#include "eld/SymbolResolver/NamePool.h"
#include "eld/SymbolResolver/ResolveInfo.h"
#include "eld/SymbolResolver/StaticResolver.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/Allocator.h"
#include "llvm/Support/ErrorOr.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/StringSaver.h"
#include "llvm/Support/ThreadPool.h"

using namespace eld;

//===----------------------------------------------------------------------===//
// Module
//===----------------------------------------------------------------------===//
Module::Module(LinkerScript &pScript, LinkerConfig &pConfig,
               LayoutPrinter *layoutPrinter)
    : m_Script(pScript), m_Config(pConfig), m_DotSymbol(nullptr),
      m_pLinker(nullptr), m_LayoutPrinter(layoutPrinter), m_Failure(false),
      usesLTO(false), Saver(BAlloc), PM(pScript, *pConfig.getDiagEngine(),
                                        pConfig.options().printTimingStats()),
      m_NamePool(pConfig, PM) {
  m_State = plugin::LinkerWrapper::Initializing;
  if (pConfig.options().isLTOCacheEnabled())
    m_Script.setHashingEnabled();
  m_Script.createSectionMap(pScript, pConfig, layoutPrinter);
  m_Printer = m_Config.getPrinter();
  if (m_Config.shouldCreateReproduceTar())
    createOutputTarWriter();
}

Module::Module(const std::string &pName, LinkerScript &pScript,
               LinkerConfig &pConfig, LayoutPrinter *layoutPrinter)
    : m_Script(pScript), m_Config(pConfig), m_DotSymbol(nullptr),
      m_pLinker(nullptr), m_LayoutPrinter(layoutPrinter), m_Failure(false),
      usesLTO(false), Saver(BAlloc), PM(pScript, *pConfig.getDiagEngine(),
                                        pConfig.options().printTimingStats()),
      m_NamePool(pConfig, PM) {
  if (pConfig.options().isLTOCacheEnabled())
    m_Script.setHashingEnabled();
  m_Script.createSectionMap(pScript, pConfig, layoutPrinter);
  m_Printer = m_Config.getPrinter();
}

Module::~Module() {
  m_ObjectList.clear();
  m_ArchiveLibraryList.clear();
  m_DynLibraryList.clear();
  m_SectionTable.clear();
  m_AtTable.clear();
  m_DynamicListSymbols.clear();
  m_DuplicateFarCalls.clear();
  m_NoReuseTrampolines.clear();
  m_Symbols.clear();
  m_CommonSymbols.clear();
  m_SectionSymbol.clear();
  m_CommonMap.clear();
  f_GroupSignatureMap.clear();
  delete m_OutputTar;
}

/// Returns an already existing output section with name 'pName', if any;
/// Otherwise returns nullptr.
ELFSection *Module::getSection(const std::string &pName) const {
  auto OutputSect = m_OutputSectionTableMap.find(pName);
  if (OutputSect == m_OutputSectionTableMap.end())
    return nullptr;
  return OutputSect->second;
}

eld::IRBuilder *Module::getIRBuilder() const {
  ASSERT(m_pLinker->getIRBuilder(), "Value must be non-null!");
  return m_pLinker->getIRBuilder();
}

/// createSection - create an output section.
ELFSection *Module::createOutputSection(const std::string &pName,
                                        LDFileFormat::Kind pKind,
                                        uint32_t pType, uint32_t pFlag,
                                        uint32_t pAlign) {
  ELFSection *output_sect = getScript().sectionMap().createOutputSectionEntry(
      pName, pKind, pType, pFlag, pAlign);
  addOutputSection(output_sect);
  if (getPrinter()->isVerbose())
    m_Config.raise(diag::creating_output_section)
        << pName << ELFSection::getELFTypeStr(pName, pType)
        << ELFSection::getELFPermissionsStr(pFlag) << pAlign;
  return output_sect;
}

InputFile *Module::createInternalInputFile(Input *I, bool createELFObjectFile) {
  if (getPrinter()->isVerbose())
    m_Config.raise(diag::creating_internal_inputs) << I->getFileName();
  I->setInputType(Input::Internal);
  if (!I->resolvePath(m_Config))
    return {};
  if (createELFObjectFile)
    return InputFile::Create(I, InputFile::ELFObjFileKind,
                             m_Config.getDiagEngine());
  else
    return make<InternalInputFile>(I, m_Config.getDiagEngine());
}

bool Module::createInternalInputs() {
  for (int i = Module::InternalInputType::Attributes;
       i < Module::InternalInputType::MAX; ++i) {
    Input *I = nullptr;
    bool createELFObjectFile = false;
    switch (i) {
    case Module::InternalInputType::Attributes:
      I = make<Input>("Attributes", m_Config.getDiagEngine());
      break;

    case Module::InternalInputType::BitcodeSections:
      I = make<Input>("BitcodeSections", m_Config.getDiagEngine());
      break;

    case Module::InternalInputType::Common:
      I = make<Input>("CommonSymbols", m_Config.getDiagEngine());
      break;

    case Module::InternalInputType::CopyRelocSymbols:
      I = make<Input>("CopyRelocSymbols", m_Config.getDiagEngine());
      break;

    case Module::InternalInputType::DynamicExports:
      I = make<Input>("Dynamic symbol table exports", m_Config.getDiagEngine());
      break;

    case Module::InternalInputType::DynamicList:
      I = make<Input>("DynamicList", m_Config.getDiagEngine());
      break;

    case Module::InternalInputType::DynamicSections:
      I = make<Input>("Linker created sections for dynamic linking",
                      m_Config.getDiagEngine());
      break;

    case Module::InternalInputType::EhFrameFiller:
      I = make<Input>("EH Frame filler", m_Config.getDiagEngine());
      break;

    case Module::InternalInputType::EhFrameHdr:
      I = make<Input>("Eh Frame Header", m_Config.getDiagEngine());
      break;

    case Module::InternalInputType::Exception:
      I = make<Input>("Exceptions", m_Config.getDiagEngine());
      break;

    case Module::InternalInputType::ExternList:
      I = make<Input>("ExternList", m_Config.getDiagEngine());
      break;

    case Module::InternalInputType::Guard:
      I = make<Input>("Linker Guard for Weak Undefined Symbols",
                      m_Config.getDiagEngine());
      break;

    case Module::InternalInputType::LinkerVersion:
      I = make<Input>("Linker Version", m_Config.getDiagEngine());
      break;

    case Module::InternalInputType::Plugin:
      I = make<Input>("Plugin", m_Config.getDiagEngine());
      break;

    case Module::InternalInputType::RegionTable:
      I = make<Input>("RegionTable", m_Config.getDiagEngine());
      break;

    case Module::InternalInputType::Script:
      I = make<Input>("Internal-LinkerScript", m_Config.getDiagEngine());
      break;

    case Module::InternalInputType::Sections:
      I = make<Input>("Sections", m_Config.getDiagEngine());
      break;

    case Module::InternalInputType::SmallData:
      I = make<Input>("SmallData", m_Config.getDiagEngine());
      break;

    case Module::InternalInputType::TLSStub:
      I = make<Input>("Linker created TLS transformation stubs",
                      m_Config.getDiagEngine());
      break;

    case Module::InternalInputType::Trampoline:
      I = make<Input>("TRAMPOLINE", m_Config.getDiagEngine());
      break;

    case Module::InternalInputType::Timing:
      I = make<Input>("TIMING", m_Config.getDiagEngine());
      break;

    case Module::InternalInputType::SectionRelocMap:
      I = make<Input>("Section map for --unique-output-sections",
                      m_Config.getDiagEngine());
      break;

    case Module::InternalInputType::GlobalDataSymbols:
      I = make<Input>("Internal global data symbols", m_Config.getDiagEngine());
      createELFObjectFile = true;
      break;

    case Module::InternalInputType::OutputSectData:
      I = make<Input>("Explicit output section data", m_Config.getDiagEngine());
      break;

    case Module::InternalInputType::GNUBuildID:
      I = make<Input>("Build ID", m_Config.getDiagEngine());
      break;

    default:
      break;
    }
    auto *IF = createInternalInputFile(I, createELFObjectFile);
    if (!IF)
      return false;
    m_InternalFiles[i] = IF;
  }

  getBackend()->createInternalInputs();

  return true;
}

ELFSection *Module::createInternalSection(InputFile &I, LDFileFormat::Kind K,
                                          std::string pName, uint32_t pType,
                                          uint32_t pFlag, uint32_t pAlign,
                                          uint32_t entSize) {
  ELFSection *inputSect = getScript().sectionMap().createELFSection(
      pName, K, pType, pFlag, /*EntSize=*/0);
  inputSect->setAddrAlign(pAlign);
  inputSect->setEntSize(entSize);
  if (m_Config.options().isSectionTracingRequested() &&
      m_Config.options().traceSection(pName))
    m_Config.raise(diag::internal_section_create)
        << inputSect->getDecoratedName(m_Config.options())
        << utility::toHex(inputSect->getAddrAlign())
        << utility::toHex(inputSect->size())
        << ELFSection::getELFPermissionsStr(inputSect->getFlags());
  inputSect->setInputFile(&I);
  if (I.getKind() == InputFile::Kind::InternalInputKind)
    llvm::dyn_cast<eld::InternalInputFile>(&I)->addSection(inputSect);
  else if (I.getKind() == InputFile::Kind::ELFObjFileKind)
    llvm::dyn_cast<eld::ELFObjectFile>(&I)->addSection(inputSect);
  if (pType == llvm::ELF::SHT_REL || pType == llvm::ELF::SHT_RELA)
    inputSect->setHasNoFragments();
  if (getPrinter()->isVerbose())
    m_Config.raise(diag::created_internal_section)
        << pName << ELFSection::getELFTypeStr(pName, pType)
        << ELFSection::getELFPermissionsStr(pFlag) << pAlign << entSize;
  return inputSect;
}

/// createSection - create an internal input section.
EhFrameHdrSection *Module::createEhFrameHdrSection(InternalInputType type,
                                                   std::string pName,
                                                   uint32_t pType,
                                                   uint32_t pFlag,
                                                   uint32_t pAlign) {
  EhFrameHdrSection *inputSect =
      getScript().sectionMap().createEhFrameHdrSection(pName, pType, pFlag);
  inputSect->setAddrAlign(pAlign);
  inputSect->setInputFile(m_InternalFiles[type]);
  llvm::dyn_cast<eld::InternalInputFile>(m_InternalFiles[type])
      ->addSection(inputSect);
  if (getPrinter()->isVerbose())
    m_Config.raise(diag::created_eh_frame_hdr)
        << pName << ELFSection::getELFTypeStr(pName, pType)
        << ELFSection::getELFPermissionsStr(pFlag) << pAlign;
  return inputSect;
}

Section *Module::createBitcodeSection(const std::string &section,
                                      BitcodeFile &InputFile, bool Internal) {

  Section *S = make<Section>(Section::Bitcode, section, 0 /*size */);

  if (S) {
    if (!Internal) {
      if (m_Config.options().isSectionTracingRequested() &&
          m_Config.options().traceSection(section))
        m_Config.raise(diag::read_bitcode_section)
            << S->getDecoratedName(m_Config.options())
            << InputFile.getInput()->decoratedPath();
    }

    S->setInputFile(
        Internal ? getInternalInput(Module::InternalInputType::BitcodeSections)
                 : &InputFile);

    // TODO: Internal sections are also added to the bitcode file, not the
    // internal file.
    // TODO: Test if it's needed at all.
    InputFile.addSection(S);
  }
  return S;
}

// Sort common symbols.
//
bool Module::sortCommonSymbols() {
  if (m_Config.options().isSortCommonEnabled()) {
    bool isSortingCommonSymbolsAscendingAlignment =
        m_Config.options().isSortCommonSymbolsAscendingAlignment();
    std::sort(m_CommonSymbols.begin(), m_CommonSymbols.end(),
              [&](const ResolveInfo *A, const ResolveInfo *B) {
                if (isSortingCommonSymbolsAscendingAlignment)
                  return A->value() < B->value();
                return A->value() > B->value();
              });
    return true;
  }
  std::stable_sort(
      m_CommonSymbols.begin(), m_CommonSymbols.end(),
      static_cast<bool (*)(ResolveInfo *, ResolveInfo *)>(
          [](ResolveInfo *A, ResolveInfo *B) -> bool {
            auto ordA = A->resolvedOrigin()->getInput()->getInputOrdinal();
            auto ordB = B->resolvedOrigin()->getInput()->getInputOrdinal();
            if (ordA == ordB) {
              auto valA = A->outSymbol()->value();
              auto valB = B->outSymbol()->value();
              if (valA == valB)
                return A->outSymbol()->getSymbolIndex() <
                       B->outSymbol()->getSymbolIndex();
              return valA < valB;
            }
            return A->resolvedOrigin()->getInput()->getInputOrdinal() <
                   B->resolvedOrigin()->getInput()->getInputOrdinal();
          }));
  return true;
}

bool Module::sortSymbols() {
  std::stable_sort(m_Symbols.begin(), m_Symbols.end(),
                   static_cast<bool (*)(ResolveInfo *, ResolveInfo *)>(
                       [](ResolveInfo *A, ResolveInfo *B) -> bool {
                         // Section symbols always appear first.
                         if (A->type() == ResolveInfo::Section &&
                             (B->type() != ResolveInfo::Section))
                           return true;
                         if (A->type() != ResolveInfo::Section &&
                             (B->type() == ResolveInfo::Section))
                           return false;
                         if (A->isLocal() && !B->isLocal())
                           return true;
                         if (!A->isLocal() && B->isLocal())
                           return false;
                         // All undefs appear after sections.
                         if (A->isUndef() && !B->isUndef())
                           return true;
                         if (!A->isUndef() && B->isUndef())
                           return false;
                         return A->outSymbol()->value() <
                                B->outSymbol()->value();
                       }));
  return true;
}

bool Module::readPluginConfig() {
  if (m_Config.options().useDefaultPlugins()) {
    std::vector<eld::sys::fs::Path> DefaultPlugins =
        m_Config.directories().getDefaultPluginConfigs();
    if (!m_Config.getDiagEngine()->diagnose())
      return false;
    for (auto &Cfg : DefaultPlugins) {
      if (!readOnePluginConfig(Cfg.native()))
        return false;
    }
  }
  for (const auto &Cfg : m_Config.options().getPluginConfig()) {
    if (!readOnePluginConfig(Cfg))
      return false;
  }
  return true;
}

bool Module::readOnePluginConfig(llvm::StringRef CfgFile) {
  eld::LinkerPlugin::Config Config;

  if (CfgFile.empty())
    return true;

  const eld::sys::fs::Path *P = m_Config.directories().findFile(
      "plugin configuration file", CfgFile.str(), /*PluginName*/ "");

  if (!P) {
    m_Config.raise(diag::error_config_file_not_found) << CfgFile.str();
    return false;
  }

  std::string CfgFilePath = P->getFullPath();

  llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> MBOrErr =
      llvm::MemoryBuffer::getFile(CfgFilePath);

  if (!MBOrErr) {
    m_Config.raise(diag::plugin_config_error) << CfgFilePath;
    return false;
  }

  llvm::yaml::Input YamlIn(*MBOrErr.get());
  YamlIn >> Config;

  if (YamlIn.error()) {
    m_Config.raise(diag::plugin_config_parse_error) << CfgFilePath;
    return false;
  }

  for (auto &G : Config.GlobalPlugins) {
    getScript().addPlugin(G.PluginType, G.LibraryName, G.PluginName, G.Options,
                          m_Config.options().printTimingStats("Plugin"), *this);
    if (getPrinter()->isVerbose())
      m_Config.raise(diag::intializing_plugin) << G.PluginName;
  }

  for (auto &O : Config.OutputSectionPlugins) {
    eld::Plugin *P = getScript().addPlugin(
        O.PluginType, O.LibraryName, O.PluginName, O.Options,
        m_Config.options().printTimingStats("Plugin"), *this);
    getScript().addPluginOutputSection(O.OutputSection, P);
    if (getPrinter()->isVerbose())
      m_Config.raise(diag::adding_output_section_for_plugin)
          << O.OutputSection << O.PluginName;
  }
  return true;
}

bool Module::updateOutputSectionsWithPlugins() {
  if (!getScript().hasPlugins())
    return true;

  llvm::StringMap<OutputSectionEntry *> OutputSections;
  for (auto &OutputSect : getScript().sectionMap())
    OutputSections.insert(std::make_pair(OutputSect->name(), OutputSect));

  for (auto &Plugin : getScript().getPluginOutputSection()) {
    llvm::StringRef OS = Plugin.first;
    eld::Plugin *P = Plugin.second;
    auto OutputSection = OutputSections.find(OS);
    if (OutputSection == OutputSections.end()) {
      m_Config.raise(diag::no_output_section_for_plugin) << OS << P->getName();
      continue;
    }
    OutputSectionEntry *CurOutputSection = OutputSection->getValue();
    CurOutputSection->prolog().setPlugin(P);
  }
  return true;
}

llvm::StringRef Module::getStateStr() const {
  switch (getState()) {
  case plugin::LinkerWrapper::Unknown:
    return "Unknown";
  case plugin::LinkerWrapper::Initializing:
    return "Initializing";
  case plugin::LinkerWrapper::BeforeLayout:
    return "BeforeLayout";
  case plugin::LinkerWrapper::CreatingSections:
    return "CreatingSections";
  case plugin::LinkerWrapper::AfterLayout:
    return "AfterLayout";
  case plugin::LinkerWrapper::CreatingSegments:
    return "CreatingSegments";
  }
}

void Module::addSymbolCreatedByPluginToFragment(Fragment *F, std::string Symbol,
                                                uint64_t Val,
                                                const eld::Plugin *plugin) {
  LayoutPrinter *LP = getLayoutPrinter();
  LDSymbol *S = m_NamePool.createPluginSymbol(
      getInternalInput(Module::InternalInputType::Plugin), Symbol, F, Val, LP);
  if (S && LP && LP->showSymbolResolution())
    m_NamePool.getSRI().recordPluginSymbol(S, plugin);
  m_PluginFragmentToSymbols[F];
  m_PluginFragmentToSymbols[F].push_back(S);
  llvm::dyn_cast<eld::ObjectFile>(F->getOwningSection()->getInputFile())
      ->addLocalSymbol(S);
  addSymbol(S->resolveInfo());
  if (getPrinter()->isVerbose())
    m_Config.raise(diag::adding_symbol_to_fragment) << Symbol;
}

Fragment *Module::createPluginFillFragment(std::string PluginName,
                                           uint32_t Alignment,
                                           uint32_t PaddingSize) {
  LayoutPrinter *P = getLayoutPrinter();
  ELFSection *inputSect = getScript().sectionMap().createELFSection(
      ".bss.paddingchunk." + PluginName, LDFileFormat::Regular,
      llvm::ELF::SHT_PROGBITS, llvm::ELF::SHF_ALLOC, /*EntSize=*/0);
  inputSect->setAddrAlign(Alignment);
  inputSect->setInputFile(m_InternalFiles[Plugin]);
  Fragment *F =
      make<FillFragment>(*this, 0x0, PaddingSize, inputSect, Alignment);
  addPluginFrag(F);
  inputSect->addFragmentAndUpdateSize(F);
  if (P)
    P->recordFragment(m_InternalFiles[Plugin], inputSect, F);
  return F;
}

Fragment *Module::createPluginCodeFragment(std::string PluginName,
                                           std::string Name, uint32_t Alignment,
                                           const char *Buf, size_t Sz) {
  LayoutPrinter *P = getLayoutPrinter();
  ELFSection *inputSect = getScript().sectionMap().createELFSection(
      ".text.codechunk." + Name + "." + PluginName, LDFileFormat::Internal,
      llvm::ELF::SHT_PROGBITS, llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_EXECINSTR,
      /*EntSize=*/0);
  inputSect->setAddrAlign(Alignment);
  inputSect->setInputFile(m_InternalFiles[Plugin]);
  Fragment *F = make<RegionFragment>(llvm::StringRef(Buf, Sz), inputSect,
                                     Fragment::Type::Region, Alignment);
  addPluginFrag(F);
  if (P)
    P->recordFragment(m_InternalFiles[Plugin], inputSect, F);
  return F;
}

Fragment *Module::createPluginDataFragmentWithCustomName(
    const std::string &PluginName, std::string Name, uint32_t Alignment,
    const char *Buf, size_t Sz) {
  LayoutPrinter *P = getLayoutPrinter();
  ELFSection *inputSect = getScript().sectionMap().createELFSection(
      Name, LDFileFormat::Internal, llvm::ELF::SHT_PROGBITS,
      llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE, /*EntSize=*/0);
  inputSect->setAddrAlign(Alignment);
  inputSect->setInputFile(m_InternalFiles[Plugin]);
  Fragment *F = make<RegionFragment>(llvm::StringRef(Buf, Sz), inputSect,
                                     Fragment::Type::Region, Alignment);
  inputSect->addFragment(F);
  addPluginFrag(F);
  if (P)
    P->recordFragment(m_InternalFiles[Plugin], inputSect, F);
  return F;
}

Fragment *Module::createPluginDataFragment(std::string PluginName,
                                           std::string Name, uint32_t Alignment,
                                           const char *Buf, size_t Size) {
  return createPluginDataFragmentWithCustomName(
      PluginName, ".data.codechunk." + Name + "." + PluginName, Alignment, Buf,
      Size);
}

Fragment *Module::createPluginBSSFragment(std::string PluginName,
                                          std::string Name, uint32_t Alignment,
                                          size_t Sz) {
  LayoutPrinter *P = getLayoutPrinter();
  ELFSection *inputSect = getScript().sectionMap().createELFSection(
      ".data.bsschunk." + Name + "." + PluginName, LDFileFormat::Internal,
      llvm::ELF::SHT_NOBITS, llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE,
      /*EntSize=*/0);
  inputSect->setAddrAlign(Alignment);
  inputSect->setInputFile(m_InternalFiles[Plugin]);
  Fragment *F = make<FillFragment>(*this, 0, Sz, inputSect, Alignment);
  addPluginFrag(F);
  if (P)
    P->recordFragment(m_InternalFiles[Plugin], inputSect, F);
  return F;
}

Fragment *
Module::createPluginFragmentWithCustomName(std::string Name, size_t sectType,
                                           size_t sectFlags, uint32_t Alignment,
                                           const char *Buf, size_t Sz) {
  LayoutPrinter *P = getLayoutPrinter();
  ELFSection *inputSect = getScript().sectionMap().createELFSection(
      Name, LDFileFormat::Internal, sectType, sectFlags, /*EntSize=*/0);
  inputSect->setAddrAlign(Alignment);
  inputSect->setInputFile(m_InternalFiles[Plugin]);
  Fragment *F = make<RegionFragment>(llvm::StringRef(Buf, Sz), inputSect,
                                     Fragment::Type::Region, Alignment);
  inputSect->addFragment(F);
  addPluginFrag(F);
  if (P)
    P->recordFragment(m_InternalFiles[Plugin], inputSect, F);
  return F;
}

GNULDBackend *Module::getBackend() const {
  ASSERT(m_pLinker->getBackend(), "The value must be non-null.");
  return m_pLinker->getBackend();
}

void Module::replaceFragment(FragmentRef *F, const uint8_t *Data, size_t Sz) {
  MemoryArea *Area =
      MemoryArea::CreateCopy(llvm::StringRef((const char *)Data, Sz));
  ReplaceFrags.push_back(std::make_pair(F, Area));
}

void Module::addSymbol(ResolveInfo *R) {
  FragmentRef *Ref = R->outSymbol()->fragRef();
  if (Ref && Ref->frag())
    Ref->frag()->addSymbol(R);
  m_Symbols.push_back(R);
  if (getPrinter()->isVerbose())
    m_Config.raise(diag::added_symbol)
        << R->getDecoratedName(m_Config.options().shouldDemangle())
        << R->infoAsString();
}

LDSymbol *Module::addSymbolFromBitCode(
    ObjectFile &pInput, const std::string &pName, ResolveInfo::Type pType,
    ResolveInfo::Desc pDesc, ResolveInfo::Binding pBinding,
    ResolveInfo::SizeType pSize, ResolveInfo::Visibility pVisibility,
    unsigned int pIdx) {

  // insert symbol and resolve it immediately
  // resolved_result is a triple <resolved_info, existent, override>
  Resolver::Result resolved_result = {nullptr, false, false};

  bool isLocalSym = false;

  if (pBinding == ResolveInfo::Local || pVisibility == ResolveInfo::Internal) {
    resolved_result.info =
        getNamePool().createSymbol(&pInput, pName, false, pType, pDesc,
                                   pBinding, pSize, pVisibility, false);
    resolved_result.info->setInBitCode(true);
    isLocalSym = true;
  } else {
    getNamePool().insertSymbol(&pInput, pName, false, pType, pDesc, pBinding,
                               pSize, 0, pVisibility, nullptr, resolved_result,
                               false /*isPostLTOPhase*/, true, pIdx, false,
                               getPrinter());
    if (!m_Config.options().renameMap().empty() &&
        pDesc == ResolveInfo::Undefined) {
      if (m_Config.options().renameMap().count(pName))
        saveWrapReference(pName);
    }
  }

  if (!resolved_result.info)
    return nullptr;

  if (m_Config.options().cref())
    getIRBuilder()->addToCref(pInput, resolved_result);

  if (resolved_result.overriden || !resolved_result.existent)
    pInput.setNeeded();

  // create a LDSymbol for the input file.
  LDSymbol *input_sym = getIRBuilder()->makeLDSymbol(resolved_result.info);
  input_sym->setFragmentRef(FragmentRef::Null());

  if (!isLocalSym) {
    SymbolResolutionInfo &SRI = getNamePool().getSRI();
    SRI.recordSymbolInfo(input_sym,
                         SymbolInfo(&pInput, pSize, pBinding, pType,
                                    pVisibility, pDesc, /*isBitcode=*/true));
  }

  if (!resolved_result.info->outSymbol())
    resolved_result.info->setOutSymbol(input_sym);

  if (isLocalSym)
    pInput.addLocalSymbol(input_sym);
  else
    pInput.addSymbol(input_sym);

  if (input_sym->resolveInfo() &&
      m_Config.options().traceSymbol(*(input_sym->resolveInfo()))) {
    m_Config.raise(diag::add_new_symbol)
        << input_sym->name() << pInput.getInput()->decoratedPath()
        << input_sym->resolveInfo()->infoAsString();
  }

  return input_sym;
}

void Module::recordPluginData(std::string PluginName, uint32_t Key, void *Data,
                              std::string Annotation) {
  eld::PluginData *D = eld::make<eld::PluginData>(Key, Data, Annotation);
  PluginDataMap[PluginName].push_back(D);
}

std::vector<eld::PluginData *> Module::getPluginData(std::string PluginName) {
  auto iter = PluginDataMap.find(PluginName);
  if (iter != PluginDataMap.end())
    return iter->second;
  return {};
}

void Module::createOutputTarWriter() {
  // FIXME: Should we perhaps use eld::make<OutputTarWriter> here?
  m_OutputTar = new eld::OutputTarWriter(m_Config);
}

void Module::setFailure(bool fails) {
  m_Failure = fails;
  if (m_Failure)
    m_Printer->recordFatalError();
}

llvm::StringRef Module::saveString(std::string S) { return Saver.save(S); }

llvm::StringRef Module::saveString(llvm::StringRef S) { return Saver.save(S); }

bool Module::checkAndRaiseLayoutPrinterDiagEntry(eld::Expected<void> E) const {
  if (E)
    return true;
  m_Config.getDiagEngine()->raiseDiagEntry(std::move(E.error()));
  return false;
}

bool Module::createLayoutPrintersForMapStyle(llvm::StringRef mapStyle) {
  if (!m_LayoutPrinter)
    return true;
  // Text
  if (mapStyle.empty() || mapStyle.equals_insensitive("llvm") ||
      mapStyle.equals_insensitive("gnu") ||
      mapStyle.equals_insensitive("txt")) {
    m_TextMapPrinter = eld::make<eld::TextLayoutPrinter>(m_LayoutPrinter);
    return checkAndRaiseLayoutPrinterDiagEntry(m_TextMapPrinter->init());
  }
  // YAML
  if (mapStyle.equals_insensitive("yaml") ||
      mapStyle.equals_insensitive("compressed")) {
    m_YAMLMapPrinter = eld::make<eld::YamlLayoutPrinter>(m_LayoutPrinter);
    return checkAndRaiseLayoutPrinterDiagEntry(m_YAMLMapPrinter->init());
  }
  return true;
}

void Module::addPluginFrag(Fragment *F) {
  m_PluginFragments.push_back(F);
  llvm::dyn_cast<eld::ObjectFile>(F->getOwningSection()->getInputFile())
      ->addSection(F->getOwningSection());
}

char *Module::getUninitBuffer(size_t Sz) { return BAlloc.Allocate<char>(Sz); }

bool Module::resetSymbol(ResolveInfo *R, Fragment *F) {
  if (!R->outSymbol())
    return false;
  if (R->isDefine())
    return false;
  FragmentRef *FRef = eld::make<FragmentRef>(*F, 0);
  R->setDesc(ResolveInfo::Define);
  R->outSymbol()->setFragmentRef(FRef);
  return true;
}

uint64_t Module::getImageLayoutChecksum() const {
  uint64_t Hash = 0;
  if (m_State != plugin::LinkerWrapper::AfterLayout)
    return 0;
  for (const eld::ELFSection *S : *this) {
    Hash = llvm::hash_combine(Hash, S->getIndex(), std::string(S->name()),
                              S->getType(), S->addr(), S->offset(), S->size(),
                              S->getEntSize(), S->getFlags(), S->getLink(),
                              S->getInfo(), S->getAddrAlign());
  }
  for (auto &S : getSymbols()) {
    Hash = llvm::hash_combine(Hash, std::string(S->name()), S->size(),
                              S->binding(), S->type(), S->visibility());
    if (!S->outSymbol())
      continue;
    Hash = llvm::hash_combine(Hash, S->outSymbol()->value());
  }
  return Hash;
}

void Module::setRelocationData(const eld::Relocation *R, uint64_t Data) {
  std::lock_guard<std::mutex> Guard(m_Mutex);
  m_RelocationData.insert({R, Data});
}

bool Module::getRelocationData(const eld::Relocation *R, uint64_t &Data) {
  std::lock_guard<std::mutex> Guard(m_Mutex);
  return getRelocationDataForSync(R, Data);
}

bool Module::getRelocationDataForSync(const eld::Relocation *R,
                                      uint64_t &Data) {
  auto It = m_RelocationData.find(R);
  if (It == m_RelocationData.end())
    return false;
  Data = It->second;
  return true;
}

void Module::addReferencedSymbol(Section &RefencingSection,
                                 ResolveInfo &RefencedSymbol) {
  m_BitcodeReferencedSymbols[&RefencingSection].push_back(&RefencedSymbol);
}

llvm::ThreadPoolInterface *Module::getThreadPool() {
  if (m_ThreadPool)
    return m_ThreadPool;
  m_ThreadPool = eld::make<llvm::StdThreadPool>(
      llvm::hardware_concurrency(m_Config.options().numThreads()));
  return m_ThreadPool;
}

bool Module::verifyInvariantsForCreatingSectionsState() const {
  if (!getScript().hasPendingSectionOverride(/*LW=*/nullptr))
    return true;
  // Emit 'Error' diagnostic for each plugin with pending section overrides.
  std::unordered_set<std::string> pluginsWithPendingOverrides;
  for (auto sectionOverride : getScript().getAllSectionOverrides())
    pluginsWithPendingOverrides.insert(sectionOverride->getPluginName());
  for (const auto &pluginName : pluginsWithPendingOverrides)
    m_Config.raise(diag::fatal_pending_section_override) << pluginName;
  return false;
}

bool Module::setState(plugin::LinkerWrapper::State S) {
  bool verification = true;
  if (m_State == plugin::LinkerWrapper::State::BeforeLayout &&
      S == plugin::LinkerWrapper::State::CreatingSections)
    verification = verifyInvariantsForCreatingSectionsState();
  m_State = S;
  return verification;
}

CommonELFSection *
Module::createCommonELFSection(const std::string &sectionName, uint32_t align,
                               InputFile *originatingInputFile) {
  CommonELFSection *commonSection =
      make<CommonELFSection>(sectionName, originatingInputFile, align);
  InternalInputFile *commonInputFile =
      llvm::cast<InternalInputFile>(getCommonInternalInput());
  commonSection->setInputFile(commonInputFile);
  commonInputFile->addSection(commonSection);
  if (m_Config.options().isSectionTracingRequested() &&
      m_Config.options().traceSection(commonSection->name().str()))
    m_Config.raise(diag::internal_section_create)
        << commonSection->getDecoratedName(m_Config.options())
        << utility::toHex(commonSection->getAddrAlign())
        << utility::toHex(commonSection->size())
        << ELFSection::getELFPermissionsStr(commonSection->getFlags());
  m_Config.raise(diag::created_internal_section)
      << commonSection->name() << commonSection->getType()
      << commonSection->getFlags() << commonSection->getAddrAlign()
      << commonSection->getEntSize();
  return commonSection;
}

bool Module::isPostLTOPhase() const {
  return m_pLinker->getObjLinker()->isPostLTOPhase();
}

void Module::setFragmentPaddingValue(Fragment *F, uint64_t V) {
  FragmentPaddingValues[F] = V;
}

std::optional<uint64_t>
Module::getFragmentPaddingValue(const Fragment *F) const {
  if (!FragmentPaddingValues.contains(F))
    return {};
  return FragmentPaddingValues.at(F);
}
