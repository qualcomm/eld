//===- LayoutPrinter.cpp---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/LayoutMap/LayoutPrinter.h"
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
// LayoutPrinter
LayoutPrinter::LayoutPrinter(LinkerConfig &config) : m_Config(config) {}

std::string LayoutPrinter::infoForFrag(const Fragment *frag) {
  FragmentInfoMapIterT fragmentInfoIter = _fragmentInfoMap.find(frag);
  if (fragmentInfoIter == _fragmentInfoMap.end())
    return "";
  LayoutFragmentInfo *fragmentInfo = fragmentInfoIter->second;
  std::string info =
      llvm::Twine(fragmentInfo->getResolvedPath() + "[" +
                  fragmentInfo->getDecoratedName(m_Config.options()) + "]")
          .str();
  return info;
}

void LayoutPrinter::recordFragment(InputFile *input,
                                   const ELFSection *inputELFSection,
                                   const Fragment *frag) {
  if (!frag)
    return;

  if (_fragmentInfoMap.find(frag) != _fragmentInfoMap.end())
    return;

  if (auto *Strings = llvm::dyn_cast<MergeStringFragment>(frag)) {
    llvm::StringRef CommandLinePrefix = "Command:";
    for (const MergeableString *String : Strings->getStrings())
      if (String->String.starts_with(CommandLinePrefix))
        recordCommentFragment(String->String.data());
  }

  LayoutFragmentInfo *fragmentInfo;
  if (input)
    fragmentInfo = make<LayoutFragmentInfo>(input, inputELFSection);
  else
    fragmentInfo = make<LayoutFragmentInfo>(inputELFSection);
  _fragmentInfoMap[frag] = fragmentInfo;
  _fragmentInfoVector.push_back(fragmentInfo);
}

void LayoutPrinter::recordSymbol(const Fragment *frag, LDSymbol *symbol) {
  FragmentInfoMapIterT fragmentInfoIter = _fragmentInfoMap.find(frag);
  if (fragmentInfoIter == _fragmentInfoMap.end() || !symbol->hasName())
    return;
  LayoutFragmentInfo *fragmentInfo = fragmentInfoIter->second;
  fragmentInfo->m_symbols.push_back(symbol);
}

void LayoutPrinter::recordThreadCount() {
  if (m_Config.options().threadsEnabled())
    LinkStats.numThreads = m_Config.options().numThreads();
}

void LayoutPrinter::recordSectionStat(const Section *sect) {
  if (sect->isBitcode() || sect->size())
    return;
  LinkStats.numZeroSizedSection++;
}

int64_t LayoutPrinter::calculateSymbolValue(LDSymbol *Symbol, Module &M) {
  const ELFSection *section = nullptr;
  const FragmentRef *fragRef = nullptr;
  int64_t symbolValue = 0;
  if (Symbol->resolveInfo()->outSymbol()->hasFragRef()) {
    fragRef = Symbol->resolveInfo()->outSymbol()->fragRef();
    section = fragRef->getOutputELFSection();
  }

  Fragment *frag = nullptr;
  if (fragRef)
    frag = fragRef->frag();

  // If allocatable section, value => (address + offset)
  if (section && (frag && frag->getOwningSection() &&
                  !frag->getOwningSection()->isIgnore() &&
                  !frag->getOwningSection()->isDiscard())) {
    if (section->isAlloc())
      symbolValue = section->addr() + fragRef->getOutputOffset(M);
    else
      symbolValue = fragRef->getOutputOffset(M);
  } else
    symbolValue = Symbol->resolveInfo()->outSymbol()->value();
  return symbolValue;
}

void LayoutPrinter::sortFragmentSymbols(LayoutFragmentInfo *FragInfo) {
  std::sort(FragInfo->m_symbols.begin(), FragInfo->m_symbols.end(),
            static_cast<bool (*)(LDSymbol *, LDSymbol *)>(
                [](LDSymbol *a, LDSymbol *b) -> bool {
                  return a->resolveInfo()->outSymbol()->value() <
                         b->resolveInfo()->outSymbol()->value();
                }));
}

bool LayoutPrinter::isSectionDetailedInfoAvailable(ELFSection *section) {
  if (!section->hasSectionData())
    return false;

  if (section->isMergeKind())
    return false;

  if (section->isIgnore())
    return false;

  // These sections are handled seperately and they dont follow
  // the same path of merging
  switch (section->getKind()) {
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

void LayoutPrinter::recordArchiveMember(Input *origin, InputFile *referred,
                                        ArchiveFile::Symbol *archSym,
                                        LDSymbol *sym) {
  _archiveRecords.push_back(std::make_tuple(origin, referred, archSym, sym));
}

void LayoutPrinter::recordWholeArchiveMember(Input *wholeArch) {
  _archiveRecords.push_back(
      std::make_tuple(wholeArch, nullptr, nullptr, nullptr));
}

uint32_t LayoutPrinter::m_LayoutDetail = 0;

std::optional<std::string> LayoutPrinter::m_Basepath;

eld::Expected<void>
LayoutPrinter::setLayoutDetail(llvm::StringRef Option,
                               DiagnosticEngine *diagEngine) {
  const llvm::StringLiteral showRelativePathOptionStr = "relative-path";
  uint32_t optionLayoutDetail =
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
          .StartsWith(showRelativePathOptionStr, ShowRelativePath)
          .Default(0);
  m_LayoutDetail |= optionLayoutDetail;
  if (Option.starts_with(showRelativePathOptionStr)) {
    Option.consume_front(showRelativePathOptionStr);
    Option = Option.ltrim("=");
    std::string basepath;
    if (Option.empty())
      basepath = std::filesystem::current_path().string();
    else
      basepath = Option;
    m_Basepath = std::filesystem::absolute(basepath).string();
    diagEngine->raise(diag::verbose_using_basepath_for_mapfiles)
        << m_Basepath.value();
  }

  if (!optionLayoutDetail)
    return std::make_unique<plugin::DiagnosticEntry>(plugin::DiagnosticEntry(
        diag::invalid_option, {Option.str(), "--MapDetail"}));
  if ((m_LayoutDetail & ShowAbsolutePath) &&
      (m_LayoutDetail & ShowRelativePath))
    return std::make_unique<plugin::DiagnosticEntry>(
        plugin::DiagnosticEntry(diag::error_map_detail_absrel_path));
  return {};
}

std::string LayoutPrinter::showSymbolName(llvm::StringRef Name) const {
  if (!m_Config.options().shouldDemangle())
    return Name.str();
  return eld::string::getDemangledName(Name);
}

void LayoutPrinter::recordInputKind(InputFile::Kind K) {
  switch (K) {
  case InputFile::ELFObjFileKind:
    LinkStats.numELFObjectFiles++;
    break;
  case InputFile::ELFExecutableFileKind:
    LinkStats.numELFExecutableFiles++;
    break;
  case InputFile::ELFDynObjFileKind:
    LinkStats.numSharedObjectFiles++;
    break;
  case InputFile::BitcodeFileKind:
    LinkStats.numBitCodeFiles++;
    break;
  case InputFile::ELFSymDefFileKind:
    LinkStats.numSymDefFiles++;
    break;
  case InputFile::GNUArchiveFileKind:
    LinkStats.numArchiveFiles++;
    break;
  case InputFile::GNULinkerScriptKind:
    LinkStats.numLinkerScripts++;
    break;
  case InputFile::Kind::BinaryFileKind:
    LinkStats.numBinaryFiles++;
    break;
  default:
    break;
  }
}

std::string LayoutPrinter::getStringFromLoadSequence(InputSequenceT ist) {
  InputKindPrefix prefix = ist.prefix;
  std::string archFlag = ist.archFlag;
  std::string pref;
  switch (prefix) {
  case Load:
    pref = "LOAD ";
    break;
  case Skipped:
    pref = "SKIPPED ";
    break;
  case SkippedRescan:
    pref = "SKIPPED (Rescan) ";
    break;
  case StartGroup:
    return "START GROUP";
  case EndGroup:
    return "END GROUP";
  }

  Input *input = ist.input;
  std::string files = "";
  InputFile::Kind K;

  if (input != nullptr) {
    files = input->decoratedPath();
    if (m_Config.options().hasMappingFile())
      files += "(" + input->getName() + ")";
    K = input->getInputFile()->getKind();
  }

  std::string fileType;
  if (archFlag.empty()) {
    switch (K) {
    case InputFile::BitcodeFileKind:
      fileType = " [Bitcode]";
      break;
    case InputFile::ELFSymDefFileKind:
      fileType = " [SymDef]";
      break;
    case InputFile::ELFObjFileKind:
    case InputFile::ELFExecutableFileKind:
    case InputFile::ELFDynObjFileKind:
      fileType = " (ELF)";
      break;
    case InputFile::GNUArchiveFileKind: {
      ArchiveFile *ARFile = nullptr;
      ARFile = llvm::dyn_cast<eld::ArchiveFile>(input->getInputFile());
      if (ARFile->isELFArchive())
        fileType = " (ELF Archive)";
      else
        fileType = " (Bitcode Archive)";
    } break;
    case InputFile::GNULinkerScriptKind:
      fileType = " (GNULinkerScript)";
      break;
    case InputFile::BinaryFileKind:
      fileType = " (Binary)";
      break;
    default:
      ASSERT(0, "Unhandled Input File Kind");
    }
    return pref + files + fileType;
  }

  std::string inputFileStr = pref + files + archFlag;
  const ObjectFile *Obj = llvm::dyn_cast<ObjectFile>(input->getInputFile());
  if (Obj) {
    std::string featureStr = Obj->getFeaturesStr();
    if (!featureStr.empty())
      inputFileStr += "[" + featureStr + "]";
  }
  return inputFileStr;
}

void LayoutPrinter::recordInputActions(InputKindPrefix prefix, Input *input,
                                       std::string fileType) {
  InputSequenceT IS;
  IS.prefix = prefix;
  IS.input = input;
  IS.archFlag = fileType;
  _inputActions.push_back(IS);
}

void LayoutPrinter::recordGroup() { LinkStats.numGroupTraversal++; }

void LayoutPrinter::recordOutputSection() { LinkStats.numOutputSections++; }

void LayoutPrinter::recordGC(const ELFSection *section) {
  LinkStats.numSectionsGarbageCollected++;
  if (!section->size())
    LinkStats.numZeroSizedSectionsGarbageCollected++;
}

void LayoutPrinter::recordLinkerScript(std::string LinkerScriptFile,
                                       bool Found) {
  ScriptInputT script;
  LinkStats.numLinkerScripts++;
  script.include = LinkerScriptFile;
  script.Depth = LinkerScriptStack.size();
  if (LinkerScriptStack.size() > 0)
    script.parent = LinkerScriptStack.top();
  script.found = Found;
  if (Found)
    LinkerScriptStack.push(LinkerScriptFile);
  m_LinkerScripts.push_back(script);
}

std::string LayoutPrinter::getPath(const std::string &hash) const {
  if (!m_Config.options().hasMappingFile())
    return hash;
  return m_Config.getFileFromHash(hash);
}

void LayoutPrinter::recordLinkerScriptRule() {
  LinkStats.numLinkerScriptRules++;
}

void LayoutPrinter::recordOrphanSection() { LinkStats.numOrphans++; }

void LayoutPrinter::recordTrampolines() { LinkStats.numTrampolines++; }

void LayoutPrinter::recordRetainedSections() {
  LinkStats.numRetainedSections++;
}

void LayoutPrinter::recordNoLinkerScriptRuleMatch() {
  LinkStats.numNoRuleMatch++;
}

void LayoutPrinter::recordPlugin() { LinkStats.numPlugins++; }

void LayoutPrinter::recordFeature(std::string Feature) {
  Features.push_back(Feature);
}

void LayoutPrinter::recordSectionOverride(plugin::LinkerWrapper *W,
                                          ChangeOutputSectionPluginOp *O) {
  m_PluginOps[W].push_back(O);
  m_Plugins.insert(W);
  m_ChangeOutputSectionOps[O->getELFSection()].push_back(O);
}

void LayoutPrinter::recordAddChunk(plugin::LinkerWrapper *W,
                                   AddChunkPluginOp *O) {
  m_PluginOps[W].push_back(O);
  m_Plugins.insert(W);
  m_ChunkOps[O->getFrag()].push_back(O);
}

void LayoutPrinter::recordResetOffset(plugin::LinkerWrapper *W,
                                      ResetOffsetPluginOp *O) {
  m_PluginOps[W].push_back(O);
}

void LayoutPrinter::recordRemoveChunk(plugin::LinkerWrapper *W,
                                      RemoveChunkPluginOp *O) {
  m_PluginOps[W].push_back(O);
  m_Plugins.insert(W);
  m_ChunkOps[O->getFrag()].push_back(O);
}

void LayoutPrinter::recordUpdateChunks(plugin::LinkerWrapper *W,
                                       UpdateChunksPluginOp *O) {
  m_PluginOps[W].push_back(O);
  m_Plugins.insert(W);
}

void LayoutPrinter::recordRemoveSymbol(plugin::LinkerWrapper *W,
                                       RemoveSymbolPluginOp *O) {
  m_PluginOps[W].push_back(O);
  m_Plugins.insert(W);
  RemovedSymbols[O->getRemovedSymbol()] = O;
}

LayoutPrinter::ResolveInfoVectorT
LayoutPrinter::getAllocatedCommonSymbols(Module &module) {
  GNULDBackend &backend = *module.getBackend();
  ObjectFile *commonInputFile =
      llvm::cast<ObjectFile>(module.getCommonInternalInput());
  ResolveInfoVectorT commonSymbols;
  for (const Section *S : commonInputFile->getSections()) {
    const CommonELFSection *comSect = llvm::cast<const CommonELFSection>(S);
    if (comSect->isIgnore() || comSect->isDiscard())
      continue;
    LDSymbol *sym = backend.getCommonSymbol(comSect);
    ASSERT(sym, "sym must be non-null!");
    // Skip non-allocated common symbols.
    if (!sym->hasFragRef())
      continue;
    commonSymbols.push_back(sym->resolveInfo());
  }
  return commonSymbols;
}

LayoutPrinter::ResolveInfoVectorT
LayoutPrinter::getCommonsGarbageCollected(Module &module) {
  GNULDBackend &backend = *module.getBackend();
  ObjectFile *commonInputFile =
      llvm::cast<ObjectFile>(module.getCommonInternalInput());
  ResolveInfoVectorT commonSymbols;
  for (const Section *S : commonInputFile->getSections()) {
    const CommonELFSection *comSect = llvm::cast<const CommonELFSection>(S);
    if (comSect->isIgnore()) {
      LDSymbol *sym = backend.getCommonSymbol(comSect);
      ASSERT(sym, "sym must be non-null!");
      commonSymbols.push_back(sym->resolveInfo());
    }
  }
  return commonSymbols;
}

void LayoutPrinter::recordRelocationData(plugin::LinkerWrapper *W,
                                         RelocationDataPluginOp *O) {
  m_PluginOps[W].push_back(O);
  m_Plugins.insert(W);
  m_ChunkOps[O->getFrag()].push_back(O);
}

void LayoutPrinter::buildMergedStringMap(Module &M) {
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

void LayoutPrinter::printStats(void *Handle, llvm::raw_ostream &OS) const {
  auto Stat = HandleToStats.find(Handle);
  if (Stat == HandleToStats.end())
    return;
  for (auto S : Stat->second)
    S->dumpStat(OS);
}
