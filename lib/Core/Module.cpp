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
Module::Module(LinkerScript &CurScript, LinkerConfig &Config,
               LayoutInfo *LayoutInfo)
    : UserLinkerScript(CurScript), ThisConfig(Config), DotSymbol(nullptr),
      Linker(nullptr), ThisLayoutInfo(LayoutInfo), Failure(false),
      UsesLto(false), Saver(BAlloc), PM(CurScript, *Config.getDiagEngine(),
                                        Config.options().printTimingStats()),
      SymbolNamePool(Config, PM) {
  State = LinkState::Initializing;
  if (Config.options().isLTOCacheEnabled())
    UserLinkerScript.setHashingEnabled();
  UserLinkerScript.createSectionMap(CurScript, Config, LayoutInfo);
  Printer = ThisConfig.getPrinter();
  if (ThisConfig.shouldCreateReproduceTar())
    createOutputTarWriter();
}

Module::Module(const std::string &Name, LinkerScript &CurScript,
               LinkerConfig &Config, LayoutInfo *LayoutInfo)
    : UserLinkerScript(CurScript), ThisConfig(Config), DotSymbol(nullptr),
      Linker(nullptr), ThisLayoutInfo(LayoutInfo), Failure(false),
      UsesLto(false), Saver(BAlloc), PM(CurScript, *Config.getDiagEngine(),
                                        Config.options().printTimingStats()),
      SymbolNamePool(Config, PM) {
  if (Config.options().isLTOCacheEnabled())
    UserLinkerScript.setHashingEnabled();
  UserLinkerScript.createSectionMap(CurScript, Config, LayoutInfo);
  Printer = ThisConfig.getPrinter();
}

Module::~Module() {
  InputObjectList.clear();
  ArchiveLibraryList.clear();
  DynLibraryList.clear();
  OutputSectionTable.clear();
  AtTable.clear();
  DynamicListSymbols.clear();
  DuplicateFarCalls.clear();
  NoReuseTrampolines.clear();
  Symbols.clear();
  CommonSymbols.clear();
  SectionSymbol.clear();
  CommonMap.clear();
  SectionGroupSignatureMap.clear();
  delete OutputTar;
}

/// Returns an already existing output section with name 'pName', if any;
/// Otherwise returns nullptr.
ELFSection *Module::getSection(const std::string &Name) const {
  auto OutputSect = OutputSectionTableMap.find(Name);
  if (OutputSect == OutputSectionTableMap.end())
    return nullptr;
  return OutputSect->second;
}

eld::IRBuilder *Module::getIRBuilder() const {
  ASSERT(Linker->getIRBuilder(), "Value must be non-null!");
  return Linker->getIRBuilder();
}

/// createSection - create an output section.
ELFSection *Module::createOutputSection(const std::string &Name,
                                        LDFileFormat::Kind PKind, uint32_t Type,
                                        uint32_t PFlag, uint32_t PAlign) {
  ELFSection *OutputSect = getScript().sectionMap().createOutputSectionEntry(
      Name, PKind, Type, PFlag, PAlign);
  addOutputSection(OutputSect);
  if (getPrinter()->isVerbose())
    ThisConfig.raise(Diag::creating_output_section)
        << Name << ELFSection::getELFTypeStr(Name, Type)
        << ELFSection::getELFPermissionsStr(PFlag) << PAlign;
  return OutputSect;
}

InputFile *Module::createInternalInputFile(Input *I, bool CreateElfObjectFile) {
  if (getPrinter()->isVerbose())
    ThisConfig.raise(Diag::creating_internal_inputs) << I->getFileName();
  I->setInputType(Input::Internal);
  if (!I->resolvePath(ThisConfig))
    return {};
  if (CreateElfObjectFile)
    return InputFile::create(I, InputFile::ELFObjFileKind,
                             ThisConfig.getDiagEngine());
  return make<InternalInputFile>(I, ThisConfig.getDiagEngine());
}

bool Module::createInternalInputs() {
  for (int IType = Module::InternalInputType::Attributes;
       IType < Module::InternalInputType::MAX; ++IType) {
    Input *I = nullptr;
    bool CreateElfObjectFile = false;
    switch (IType) {
    case Module::InternalInputType::Attributes:
      I = make<Input>("Attributes", ThisConfig.getDiagEngine());
      break;

    case Module::InternalInputType::BitcodeSections:
      I = make<Input>("BitcodeSections", ThisConfig.getDiagEngine());
      break;

    case Module::InternalInputType::Common:
      I = make<Input>("CommonSymbols", ThisConfig.getDiagEngine());
      break;

    case Module::InternalInputType::CopyRelocSymbols:
      I = make<Input>("CopyRelocSymbols", ThisConfig.getDiagEngine());
      break;

    case Module::InternalInputType::DynamicExports:
      I = make<Input>("Dynamic symbol table exports",
                      ThisConfig.getDiagEngine());
      break;

    case Module::InternalInputType::DynamicList:
      I = make<Input>("DynamicList", ThisConfig.getDiagEngine());
      break;

    case Module::InternalInputType::DynamicSections:
      I = make<Input>("Linker created sections for dynamic linking",
                      ThisConfig.getDiagEngine());
      break;

    case Module::InternalInputType::EhFrameFiller:
      I = make<Input>("EH Frame filler", ThisConfig.getDiagEngine());
      break;

    case Module::InternalInputType::EhFrameHdr:
      I = make<Input>("Eh Frame Header", ThisConfig.getDiagEngine());
      break;

    case Module::InternalInputType::Exception:
      I = make<Input>("Exceptions", ThisConfig.getDiagEngine());
      break;

    case Module::InternalInputType::ExternList:
      I = make<Input>("ExternList", ThisConfig.getDiagEngine());
      break;

    case Module::InternalInputType::Guard:
      I = make<Input>("Linker Guard for Weak Undefined Symbols",
                      ThisConfig.getDiagEngine());
      break;

    case Module::InternalInputType::LinkerVersion:
      I = make<Input>("Linker Version", ThisConfig.getDiagEngine());
      break;

    case Module::InternalInputType::Plugin:
      I = make<Input>("Plugin", ThisConfig.getDiagEngine());
      break;

    case Module::InternalInputType::RegionTable:
      I = make<Input>("RegionTable", ThisConfig.getDiagEngine());
      break;

    case Module::InternalInputType::Script:
      I = make<Input>("Internal-LinkerScript", ThisConfig.getDiagEngine());
      break;

    case Module::InternalInputType::Sections:
      I = make<Input>("Sections", ThisConfig.getDiagEngine());
      break;

    case Module::InternalInputType::SmallData:
      I = make<Input>("SmallData", ThisConfig.getDiagEngine());
      break;

    case Module::InternalInputType::TLSStub:
      I = make<Input>("Linker created TLS transformation stubs",
                      ThisConfig.getDiagEngine());
      break;

    case Module::InternalInputType::Trampoline:
      I = make<Input>("TRAMPOLINE", ThisConfig.getDiagEngine());
      break;

    case Module::InternalInputType::Timing:
      I = make<Input>("TIMING", ThisConfig.getDiagEngine());
      break;

    case Module::InternalInputType::SectionRelocMap:
      I = make<Input>("Section map for --unique-output-sections",
                      ThisConfig.getDiagEngine());
      break;

    case Module::InternalInputType::GlobalDataSymbols:
      I = make<Input>("Internal global data symbols",
                      ThisConfig.getDiagEngine());
      CreateElfObjectFile = true;
      break;

    case Module::InternalInputType::OutputSectData:
      I = make<Input>("Explicit output section data",
                      ThisConfig.getDiagEngine());
      break;

    case Module::InternalInputType::GNUBuildID:
      I = make<Input>("Build ID", ThisConfig.getDiagEngine());
      break;

    default:
      break;
    }
    auto *IF = createInternalInputFile(I, CreateElfObjectFile);
    if (!IF)
      return false;
    InternalFiles[IType] = IF;
  }

  if (Linker->getBackend())
    getBackend().createInternalInputs();

  // Add implicit dot symbol
  Resolver::Result ResolvedResult;
  InputFile *I = getInternalInput(eld::Module::InternalInputType::Script);
  getNamePool().insertSymbol(
      I, ".", true, ResolveInfo::NoType, ResolveInfo::Define,
      ResolveInfo::NoneBinding, 0, 0, ResolveInfo::Hidden, nullptr,
      ResolvedResult, true, false, 0, false /* isPatchable */, getPrinter());
  LDSymbol *DotSym = make<LDSymbol>(ResolvedResult.Info, true);
  DotSym->setFragmentRef(FragmentRef::null());
  DotSym->setValue(0);
  ResolvedResult.Info->setOutSymbol(DotSym);
  setDotSymbol(DotSym);

  return true;
}

ELFSection *Module::createInternalSection(InputFile &I, LDFileFormat::Kind K,
                                          std::string Name, uint32_t Type,
                                          uint32_t PFlag, uint32_t PAlign,
                                          uint32_t EntSize) {
  ELFSection *InputSect = getScript().sectionMap().createELFSection(
      Name, K, Type, PFlag, /*EntSize=*/0);
  InputSect->setAddrAlign(PAlign);
  InputSect->setEntSize(EntSize);
  if (ThisConfig.options().isSectionTracingRequested() &&
      ThisConfig.options().traceSection(Name))
    ThisConfig.raise(Diag::internal_section_create)
        << InputSect->getDecoratedName(ThisConfig.options())
        << utility::toHex(InputSect->getAddrAlign())
        << utility::toHex(InputSect->size())
        << ELFSection::getELFPermissionsStr(InputSect->getFlags());
  InputSect->setInputFile(&I);
  if (I.getKind() == InputFile::InputFileKind::InternalInputKind)
    llvm::dyn_cast<eld::InternalInputFile>(&I)->addSection(InputSect);
  else if (I.getKind() == InputFile::InputFileKind::ELFObjFileKind)
    llvm::dyn_cast<eld::ELFObjectFile>(&I)->addSection(InputSect);
  if (Type == llvm::ELF::SHT_REL || Type == llvm::ELF::SHT_RELA)
    InputSect->setHasNoFragments();
  if (getPrinter()->isVerbose())
    ThisConfig.raise(Diag::created_internal_section)
        << Name << ELFSection::getELFTypeStr(Name, Type)
        << ELFSection::getELFPermissionsStr(PFlag) << PAlign << EntSize;
  return InputSect;
}

/// createSection - create an internal input section.
EhFrameHdrSection *Module::createEhFrameHdrSection(InternalInputType IType,
                                                   std::string Name,
                                                   uint32_t Type,
                                                   uint32_t PFlag,
                                                   uint32_t PAlign) {
  EhFrameHdrSection *InputSect =
      getScript().sectionMap().createEhFrameHdrSection(Name, Type, PFlag);
  InputSect->setAddrAlign(PAlign);
  InputSect->setInputFile(InternalFiles[Type]);
  llvm::dyn_cast<eld::InternalInputFile>(InternalFiles[IType])
      ->addSection(InputSect);
  if (getPrinter()->isVerbose())
    ThisConfig.raise(Diag::created_eh_frame_hdr)
        << Name << ELFSection::getELFTypeStr(Name, Type)
        << ELFSection::getELFPermissionsStr(PFlag) << PAlign;
  return InputSect;
}

Section *Module::createBitcodeSection(const std::string &Section,
                                      BitcodeFile &InputFile, bool Internal) {

  eld::Section *S = make<eld::Section>(Section::Bitcode, Section, 0 /*size */);

  if (S) {
    if (!Internal) {
      if (ThisConfig.options().isSectionTracingRequested() &&
          ThisConfig.options().traceSection(Section))
        ThisConfig.raise(Diag::read_bitcode_section)
            << S->getDecoratedName(ThisConfig.options())
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
  if (ThisConfig.options().isSortCommonEnabled()) {
    bool IsSortingCommonSymbolsAscendingAlignment =
        ThisConfig.options().isSortCommonSymbolsAscendingAlignment();
    std::sort(CommonSymbols.begin(), CommonSymbols.end(),
              [&](const ResolveInfo *A, const ResolveInfo *B) {
                if (IsSortingCommonSymbolsAscendingAlignment)
                  return A->value() < B->value();
                return A->value() > B->value();
              });
    return true;
  }
  std::stable_sort(
      CommonSymbols.begin(), CommonSymbols.end(),
      static_cast<bool (*)(ResolveInfo *, ResolveInfo *)>(
          [](ResolveInfo *A, ResolveInfo *B) -> bool {
            auto OrdA = A->resolvedOrigin()->getInput()->getInputOrdinal();
            auto OrdB = B->resolvedOrigin()->getInput()->getInputOrdinal();
            if (OrdA == OrdB) {
              auto ValA = A->outSymbol()->value();
              auto ValB = B->outSymbol()->value();
              if (ValA == ValB)
                return A->outSymbol()->getSymbolIndex() <
                       B->outSymbol()->getSymbolIndex();
              return ValA < ValB;
            }
            return A->resolvedOrigin()->getInput()->getInputOrdinal() <
                   B->resolvedOrigin()->getInput()->getInputOrdinal();
          }));
  return true;
}

bool Module::sortSymbols() {
  std::stable_sort(Symbols.begin(), Symbols.end(),
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
  if (ThisConfig.options().useDefaultPlugins()) {
    std::vector<eld::sys::fs::Path> DefaultPlugins =
        ThisConfig.directories().getDefaultPluginConfigs();
    if (!ThisConfig.getDiagEngine()->diagnose())
      return false;
    for (auto &Cfg : DefaultPlugins) {
      if (!readOnePluginConfig(Cfg.native(), /*IsDefaultConfig=*/true))
        return false;
    }
  }
  for (const auto &Cfg : ThisConfig.options().getPluginConfig()) {
    if (!readOnePluginConfig(Cfg, /*IsDefaultConfig=*/false))
      return false;
  }
  return true;
}

bool Module::readOnePluginConfig(llvm::StringRef CfgFile,
                                 bool IsDefaultConfig) {
  eld::LinkerPlugin::Config Config;

  if (CfgFile.empty())
    return true;

  const eld::sys::fs::Path *P = ThisConfig.directories().findFile(
      "plugin configuration file", CfgFile.str(), /*PluginName*/ "");

  if (!P) {
    ThisConfig.raise(Diag::error_config_file_not_found) << CfgFile.str();
    return false;
  }

  std::string CfgFilePath = P->getFullPath();

  llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> MBOrErr =
      llvm::MemoryBuffer::getFile(CfgFilePath);

  if (!MBOrErr) {
    ThisConfig.raise(Diag::plugin_config_error) << CfgFilePath;
    return false;
  }

  llvm::yaml::Input YamlIn(*MBOrErr.get());
  YamlIn >> Config;

  if (YamlIn.error()) {
    ThisConfig.raise(Diag::plugin_config_parse_error) << CfgFilePath;
    return false;
  }

  for (auto &G : Config.GlobalPlugins) {
    getScript().addPlugin(G.PluginType, G.LibraryName, G.PluginName, G.Options,
                          ThisConfig.options().printTimingStats("Plugin"),
                          IsDefaultConfig, *this);
    if (getPrinter()->isVerbose())
      ThisConfig.raise(Diag::intializing_plugin) << G.PluginName;
  }

  for (auto &O : Config.OutputSectionPlugins) {
    eld::Plugin *P = getScript().addPlugin(
        O.PluginType, O.LibraryName, O.PluginName, O.Options,
        ThisConfig.options().printTimingStats("Plugin"), IsDefaultConfig,
        *this);
    getScript().addPluginOutputSection(O.OutputSection, P);
    if (getPrinter()->isVerbose())
      ThisConfig.raise(Diag::adding_output_section_for_plugin)
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
      ThisConfig.raise(Diag::no_output_section_for_plugin)
          << OS << P->getName();
      continue;
    }
    OutputSectionEntry *CurOutputSection = OutputSection->getValue();
    CurOutputSection->prolog().setPlugin(P);
  }
  return true;
}

llvm::StringRef Module::getStateStr() const {
#define ADD_CASE(S)                                                            \
  case LinkState::S:                                                           \
    return #S;
  switch (getState()) {
    ADD_CASE(Unknown)
    ADD_CASE(Initializing)
    ADD_CASE(ActBeforeRuleMatching)
    ADD_CASE(BeforeLayout)
    ADD_CASE(ActBeforeSectionMerging)
    ADD_CASE(CreatingSections)
    ADD_CASE(ActBeforePerformingLayout)
    ADD_CASE(CreatingSegments)
    ADD_CASE(AfterLayout)
    ADD_CASE(ActBeforeWritingOutput)
  }
#undef ADD_CASE
}

void Module::addSymbolCreatedByPluginToFragment(Fragment *F, std::string Symbol,
                                                uint64_t Val,
                                                const eld::Plugin *Plugin) {
  LayoutInfo *layoutInfo = getLayoutInfo();
  LDSymbol *S = SymbolNamePool.createPluginSymbol(
      getInternalInput(Module::InternalInputType::Plugin), Symbol, F, Val,
      layoutInfo);
  if (S && layoutInfo && layoutInfo->showSymbolResolution())
    SymbolNamePool.getSRI().recordPluginSymbol(S, Plugin);
  PluginFragmentToSymbols[F];
  PluginFragmentToSymbols[F].push_back(S);
  llvm::dyn_cast<eld::ObjectFile>(F->getOwningSection()->getInputFile())
      ->addLocalSymbol(S);
  addSymbol(S->resolveInfo());
  if (getPrinter()->isVerbose())
    ThisConfig.raise(Diag::adding_symbol_to_fragment) << Symbol;
}

Fragment *Module::createPluginFillFragment(std::string PluginName,
                                           uint32_t Alignment,
                                           uint32_t PaddingSize) {
  LayoutInfo *layoutInfo = getLayoutInfo();
  ELFSection *InputSect = getScript().sectionMap().createELFSection(
      ".bss.paddingchunk." + PluginName, LDFileFormat::Regular,
      llvm::ELF::SHT_PROGBITS, llvm::ELF::SHF_ALLOC, /*EntSize=*/0);
  InputSect->setAddrAlign(Alignment);
  InputSect->setInputFile(InternalFiles[Plugin]);
  Fragment *F =
      make<FillFragment>(*this, 0x0, PaddingSize, InputSect, Alignment);
  addPluginFrag(F);
  InputSect->addFragmentAndUpdateSize(F);
  if (layoutInfo)
    layoutInfo->recordFragment(InternalFiles[Plugin], InputSect, F);
  return F;
}

Fragment *Module::createPluginCodeFragment(std::string PluginName,
                                           std::string Name, uint32_t Alignment,
                                           const char *Buf, size_t Sz) {
  LayoutInfo *layoutInfo = getLayoutInfo();
  ELFSection *InputSect = getScript().sectionMap().createELFSection(
      ".text.codechunk." + Name + "." + PluginName, LDFileFormat::Internal,
      llvm::ELF::SHT_PROGBITS, llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_EXECINSTR,
      /*EntSize=*/0);
  InputSect->setAddrAlign(Alignment);
  InputSect->setInputFile(InternalFiles[Plugin]);
  Fragment *F = make<RegionFragment>(llvm::StringRef(Buf, Sz), InputSect,
                                     Fragment::Type::Region, Alignment);
  addPluginFrag(F);
  if (layoutInfo)
    layoutInfo->recordFragment(InternalFiles[Plugin], InputSect, F);
  return F;
}

Fragment *Module::createPluginDataFragmentWithCustomName(
    const std::string &PluginName, std::string Name, uint32_t Alignment,
    const char *Buf, size_t Sz) {
  LayoutInfo *layoutInfo = getLayoutInfo();
  ELFSection *InputSect = getScript().sectionMap().createELFSection(
      Name, LDFileFormat::Internal, llvm::ELF::SHT_PROGBITS,
      llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE, /*EntSize=*/0);
  InputSect->setAddrAlign(Alignment);
  InputSect->setInputFile(InternalFiles[Plugin]);
  Fragment *F = make<RegionFragment>(llvm::StringRef(Buf, Sz), InputSect,
                                     Fragment::Type::Region, Alignment);
  InputSect->addFragment(F);
  addPluginFrag(F);
  if (layoutInfo)
    layoutInfo->recordFragment(InternalFiles[Plugin], InputSect, F);
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
  LayoutInfo *layoutInfo = getLayoutInfo();
  ELFSection *InputSect = getScript().sectionMap().createELFSection(
      ".data.bsschunk." + Name + "." + PluginName, LDFileFormat::Internal,
      llvm::ELF::SHT_NOBITS, llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE,
      /*EntSize=*/0);
  InputSect->setAddrAlign(Alignment);
  InputSect->setInputFile(InternalFiles[Plugin]);
  Fragment *F = make<FillFragment>(*this, 0, Sz, InputSect, Alignment);
  addPluginFrag(F);
  if (layoutInfo)
    layoutInfo->recordFragment(InternalFiles[Plugin], InputSect, F);
  return F;
}

Fragment *
Module::createPluginFragmentWithCustomName(std::string Name, size_t SectType,
                                           size_t SectFlags, uint32_t Alignment,
                                           const char *Buf, size_t Sz) {
  LayoutInfo *layoutInfo = getLayoutInfo();
  ELFSection *InputSect = getScript().sectionMap().createELFSection(
      Name, LDFileFormat::Internal, SectType, SectFlags, /*EntSize=*/0);
  InputSect->setAddrAlign(Alignment);
  InputSect->setInputFile(InternalFiles[Plugin]);
  Fragment *F = make<RegionFragment>(llvm::StringRef(Buf, Sz), InputSect,
                                     Fragment::Type::Region, Alignment);
  InputSect->addFragment(F);
  addPluginFrag(F);
  if (layoutInfo)
    layoutInfo->recordFragment(InternalFiles[Plugin], InputSect, F);
  return F;
}

GNULDBackend &Module::getBackend() const {
  ASSERT(Linker->getBackend(), "The value must be non-null.");
  return *Linker->getBackend();
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
  Symbols.push_back(R);
  if (getPrinter()->isVerbose())
    ThisConfig.raise(Diag::added_symbol)
        << R->getDecoratedName(ThisConfig.options().shouldDemangle())
        << R->infoAsString();
}

LDSymbol *Module::addSymbolFromBitCode(
    ObjectFile &CurInput, const std::string &Name, ResolveInfo::Type Type,
    ResolveInfo::Desc Desc, ResolveInfo::Binding Binding,
    ResolveInfo::SizeType Size, ResolveInfo::Visibility Visibility,
    unsigned int PIdx) {

  // insert symbol and resolve it immediately
  // resolved_result is a triple <resolved_info, existent, override>
  Resolver::Result ResolvedResult = {nullptr, false, false};

  bool IsLocalSym = false;

  if (Binding == ResolveInfo::Local || Visibility == ResolveInfo::Internal) {
    ResolvedResult.Info = getNamePool().createSymbol(
        &CurInput, Name, false, Type, Desc, Binding, Size, Visibility, false);
    ResolvedResult.Info->setInBitCode(true);
    IsLocalSym = true;
  } else {
    getNamePool().insertSymbol(&CurInput, Name, false, Type, Desc, Binding,
                               Size, 0, Visibility, nullptr, ResolvedResult,
                               false /*isPostLTOPhase*/, true, PIdx, false,
                               getPrinter());
    if (!ThisConfig.options().renameMap().empty() &&
        Desc == ResolveInfo::Undefined) {
      if (ThisConfig.options().renameMap().count(Name))
        saveWrapReference(Name);
    }
  }

  if (!ResolvedResult.Info)
    return nullptr;

  if (ThisConfig.options().cref())
    getIRBuilder()->addToCref(CurInput, ResolvedResult);

  if (ResolvedResult.Overriden || !ResolvedResult.Existent)
    CurInput.setNeeded();

  // create a LDSymbol for the input file.
  LDSymbol *InputSym = getIRBuilder()->makeLDSymbol(ResolvedResult.Info);
  InputSym->setFragmentRef(FragmentRef::null());

  if (!IsLocalSym) {
    SymbolResolutionInfo &SRI = getNamePool().getSRI();
    SRI.recordSymbolInfo(InputSym,
                         SymbolInfo(&CurInput, Size, Binding, Type, Visibility,
                                    Desc, /*isBitcode=*/true));
  }

  if (!ResolvedResult.Info->outSymbol())
    ResolvedResult.Info->setOutSymbol(InputSym);

  if (IsLocalSym)
    CurInput.addLocalSymbol(InputSym);
  else
    CurInput.addSymbol(InputSym);

  if (InputSym->resolveInfo() &&
      ThisConfig.options().traceSymbol(*(InputSym->resolveInfo()))) {
    ThisConfig.raise(Diag::add_new_symbol)
        << InputSym->name() << CurInput.getInput()->decoratedPath()
        << InputSym->resolveInfo()->infoAsString();
  }

  return InputSym;
}

void Module::recordPluginData(std::string PluginName, uint32_t Key, void *Data,
                              std::string Annotation) {
  eld::PluginData *D = eld::make<eld::PluginData>(Key, Data, Annotation);
  PluginDataMap[PluginName].push_back(D);
}

std::vector<eld::PluginData *> Module::getPluginData(std::string PluginName) {
  auto Iter = PluginDataMap.find(PluginName);
  if (Iter != PluginDataMap.end())
    return Iter->second;
  return {};
}

void Module::createOutputTarWriter() {
  // FIXME: Should we perhaps use eld::make<OutputTarWriter> here?
  OutputTar = new eld::OutputTarWriter(ThisConfig);
}

void Module::setFailure(bool Fails) {
  Failure = Fails;
  if (Failure)
    Printer->recordFatalError();
}

llvm::StringRef Module::saveString(std::string S) { return Saver.save(S); }

llvm::StringRef Module::saveString(llvm::StringRef S) { return Saver.save(S); }

bool Module::checkAndRaiseLayoutPrinterDiagEntry(eld::Expected<void> E) const {
  if (E)
    return true;
  ThisConfig.getDiagEngine()->raiseDiagEntry(std::move(E.error()));
  return false;
}

bool Module::createLayoutPrintersForMapStyle(llvm::StringRef MapStyle) {
  if (!ThisLayoutInfo)
    return true;
  // Text
  if (MapStyle.empty() || MapStyle.equals_insensitive("llvm") ||
      MapStyle.equals_insensitive("gnu") ||
      MapStyle.equals_insensitive("txt")) {
    TextMapPrinter = eld::make<eld::TextLayoutPrinter>(ThisLayoutInfo);
    return checkAndRaiseLayoutPrinterDiagEntry(TextMapPrinter->init());
  }
  // YAML
  if (MapStyle.equals_insensitive("yaml") ||
      MapStyle.equals_insensitive("compressed")) {
    YamlMapPrinter = eld::make<eld::YamlLayoutPrinter>(ThisLayoutInfo);
    return checkAndRaiseLayoutPrinterDiagEntry(YamlMapPrinter->init());
  }
  return true;
}

void Module::addPluginFrag(Fragment *F) {
  PluginFragments.push_back(F);
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
  if (!isLinkStateAfterLayout())
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
  std::lock_guard<std::mutex> Guard(Mutex);
  RelocationData.insert({R, Data});
}

bool Module::getRelocationData(const eld::Relocation *R, uint64_t &Data) {
  std::lock_guard<std::mutex> Guard(Mutex);
  return getRelocationDataForSync(R, Data);
}

bool Module::getRelocationDataForSync(const eld::Relocation *R,
                                      uint64_t &Data) {
  auto It = RelocationData.find(R);
  if (It == RelocationData.end())
    return false;
  Data = It->second;
  return true;
}

void Module::addReferencedSymbol(Section &RefencingSection,
                                 ResolveInfo &RefencedSymbol) {
  BitcodeReferencedSymbols[&RefencingSection].push_back(&RefencedSymbol);
}

llvm::ThreadPoolInterface *Module::getThreadPool() {
  if (LinkerThreadPool)
    return LinkerThreadPool;
  LinkerThreadPool = eld::make<llvm::StdThreadPool>(
      llvm::hardware_concurrency(ThisConfig.options().numThreads()));
  return LinkerThreadPool;
}

bool Module::verifyInvariantsForCreatingSectionsState() const {
  if (!getScript().hasPendingSectionOverride(/*LW=*/nullptr))
    return true;
  // Emit 'Error' diagnostic for each plugin with pending section overrides.
  std::unordered_set<std::string> PluginsWithPendingOverrides;
  for (auto *SectionOverride : getScript().getAllSectionOverrides())
    PluginsWithPendingOverrides.insert(SectionOverride->getPluginName());
  for (const auto &PluginName : PluginsWithPendingOverrides)
    ThisConfig.raise(Diag::fatal_pending_section_override) << PluginName;
  return false;
}

bool Module::setLinkState(LinkState S) {
  bool Verification = true;
  if (S == LinkState::CreatingSections)
    Verification = verifyInvariantsForCreatingSectionsState();
  State = S;
  return Verification;
}

CommonELFSection *
Module::createCommonELFSection(const std::string &SectionName, uint32_t Align,
                               InputFile *OriginatingInputFile) {
  CommonELFSection *CommonSection =
      make<CommonELFSection>(SectionName, OriginatingInputFile, Align);
  InternalInputFile *CommonInputFile =
      llvm::cast<InternalInputFile>(getCommonInternalInput());
  CommonSection->setInputFile(CommonInputFile);
  CommonInputFile->addSection(CommonSection);
  if (ThisConfig.options().isSectionTracingRequested() &&
      ThisConfig.options().traceSection(CommonSection->name().str()))
    ThisConfig.raise(Diag::internal_section_create)
        << CommonSection->getDecoratedName(ThisConfig.options())
        << utility::toHex(CommonSection->getAddrAlign())
        << utility::toHex(CommonSection->size())
        << ELFSection::getELFPermissionsStr(CommonSection->getFlags());
  ThisConfig.raise(Diag::created_internal_section)
      << CommonSection->name() << CommonSection->getType()
      << CommonSection->getFlags() << CommonSection->getAddrAlign()
      << CommonSection->getEntSize();
  return CommonSection;
}

bool Module::isPostLTOPhase() const {
  return Linker->getObjLinker()->isPostLTOPhase();
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

bool Module::isBackendInitialized() const {
  return Linker->getBackend() != nullptr;
}
