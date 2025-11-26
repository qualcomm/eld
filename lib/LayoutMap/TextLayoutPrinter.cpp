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
// FIXME: It might be better to make m_LayoutInfo
// as a const reference, as then there would be no
// risk of nullptr dereference when accessing
// m_LayoutInfo.
TextLayoutPrinter::TextLayoutPrinter(LayoutInfo *layoutInfo)
    : LayoutFile(nullptr), ThisLayoutInfo(layoutInfo) {}

eld::Expected<void> TextLayoutPrinter::init() {
  if (ThisLayoutInfo->getConfig().options().printMap())
    return {};
  std::string SuffixExtension = "";
  if (!ThisLayoutInfo->getConfig().options().isDefaultMapStyleText())
    SuffixExtension = ".txt";
  std::error_code Error;
  // Check if a map file was provided in Linker arguments and open it for
  // writing.
  if (!ThisLayoutInfo->getConfig().options().layoutFile().empty()) {
    std::string LayoutFileName =
        ThisLayoutInfo->getConfig().options().layoutFile();
    if (!SuffixExtension.empty())
      LayoutFileName = ThisLayoutInfo->getConfig().options().layoutFile() +
                       SuffixExtension;
    LayoutFile = std::make_unique<llvm::raw_fd_ostream>(LayoutFileName, Error,
                                                        llvm::sys::fs::OF_None);
    Buffer = std::make_unique<llvm::raw_string_ostream>(Storage);
    // Check if a map file was provided in Linker arguments and an error
    // occured, then write error to standard error and exit with code 1.
    if (Error) {
      return std::make_unique<plugin::DiagnosticEntry>(
          plugin::FatalDiagnosticEntry(
              Diag::fatal_unwritable_output,
              {ThisLayoutInfo->getConfig().options().layoutFile(),
               Error.message()}));
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

void TextLayoutPrinter::printOnlyLayoutSection(GNULDBackend const &Backend,
                                               const OutputSectionEntry *OS,
                                               bool UseColor) {
  const ELFSection *Section = OS->getSection();
  if (UseColor)
    outputStream().changeColor(llvm::raw_ostream::GREEN);
  outputStream() << "\n";
  outputStream() << Section->name() << "\t0x";
  outputStream().write_hex(Section->size());
  outputStream() << "\t# Alignment: 0x";
  outputStream().write_hex(Section->getAddrAlign());
  if (Section->isAlloc())
    printSegments(Backend, OS);
  outputStream() << "\n";

  if (UseColor)
    outputStream().resetColor();

  auto Info = ThisLayoutInfo->getPluginInfo().find(Section);
  if (Info == ThisLayoutInfo->getPluginInfo().end())
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
                                  const std::string &Stat) const {
  if (Stat.empty())
    return;
  outputStream() << "# " << S << " : " << Stat << "\n";
}

void TextLayoutPrinter::printPluginStats(
    const std::vector<std::pair<std::string, std::string>> &Stats,
    Plugin *P) const {
  for (auto &it : Stats) {
    outputStream() << "# " << it.first << ": " << it.second << " "
                   << P->getPluginName();
    outputStream() << "\n";
  }
}

void TextLayoutPrinter::printStats(LayoutInfo::Stats &L,
                                   const Module &Module) {
  const ObjectLinker &ObjLinker = *(Module.getLinker()->getObjectLinker());
  const GNULDBackend &Backend = Module.getBackend();
  if (!L.hasStats())
    return;
  outputStream() << "# "
                 << "LinkStats Begin"
                 << "\n";
  printStat("ObjectFiles", L.NumElfObjectFiles);
  printStat("ExecutableFiles", L.NumElfExecutableFiles);
  printStat("LinkerScripts", L.NumLinkerScripts);
  printStat("SharedObjects", L.NumSharedObjectFiles);
  printStat("SymDefFiles", L.NumSymDefFiles);
  printStat("ThreadCount", L.NumThreads);
  printStat("ArchiveFiles", L.NumArchiveFiles);
  printStat("GroupTraversals", L.NumGroupTraversal);
  printStat("BitcodeFiles", L.NumBitCodeFiles);
  printStat("NumInputSections", ObjLinker.getAllInputSections().size());
  printStat("ZeroSizedSections", L.NumZeroSizedSection);
  printStat("SectionsGarbageCollected", L.NumSectionsGarbageCollected);
  printStat("ZeroSizedSectionsGarbageCollected",
            L.NumZeroSizedSectionsGarbageCollected);
  printStat("RetainedSections", L.NumRetainedSections);
  printTotalSymbolStats(Module);
  printDiscardedSymbolStats(Module);
  printStat("NumLinkerScriptRules", L.NumLinkerScriptRules);
  printStat("NumOutputSections", L.NumOutputSections);
  printStat("NumGOTEntries", Backend.GOTEntriesCount());
  printStat("NumPLTEntries", Backend.PLTEntriesCount());
  printStat("NumPlugins", L.NumPlugins);
  printStat("NumOrphans", L.NumOrphans);
  printStat("NumTrampolines", L.NumTrampolines);
  printStat("NoRuleMatches", L.NumNoRuleMatch);
  printStat("NumNOLOADSections", Backend.NOLOADSectionsCount());
  printStat("NumSegments", Backend.elfSegmentTable().size());
  if (L.OutputFileSize)
    printStat("OutputFileSize",
              std::to_string(L.OutputFileSize.value()) + " bytes");

  printStat("LinkTime", L.LinkTime);
  for (auto &W : ThisLayoutInfo->getPlugins()) {
    Plugin *P = W->getPlugin();
    printPluginStats(P->getPluginLinkStats(), P);
  }
  ThisLayoutInfo->printStats((void *)(&Module), outputStream());
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

  std::string VMARegionName = OS->epilog().region().getDecoratedName();
  std::optional<std::string> LMARegionName;
  // If LMA is set on the output section those sections are not
  // added to the default LMA region which is nothing but the
  // alias VMA region
  if (!OS->prolog().hasLMA()) {
    if (OS->epilog().hasLMARegion())
      LMARegionName = OS->epilog().lmaRegion().getDecoratedName();
    else
      LMARegionName = VMARegionName;
  }
  outputStream() << ", Memory : [" << VMARegionName;
  if (LMARegionName)
    outputStream() << ", " << LMARegionName;
  outputStream() << "]";
}

std::optional<std::string>
TextLayoutPrinter::getPluginSectionAnnotations(const ELFSection *S) const {
  if (!S || !ThisLayoutInfo)
    return {};
  llvm::SmallVector<std::string, 4> Annotations;
  for (auto &[_, Ops] : ThisLayoutInfo->getPluginOps()) {
    for (auto *Op : Ops) {
      auto *UR = llvm::dyn_cast_or_null<UpdateRulePluginOp>(Op);
      if (!UR || UR->getSection() != S)
        continue;
      Annotations.push_back(Op->getAnnotation());
    }
  }
  if (Annotations.empty())
    return {};
  return llvm::join(Annotations, ", ");
}

// Print section information like name, address, offset, size, alignment,
// etc. based on the section type.
void TextLayoutPrinter::printSection(GNULDBackend const &Backend,
                                     const OutputSectionEntry *OS,
                                     bool UseColor) {
  if (ThisLayoutInfo->showOnlyLayout()) {
    printOnlyLayoutSection(Backend, OS, UseColor);
    return;
  }
  const ELFSection *Section = OS->getSection();
  if (UseColor)
    outputStream().changeColor(llvm::raw_ostream::GREEN);
  outputStream() << "\n";
  outputStream() << Section->name();
  if (Backend.getModule().isLinkStateBeforeLayout()) {
    outputStream() << "\n";
    return;
  }
  outputStream() << "\t0x";
  if (Section->isAlloc())
    outputStream().write_hex(Section->addr());
  else
    outputStream().write_hex(Section->offset());

  outputStream() << "\t0x";
  outputStream().write_hex(Section->size());

  outputStream() << " # Offset: 0x";
  outputStream().write_hex(Section->offset());

  if (Section->isAlloc()) {
    outputStream() << ", LMA: 0x";
    outputStream().write_hex(Section->pAddr());
  }

  // Print alignment
  outputStream() << ", Alignment: 0x";
  outputStream().write_hex(Section->getAddrAlign());

  // Print flags
  outputStream() << ", Flags: "
                 << ELFSection::getELFPermissionsStr(Section->getFlags());

  outputStream() << ", Type: " << OS->getSectionTypeStr();

  if (Section->isAlloc()) {
    printSegments(Backend, OS);
    printMemoryRegions(Backend, OS);
  }

  outputStream() << "\n";

  if (UseColor)
    outputStream().resetColor();

  if (OS->getTotalTrampolineCount()) {
    outputStream() << "# NumTrampolines: " << OS->getTotalTrampolineCount();
    outputStream() << "\n";
  }

  ThisLayoutInfo->printStats((void *)(OS), outputStream());

  auto Info = ThisLayoutInfo->getPluginInfo().find(Section);
  if (Info == ThisLayoutInfo->getPluginInfo().end())
    return;
  for (auto &P : Info->second) {
    outputStream() << "# " << P->getName() << "\t" << P->getLibraryName()
                   << "\t" << P->getPluginType() << "\t"
                   << P->getPluginOptions() << "\n";
  }
}

// Print Linker Plugin information for all available Plugin types.
void TextLayoutPrinter::printGlobalPluginInfo(Module &M, bool UseColor) {
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
  const LinkerScript::PluginVectorT &UniversalPlugins =
      M.getPluginManager().getUniversalPlugins();

  if (UseColor)
    outputStream().changeColor(llvm::raw_ostream::GREEN);
  outputStream() << "Linker Plugin Information"
                 << "\n";
  if (UseColor)
    outputStream().resetColor();

  auto PrintPlugin = [this](const Plugin &P) {
    std::string features;
    if (P.isDefaultPlugin())
      features += "[autoloaded]";
    outputStream() << "\t" << "\t" << P.getName() << "\t"
                   << P.getLibraryName() << "\t" << P.getPluginType() << "\t"
                   << P.getPluginOptions() << features << "\n";
  };

  if (UniversalPlugins.size()) {
    outputStream() << "\t";
    outputStream() << "Linker Plugins" << "\n";
    for (auto &P : UniversalPlugins)
      PrintPlugin(*P);
  }
  if (PluginListForSectionMatcher.size()) {
    outputStream() << "\t";
    outputStream() << "SectionMatcher Plugins"
                   << "\n";
    for (auto &P : PluginListForSectionMatcher)
      PrintPlugin(*P);
  }
  if (PluginListForSectionIterator.size()) {
    outputStream() << "\t";
    outputStream() << "SectionIterator Plugins"
                   << "\n";
    for (auto &P : PluginListForSectionIterator)
      PrintPlugin(*P);
  }
  if (PluginListForOutputSectionIterator.size()) {
    outputStream() << "\t";
    outputStream() << "Output SectionIterator Plugins"
                   << "\n";
    for (auto &P : PluginListForOutputSectionIterator)
      PrintPlugin(*P);
  }
  if (PluginListForMemorySize.size()) {
    outputStream() << "\t";
    outputStream() << "Memory Size Plugin(s)"
                   << "\n";
    for (auto &P : PluginListForMemorySize)
      PrintPlugin(*P);
  }
  if (PluginListForFileSize.size()) {
    outputStream() << "\t";
    outputStream() << "File Size Plugin(s)"
                   << "\n";
    for (auto &P : PluginListForFileSize)
      PrintPlugin(*P);
  }
  outputStream() << "Linker Plugin Run Information"
                 << "\n";
  int Index = 0;
  for (auto &P : M.getScript().getPluginRunList()) {
    outputStream() << "#" << ++Index << "\t";
    PrintPlugin(*P);
  }
}

bool TextLayoutPrinter::printChangeOutputSectionPluginOpDetailed(
    PluginOp *Pop) {
  ChangeOutputSectionPluginOp *Op =
      llvm::dyn_cast<ChangeOutputSectionPluginOp>(Pop);
  if (!Op)
    return false;
  // If the Op does not have a modified rule then that implies the output
  // section was not changed due to some reason. For example, if an invalid
  // output section was provided to the change output section API.
  if (!Op->getModifiedRule())
    return true;
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

bool TextLayoutPrinter::printUpdateRulePluginOp(PluginOp *Pop) const {
  UpdateRulePluginOp *Op = llvm::dyn_cast<UpdateRulePluginOp>(Pop);
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
                 << M.getBackend().getRelocator()->getName(R->type()) << "\n";
  outputStream() << "#\tOriginal: 0x";
  outputStream().write_hex(R->target());
  uint64_t RelocationData;
  bool RelocDataFound = M.getRelocationDataForSync(R, RelocationData);
  ASSERT(RelocDataFound, "Relocation data must be available");
  if (RelocDataFound) {
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
  if (!ThisLayoutInfo->getPlugins().size())
    return;
  outputStream() << "# Detailed Plugin information "
                 << "\n";
  int PluginCount = 0;
  for (auto &W : ThisLayoutInfo->getPlugins()) {
    eld::Plugin *Plugin = W->getPlugin();
    outputStream() << "# Plugin #" << PluginCount << "\t" << Plugin->getName()
                   << "\t" << Plugin->getLibraryName() << "\n";
    int Count = 0;
    for (auto &Op : ThisLayoutInfo->getPluginOps()[W]) {
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
void TextLayoutPrinter::printOnlyLayoutPadding(ELFSection *Section,
                                               int64_t StartOffset, int64_t Sz,
                                               int64_t FillValue,
                                               bool IsAlignment,
                                               bool UseColor) const {
  if (UseColor)
    outputStream().changeColor(llvm::raw_ostream::RED);
  outputStream() << "PADDING";
  if (IsAlignment)
    outputStream() << "_ALIGNMENT";
  outputStream() << "\t0x";
  outputStream().write_hex(Sz);
  outputStream() << "\t";
  outputStream() << "0x";
  outputStream().write_hex(FillValue);
  outputStream() << "\n";
  if (UseColor)
    outputStream().resetColor();
}

void TextLayoutPrinter::printPadding(ELFSection *Section, int64_t StartOffset,
                                     int64_t Sz, int64_t FillValue,
                                     bool IsAlignment, bool UseColor) const {
  if (!Sz)
    return;
  if (ThisLayoutInfo->showOnlyLayout()) {
    printOnlyLayoutPadding(Section, StartOffset, Sz, FillValue, IsAlignment,
                           UseColor);
    return;
  }
  if (UseColor)
    outputStream().changeColor(llvm::raw_ostream::RED);
  outputStream() << "PADDING";
  if (IsAlignment)
    outputStream() << "_ALIGNMENT";
  outputStream() << "\t0x";
  if (Section->isAlloc())
    outputStream().write_hex(Section->addr() + StartOffset);
  else
    outputStream().write_hex(Section->offset() + StartOffset);
  outputStream() << "\t";
  outputStream() << "0x";
  outputStream().write_hex(Sz);
  outputStream() << "\t";
  outputStream() << "0x";
  outputStream().write_hex(FillValue);
  outputStream() << "\n";
  if (UseColor)
    outputStream().resetColor();
}

void TextLayoutPrinter::printMergeString(MergeableString *S, Module &M) const {
  if (S->Exclude) {
    outputStream() << "\n";
    return;
  }
  if (ThisLayoutInfo->showStrings()) {
    outputStream() << "[ Contents: " << S->String.data() << "]";
    outputStream() << "\n";
  }
  for (MergeableString *Merged : ThisLayoutInfo->getMergedStrings(S)) {
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
      ThisLayoutInfo->showOnlyLayout() || M.isLinkStateBeforeLayout();
  std::string Type =
      ELFSection::getELFTypeStr(Info->section()->name(), Info->type()).str();
  std::string Permissions = ELFSection::getELFPermissionsStr(Info->flag());
  std::string Path;
  if (Info->section()->hasOldInputFile())
    Path = getDecoratedPath(Info->section()->getOldInputFile()->getInput());
  else if (Info->ThisInputFile)
    Path = getDecoratedPath(Info->ThisInputFile->getInput());

  bool GC = Frag->getOwningSection()->isIgnore();
  uint32_t Alignment = Frag->alignment();
  const GeneralOptions &Options = ThisLayoutInfo->getConfig().options();
  const auto SectionAnnotations = getPluginSectionAnnotations(Info->section());
  auto PrintOneFragOrString =
      [&, this](uint32_t Size, std::optional<uint64_t> AddressOrOffset) {
        if (GC && !Onlylayout)
          outputStream() << "# ";
        outputStream() << Info->getDecoratedName(Options);
        if (!GC && !Onlylayout) {
          outputStream() << "\t";
          printOffsetHelper(AddressOrOffset.has_value(), [&, this]() {
            outputStream() << "0x";
            outputStream().write_hex(AddressOrOffset.value());
          });
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
        if (SectionAnnotations && !SectionAnnotations->empty())
          outputStream() << ", Annotations: " << *SectionAnnotations;
        if (!Onlylayout) {
          printChangeOutputSectionInfo(Info->section());
          printChunkOps(M, Frag);
        }
      };

  std::optional<uint64_t> AddressOrOffset;
  bool HasFragInfo =
      (M.isLinkStateCreatingSegments() || M.isLinkStateAfterLayout());
  if (llvm::isa<MergeStringFragment>(Frag) && !M.isLinkStateBeforeLayout()) {
    auto *Strings = llvm::cast<MergeStringFragment>(Frag);
    for (MergeableString *S : Strings->getStrings()) {
      if (S->Exclude)
        continue;
      if (!GC && HasFragInfo)
        AddressOrOffset =
            S->OutputOffset +
            (Section->isAlloc() ? Section->addr() : Section->offset());
      PrintOneFragOrString(S->size(), AddressOrOffset);
      /// if showStrings there is still extra content to be printed on this line
      if (!ThisLayoutInfo->showStrings())
        outputStream() << "\n";
      printMergeString(S, M);
    }
  } else {
    if (!GC && HasFragInfo)
      AddressOrOffset =
          Frag->getOffset() +
          (Section->isAlloc() ? Section->addr() : Section->offset());
    PrintOneFragOrString(Frag->size(), AddressOrOffset);
    outputStream() << "\n";
  }
}

void TextLayoutPrinter::printChangeOutputSectionInfo(
    const ELFSection *S) const {
  auto Ops = ThisLayoutInfo->getSectionOps().find(S);
  if (Ops == ThisLayoutInfo->getSectionOps().end())
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
  auto Ops = ThisLayoutInfo->getChunkOps().find(F);
  if (Ops == ThisLayoutInfo->getChunkOps().end())
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
void TextLayoutPrinter::printOnlyLayoutFrag(eld::Module &CurModule,
                                            ELFSection *Section, Fragment *Frag,
                                            bool UseColor) {
  if (!Frag->size())
    return;
  DiagnosticEngine *DiagEngine = CurModule.getConfig().getDiagEngine();
  if (Frag->paddingSize() > 0) {
    std::optional<uint64_t> PaddingValue =
        CurModule.getFragmentPaddingValue(Frag);
    printOnlyLayoutPadding(
        Section, Frag->getOffset(DiagEngine) - Frag->paddingSize(),
        Frag->paddingSize(), PaddingValue ? *PaddingValue : 0, true, UseColor);
  }
  if (UseColor)
    outputStream().changeColor(llvm::raw_ostream::YELLOW);

  LayoutInfo::FragmentInfoMapIterT FragmentInfoIter =
      ThisLayoutInfo->getFragmentInfoMap().find(Frag);
  if (FragmentInfoIter == ThisLayoutInfo->getFragmentInfoMap().end())
    return;
  LayoutFragmentInfo *FragmentInfo = FragmentInfoIter->second;
  printFragInfo(Frag, FragmentInfo, Section, CurModule);
  LayoutInfo::SymVectorTIterT Syms, EndSymbols = FragmentInfo->Symbols.end();

  ThisLayoutInfo->sortFragmentSymbols(FragmentInfo);

  for (Syms = FragmentInfo->Symbols.begin(); Syms != EndSymbols; ++Syms) {
    // Handle weak symbols.
    std::string ResolvedPath =
        getDecoratedPath((*Syms)->resolveInfo()->resolvedOrigin()->getInput());

    InputFile *ResolvedOrigin = (*Syms)->resolveInfo()->resolvedOrigin();
    bool IsBitcode = false;

    if (ResolvedOrigin->isBitcode())
      IsBitcode = true;
    else if (getDecoratedPath(ResolvedOrigin->getInput()) !=
             FragmentInfo->getResolvedPath())
      continue;

    if ((*Syms)->name() == FragmentInfo->name())
      continue;

    outputStream() << "\t"
                   << showDecoratedSymbolName(CurModule,
                                              (*Syms)->resolveInfo());
    if (IsBitcode) {
      outputStream() << "\t"
                     << "#(Bitcode origin:"
                     << getDecoratedPath(ResolvedOrigin->getInput()) << ")";
    }
    outputStream() << "\n";
  }

  // Dump any information that can be used for debugging.
  Frag->dump(outputStream());

  if (UseColor)
    outputStream().resetColor();
}

/// FIXME: Lots of unnecessary code duplication with this function and
/// printOnlyLayoutFrag()
void TextLayoutPrinter::printFrag(eld::Module &CurModule, ELFSection *Section,
                                  Fragment *Frag, bool UseColor) {
  if (ThisLayoutInfo->showOnlyLayout() ||
      CurModule.isLinkStateBeforeLayout()) {
    printOnlyLayoutFrag(CurModule, Section, Frag, UseColor);
    return;
  }

  DiagnosticEngine *DiagEngine = CurModule.getConfig().getDiagEngine();

  if (Frag->paddingSize() > 0) {
    std::optional<uint64_t> PaddingValue =
        CurModule.getFragmentPaddingValue(Frag);
    printPadding(Section, Frag->getOffset(DiagEngine) - Frag->paddingSize(),
                 Frag->paddingSize(), PaddingValue ? *PaddingValue : 0, true,
                 UseColor);
  }

  if (UseColor)
    outputStream().changeColor(llvm::raw_ostream::YELLOW);

  LayoutInfo::FragmentInfoMapIterT FragmentInfoIter =
      ThisLayoutInfo->getFragmentInfoMap().find(Frag);
  if (FragmentInfoIter == ThisLayoutInfo->getFragmentInfoMap().end())
    return;

  LayoutFragmentInfo *FragmentInfo = FragmentInfoIter->second;
  bool IsGc = Frag->getOwningSection()->isIgnore();
  printFragInfo(Frag, FragmentInfo, Section, CurModule);
  LayoutInfo::SymVectorTIterT Syms, EndSymbols = FragmentInfo->Symbols.end();

  ThisLayoutInfo->sortFragmentSymbols(FragmentInfo);

  const LayoutInfo::RemoveSymbolOpsMapT RemovedSymbols =
      ThisLayoutInfo->getRemovedSymbols();

  bool HasFragOffsets = (CurModule.isLinkStateCreatingSegments() ||
                         CurModule.isLinkStateAfterLayout());

  for (Syms = FragmentInfo->Symbols.begin(); Syms != EndSymbols; ++Syms) {
    // Handle weak symbols.
    std::string ResolvedPath =
        getDecoratedPath((*Syms)->resolveInfo()->resolvedOrigin()->getInput());

    InputFile *ResolvedOrigin = (*Syms)->resolveInfo()->resolvedOrigin();
    bool IsBitcode = false;

    if (ResolvedOrigin->isBitcode())
      IsBitcode = true;
    else if (ResolvedOrigin->getInput()->decoratedPath(
                 ThisLayoutInfo->showAbsolutePath()) !=
             FragmentInfo->getResolvedPath())
      continue;

    if ((*Syms)->name() == FragmentInfo->name())
      continue;

    if ((*Syms)->name() == FragmentInfo->getRealName())
      continue;

    auto Removed = RemovedSymbols.find((*Syms)->resolveInfo());

    if (!IsGc) {
      outputStream() << "\t";
      printOffsetHelper(HasFragOffsets, [&, this]() {
        outputStream() << "0x";
        outputStream().write_hex(
            ThisLayoutInfo->calculateSymbolValue(*Syms, CurModule));
        outputStream() << "\t";
      });
    } else {
      outputStream() << "#";
    }
    outputStream() << "\t"
                   << showDecoratedSymbolName(CurModule,
                                              (*Syms)->resolveInfo());
    if (Removed != RemovedSymbols.end())
      outputStream() << " {" << Removed->getSecond()->getPluginOpStr() << ", "
                     << Removed->getSecond()->getPluginName() << "}";
    if (IsBitcode) {
      outputStream() << "\t"
                     << "#(Bitcode origin:"
                     << getDecoratedPath(ResolvedOrigin->getInput()) << ")";
    }
    outputStream() << "\n";
  }

  // Dump any information that can be used for debugging.
  Frag->dump(outputStream());

  if (UseColor)
    outputStream().resetColor();
}

// Write included Archive members in Map file.
void TextLayoutPrinter::printArchiveRecords(Module &Module, bool UseColor) {
  if (UseColor)
    outputStream().changeColor(llvm::raw_ostream::CYAN);

  if (ThisLayoutInfo->getArchiveRecords().size()) {
    auto It = ThisLayoutInfo->getArchiveRecords().begin(),
         Ie = ThisLayoutInfo->getArchiveRecords().end();
    outputStream() << "Archive member included because of file (symbol)\n";
    for (; It != Ie; It++) {
      auto Ret = ThisLayoutInfo->getArchiveRecord(*It);
      outputStream() << Ret.first << "\n\t\t" << Ret.second << "\n";
    }
  }
  if (!Module.getArchiveLibraryList().size())
    return;
  outputStream() << "\nCount for Loaded archive member/Total archive members\n";
  for (auto *Item : Module.getArchiveLibraryList()) {
    ArchiveFile *File = llvm::dyn_cast_or_null<ArchiveFile>(Item);
    if (!File)
      continue;
    // FIXME: Here, we are ignoring MapDetail options show-absolute-path and
    // show-relative-path=...
    outputStream() << File->getInput()->decoratedPath() << " "
                   << File->getLoadedMemberCount() << "/"
                   << File->getTotalMemberCount() << "\n";
  }
  if (UseColor)
    outputStream().resetColor();
}

std::string
TextLayoutPrinter::showDecoratedSymbolName(eld::Module &CurModule,
                                           const ResolveInfo *R) const {
  return CurModule.getNamePool().getDecoratedName(
      R, ThisLayoutInfo->getConfig().options().shouldDemangle());
}

// If common symbols are present, write them in Map file
// along with size and resolved location.
void TextLayoutPrinter::printCommons(eld::Module &CurModule, bool UseColor) {
  if (UseColor)
    outputStream().changeColor(llvm::raw_ostream::MAGENTA);

  auto CommonSyms = ThisLayoutInfo->getAllocatedCommonSymbols(CurModule);

  if (CommonSyms.empty())
    return;

  outputStream()
      << "\nAllocating common symbols\nCommon symbol\tsize\tfile\n\n";

  for (auto *Sym : CommonSyms) {
    outputStream() << showDecoratedSymbolName(CurModule, Sym) << "\t0x";
    outputStream().write_hex(Sym->size());
    InputFile *Input = Sym->resolvedOrigin();
    if (Input)
      outputStream() << "\t" << getDecoratedPath(Input->getInput());
    outputStream() << "\n";
  }
  if (UseColor)
    outputStream().resetColor();
}

// If exclude list is present, add it to map file along with the symbol list.
void TextLayoutPrinter::printExternList(Module &CurModule, bool UseColor) {
  if (!CurModule.getScript().hasExternCommand())
    return;
  const std::vector<ScriptCommand *> &ScriptCommands =
      CurModule.getScript().getScriptCommands();
  if (UseColor)
    outputStream().changeColor(llvm::raw_ostream::MAGENTA);
  outputStream() << "\nExtern list files\n";
  std::string CurrFile = "";
  std::function<std::string(const Input *)> GetDecoratedPath =
      [=](const Input *I) { return this->getDecoratedPath(I); };
  for (ScriptCommand *Cmd : ScriptCommands) {
    if (!Cmd->isExtern())
      continue;
    if (CurrFile != Cmd->getContext()) {
      CurrFile = Cmd->getContext();
      outputStream() << CurrFile << "\n";
    }
    for (const SymbolContainer *SymContainer :
         llvm::cast<ExternCmd>(Cmd)->getSymbolContainers()) {
      outputStream() << "Pattern: "
                     << SymContainer->getDecoratedWildcardPattern() << "\n";

      SymContainer->dump(outputStream(), GetDecoratedPath);
    }
  }
  if (UseColor)
    outputStream().resetColor();
}

// If dynamic list is present, add it to map file along with the symbol list.
void TextLayoutPrinter::printDynamicList(Module &CurModule, bool UseColor) {
  const llvm::DenseMap<InputFile *, Module::ScriptSymbolList>
      &DynamicListFileToScriptSymbolsMap =
          CurModule.getDynamicListFileToScriptSymbolsMap();
  if (!DynamicListFileToScriptSymbolsMap.size())
    return;
  if (UseColor)
    outputStream().changeColor(llvm::raw_ostream::MAGENTA);
  outputStream() << "\nDynamic list files\n";
  std::function<std::string(const Input *)> GetDecoratedPath =
      [=](const Input *I) { return this->getDecoratedPath(I); };
  for (auto Item : DynamicListFileToScriptSymbolsMap) {
    if (!Item.second.size())
      continue;
    outputStream() << GetDecoratedPath(Item.first->getInput());
    for (const ScriptSymbol *Sym : Item.second) {
      outputStream() << "\nPattern: "
                     << Sym->getSymbolContainer()->getWildcardPatternAsString()
                     << "\n";
      Sym->getSymbolContainer()->dump(outputStream(), GetDecoratedPath);
    }
  }
  if (UseColor)
    outputStream().resetColor();
}

// If version list is present, add it to map file along with the symbol list.
void TextLayoutPrinter::printVersionList(Module &CurModule, bool UseColor) {
  auto &VersionScripts = CurModule.getVersionScripts();
  if (!VersionScripts.size())
    return;
  outputStream() << "\nVersion Script Information\n";
  if (UseColor)
    outputStream().changeColor(llvm::raw_ostream::MAGENTA);
  std::function<std::string(const Input *)> GetDecoratedPath =
      [=](const Input *I) { return this->getDecoratedPath(I); };
  for (const auto *Item : VersionScripts)
    Item->dump(outputStream(), GetDecoratedPath);
  if (UseColor)
    outputStream().resetColor();
}

// Write the Linker scripts used to the Map file.
void TextLayoutPrinter::printScriptIncludes(bool UseColor) {
  if (UseColor)
    outputStream().changeColor(llvm::raw_ostream::GREEN);
  if (ThisLayoutInfo->getLinkerScripts().size())
    outputStream() << "\nLinker scripts used (including INCLUDE command)\n";
  for (auto &Script : ThisLayoutInfo->getLinkerScripts()) {
    std::string Indent = "";
    for (uint32_t I = 0; I < Script.Depth; ++I)
      Indent += "\t";
    std::string LinkerScriptName = Script.Include;
    if (ThisLayoutInfo->getConfig().options().hasMappingFile())
      LinkerScriptName = ThisLayoutInfo->getPath(LinkerScriptName) + "(" +
                         LinkerScriptName + ")";
    if (!Script.Found)
      LinkerScriptName += "(NOTFOUND)";
    outputStream() << Indent << LinkerScriptName;
    outputStream() << "\n";
  }
  if (UseColor)
    outputStream().resetColor();
}

void TextLayoutPrinter::printVersionScripts(bool UseColor) {
  if (UseColor)
    outputStream().changeColor(llvm::raw_ostream::GREEN);
  if (ThisLayoutInfo->getVersionScripts().size())
    outputStream() << "\nVersion scripts used\n";
  for (auto &Script : ThisLayoutInfo->getVersionScripts()) {
    std::string VersionScriptName = Script;
    if (ThisLayoutInfo->getConfig().options().hasMappingFile())
      VersionScriptName = ThisLayoutInfo->getPath(VersionScriptName) + "(" +
                          VersionScriptName + ")";
    outputStream() << VersionScriptName;
    outputStream() << "\n";
  }
  if (UseColor)
    outputStream().resetColor();
}

void TextLayoutPrinter::printLinkerInsertedTimingStats(Module &CurModule) {
  TimingFragment *F = CurModule.getBackend().getTimingFragment();
  const TimingSlice *T = F->getTimingSlice();
  outputStream() << T->getModuleName() << " " << T->getDate() << " "
                 << T->getDurationSeconds() << "\n";
}

void TextLayoutPrinter::printBuildStatistics(Module &CurModule, bool UseColor) {
  if (UseColor)
    outputStream().changeColor(llvm::raw_ostream::MAGENTA);
  outputStream() << "\nBuild Statistics";
  outputStream() << "\n# <file> <start time> <duration>\n";
  for (auto &I : ThisLayoutInfo->getInputActions()) {
    if (I.Input == nullptr || I.Input->getInputFile() == nullptr)
      continue;
    if (!I.Input->getInputFile()->isObjectFile())
      continue;
    ELFObjectFile *EObj =
        llvm::dyn_cast<ELFObjectFile>(I.Input->getInputFile());
    if (EObj->getTimingSection()) {
      for (const Fragment *TF : EObj->getTimingSection()->getFragmentList()) {
        const TimingSlice *TS =
            llvm::dyn_cast<TimingFragment>(TF)->getTimingSlice();
        outputStream() << TS->getModuleName() << " " << TS->getDate() << " "
                       << TS->getDurationSeconds() << "\n";
      }
    }
  }
  if (ThisLayoutInfo->getConfig().options().getInsertTimingStats()) {
    printLinkerInsertedTimingStats(CurModule);
  }
  outputStream() << "\n";
  outputStream().resetColor();
}

// Print the loaded Linker scripts and Memory Map files.
void TextLayoutPrinter::printInputActions(bool UseColor) {
  if (UseColor)
    outputStream().changeColor(llvm::raw_ostream::BLUE);
  outputStream() << "\nLinker Script and memory map\n";
  for (auto &I : ThisLayoutInfo->getInputActions())
    outputStream() << ThisLayoutInfo->getStringFromLoadSequence(I) << "\n";
  if (UseColor)
    outputStream().resetColor();
}

std::string TextLayoutPrinter::commandLineWithMappings() {
  std::string Original = "";
  for (const auto *Arg : ThisLayoutInfo->getConfig().options().args())
    if (Arg) {
      Original.append(
          ThisLayoutInfo->getConfig().getFileFromHash(std::string(Arg)));
      Original.append(" ");
    }
  return Original;
}

// Print Map file header information like Linker architecture, version, etc.
void TextLayoutPrinter::printArchAndVersion(bool UseColor, Module const &M) {
  if (!eld::getVendorName().empty())
    outputStream() << "# Linker from " << eld::getVendorName() << " Version "
                   << eld::getVendorVersion() << "\n";
  outputStream() << "# Linker based on LLVM version: " << eld::getELDVersion()
                 << "\n";
  outputStream() << "# Linker: " << getELDRevision() << "\n";
  outputStream() << "# LLVM: " << getLLVMRevision() << "\n";
  this->ThisLayoutInfo->getConfig().printOptions(outputStream(), M, UseColor);

  // Print the command line in the Map file.
  std::string CommandLine;
  std::string Separator = "";
  for (const auto *Arg : ThisLayoutInfo->getConfig().options().args()) {
    CommandLine.append(Separator);
    Separator = " ";
    if (Arg)
      CommandLine.append(std::string(Arg));
  }
  outputStream() << "# CommandLine : " << CommandLine;
  outputStream() << "\n";
  if (ThisLayoutInfo->getConfig().options().hasMappingFile()) {
    outputStream() << "# Original CommandLine : " << commandLineWithMappings();
    outputStream() << "\n";
  }
}

void TextLayoutPrinter::printMemoryCommand(const MemoryCmd *M) {
  std::string Str;
  {
    llvm::raw_string_ostream OS(Str);
    M->dumpOnlyThis(OS);
    outputStream() << "# " << OS.str();
    outputStream() << "\n";
    outputStream() << "#{";
    outputStream() << "\n";
  }
  for (auto &MemSpec : M->getMemoryDescriptors()) {
    llvm::raw_string_ostream OS(Str);
    Str.clear();
    MemSpec->dump(OS);
    outputStream() << "#"
                   << "\t" << OS.str();
  }
  outputStream() << "#}";
  outputStream() << "\n";
}

void TextLayoutPrinter::printScriptCommands(const LinkerScript &Script) {
  if (Script.getMemoryCommand())
    printMemoryCommand(Script.getMemoryCommand());
}

// Start adding mapping information to map file
// starting with Map file Header information, e.g. Architechture, Version,
// etc. If use of color for text is enabled, print text with a foreground
// color. Reset the colors to terminal defaults after writing.
void TextLayoutPrinter::printMapFile(eld::Module &Module) {
  ThisLayoutInfo->buildMergedStringMap(Module);
  GNULDBackend &Backend = Module.getBackend();
  bool UseColor = Backend.config().options().color() &&
                  Backend.config().options().colorMap();
  LinkerScript const &Script = Module.getScript();

  printArchAndVersion(UseColor, Module);
  printStats(ThisLayoutInfo->getLinkStats(), Module);
  printIsFileHeaderLoadedInfo(Backend.isFileHeaderLoaded(), UseColor);
  printIsPHDRSLoadedInfo(Backend.isPHDRSLoaded(), UseColor);
  // Print image start address.
  if (Backend.hasImageStartVMA()) {
    outputStream() << "# Image start address: "
                   << "~ 0x";
    outputStream().write_hex(Backend.getImageStartVMA());
    outputStream() << "\n";
  }

  if (ThisLayoutInfo->showRelativePath())
    outputStream() << "# Basepath: " << ThisLayoutInfo->getBasepath().value()
                   << "\n";

  printArchiveRecords(Module, UseColor);
  printExternList(Module, UseColor);
  printDynamicList(Module, UseColor);
  printVersionList(Module, UseColor);
  printCommons(Module, UseColor);
  printInputActions(UseColor);
  printBuildStatistics(Module, UseColor);
  printScriptIncludes(UseColor);
  printVersionScripts(UseColor);
  printGlobalPluginInfo(Module, UseColor);

  if (Backend.getEntrySymbol())
    outputStream() << "\n"
                   << "Entry point: " << Backend.getEntrySymbol()->name()
                   << "\n";

  outputStream() << "\n";
  outputStream() << "# Output Section and Layout "
                 << "\n";

  printScriptCommands(Script);

  for (const auto &X : (Script.getScriptCommands())) {
    if (X->getKind() == ScriptCommand::ASSIGNMENT)
      X->dumpMap(outputStream(), UseColor, true,
                 !ThisLayoutInfo->showOnlyLayout());
  }

  printLayout(Module);

  if (!ThisLayoutInfo->showOnlyLayout())
    printPluginInfo(Module);

  if (ThisLayoutInfo->showSymbolResolution())
    printSymbolResolution(Module);
}

void TextLayoutPrinter::printLayout(eld::Module &Module) {
  // Get start and end points from section map to iterate over each section.
  SectionMap::const_iterator Out, OutBegin, OutEnd;
  LinkerScript const &Script = Module.getScript();
  GNULDBackend &Backend = Module.getBackend();
  bool UseColor = Backend.config().options().color() &&
                  Backend.config().options().colorMap();

  bool LinkerScriptHasSectionsCommand =
      Module.getScript().linkerScriptHasSectionsCommand();

  OutBegin = Script.sectionMap().begin();
  OutEnd = Script.sectionMap().end();
  if (Module.isLinkStateBeforeLayout())
    outputStream() << "Initial layout:\n";
  for (Out = OutBegin; Out != OutEnd; ++Out) {

    ELFSection *Cur = (*Out)->getSection();
    if ((ThisLayoutInfo->showHeaderDetails() &&
         (Cur->name() == "__ehdr__" || Cur->name() == "__pHdr__")) ||
        (!Cur->isNullType() &&
         (LinkerScriptHasSectionsCommand || Cur->isWanted() || Cur->size() ||
          (*Out)->size())) ||
        (Module.isLinkStateBeforeLayout() && !(*Out)->name().empty()))
      printSection(Backend, *Out, UseColor);

    // print padding from output section start to first frag
    for (const GNULDBackend::PaddingT &P : Backend.getPaddingBetweenFragments(
             Cur, nullptr, (*Out)->getFirstFrag())) {

      printPadding(Cur, P.startOffset, P.endOffset - P.startOffset,
                   P.Exp ? P.Exp->resultOrZero() : 0, false, UseColor);
    }
    for (OutputSectionEntry::iterator in = (*Out)->begin(),
                                      InEnd = (*Out)->end();
         in != InEnd; ++in) {
      ELFSection *IS = (*in)->getSection();
      // Evaluate all assignments at the beginning of input section.
      for (RuleContainer::sym_iterator It = (*in)->symBegin(),
                                       Ie = (*in)->symEnd();
           It != Ie; ++It) {
        if (!ThisLayoutInfo->showOnlyLayout()) {
          (*It)->dumpMap(outputStream(), UseColor, false,
                         /*withValues=*/!Module.isLinkStateBeforeLayout());
          // Show the actual expression as present in the linker script
          outputStream() << " # ";
        }
        // Show expression in the linker script.
        (*It)->dumpMap(outputStream(), UseColor, true /* NewLine */,
                       false /* No Values */,
                       ThisLayoutInfo->showOnlyLayout() /* Indent */);
      }
      if ((*in)->desc()) {
        (*in)->desc()->dumpMap(outputStream(), UseColor, false);
        if (!ThisLayoutInfo->dontShowTiming() &&
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
      if (!Module.isLinkStateBeforeLayout()) {
        for (auto *S : (*in)->getMatchedInputSections()) {
          if (!S->isIgnore())
            continue;
          for (auto *F : S->getFragmentList())
            printFrag(Module, Cur, F, UseColor);
        }
      }
      if ((!IS || !IS->getFragmentList().size()) &&
          !Module.isLinkStateBeforeLayout())
        continue;
      // Do not print fragments of OutputSectData commands.
      // Fragments and sections of OutputSectData is an internal
      // implementation and may confuse the users.
      if ((*in)->desc() && llvm::isa<OutputSectData>((*in)->desc()))
        continue;
      printFragments(Module, *Cur, **in, UseColor);
      // print padding between current rule and next rule with content
      const RuleContainer *NextRuleWithContent =
          (*in)->getNextRuleWithContent();

      if ((*in)->hasContent()) {
        for (const GNULDBackend::PaddingT &P :
             Backend.getPaddingBetweenFragments(
                 Cur, (*in)->getLastFrag(),
                 NextRuleWithContent ? NextRuleWithContent->getFirstFrag()
                                     : nullptr)) {

          printPadding(Cur, P.startOffset, P.endOffset - P.startOffset,
                       P.Exp ? P.Exp->resultOrZero() : 0, false, UseColor);
        }
      }
    }

    // Evaluate all assignments at the end of the output section.
    for (OutputSectionEntry::sym_iterator It = (*Out)->sectionendsymBegin(),
                                          Ie = (*Out)->sectionendsymEnd();
         It != Ie; ++It) {
      if (!ThisLayoutInfo->showOnlyLayout()) {
        (*It)->dumpMap(outputStream(), UseColor, false,
                       /*withValues=*/!Module.isLinkStateBeforeLayout());
        // Show the actual expression as present in the linker script
        outputStream() << " # ";
      }
      // Show expression in the linker script.
      (*It)->dumpMap(outputStream(), UseColor, true /* NewLine */,
                     false /* No Values */,
                     ThisLayoutInfo->showOnlyLayout() /* Indent */);
    }
  }
}

void TextLayoutPrinter::flush() {
  if (Buffer) {
    (*LayoutFile) << Buffer->str();
    Buffer->str().clear();
    (*LayoutFile).flush();
  }
}

TextLayoutPrinter::~TextLayoutPrinter() {
  flush();
}

void TextLayoutPrinter::clearInputRecords() {
  ThisLayoutInfo->resetArchiveRecords();
  ThisLayoutInfo->resetInputActions();
}

void TextLayoutPrinter::printIsFileHeaderLoadedInfo(bool IsLoaded,
                                                    bool UseColor) {
  if (UseColor)
    outputStream().changeColor(llvm::raw_ostream::GREEN);

  outputStream() << "# Is file header loaded: " << (IsLoaded ? "true" : "false")
                 << "\n";

  if (UseColor)
    outputStream().resetColor();
}

void TextLayoutPrinter::printIsPHDRSLoadedInfo(bool IsLoaded, bool UseColor) {
  if (UseColor)
    outputStream().changeColor(llvm::raw_ostream::GREEN);

  outputStream() << "# Is PHDRS loaded: " << (IsLoaded ? "true" : "false")
                 << "\n";

  if (UseColor)
    outputStream().resetColor();
}

void TextLayoutPrinter::printTotalSymbolStats(const Module &Module) {
  const ObjectLinker::SymbolStats &SymStats =
      Module.getLinker()->getObjectLinker()->getTotalSymbolStats();
  outputStream() << "# Total Symbols: "
                 << SymStats.Global + SymStats.Local + SymStats.Weak +
                        SymStats.Absolute - SymStats.File
                 << " "
                 << "{ "
                 << "Global: " << SymStats.Global << ", "
                 << "Local: " << SymStats.Local << ", "
                 << "Weak: " << SymStats.Weak << ", "
                 << "Absolute: " << SymStats.Absolute << ", "
                 << "Hidden: " << SymStats.Hidden << ", "
                 << "Protected: " << SymStats.ProtectedSyms << " "
                 << "}\n";
}

void TextLayoutPrinter::printDiscardedSymbolStats(const Module &Module) {
  const ObjectLinker::SymbolStats &SymStats =
      Module.getLinker()->getObjectLinker()->getDiscardedSymbolStats();
  outputStream() << "# Discarded Symbols: "
                 << SymStats.Global + SymStats.Local + SymStats.Weak +
                        SymStats.Absolute - SymStats.File
                 << " "
                 << "{ "
                 << "Global: " << SymStats.Global << ", "
                 << "Local: " << SymStats.Local << ", "
                 << "Weak: " << SymStats.Weak << ", "
                 << "Absolute: " << SymStats.Absolute << ", "
                 << "Hidden: " << SymStats.Hidden << ", "
                 << "Protected: " << SymStats.ProtectedSyms << " "
                 << "}\n";
}

std::string TextLayoutPrinter::getDecoratedPath(const Input *I) const {
  std::optional<std::string> Basepath = ThisLayoutInfo->getBasepath();
  if (Basepath.has_value())
    return I->getDecoratedRelativePath(Basepath.value());
  return I->decoratedPath(ThisLayoutInfo->showAbsolutePath());
}

void TextLayoutPrinter::printFragments(Module &Module, ELFSection &OutSect,
                                       RuleContainer &R, bool UseColor) {
  if (Module.isLinkStateBeforeLayout()) {
    for (auto *S : R.getMatchedInputSections()) {
      for (auto *F : S->getFragmentList())
        printFrag(Module, &OutSect, F, UseColor);
    }
  } else {
    for (auto &F : R.getSection()->getFragmentList())
      printFrag(Module, &OutSect, F, UseColor);
  }
}

void TextLayoutPrinter::printSymbolResolution(Module &Module) {
  NamePool &NP = Module.getNamePool();
  SymbolResolutionInfo &SRI = NP.getSRI();
  const auto &Symbols = Module.getSymbols();
  const GeneralOptions &Options = ThisLayoutInfo->getConfig().options();
  SRI.setupCandidatesInfo(NP, Module.getScript());

  outputStream() << "# Symbol Resolution: "
                 << "\n";

  size_t Index = 0;
  for (const auto *RI : Symbols) {
    if (RI->isLocal() &&
        RI->resolvedOrigin() != Module.getInternalInput(Module::Plugin))
      continue;
    ++Index;
    llvm::StringRef SymName = RI->getName();
    const SymbolResolutionInfo::CandidatesType Candidates =
        SRI.getCandidates(SymName);
    outputStream() << Index << ") " << SymName << "\n";
    for (const auto &Candidate : Candidates) {
      std::optional<SymbolInfo> OptSymbolInfo = SRI.getSymbolInfo(Candidate);
      ASSERT(OptSymbolInfo, "Symbol info must be present!");
      SymbolInfo CandidateInfo = OptSymbolInfo.value();

      std::string CandidateInfoAsString =
          SRI.getSymbolInfoAsString(Candidate, Options);
      outputStream() << "\t" << CandidateInfoAsString;
      if (Candidate->resolveInfo()->outSymbol() == Candidate ||
          (CandidateInfo.isBitcodeSymbol() &&
           CandidateInfo.getInputFile() ==
               Candidate->resolveInfo()->resolvedOrigin()))
        outputStream() << " [Selected]";
      if (CandidateInfo.isBitcodeSymbol()) {
        if (const LDSymbol *LTOSym =
                SRI.getCorrespondingLTOObjectSymIfAny(Candidate)) {
          outputStream() << "\n\t  "
                         << SRI.getSymbolInfoAsString(LTOSym, Options);
        }
      }
      outputStream() << "\n";
    }
  }
}

void TextLayoutPrinter::printOffsetHelper(bool HasOffset,
                                          std::function<void()> F) const {
  if (!HasOffset) {
    outputStream() << "NA";
    return;
  }
  F();
}
