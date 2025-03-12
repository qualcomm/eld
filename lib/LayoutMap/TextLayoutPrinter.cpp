//===- TextLayoutPrinter.cpp-----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/LayoutMap/TextLayoutPrinter.h"
#include "eld/Config/GeneralOptions.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Config/Version.h"
#include "eld/Core/Linker.h"
#include "eld/Core/Module.h"
#include "eld/Diagnostics/MsgHandler.h"
#include "eld/Fragment/FillFragment.h"
#include "eld/Fragment/FragUtils.h"
#include "eld/Fragment/Fragment.h"
#include "eld/Fragment/FragmentRef.h"
#include "eld/Fragment/RegionFragment.h"
#include "eld/Fragment/RegionFragmentEx.h"
#include "eld/Fragment/TimingFragment.h"
#include "eld/Input/ArchiveFile.h"
#include "eld/Input/ELFObjectFile.h"
#include "eld/Input/Input.h"
#include "eld/Object/ObjectLinker.h"
#include "eld/Object/OutputSectionEntry.h"
#include "eld/PluginAPI/LinkerWrapper.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Readers/TimingSection.h"
#include "eld/Readers/TimingSlice.h"
#include "eld/Script/ExternCmd.h"
#include "eld/Script/MemoryCmd.h"
#include "eld/Script/OutputSectData.h"
#include "eld/Script/ScriptFile.h"
#include "eld/Script/ScriptSymbol.h"
#include "eld/Script/StrToken.h"
#include "eld/Script/SymbolContainer.h"
#include "eld/Script/VersionScript.h"
#include "eld/Support/RegisterTimer.h"
#include "eld/SymbolResolver/NamePool.h"
#include "eld/Target/ELFSegmentFactory.h"
#include "eld/Target/GNULDBackend.h"
#include "eld/Target/Relocator.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/raw_ostream.h"
#include <filesystem>
#include <optional>
#include <string>

using namespace eld;

//===----------------------------------------------------------------------===//
// TextLayoutPrinter
// FIXME: It might be better to make m_LayoutPrinter
// as a const reference, as then there would be no
// risk of nullptr dereference when accessing
// m_LayoutPrinter.
TextLayoutPrinter::TextLayoutPrinter(LayoutPrinter *P)
    : layoutFile(nullptr), m_LayoutPrinter(P) {}

eld::Expected<void> TextLayoutPrinter::init() {
  if (m_LayoutPrinter->getConfig().options().printMap())
    return {};
  std::string SuffixExtension = "";
  if (!m_LayoutPrinter->getConfig().options().isDefaultMapStyleText())
    SuffixExtension = ".txt";
  std::error_code error;
  // Check if a map file was provided in Linker arguments and open it for
  // writing.
  if (!m_LayoutPrinter->getConfig().options().layoutFile().empty()) {
    std::string layoutFileName =
        m_LayoutPrinter->getConfig().options().layoutFile();
    if (!SuffixExtension.empty())
      layoutFileName =
          m_LayoutPrinter->getConfig().options().layoutFile() + SuffixExtension;
    layoutFile = std::make_unique<llvm::raw_fd_ostream>(layoutFileName, error,
                                                        llvm::sys::fs::OF_None);
    Buffer = std::make_unique<llvm::raw_string_ostream>(Storage);
    // Check if a map file was provided in Linker arguments and an error
    // occured, then write error to standard error and exit with code 1.
    if (error) {
      return std::make_unique<plugin::DiagnosticEntry>(
          plugin::FatalDiagnosticEntry(
              diag::fatal_unwritable_output,
              {m_LayoutPrinter->getConfig().options().layoutFile(),
               error.message()}));
    }
  }
  return {};
}

// Flush memory map contents to provided map file or return error if any.
llvm::raw_ostream &TextLayoutPrinter::outputStream() const {
  if (Buffer)
    return *Buffer;
  return llvm::errs();
}

void TextLayoutPrinter::printOnlyLayoutSection(GNULDBackend const &backend,
                                               const OutputSectionEntry *OS,
                                               bool useColor) {
  const ELFSection *section = OS->getSection();
  if (useColor)
    outputStream().changeColor(llvm::raw_ostream::GREEN);
  outputStream() << "\n";
  outputStream() << section->name() << "\t0x";
  outputStream().write_hex(section->size());
  outputStream() << "\t# Alignment: 0x";
  outputStream().write_hex(section->getAddrAlign());
  if (section->isAlloc())
    printSegments(backend, OS);
  outputStream() << "\n";

  if (useColor)
    outputStream().resetColor();

  auto Info = m_LayoutPrinter->getPluginInfo().find(section);
  if (Info == m_LayoutPrinter->getPluginInfo().end())
    return;
  for (auto &P : Info->second) {
    outputStream() << "# " << P->getName() << "\t" << P->getLibraryName()
                   << "\t" << P->getPluginType() << "\t"
                   << P->getPluginOptions() << "\n";
  }
}

void TextLayoutPrinter::printStat(llvm::StringRef S, uint64_t Stats) {
  if (!Stats)
    return;
  outputStream() << "# " << S << " : " << Stats << "\n";
}

void TextLayoutPrinter::printStat(llvm::StringRef S,
                                  const std::string &stat) const {
  if (stat.empty())
    return;
  outputStream() << "# " << S << " : " << stat << "\n";
}

void TextLayoutPrinter::printStats(LayoutPrinter::Stats &L,
                                   const Module &module) {
  const ObjectLinker &objLinker = *(module.getLinker()->getObjectLinker());
  const GNULDBackend &backend = *module.getBackend();

  if (!L.hasStats())
    return;
  outputStream() << "# "
                 << "LinkStats Begin"
                 << "\n";
  printStat("ObjectFiles", L.numELFObjectFiles);
  printStat("ExecutableFiles", L.numELFExecutableFiles);
  printStat("LinkerScripts", L.numLinkerScripts);
  printStat("SharedObjects", L.numSharedObjectFiles);
  printStat("SymDefFiles", L.numSymDefFiles);
  printStat("ThreadCount", L.numThreads);
  printStat("ArchiveFiles", L.numArchiveFiles);
  printStat("GroupTraversals", L.numGroupTraversal);
  printStat("BitcodeFiles", L.numBitCodeFiles);
  printStat("NumInputSections", objLinker.getAllInputSections().size());
  printStat("ZeroSizedSections", L.numZeroSizedSection);
  printStat("SectionsGarbageCollected", L.numSectionsGarbageCollected);
  printStat("ZeroSizedSectionsGarbageCollected",
            L.numZeroSizedSectionsGarbageCollected);
  printStat("RetainedSections", L.numRetainedSections);
  printTotalSymbolStats(module);
  printDiscardedSymbolStats(module);
  printStat("NumLinkerScriptRules", L.numLinkerScriptRules);
  printStat("NumOutputSections", L.numOutputSections);
  printStat("NumGOTEntries", backend.GOTEntriesCount());
  printStat("NumPLTEntries", backend.PLTEntriesCount());
  printStat("NumPlugins", L.numPlugins);
  printStat("NumOrphans", L.numOrphans);
  printStat("NumTrampolines", L.numTrampolines);
  printStat("NoRuleMatches", L.numNoRuleMatch);
  printStat("NumNOLOADSections", backend.NOLOADSectionsCount());
  printStat("NumSegments", backend.elfSegmentTable().size());
  if (L.OutputFileSize)
    printStat("OutputFileSize",
              std::to_string(L.OutputFileSize.value()) + " bytes");

  printStat("LinkTime", L.LinkTime);
  m_LayoutPrinter->printStats((void *)(&module), outputStream());
  outputStream() << "# "
                 << "LinkStats End"
                 << "\n";
}

void TextLayoutPrinter::printSegments(GNULDBackend const &Backend,
                                      const OutputSectionEntry *OS) {
  const auto &Segments = Backend.getSegmentsForSection(OS);
  if (!Segments.size())
    return;
  std::ostringstream Output;
  std::for_each(Segments.begin(), Segments.end() - 1,
                [&Output](const ELFSegment *Seg) {
                  Output << Seg->getSpec()->name() << ", ";
                });
  if (!Segments.back()->getSpec())
    Output << Segments.back()->name();
  else
    Output << Segments.back()->getSpec()->name();
  outputStream() << ", Segments : [ " << Output.str() << " ]";
}

void TextLayoutPrinter::printMemoryRegions(GNULDBackend const &Backend,
                                           const OutputSectionEntry *OS) {
  if (!OS->epilog().hasRegion())
    return;

  std::string VMARegionName = OS->epilog().region().getName();
  std::optional<std::string> LMARegionName;
  if (OS->epilog().hasLMARegion())
    LMARegionName = OS->epilog().lmaRegion().getName();
  outputStream() << ", Memory : [" << VMARegionName;
  if (LMARegionName)
    outputStream() << ", " << LMARegionName;
  outputStream() << "]";
}

// Print section information like name, address, offset, size, alignment,
// etc. based on the section type.
void TextLayoutPrinter::printSection(GNULDBackend const &backend,
                                     const OutputSectionEntry *OS,
                                     bool useColor) {
  if (m_LayoutPrinter->showOnlyLayout()) {
    printOnlyLayoutSection(backend, OS, useColor);
    return;
  }
  const ELFSection *section = OS->getSection();
  if (useColor)
    outputStream().changeColor(llvm::raw_ostream::GREEN);
  outputStream() << "\n";
  outputStream() << section->name();
  if (backend.getModule().getState() ==
      plugin::LinkerWrapper::State::BeforeLayout) {
    outputStream() << "\n";
    return;
  }
  outputStream() << "\t0x";
  if (section->isAlloc())
    outputStream().write_hex(section->addr());
  else
    outputStream().write_hex(section->offset());

  outputStream() << "\t0x";
  outputStream().write_hex(section->size());

  outputStream() << " # Offset: 0x";
  outputStream().write_hex(section->offset());

  if (section->isAlloc()) {
    outputStream() << ", LMA: 0x";
    outputStream().write_hex(section->pAddr());
  }

  // Print alignment
  outputStream() << ", Alignment: 0x";
  outputStream().write_hex(section->getAddrAlign());

  // Print flags
  outputStream() << ", Flags: "
                 << ELFSection::getELFPermissionsStr(section->getFlags());

  outputStream() << ", Type: " << OS->getSectionTypeStr();

  if (section->isAlloc()) {
    printSegments(backend, OS);
    printMemoryRegions(backend, OS);
  }

  outputStream() << "\n";

  if (useColor)
    outputStream().resetColor();

  if (OS->getTotalTrampolineCount()) {
    outputStream() << "# NumTrampolines: " << OS->getTotalTrampolineCount();
    outputStream() << "\n";
  }

  m_LayoutPrinter->printStats((void *)(OS), outputStream());

  auto Info = m_LayoutPrinter->getPluginInfo().find(section);
  if (Info == m_LayoutPrinter->getPluginInfo().end())
    return;
  for (auto &P : Info->second) {
    outputStream() << "# " << P->getName() << "\t" << P->getLibraryName()
                   << "\t" << P->getPluginType() << "\t"
                   << P->getPluginOptions() << "\n";
  }
}

// Print Linker Plugin information for all available Plugin types.
void TextLayoutPrinter::printGlobalPluginInfo(Module &M, bool useColor) {
  LinkerScript::PluginVectorT PluginListForSectionMatcher =
      M.getScript().getPluginForType(plugin::Plugin::Type::SectionMatcher);
  LinkerScript::PluginVectorT PluginListForSectionIterator =
      M.getScript().getPluginForType(plugin::Plugin::Type::SectionIterator);
  LinkerScript::PluginVectorT PluginListForOutputSectionIterator =
      M.getScript().getPluginForType(
          plugin::Plugin::Type::OutputSectionIterator);
  LinkerScript::PluginVectorT PluginListForMemorySize =
      M.getScript().getPluginForType(plugin::Plugin::Type::ControlMemorySize);
  LinkerScript::PluginVectorT PluginListForFileSize =
      M.getScript().getPluginForType(plugin::Plugin::Type::ControlFileSize);
  const LinkerScript::PluginVectorT &universalPlugins =
      M.getPluginManager().getUniversalPlugins();

  if (useColor)
    outputStream().changeColor(llvm::raw_ostream::GREEN);
  outputStream() << "Linker Plugin Information"
                 << "\n";
  if (useColor)
    outputStream().resetColor();

  if (universalPlugins.size()) {
    outputStream() << "\t";
    outputStream() << "Linker Plugins" << "\n";
    for (auto &P : universalPlugins)
      outputStream() << "\t" << "\t" << P->getName() << "\t"
                     << P->getLibraryName() << "\t" << P->getPluginType()
                     << "\t" << P->getPluginOptions() << "\n";
  }
  if (PluginListForSectionMatcher.size()) {
    outputStream() << "\t";
    outputStream() << "SectionMatcher Plugins"
                   << "\n";
    for (auto &P : PluginListForSectionMatcher)
      outputStream() << "\t"
                     << "\t" << P->getName() << "\t" << P->getLibraryName()
                     << "\t" << P->getPluginType() << "\t"
                     << P->getPluginOptions() << "\n";
  }
  if (PluginListForSectionIterator.size()) {
    outputStream() << "\t";
    outputStream() << "SectionIterator Plugins"
                   << "\n";
    for (auto &P : PluginListForSectionIterator)
      outputStream() << "\t"
                     << "\t" << P->getName() << "\t" << P->getLibraryName()
                     << "\t" << P->getPluginType() << "\t"
                     << P->getPluginOptions() << "\n";
  }
  if (PluginListForOutputSectionIterator.size()) {
    outputStream() << "\t";
    outputStream() << "Output SectionIterator Plugins"
                   << "\n";
    for (auto &P : PluginListForOutputSectionIterator)
      outputStream() << "\t"
                     << "\t" << P->getName() << "\t" << P->getLibraryName()
                     << "\t" << P->getPluginType() << "\t"
                     << P->getPluginOptions() << "\n";
  }
  if (PluginListForMemorySize.size()) {
    outputStream() << "\t";
    outputStream() << "Memory Size Plugin(s)"
                   << "\n";
    for (auto &P : PluginListForMemorySize)
      outputStream() << "\t"
                     << "\t" << P->getName() << "\t" << P->getLibraryName()
                     << "\t" << P->getPluginType() << "\t"
                     << P->getPluginOptions() << "\n";
  }
  if (PluginListForFileSize.size()) {
    outputStream() << "\t";
    outputStream() << "File Size Plugin(s)"
                   << "\n";
    for (auto &P : PluginListForFileSize)
      outputStream() << "\t"
                     << "\t" << P->getName() << "\t" << P->getLibraryName()
                     << "\t" << P->getPluginType() << "\t"
                     << P->getPluginOptions() << "\n";
  }
  outputStream() << "Linker Plugin Run Information"
                 << "\n";
  int Index = 0;
  for (auto &P : M.getScript().getPluginRunList()) {
    outputStream() << "#" << ++Index << "\t";
    outputStream() << P->getName() << "\t" << P->getLibraryName() << "\t"
                   << P->getPluginType() << "\t" << P->getPluginOptions()
                   << "\n";
  }
}

bool TextLayoutPrinter::printChangeOutputSectionPluginOpDetailed(
    PluginOp *Pop) {
  ChangeOutputSectionPluginOp *Op =
      llvm::dyn_cast<ChangeOutputSectionPluginOp>(Pop);
  if (!Op)
    return false;
  ELFSection *S = Op->getELFSection();
  outputStream() << "#\tSection :" << S->name() << "\t";
  if (S->hasOldInputFile()) {
    outputStream() << getDecoratedPath(S->getOldInputFile()->getInput());
  } else {
    outputStream() << getDecoratedPath(S->getInputFile()->getInput());
  }
  outputStream() << "\n";
  outputStream() << "#\tOriginal Rule : ";
  RuleContainer *Orig = Op->getOrigRule();
  if (Orig && Orig->desc())
    Orig->desc()->dumpMap(outputStream());
  outputStream() << "#\tModified Rule : ";
  RuleContainer *Mod = Op->getModifiedRule();
  if (Mod->desc())
    Mod->desc()->dumpMap(outputStream());
  return true;
}

bool TextLayoutPrinter::printAddChunkPluginOp(PluginOp *Pop) const {
  AddChunkPluginOp *Op = llvm::dyn_cast<AddChunkPluginOp>(Pop);
  if (!Op)
    return false;
  ELFSection *S = Op->getFrag()->getOwningSection();
  outputStream() << "#\tSection :" << S->name() << "\t";
  if (S->hasOldInputFile()) {
    outputStream() << getDecoratedPath(S->getOldInputFile()->getInput());
  } else {
    outputStream() << getDecoratedPath(S->getInputFile()->getInput());
  }
  outputStream() << "\n";
  outputStream() << "#\tRule : ";
  RuleContainer *Orig = Op->getRule();
  if (Orig->desc())
    Orig->desc()->dumpMap(outputStream());
  return true;
}

bool TextLayoutPrinter::printRemoveChunkPluginOp(PluginOp *Pop) const {
  RemoveChunkPluginOp *Op = llvm::dyn_cast<RemoveChunkPluginOp>(Pop);
  if (!Op)
    return false;
  ELFSection *S = Op->getFrag()->getOwningSection();
  outputStream() << "#\tSection :" << S->name() << "\t";
  if (S->hasOldInputFile()) {
    outputStream() << getDecoratedPath(S->getOldInputFile()->getInput());
  } else {
    outputStream() << getDecoratedPath(S->getInputFile()->getInput());
  }
  outputStream() << "\n";
  outputStream() << "#\tRule : ";
  RuleContainer *Orig = Op->getRule();
  if (Orig->desc())
    Orig->desc()->dumpMap(outputStream());
  return true;
}

bool TextLayoutPrinter::printUpdateChunksPluginOp(PluginOp *Pop) const {
  UpdateChunksPluginOp *Op = llvm::dyn_cast<UpdateChunksPluginOp>(Pop);
  if (!Op)
    return false;
  return true;
}

bool TextLayoutPrinter::printResetOffsetPluginOp(PluginOp *Op) const {
  auto *R = llvm::dyn_cast<ResetOffsetPluginOp>(Op);
  if (!R)
    return false;
  outputStream() << "\tSection: " << R->getOutputSection()->name()
                 << " OldOffset: " << R->getOldOffset() << "\n";
  return true;
}

bool TextLayoutPrinter::printRelocationDataPluginOp(eld::Module &M,
                                                    PluginOp *Pop) const {
  auto *Op = llvm::dyn_cast<RelocationDataPluginOp>(Pop);
  if (!Op)
    return false;
  const auto *R = Op->getRelocation();
  if (const auto *F = R->targetRef()) {
    const auto *S = F->frag()->getOwningSection();
    outputStream() << "#\tFile: "
                   << getDecoratedPath(S->getInputFile()->getInput())
                   << "\tSection: " << S->name() << "\tOffset: 0x";
    outputStream().write_hex(F->frag()->getOffset() + F->offset());
    outputStream() << "\n";
  }
  outputStream() << "#\tType: "
                 << M.getBackend()->getRelocator()->getName(R->type()) << "\n";
  outputStream() << "#\tOriginal: 0x";
  outputStream().write_hex(R->target());
  uint64_t RelocationData;
  bool relocDataFound = M.getRelocationDataForSync(R, RelocationData);
  ASSERT(relocDataFound, "Relocation data must be available");
  if (relocDataFound) {
    outputStream() << "\tUpdated: 0x";
    outputStream().write_hex(RelocationData);
  }
  outputStream() << "\n";
  return true;
}

bool TextLayoutPrinter::printChunkOps(PluginOp *Pop) const {
  if (printAddChunkPluginOp(Pop))
    return true;
  if (printRemoveChunkPluginOp(Pop))
    return true;
  if (printUpdateChunksPluginOp(Pop))
    return true;
  if (printResetOffsetPluginOp(Pop))
    return true;
  return false;
}

void TextLayoutPrinter::printPluginInfo(eld::Module &M) {
  if (!m_LayoutPrinter->getPlugins().size())
    return;
  outputStream() << "# Detailed Plugin information "
                 << "\n";
  int PluginCount = 0;
  for (auto &W : m_LayoutPrinter->getPlugins()) {
    eld::Plugin *Plugin = W->getPlugin();
    outputStream() << "# Plugin #" << PluginCount << "\t" << Plugin->getName()
                   << "\t" << Plugin->getLibraryName() << "\n";
    int Count = 0;
    for (auto &Op : m_LayoutPrinter->getPluginOps()[W]) {
      outputStream() << "#\tModification #" << ++Count;
      std::string Annotation = Op->getAnnotation();
      if (!Annotation.empty())
        Annotation = ", Comment : " + Annotation;
      outputStream() << "\t{" << Op->getPluginOpStr() << Annotation << "}"
                     << "\n";
      if (printChangeOutputSectionPluginOpDetailed(Op))
        continue;
      if (printResetOffsetPluginOp(Op))
        continue;
      if (printChunkOps(Op))
        continue;
      if (printRelocationDataPluginOp(M, Op))
        continue;
    }
  }
}

// Print the padding information to the Map file.
void TextLayoutPrinter::printOnlyLayoutPadding(ELFSection *section,
                                               int64_t startOffset, int64_t sz,
                                               int64_t fillValue,
                                               bool isAlignment,
                                               bool useColor) const {
  if (useColor)
    outputStream().changeColor(llvm::raw_ostream::RED);
  outputStream() << "PADDING";
  if (isAlignment)
    outputStream() << "_ALIGNMENT";
  outputStream() << "\t0x";
  outputStream().write_hex(sz);
  outputStream() << "\t";
  outputStream() << "0x";
  outputStream().write_hex(fillValue);
  outputStream() << "\n";
  if (useColor)
    outputStream().resetColor();
}

void TextLayoutPrinter::printPadding(ELFSection *section, int64_t startOffset,
                                     int64_t sz, int64_t fillValue,
                                     bool isAlignment, bool useColor) const {
  if (!sz)
    return;
  if (m_LayoutPrinter->showOnlyLayout()) {
    printOnlyLayoutPadding(section, startOffset, sz, fillValue, isAlignment,
                           useColor);
    return;
  }
  if (useColor)
    outputStream().changeColor(llvm::raw_ostream::RED);
  outputStream() << "PADDING";
  if (isAlignment)
    outputStream() << "_ALIGNMENT";
  outputStream() << "\t0x";
  if (section->isAlloc())
    outputStream().write_hex(section->addr() + startOffset);
  else
    outputStream().write_hex(section->offset() + startOffset);
  outputStream() << "\t";
  outputStream() << "0x";
  outputStream().write_hex(sz);
  outputStream() << "\t";
  outputStream() << "0x";
  outputStream().write_hex(fillValue);
  outputStream() << "\n";
  if (useColor)
    outputStream().resetColor();
}

void TextLayoutPrinter::printMergeString(MergeableString *S, Module &M) const {
  if (S->Exclude) {
    outputStream() << "\n";
    return;
  }
  if (m_LayoutPrinter->showStrings()) {
    outputStream() << "[ Contents: " << S->String.data() << "]";
    outputStream() << "\n";
  }
  for (MergeableString *Merged : m_LayoutPrinter->getMergedStrings(S)) {
    assert(Merged->Exclude);
    outputStream() << "\t# ";
    ELFSection *S = Merged->Fragment->getOwningSection();
    outputStream() << S->getDecoratedName(M.getConfig().options()) << "\t0x";
    outputStream().write_hex(Merged->InputOffset);
    outputStream() << "\t" << getDecoratedPath(S->getInputFile()->getInput());
    outputStream() << "\n";
  }
}

void TextLayoutPrinter::printFragInfo(Fragment *Frag, LayoutFragmentInfo *Info,
                                      ELFSection *Section, Module &M) const {

  bool Onlylayout =
      m_LayoutPrinter->showOnlyLayout() || M.isBeforeLayoutState();
  std::string Type =
      ELFSection::getELFTypeStr(Info->section()->name(), Info->type()).str();
  std::string Permissions = ELFSection::getELFPermissionsStr(Info->flag());
  std::string Path;
  if (Info->section()->hasOldInputFile())
    Path = getDecoratedPath(Info->section()->getOldInputFile()->getInput());
  else if (Info->m_InputFile)
    Path = getDecoratedPath(Info->m_InputFile->getInput());

  bool GC = Frag->getOwningSection()->isIgnore();
  uint32_t Alignment = Frag->alignment();
  const GeneralOptions &options = m_LayoutPrinter->getConfig().options();
  auto PrintOneFragOrString = [&](uint32_t Size, uint64_t AddressOrOffset) {
    if (GC && !Onlylayout)
      outputStream() << "# ";
    outputStream() << Info->getDecoratedName(options);
    if (!GC && !Onlylayout) {
      outputStream() << "\t0x";
      outputStream().write_hex(AddressOrOffset);
    }
    if (GC && !Onlylayout)
      outputStream() << "\t<GC>";
    outputStream() << "\t0x";
    outputStream().write_hex(Size);
    outputStream() << "\t" << Path;
    outputStream() << "\t";
    outputStream() << "#";
    outputStream() << Type << "," << Permissions << "," << Alignment;
    if (Info->section()->hasOldInputFile())
      outputStream() << "," << Info->getResolvedPath();
    if (!Onlylayout) {
      printChangeOutputSectionInfo(Info->section());
      printChunkOps(M, Frag);
    }
  };

  uint64_t AddressOrOffset = -1;
  if (llvm::isa<MergeStringFragment>(Frag) && !M.isBeforeLayoutState()) {
    auto *Strings = llvm::cast<MergeStringFragment>(Frag);
    for (MergeableString *S : Strings->getStrings()) {
      if (S->Exclude)
        continue;
      if (!GC)
        AddressOrOffset =
            S->OutputOffset +
            (Section->isAlloc() ? Section->addr() : Section->offset());
      PrintOneFragOrString(S->size(), AddressOrOffset);
      /// if showStrings there is still extra content to be printed on this line
      if (!m_LayoutPrinter->showStrings())
        outputStream() << "\n";
      printMergeString(S, M);
    }
  } else {
    if (!GC)
      AddressOrOffset =
          Frag->getOffset() +
          (Section->isAlloc() ? Section->addr() : Section->offset());
    PrintOneFragOrString(Frag->size(), AddressOrOffset);
    outputStream() << "\n";
  }
}

void TextLayoutPrinter::printChangeOutputSectionInfo(
    const ELFSection *S) const {
  auto Ops = m_LayoutPrinter->getSectionOps().find(S);
  if (Ops == m_LayoutPrinter->getSectionOps().end())
    return;
  outputStream() << " [Plugin : ";
  for (auto &Op : Ops->second) {
    std::string Annotation = Op->getAnnotation();
    if (!Annotation.empty())
      Annotation = ", Comment : " + Annotation;
    outputStream() << "{" << Op->getPluginOpStr() << "," << Op->getPluginName()
                   << Annotation << "}";
  }
  outputStream() << "]";
}

void TextLayoutPrinter::printChunkOps(eld::Module &M, Fragment *F) const {
  auto Ops = m_LayoutPrinter->getChunkOps().find(F);
  if (Ops == m_LayoutPrinter->getChunkOps().end())
    return;
  outputStream() << " [Plugin : ";
  for (auto &Op : Ops->second) {
    std::string Annotation = Op->getAnnotation();
    if (!Annotation.empty())
      Annotation = ", Comment : " + Annotation;
    outputStream() << "{" << Op->getPluginOpStr() << "," << Op->getPluginName()
                   << Annotation << "}";
  }
  outputStream() << "]";
}

// Print fragment information for each section.
void TextLayoutPrinter::printOnlyLayoutFrag(eld::Module &pModule,
                                            ELFSection *section, Fragment *frag,
                                            bool useColor) {
  if (!frag->size())
    return;
  DiagnosticEngine *DiagEngine = pModule.getConfig().getDiagEngine();
  if (frag->paddingSize() > 0) {
    std::optional<uint64_t> paddingValue =
        pModule.getFragmentPaddingValue(frag);
    printOnlyLayoutPadding(
        section, frag->getOffset(DiagEngine) - frag->paddingSize(),
        frag->paddingSize(), paddingValue ? *paddingValue : 0, true, useColor);
  }
  if (useColor)
    outputStream().changeColor(llvm::raw_ostream::YELLOW);

  LayoutPrinter::FragmentInfoMapIterT fragmentInfoIter =
      m_LayoutPrinter->getFragmentInfoMap().find(frag);
  if (fragmentInfoIter == m_LayoutPrinter->getFragmentInfoMap().end())
    return;
  LayoutFragmentInfo *fragmentInfo = fragmentInfoIter->second;
  printFragInfo(frag, fragmentInfo, section, pModule);
  LayoutPrinter::SymVectorTIterT syms,
      end_symbols = fragmentInfo->m_symbols.end();

  m_LayoutPrinter->sortFragmentSymbols(fragmentInfo);

  for (syms = fragmentInfo->m_symbols.begin(); syms != end_symbols; ++syms) {
    // Handle weak symbols.
    std::string resolvedPath =
        getDecoratedPath((*syms)->resolveInfo()->resolvedOrigin()->getInput());

    InputFile *resolvedOrigin = (*syms)->resolveInfo()->resolvedOrigin();
    bool isBitcode = false;

    if (resolvedOrigin->isBitcode())
      isBitcode = true;
    else if (getDecoratedPath(resolvedOrigin->getInput()) !=
             fragmentInfo->getResolvedPath())
      continue;

    if ((*syms)->name() == fragmentInfo->name())
      continue;

    outputStream() << "\t"
                   << showDecoratedSymbolName(pModule, (*syms)->resolveInfo());
    if (isBitcode) {
      outputStream() << "\t"
                     << "#(Bitcode origin:"
                     << getDecoratedPath(resolvedOrigin->getInput()) << ")";
    }
    outputStream() << "\n";
  }

  // Dump any information that can be used for debugging.
  frag->dump(outputStream());

  if (useColor)
    outputStream().resetColor();
}

/// FIXME: Lots of unnecessary code duplication with this function and
/// printOnlyLayoutFrag()
void TextLayoutPrinter::printFrag(eld::Module &pModule, ELFSection *section,
                                  Fragment *frag, bool useColor) {
  if (m_LayoutPrinter->showOnlyLayout() ||
      pModule.getState() == plugin::LinkerWrapper::State::BeforeLayout) {
    printOnlyLayoutFrag(pModule, section, frag, useColor);
    return;
  }

  DiagnosticEngine *DiagEngine = pModule.getConfig().getDiagEngine();

  if (frag->paddingSize() > 0) {
    std::optional<uint64_t> paddingValue =
        pModule.getFragmentPaddingValue(frag);
    printPadding(section, frag->getOffset(DiagEngine) - frag->paddingSize(),
                 frag->paddingSize(), paddingValue ? *paddingValue : 0, true,
                 useColor);
  }

  if (useColor)
    outputStream().changeColor(llvm::raw_ostream::YELLOW);

  LayoutPrinter::FragmentInfoMapIterT fragmentInfoIter =
      m_LayoutPrinter->getFragmentInfoMap().find(frag);
  if (fragmentInfoIter == m_LayoutPrinter->getFragmentInfoMap().end())
    return;

  LayoutFragmentInfo *fragmentInfo = fragmentInfoIter->second;
  bool isGC = frag->getOwningSection()->isIgnore();
  printFragInfo(frag, fragmentInfo, section, pModule);
  LayoutPrinter::SymVectorTIterT syms,
      end_symbols = fragmentInfo->m_symbols.end();

  m_LayoutPrinter->sortFragmentSymbols(fragmentInfo);

  const LayoutPrinter::RemoveSymbolOpsMapT RemovedSymbols =
      m_LayoutPrinter->getRemovedSymbols();

  for (syms = fragmentInfo->m_symbols.begin(); syms != end_symbols; ++syms) {
    // Handle weak symbols.
    std::string resolvedPath =
        getDecoratedPath((*syms)->resolveInfo()->resolvedOrigin()->getInput());

    InputFile *resolvedOrigin = (*syms)->resolveInfo()->resolvedOrigin();
    bool isBitcode = false;

    if (resolvedOrigin->isBitcode())
      isBitcode = true;
    else if (resolvedOrigin->getInput()->decoratedPath(
                 m_LayoutPrinter->showAbsolutePath()) !=
             fragmentInfo->getResolvedPath())
      continue;

    if ((*syms)->name() == fragmentInfo->name())
      continue;

    if ((*syms)->name() == fragmentInfo->getRealName())
      continue;

    auto Removed = RemovedSymbols.find((*syms)->resolveInfo());

    if (!isGC) {
      outputStream() << "\t0x";
      outputStream().write_hex(
          m_LayoutPrinter->calculateSymbolValue(*syms, pModule));
      outputStream() << "\t";
    } else {
      outputStream() << "#";
    }
    outputStream() << "\t"
                   << showDecoratedSymbolName(pModule, (*syms)->resolveInfo());
    if (Removed != RemovedSymbols.end())
      outputStream() << " {" << Removed->getSecond()->getPluginOpStr() << ", "
                     << Removed->getSecond()->getPluginName() << "}";
    if (isBitcode) {
      outputStream() << "\t"
                     << "#(Bitcode origin:"
                     << getDecoratedPath(resolvedOrigin->getInput()) << ")";
    }
    outputStream() << "\n";
  }

  // Dump any information that can be used for debugging.
  frag->dump(outputStream());

  if (useColor)
    outputStream().resetColor();
}

// Write included Archive members in Map file.
void TextLayoutPrinter::printArchiveRecords(Module &module, bool useColor) {
  if (useColor)
    outputStream().changeColor(llvm::raw_ostream::CYAN);

  if (m_LayoutPrinter->getArchiveRecords().size()) {
    auto it = m_LayoutPrinter->getArchiveRecords().begin(),
         ie = m_LayoutPrinter->getArchiveRecords().end();
    outputStream() << "Archive member included because of file (symbol)\n";
    for (; it != ie; it++) {
      auto ret = m_LayoutPrinter->getArchiveRecord(*it);
      outputStream() << ret.first << "\n\t\t" << ret.second << "\n";
    }
  }
  if (!module.getArchiveLibraryList().size())
    return;
  outputStream() << "\nCount for Loaded archive member/Total archive members\n";
  for (auto item : module.getArchiveLibraryList()) {
    ArchiveFile *file = llvm::dyn_cast_or_null<ArchiveFile>(item);
    if (!file)
      continue;
    // FIXME: Here, we are ignoring MapDetail options show-absolute-path and
    // show-relative-path=...
    outputStream() << file->getInput()->decoratedPath() << " "
                   << file->getLoadedMemberCount() << "/"
                   << file->getTotalMemberCount() << "\n";
  }
  if (useColor)
    outputStream().resetColor();
}

std::string
TextLayoutPrinter::showDecoratedSymbolName(eld::Module &pModule,
                                           const ResolveInfo *R) const {
  return pModule.getNamePool().getDecoratedName(
      R, m_LayoutPrinter->getConfig().options().shouldDemangle());
}

// If common symbols are present, write them in Map file
// along with size and resolved location.
void TextLayoutPrinter::printCommons(eld::Module &pModule, bool useColor) {
  if (useColor)
    outputStream().changeColor(llvm::raw_ostream::MAGENTA);

  auto commonSyms = m_LayoutPrinter->getAllocatedCommonSymbols(pModule);

  if (commonSyms.empty())
    return;

  outputStream()
      << "\nAllocating common symbols\nCommon symbol\tsize\tfile\n\n";

  for (auto sym : commonSyms) {
    outputStream() << showDecoratedSymbolName(pModule, sym) << "\t0x";
    outputStream().write_hex(sym->size());
    InputFile *input = sym->resolvedOrigin();
    if (input)
      outputStream() << "\t" << getDecoratedPath(input->getInput());
    outputStream() << "\n";
  }
  if (useColor)
    outputStream().resetColor();
}

// If exclude list is present, add it to map file along with the symbol list.
void TextLayoutPrinter::printExternList(Module &pModule, bool useColor) {
  if (!pModule.getScript().hasExternCommand())
    return;
  const std::vector<ScriptCommand *> &scriptCommands =
      pModule.getScript().getScriptCommands();
  if (useColor)
    outputStream().changeColor(llvm::raw_ostream::MAGENTA);
  outputStream() << "\nExtern list files\n";
  std::string currFile = "";
  std::function<std::string(const Input *)> getDecoratedPath =
      [=](const Input *I) { return this->getDecoratedPath(I); };
  for (ScriptCommand *cmd : scriptCommands) {
    if (!cmd->isExtern())
      continue;
    if (currFile != cmd->getContext()) {
      currFile = cmd->getContext();
      outputStream() << currFile << "\n";
    }
    for (const SymbolContainer *symContainer :
         llvm::cast<ExternCmd>(cmd)->getSymbolContainers()) {
      outputStream() << "Pattern: "
                     << symContainer->getWildcardPatternAsString() << "\n";

      symContainer->dump(outputStream(), getDecoratedPath);
    }
  }
  if (useColor)
    outputStream().resetColor();
}

// If dynamic list is present, add it to map file along with the symbol list.
void TextLayoutPrinter::printDynamicList(Module &pModule, bool useColor) {
  const llvm::DenseMap<InputFile *, Module::ScriptSymbolList>
      &dynamicListFileToScriptSymbolsMap =
          pModule.getDynamicListFileToScriptSymbolsMap();
  if (!dynamicListFileToScriptSymbolsMap.size())
    return;
  if (useColor)
    outputStream().changeColor(llvm::raw_ostream::MAGENTA);
  outputStream() << "\nDynamic list files\n";
  std::function<std::string(const Input *)> getDecoratedPath =
      [=](const Input *I) { return this->getDecoratedPath(I); };
  for (auto item : dynamicListFileToScriptSymbolsMap) {
    if (!item.second.size())
      continue;
    outputStream() << getDecoratedPath(item.first->getInput());
    for (const ScriptSymbol *sym : item.second) {
      outputStream() << "\nPattern: "
                     << sym->getSymbolContainer()->getWildcardPatternAsString()
                     << "\n";
      sym->getSymbolContainer()->dump(outputStream(), getDecoratedPath);
    }
  }
  if (useColor)
    outputStream().resetColor();
}

// If version list is present, add it to map file along with the symbol list.
void TextLayoutPrinter::printVersionList(Module &pModule, bool useColor) {
  auto &versionScripts = pModule.getVersionScripts();
  if (!versionScripts.size())
    return;
  outputStream() << "\nVersion Script Information\n";
  if (useColor)
    outputStream().changeColor(llvm::raw_ostream::MAGENTA);
  std::function<std::string(const Input *)> getDecoratedPath =
      [=](const Input *I) { return this->getDecoratedPath(I); };
  for (auto item : versionScripts)
    item->dump(outputStream(), getDecoratedPath);
  if (useColor)
    outputStream().resetColor();
}

// Write the Linker scripts used to the Map file.
void TextLayoutPrinter::printScriptIncludes(bool useColor) {
  if (useColor)
    outputStream().changeColor(llvm::raw_ostream::GREEN);
  if (m_LayoutPrinter->getLinkerScripts().size())
    outputStream() << "\nLinker scripts used (including INCLUDE command)\n";
  for (auto &Script : m_LayoutPrinter->getLinkerScripts()) {
    std::string Indent = "";
    for (uint32_t i = 0; i < Script.Depth; ++i)
      Indent += "\t";
    std::string LinkerScriptName = Script.include;
    if (m_LayoutPrinter->getConfig().options().hasMappingFile())
      LinkerScriptName = m_LayoutPrinter->getPath(LinkerScriptName) + "(" +
                         LinkerScriptName + ")";
    if (!Script.found)
      LinkerScriptName += "(NOTFOUND)";
    outputStream() << Indent << LinkerScriptName;
    outputStream() << "\n";
  }
  if (useColor)
    outputStream().resetColor();
}

void TextLayoutPrinter::printVersionScripts(bool useColor) {
  if (useColor)
    outputStream().changeColor(llvm::raw_ostream::GREEN);
  if (m_LayoutPrinter->getVersionScripts().size())
    outputStream() << "\nVersion scripts used\n";
  for (auto &Script : m_LayoutPrinter->getVersionScripts()) {
    std::string VersionScriptName = Script;
    if (m_LayoutPrinter->getConfig().options().hasMappingFile())
      VersionScriptName = m_LayoutPrinter->getPath(VersionScriptName) + "(" +
                          VersionScriptName + ")";
    outputStream() << VersionScriptName;
    outputStream() << "\n";
  }
  if (useColor)
    outputStream().resetColor();
}

void TextLayoutPrinter::printLinkerInsertedTimingStats(Module &pModule) {
  TimingFragment *F = pModule.getBackend()->getTimingFragment();
  const TimingSlice *T = F->getTimingSlice();
  outputStream() << T->getModuleName() << " " << T->getDate() << " "
                 << T->getDurationSeconds() << "\n";
}

void TextLayoutPrinter::printBuildStatistics(Module &pModule, bool useColor) {
  if (useColor)
    outputStream().changeColor(llvm::raw_ostream::MAGENTA);
  outputStream() << "\nBuild Statistics";
  outputStream() << "\n# <file> <start time> <duration>\n";
  for (auto &I : m_LayoutPrinter->getInputActions()) {
    if (I.input == nullptr || I.input->getInputFile() == nullptr)
      continue;
    if (!I.input->getInputFile()->isObjectFile())
      continue;
    ELFObjectFile *EObj =
        llvm::dyn_cast<ELFObjectFile>(I.input->getInputFile());
    if (EObj->getTimingSection()) {
      for (const Fragment *TF : EObj->getTimingSection()->getFragmentList()) {
        const TimingSlice *TS =
            llvm::dyn_cast<TimingFragment>(TF)->getTimingSlice();
        outputStream() << TS->getModuleName() << " " << TS->getDate() << " "
                       << TS->getDurationSeconds() << "\n";
      }
    }
  }
  if (m_LayoutPrinter->getConfig().options().getInsertTimingStats()) {
    printLinkerInsertedTimingStats(pModule);
  }
  outputStream() << "\n";
  outputStream().resetColor();
}

// Print the loaded Linker scripts and Memory Map files.
void TextLayoutPrinter::printInputActions(bool useColor) {
  if (useColor)
    outputStream().changeColor(llvm::raw_ostream::BLUE);
  outputStream() << "\nLinker Script and memory map\n";
  for (auto &I : m_LayoutPrinter->getInputActions())
    outputStream() << m_LayoutPrinter->getStringFromLoadSequence(I) << "\n";
  if (useColor)
    outputStream().resetColor();
}

std::string TextLayoutPrinter::commandLineWithMappings() {
  std::string original = "";
  for (auto arg : m_LayoutPrinter->getConfig().options().Args())
    if (arg) {
      original.append(
          m_LayoutPrinter->getConfig().getFileFromHash(std::string(arg)));
      original.append(" ");
    }
  return original;
}

// Print Map file header information like Linker architecture, version, etc.
void TextLayoutPrinter::printArchAndVersion(bool useColor,
                                            GNULDBackend const &backend) {
  if (!eld::getVendorName().empty())
    outputStream() << "# Linker from " << eld::getVendorName() << " Version "
                   << eld::getVendorVersion() << "\n";
  outputStream() << "# Linker based on LLVM version: " << eld::getELDVersion()
                 << "\n";
  outputStream() << "# Linker: " << getELDRevision() << "\n";
  outputStream() << "# LLVM: " << getLLVMRevision() << "\n";
  this->m_LayoutPrinter->getConfig().printOptions(outputStream(), backend,
                                                  useColor);

  // Print the command line in the Map file.
  std::string CommandLine;
  std::string Separator = "";
  for (auto arg : m_LayoutPrinter->getConfig().options().Args()) {
    CommandLine.append(Separator);
    Separator = " ";
    if (arg)
      CommandLine.append(std::string(arg));
  }
  outputStream() << "# CommandLine : " << CommandLine;
  outputStream() << "\n";
  if (m_LayoutPrinter->getConfig().options().hasMappingFile()) {
    outputStream() << "# Original CommandLine : " << commandLineWithMappings();
    outputStream() << "\n";
  }
}

void TextLayoutPrinter::printMemoryCommand(const MemoryCmd *M) {
  std::string str;
  {
    llvm::raw_string_ostream OS(str);
    M->dumpOnlyThis(OS);
    outputStream() << "# " << OS.str();
    outputStream() << "\n";
    outputStream() << "#{";
    outputStream() << "\n";
  }
  for (auto &MemSpec : M->getMemoryDescriptors()) {
    llvm::raw_string_ostream OS(str);
    str.clear();
    MemSpec->dump(OS);
    outputStream() << "#"
                   << "\t" << OS.str();
  }
  outputStream() << "#}";
  outputStream() << "\n";
}

void TextLayoutPrinter::printScriptCommands(const LinkerScript &script) {
  if (script.getMemoryCommand())
    printMemoryCommand(script.getMemoryCommand());
}

// Start adding mapping information to map file
// starting with Map file Header information, e.g. Architechture, Version,
// etc. If use of color for text is enabled, print text with a foreground
// color. Reset the colors to terminal defaults after writing.
void TextLayoutPrinter::printMapFile(eld::Module &module) {
  m_LayoutPrinter->buildMergedStringMap(module);
  GNULDBackend &backend = *module.getBackend();
  bool useColor = backend.config().options().color() &&
                  backend.config().options().colorMap();
  LinkerScript const &script = module.getScript();

  printArchAndVersion(useColor, backend);
  printStats(m_LayoutPrinter->getLinkStats(), module);
  printIsFileHeaderLoadedInfo(backend.isFileHeaderLoaded(), useColor);
  printIsPHDRSLoadedInfo(backend.isPHDRSLoaded(), useColor);
  // Print image start address.
  if (backend.hasImageStartVMA()) {
    outputStream() << "# Image start address: "
                   << "~ 0x";
    outputStream().write_hex(backend.getImageStartVMA());
    outputStream() << "\n";
  }

  if (m_LayoutPrinter->showRelativePath())
    outputStream() << "# Basepath: " << m_LayoutPrinter->getBasepath().value()
                   << "\n";

  printArchiveRecords(module, useColor);
  printExternList(module, useColor);
  printDynamicList(module, useColor);
  printVersionList(module, useColor);
  printCommons(module, useColor);
  printInputActions(useColor);
  printBuildStatistics(module, useColor);
  printScriptIncludes(useColor);
  printVersionScripts(useColor);
  printGlobalPluginInfo(module, useColor);

  if (backend.getEntrySymbol())
    outputStream() << "\n"
                   << "Entry point: " << backend.getEntrySymbol()->name()
                   << "\n";

  outputStream() << "\n";
  outputStream() << "# Output Section and Layout "
                 << "\n";

  printScriptCommands(script);

  for (const auto &x : (script.getScriptCommands())) {
    if (x->getKind() == ScriptCommand::ASSIGNMENT)
      x->dumpMap(outputStream(), useColor, true,
                 !m_LayoutPrinter->showOnlyLayout());
  }

  printLayout(module);

  if (!m_LayoutPrinter->showOnlyLayout())
    printPluginInfo(module);

  if (m_LayoutPrinter->showSymbolResolution())
    printSymbolResolution(module);
}

void TextLayoutPrinter::printLayout(eld::Module &module) {
  // Get start and end points from section map to iterate over each section.
  SectionMap::const_iterator out, outBegin, outEnd;
  LinkerScript const &script = module.getScript();
  GNULDBackend &backend = *module.getBackend();
  bool useColor = backend.config().options().color() &&
                  backend.config().options().colorMap();

  bool linkerScriptHasSectionsCommand =
      module.getScript().linkerScriptHasSectionsCommand();

  outBegin = script.sectionMap().begin();
  outEnd = script.sectionMap().end();
  if (module.isBeforeLayoutState())
    outputStream() << "Initial layout:\n";
  for (out = outBegin; out != outEnd; ++out) {

    ELFSection *cur = (*out)->getSection();
    if ((m_LayoutPrinter->showHeaderDetails() &&
         (cur->name() == "__ehdr__" || cur->name() == "__pHdr__")) ||
        (!cur->isNullType() &&
         (linkerScriptHasSectionsCommand || cur->isWanted() || cur->size() ||
          (*out)->size())) ||
        (module.isBeforeLayoutState() && !(*out)->name().empty()))
      printSection(backend, *out, useColor);

    // print padding from output section start to first frag
    for (const GNULDBackend::PaddingT &p : backend.getPaddingBetweenFragments(
             cur, nullptr, (*out)->getFirstFrag())) {

      printPadding(cur, p.startOffset, p.endOffset - p.startOffset,
                   p.Exp ? p.Exp->result() : 0, false, useColor);
    }
    for (OutputSectionEntry::iterator in = (*out)->begin(),
                                      inEnd = (*out)->end();
         in != inEnd; ++in) {
      ELFSection *IS = (*in)->getSection();
      // Evaluate all assignments at the beginning of input section.
      for (RuleContainer::sym_iterator it = (*in)->sym_begin(),
                                       ie = (*in)->sym_end();
           it != ie; ++it) {
        if (!m_LayoutPrinter->showOnlyLayout()) {
          (*it)->dumpMap(outputStream(), useColor, false,
                         /*withValues=*/!module.isBeforeLayoutState());
          // Show the actual expression as present in the linker script
          outputStream() << " # ";
        }
        // Show expression in the linker script.
        (*it)->dumpMap(outputStream(), useColor, true /* NewLine */,
                       false /* No Values */,
                       m_LayoutPrinter->showOnlyLayout() /* Indent */);
      }
      if ((*in)->desc()) {
        (*in)->desc()->dumpMap(outputStream(), useColor, false);
        if (!m_LayoutPrinter->dontShowTiming() &&
            !llvm::isa<OutputSectData>((*in)->desc()))
          (*in)->dumpMap(outputStream());
        outputStream() << "\n";
      }

      // Print garbage collected sections.
      // We do not print garbage-collected sections in BeforeLayout state
      // because in before layout state garbage-collection may or may not
      // have happened and we are printing all the sections(fragments) later on
      // anyways. If we print ignored sections here too, then we will be
      // printing many sections two times.
      if (!module.isBeforeLayoutState()) {
        for (auto S : (*in)->getMatchedInputSections()) {
          if (!S->isIgnore())
            continue;
          for (auto F : S->getFragmentList())
            printFrag(module, cur, F, useColor);
        }
      }
      if ((!IS || !IS->getFragmentList().size()) &&
          !module.isBeforeLayoutState())
        continue;
      // Do not print fragments of OutputSectData commands.
      // Fragments and sections of OutputSectData is an internal
      // implementation and may confuse the users.
      if ((*in)->desc() && llvm::isa<OutputSectData>((*in)->desc()))
        continue;
      printFragments(module, *cur, **in, useColor);
      // print padding between current rule and next rule with content
      const RuleContainer *NextRuleWithContent =
          (*in)->getNextRuleWithContent();

      if ((*in)->hasContent()) {
        for (const GNULDBackend::PaddingT &p :
             backend.getPaddingBetweenFragments(
                 cur, (*in)->getLastFrag(),
                 NextRuleWithContent ? NextRuleWithContent->getFirstFrag()
                                     : nullptr)) {

          printPadding(cur, p.startOffset, p.endOffset - p.startOffset,
                       p.Exp ? p.Exp->result() : 0, false, useColor);
        }
      }
    }

    // Evaluate all assignments at the end of the output section.
    for (OutputSectionEntry::sym_iterator it = (*out)->sectionendsym_begin(),
                                          ie = (*out)->sectionendsym_end();
         it != ie; ++it) {
      if (!m_LayoutPrinter->showOnlyLayout()) {
        (*it)->dumpMap(outputStream(), useColor, false,
                       /*withValues=*/!module.isBeforeLayoutState());
        // Show the actual expression as present in the linker script
        outputStream() << " # ";
      }
      // Show expression in the linker script.
      (*it)->dumpMap(outputStream(), useColor, true /* NewLine */,
                     false /* No Values */,
                     m_LayoutPrinter->showOnlyLayout() /* Indent */);
    }
  }
}

TextLayoutPrinter::~TextLayoutPrinter() {
  if (Buffer)
    (*layoutFile) << Buffer->str();
}

void TextLayoutPrinter::destroy() {}

void TextLayoutPrinter::clearInputRecords() {
  m_LayoutPrinter->resetArchiveRecords();
  m_LayoutPrinter->resetInputActions();
}

void TextLayoutPrinter::printIsFileHeaderLoadedInfo(bool isLoaded,
                                                    bool useColor) {
  if (useColor)
    outputStream().changeColor(llvm::raw_ostream::GREEN);

  outputStream() << "# Is file header loaded: " << (isLoaded ? "true" : "false")
                 << "\n";

  if (useColor)
    outputStream().resetColor();
}

void TextLayoutPrinter::printIsPHDRSLoadedInfo(bool isLoaded, bool useColor) {
  if (useColor)
    outputStream().changeColor(llvm::raw_ostream::GREEN);

  outputStream() << "# Is PHDRS loaded: " << (isLoaded ? "true" : "false")
                 << "\n";

  if (useColor)
    outputStream().resetColor();
}

void TextLayoutPrinter::printTotalSymbolStats(const Module &module) {
  const ObjectLinker::SymbolStats &symStats =
      module.getLinker()->getObjectLinker()->getTotalSymbolStats();
  outputStream() << "# Total Symbols: "
                 << symStats.global + symStats.local + symStats.weak +
                        symStats.absolute - symStats.file
                 << " "
                 << "{ "
                 << "Global: " << symStats.global << ", "
                 << "Local: " << symStats.local << ", "
                 << "Weak: " << symStats.weak << ", "
                 << "Absolute: " << symStats.absolute << ", "
                 << "Hidden: " << symStats.hidden << ", "
                 << "Protected: " << symStats.protectedSyms << " "
                 << "}\n";
}

void TextLayoutPrinter::printDiscardedSymbolStats(const Module &module) {
  const ObjectLinker::SymbolStats &symStats =
      module.getLinker()->getObjectLinker()->getDiscardedSymbolStats();
  outputStream() << "# Discarded Symbols: "
                 << symStats.global + symStats.local + symStats.weak +
                        symStats.absolute - symStats.file
                 << " "
                 << "{ "
                 << "Global: " << symStats.global << ", "
                 << "Local: " << symStats.local << ", "
                 << "Weak: " << symStats.weak << ", "
                 << "Absolute: " << symStats.absolute << ", "
                 << "Hidden: " << symStats.hidden << ", "
                 << "Protected: " << symStats.protectedSyms << " "
                 << "}\n";
}

std::string TextLayoutPrinter::getDecoratedPath(const Input *I) const {
  std::optional<std::string> basepath = m_LayoutPrinter->getBasepath();
  if (basepath.has_value())
    return I->getDecoratedRelativePath(basepath.value());
  return I->decoratedPath(m_LayoutPrinter->showAbsolutePath());
}

void TextLayoutPrinter::printFragments(Module &module, ELFSection &outSect,
                                       RuleContainer &R, bool useColor) {
  if (module.isBeforeLayoutState()) {
    for (auto S : R.getMatchedInputSections()) {
      for (auto F : S->getFragmentList())
        printFrag(module, &outSect, F, useColor);
    }
  } else {
    for (auto &F : R.getSection()->getFragmentList())
      printFrag(module, &outSect, F, useColor);
  }
}

void TextLayoutPrinter::printSymbolResolution(Module &module) {
  NamePool &NP = module.getNamePool();
  SymbolResolutionInfo &SRI = NP.getSRI();
  const auto &symbols = module.getSymbols();
  const GeneralOptions &options = m_LayoutPrinter->getConfig().options();
  SRI.setupCandidatesInfo(NP, module.getScript());

  outputStream() << "# Symbol Resolution: "
                 << "\n";

  size_t index = 0;
  for (const auto *RI : symbols) {
    if (RI->isLocal() &&
        RI->resolvedOrigin() != module.getInternalInput(Module::Plugin))
      continue;
    ++index;
    llvm::StringRef symName = RI->getName();
    const SymbolResolutionInfo::CandidatesType candidates =
        SRI.getCandidates(symName);
    outputStream() << index << ") " << symName << "\n";
    for (const auto &candidate : candidates) {
      std::optional<SymbolInfo> optSymbolInfo = SRI.getSymbolInfo(candidate);
      ASSERT(optSymbolInfo, "Symbol info must be present!");
      SymbolInfo candidateInfo = optSymbolInfo.value();

      std::string candidateInfoAsString =
          SRI.getSymbolInfoAsString(candidate, options);
      outputStream() << "\t" << candidateInfoAsString;
      if (candidate->resolveInfo()->outSymbol() == candidate ||
          (candidateInfo.isBitcodeSymbol() &&
           candidateInfo.getInputFile() ==
               candidate->resolveInfo()->resolvedOrigin()))
        outputStream() << " [Selected]";
      if (candidateInfo.isBitcodeSymbol()) {
        if (const LDSymbol *LTOSym =
                SRI.getCorrespondingLTOObjectSymIfAny(candidate)) {
          outputStream() << "\n\t  "
                         << SRI.getSymbolInfoAsString(LTOSym, options);
        }
      }
      outputStream() << "\n";
    }
  }
}
