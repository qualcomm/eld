//===- LayoutInfo.cpp---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/LayoutMap/LayoutInfo.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Config/Version.h"
#include "eld/Core/Module.h"
#include "eld/Fragment/FillFragment.h"
#include "eld/Fragment/Fragment.h"
#include "eld/Fragment/FragmentRef.h"
#include "eld/Fragment/RegionFragment.h"
#include "eld/Input/Input.h"
#include "eld/Input/ObjectFile.h"
#include "eld/Plugin/PluginOp.h"
#include "eld/PluginAPI/DiagnosticEntry.h"
#include "eld/Readers/CommonELFSection.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Support/Memory.h"
#include "eld/SymbolResolver/LDSymbol.h"
#include "eld/Target/GNULDBackend.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Signals.h"
#include <filesystem>
#include <string>

using namespace eld;

//===----------------------------------------------------------------------===//
// LayoutInfo
LayoutInfo::LayoutInfo(LinkerConfig &Config) : ThisConfig(Config) {}

std::string LayoutInfo::infoForFrag(const Fragment *Frag) {
  FragmentInfoMapIterT FragmentInfoIter = FragmentInfoMap.find(Frag);
  if (FragmentInfoIter == FragmentInfoMap.end())
    return "";
  LayoutFragmentInfo *FragmentInfo = FragmentInfoIter->second;
  std::string Info =
      llvm::Twine(FragmentInfo->getResolvedPath() + "[" +
                  FragmentInfo->getDecoratedName(ThisConfig.options()) + "]")
          .str();
  return Info;
}

void LayoutInfo::recordFragment(InputFile *Input,
                                   const ELFSection *InputElfSection,
                                   const Fragment *Frag) {
  if (!Frag)
    return;

  if (FragmentInfoMap.find(Frag) != FragmentInfoMap.end())
    return;

  if (auto *Strings = llvm::dyn_cast<MergeStringFragment>(Frag)) {
    llvm::StringRef CommandLinePrefix = "Command:";
    for (const MergeableString *String : Strings->getStrings())
      if (String->String.starts_with(CommandLinePrefix))
        recordCommentFragment(String->String.data());
  }

  LayoutFragmentInfo *FragmentInfo;
  if (Input)
    FragmentInfo = make<LayoutFragmentInfo>(Input, InputElfSection);
  else
    FragmentInfo = make<LayoutFragmentInfo>(InputElfSection);
  FragmentInfoMap[Frag] = FragmentInfo;
  FragmentInfoVector.push_back(FragmentInfo);
}

void LayoutInfo::recordSymbol(const Fragment *Frag, LDSymbol *Symbol) {
  FragmentInfoMapIterT FragmentInfoIter = FragmentInfoMap.find(Frag);
  if (FragmentInfoIter == FragmentInfoMap.end() || !Symbol->hasName())
    return;
  LayoutFragmentInfo *FragmentInfo = FragmentInfoIter->second;
  FragmentInfo->Symbols.push_back(Symbol);
}

void LayoutInfo::recordThreadCount() {
  if (ThisConfig.options().threadsEnabled())
    LinkStats.NumThreads = ThisConfig.options().numThreads();
}

void LayoutInfo::recordSectionStat(const Section *Sect) {
  if (Sect->isBitcode() || Sect->size())
    return;
  LinkStats.NumZeroSizedSection++;
}

int64_t LayoutInfo::calculateSymbolValue(LDSymbol *Symbol, Module &M) {
  const ELFSection *Section = nullptr;
  const FragmentRef *FragRef = nullptr;
  int64_t SymbolValue = 0;
  if (Symbol->resolveInfo()->outSymbol()->hasFragRef()) {
    FragRef = Symbol->resolveInfo()->outSymbol()->fragRef();
    Section = FragRef->getOutputELFSection();
  }

  Fragment *Frag = nullptr;
  if (FragRef)
    Frag = FragRef->frag();

  // If allocatable section, value => (address + offset)
  if (Section && (Frag && Frag->getOwningSection() &&
                  !Frag->getOwningSection()->isIgnore() &&
                  !Frag->getOwningSection()->isDiscard())) {
    if (Section->isAlloc())
      SymbolValue = Section->addr() + FragRef->getOutputOffset(M);
    else
      SymbolValue = FragRef->getOutputOffset(M);
  } else
    SymbolValue = Symbol->resolveInfo()->outSymbol()->value();
  return SymbolValue;
}

void LayoutInfo::sortFragmentSymbols(LayoutFragmentInfo *FragInfo) {
  std::sort(FragInfo->Symbols.begin(), FragInfo->Symbols.end(),
            static_cast<bool (*)(LDSymbol *, LDSymbol *)>(
                [](LDSymbol *A, LDSymbol *B) -> bool {
                  return A->resolveInfo()->outSymbol()->value() <
                         B->resolveInfo()->outSymbol()->value();
                }));
}

bool LayoutInfo::isSectionDetailedInfoAvailable(ELFSection *Section) {
  if (!Section->hasSectionData())
    return false;

  if (Section->isMergeKind())
    return false;

  if (Section->isIgnore())
    return false;

  // These sections are handled seperately and they dont follow
  // the same path of merging
  switch (Section->getKind()) {
  case LDFileFormat::Discard:
  case LDFileFormat::Null:
  case LDFileFormat::Relocation:
  case LDFileFormat::NamePool:
  case LDFileFormat::Group:
  case LDFileFormat::StackNote:
  case LDFileFormat::EhFrame:
    return false;
  default:
    break;
  }
  return true;
}

void LayoutInfo::recordArchiveMember(Input *Origin, InputFile *Referred,
                                        ArchiveFile::Symbol *ArchSym,
                                        LDSymbol *Sym) {
  ArchiveRecords.push_back(std::make_tuple(Origin, Referred, ArchSym, Sym));
}

void LayoutInfo::recordWholeArchiveMember(Input *WholeArch) {
  ArchiveRecords.push_back(
      std::make_tuple(WholeArch, nullptr, nullptr, nullptr));
}

uint32_t LayoutInfo::LayoutDetail = 0;

std::optional<std::string> LayoutInfo::ThisBasepath;

eld::Expected<void>
LayoutInfo::setLayoutDetail(llvm::StringRef Option,
                               DiagnosticEngine *DiagEngine) {
  const llvm::StringLiteral ShowRelativePathOptionStr = "relative-path";
  uint32_t OptionLayoutDetail =
      llvm::StringSwitch<uint32_t>(Option)
          .Case("show-strings", ShowStrings)
          .Case("absolute-path", ShowAbsolutePath)
          .Case("no-timing", NoTimeStats)
          .Case("only-layout", OnlyLayout)
          .Case("show-header-details", ShowHeaderDetails)
          .Case("show-timing", ShowTiming)
          .Case("show-debug-strings", ShowDebugStrings)
          .Case("show-initial-layout", ShowInitialLayout)
          .Case("show-symbol-resolution", ShowSymbolResolution)
          .StartsWith(ShowRelativePathOptionStr, ShowRelativePath)
          .Default(0);
  LayoutDetail |= OptionLayoutDetail;
  if (Option.starts_with(ShowRelativePathOptionStr)) {
    Option.consume_front(ShowRelativePathOptionStr);
    Option = Option.ltrim("=");
    std::string Basepath;
    if (Option.empty())
      Basepath = std::filesystem::current_path().string();
    else
      Basepath = Option;
    ThisBasepath = std::filesystem::absolute(Basepath).string();
    DiagEngine->raise(Diag::verbose_using_basepath_for_mapfiles)
        << ThisBasepath.value();
  }

  if (!OptionLayoutDetail)
    return std::make_unique<plugin::DiagnosticEntry>(plugin::DiagnosticEntry(
        Diag::invalid_option, {Option.str(), "--MapDetail"}));
  if ((LayoutDetail & ShowAbsolutePath) && (LayoutDetail & ShowRelativePath))
    return std::make_unique<plugin::DiagnosticEntry>(
        plugin::DiagnosticEntry(Diag::error_map_detail_absrel_path));
  return {};
}

std::string LayoutInfo::showSymbolName(llvm::StringRef Name) const {
  if (!ThisConfig.options().shouldDemangle())
    return Name.str();
  return eld::string::getDemangledName(Name);
}

void LayoutInfo::recordInputKind(InputFile::InputFileKind K) {
  switch (K) {
  case InputFile::ELFObjFileKind:
    LinkStats.NumElfObjectFiles++;
    break;
  case InputFile::ELFExecutableFileKind:
    LinkStats.NumElfExecutableFiles++;
    break;
  case InputFile::ELFDynObjFileKind:
    LinkStats.NumSharedObjectFiles++;
    break;
  case InputFile::BitcodeFileKind:
    LinkStats.NumBitCodeFiles++;
    break;
  case InputFile::ELFSymDefFileKind:
    LinkStats.NumSymDefFiles++;
    break;
  case InputFile::GNUArchiveFileKind:
    LinkStats.NumArchiveFiles++;
    break;
  case InputFile::GNULinkerScriptKind:
    LinkStats.NumLinkerScripts++;
    break;
  case InputFile::InputFileKind::BinaryFileKind:
    LinkStats.NumBinaryFiles++;
    break;
  default:
    break;
  }
}

std::string LayoutInfo::getStringFromLoadSequence(InputSequenceT Ist) {
  InputKindPrefix Prefix = Ist.Prefix;
  std::string ArchFlag = Ist.ArchFlag;
  std::string Pref;
  switch (Prefix) {
  case Load:
    Pref = "LOAD ";
    break;
  case Skipped:
    Pref = "SKIPPED ";
    break;
  case SkippedRescan:
    Pref = "SKIPPED (Rescan) ";
    break;
  case StartGroup:
    return "START GROUP";
  case EndGroup:
    return "END GROUP";
  }

  Input *Input = Ist.Input;
  std::string Files = "";
  InputFile::InputFileKind K;

  if (Input != nullptr) {
    Files = Input->decoratedPath();
    if (ThisConfig.options().hasMappingFile())
      Files += "(" + Input->getName() + ")";
    K = Input->getInputFile()->getKind();
  }

  std::string FileType;
  if (ArchFlag.empty()) {
    switch (K) {
    case InputFile::BitcodeFileKind:
      FileType = " [Bitcode]";
      break;
    case InputFile::ELFSymDefFileKind:
      FileType = " [SymDef]";
      break;
    case InputFile::ELFObjFileKind:
    case InputFile::ELFExecutableFileKind:
    case InputFile::ELFDynObjFileKind:
      FileType = " (ELF)";
      break;
    case InputFile::GNUArchiveFileKind: {
      ArchiveFile *ARFile = nullptr;
      ARFile = llvm::dyn_cast<eld::ArchiveFile>(Input->getInputFile());
      if (ARFile->isELFArchive())
        FileType = " (ELF Archive)";
      else
        FileType = " (Bitcode Archive)";
    } break;
    case InputFile::GNULinkerScriptKind:
      FileType = " (GNULinkerScript)";
      break;
    case InputFile::BinaryFileKind:
      FileType = " (Binary)";
      break;
    default:
      ASSERT(0, "Unhandled Input File Kind");
    }
    return Pref + Files + FileType;
  }

  std::string InputFileStr = Pref + Files + ArchFlag;
  const ObjectFile *Obj = llvm::dyn_cast<ObjectFile>(Input->getInputFile());
  if (Obj) {
    std::string FeatureStr = Obj->getFeaturesStr();
    if (!FeatureStr.empty())
      InputFileStr += "[" + FeatureStr + "]";
  }
  return InputFileStr;
}

void LayoutInfo::recordInputActions(InputKindPrefix Prefix, Input *Input,
                                       std::string FileType) {
  InputSequenceT IS;
  IS.Prefix = Prefix;
  IS.Input = Input;
  IS.ArchFlag = FileType;
  InputActions.push_back(IS);
}

void LayoutInfo::recordGroup() { LinkStats.NumGroupTraversal++; }

void LayoutInfo::recordOutputSection() { LinkStats.NumOutputSections++; }

void LayoutInfo::recordGC(const ELFSection *Section) {
  LinkStats.NumSectionsGarbageCollected++;
  if (!Section->size())
    LinkStats.NumZeroSizedSectionsGarbageCollected++;
}

void LayoutInfo::recordLinkerScript(std::string LinkerScriptFile,
                                       bool Found) {
  ScriptInputT Script;
  LinkStats.NumLinkerScripts++;
  Script.Include = LinkerScriptFile;
  Script.Depth = LinkerScriptStack.size();
  if (LinkerScriptStack.size() > 0)
    Script.Parent = LinkerScriptStack.top();
  Script.Found = Found;
  if (Found)
    LinkerScriptStack.push(LinkerScriptFile);
  LinkerScripts.push_back(Script);
}

std::string LayoutInfo::getPath(const std::string &Hash) const {
  if (!ThisConfig.options().hasMappingFile())
    return Hash;
  return ThisConfig.getFileFromHash(Hash);
}

void LayoutInfo::recordLinkerScriptRule() {
  LinkStats.NumLinkerScriptRules++;
}

void LayoutInfo::recordOrphanSection() { LinkStats.NumOrphans++; }

void LayoutInfo::recordTrampolines() { LinkStats.NumTrampolines++; }

void LayoutInfo::recordRetainedSections() {
  LinkStats.NumRetainedSections++;
}

void LayoutInfo::recordNoLinkerScriptRuleMatch() {
  LinkStats.NumNoRuleMatch++;
}

void LayoutInfo::recordPlugin() { LinkStats.NumPlugins++; }

void LayoutInfo::recordFeature(std::string Feature) {
  Features.push_back(Feature);
}

void LayoutInfo::recordSectionOverride(plugin::LinkerWrapper *W,
                                          ChangeOutputSectionPluginOp *O) {
  PluginOps[W].push_back(O);
  Plugins.insert(W);
  ChangeOutputSectionOps[O->getELFSection()].push_back(O);
}

void LayoutInfo::recordAddChunk(plugin::LinkerWrapper *W,
                                   AddChunkPluginOp *O) {
  PluginOps[W].push_back(O);
  Plugins.insert(W);
  ChunkOps[O->getFrag()].push_back(O);
}

void LayoutInfo::recordResetOffset(plugin::LinkerWrapper *W,
                                      ResetOffsetPluginOp *O) {
  PluginOps[W].push_back(O);
}

void LayoutInfo::recordRemoveChunk(plugin::LinkerWrapper *W,
                                      RemoveChunkPluginOp *O) {
  PluginOps[W].push_back(O);
  Plugins.insert(W);
  ChunkOps[O->getFrag()].push_back(O);
}

void LayoutInfo::recordUpdateChunks(plugin::LinkerWrapper *W,
                                       UpdateChunksPluginOp *O) {
  PluginOps[W].push_back(O);
  Plugins.insert(W);
}

void LayoutInfo::recordRemoveSymbol(plugin::LinkerWrapper *W,
                                       RemoveSymbolPluginOp *O) {
  PluginOps[W].push_back(O);
  Plugins.insert(W);
  RemovedSymbols[O->getRemovedSymbol()] = O;
}

LayoutInfo::ResolveInfoVectorT
LayoutInfo::getAllocatedCommonSymbols(Module &Module) {
  GNULDBackend &Backend = *Module.getBackend();
  ObjectFile *CommonInputFile =
      llvm::cast<ObjectFile>(Module.getCommonInternalInput());
  ResolveInfoVectorT CommonSymbols;
  for (const Section *S : CommonInputFile->getSections()) {
    const CommonELFSection *ComSect = llvm::cast<const CommonELFSection>(S);
    if (ComSect->isIgnore() || ComSect->isDiscard())
      continue;
    LDSymbol *Sym = Backend.getCommonSymbol(ComSect);
    ASSERT(Sym, "sym must be non-null!");
    // Skip non-allocated common symbols.
    if (!Sym->hasFragRef())
      continue;
    CommonSymbols.push_back(Sym->resolveInfo());
  }
  return CommonSymbols;
}

LayoutInfo::ResolveInfoVectorT
LayoutInfo::getCommonsGarbageCollected(Module &Module) {
  GNULDBackend &Backend = *Module.getBackend();
  ObjectFile *CommonInputFile =
      llvm::cast<ObjectFile>(Module.getCommonInternalInput());
  ResolveInfoVectorT CommonSymbols;
  for (const Section *S : CommonInputFile->getSections()) {
    const CommonELFSection *ComSect = llvm::cast<const CommonELFSection>(S);
    if (ComSect->isIgnore()) {
      LDSymbol *Sym = Backend.getCommonSymbol(ComSect);
      ASSERT(Sym, "sym must be non-null!");
      CommonSymbols.push_back(Sym->resolveInfo());
    }
  }
  return CommonSymbols;
}

void LayoutInfo::recordRelocationData(plugin::LinkerWrapper *W,
                                         RelocationDataPluginOp *O) {
  PluginOps[W].push_back(O);
  Plugins.insert(W);
  ChunkOps[O->getFrag()].push_back(O);
}

void LayoutInfo::buildMergedStringMap(Module &M) {
  if (!MergedStrings.empty())
    return;
  std::vector<OutputSectionEntry *> OutputSections;
  for (ELFSection *S : M) {
    if (S->isRelocationSection())
      continue;
    if (auto *O = S->getOutputSection())
      OutputSections.push_back(O);
  }
  for (OutputSectionEntry *O : M.getScript().sectionMap()) {
    OutputSections.push_back(O);
  }
  bool GlobalMerge = M.getConfig().options().shouldGlobalStringMerge();
  auto AddString = [&](OutputSectionEntry *O, MergeableString *S) {
    if (!S->Exclude)
      return;
    MergeableString *Merged =
        (GlobalMerge && !S->isAlloc() ? M.getMergedNonAllocString(S)
                                      : O->getMergedString(S));
    ASSERT(Merged, "expected to find a merged string");
    addMergedStrings(Merged, S);
  };
  for (OutputSectionEntry *O : OutputSections)
    for (MergeableString *S : O->getMergeStrings())
      AddString(O, S);
  for (MergeableString *S : M.getNonAllocStrings())
    AddString(nullptr, S);
}

void LayoutInfo::printStats(void *Handle, llvm::raw_ostream &OS) const {
  auto Stat = HandleToStats.find(Handle);
  if (Stat == HandleToStats.end())
    return;
  for (const auto *S : Stat->second)
    S->dumpStat(OS);
}
