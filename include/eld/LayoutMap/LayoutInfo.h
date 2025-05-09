//===- LayoutInfo.h-----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_LAYOUTMAP_LAYOUTPRINTER_H
#define ELD_LAYOUTMAP_LAYOUTPRINTER_H
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Fragment/RegionTableFragment.h"
#include "eld/Input/ArchiveFile.h"
#include "eld/Input/ArchiveMemberInput.h"
#include "eld/Input/Input.h"
#include "eld/Input/InputFile.h"
#include "eld/LayoutMap/LinkStats.h"
#include "eld/Plugin/PluginOp.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Support/MemoryArea.h"
#include "eld/Support/MemoryRegion.h"
#include "eld/Support/StringRefUtils.h"
#include "eld/SymbolResolver/LDSymbol.h"
#include "eld/Target/GNULDBackend.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/raw_ostream.h"
#include <stack>
#include <tuple>
#include <unordered_set>

namespace eld {

class ChangeOutputSectionPluginOp;
class AddChunkPluginOp;
class RemoveChunkPluginOp;
class UpdateChunksPluginOp;

class LinkerConfig;
class Module;
class Stats;

struct LayoutFragmentInfo {
  LayoutFragmentInfo(InputFile *F, const ELFSection *Section)
      : ThisInputFile(F), ThisSection(Section) {}

  LayoutFragmentInfo(const ELFSection *Section)
      : ThisInputFile(nullptr), ThisSection(Section) {}

  InputFile *ThisInputFile;
  const ELFSection *ThisSection;

  std::string getResolvedPath() const {
    if (ThisInputFile != nullptr)
      return ThisInputFile->getInput()->decoratedPath();
    return "";
  }

  std::string getDecoratedName(const GeneralOptions &Options) const {
    return ThisSection->getDecoratedName(Options);
  }

  std::string name() const { return ThisSection->name().str(); }

  std::string getRealName() const { return ThisSection->name().str(); }

  uint32_t flag() const { return ThisSection->getFlags(); }
  uint32_t type() const { return ThisSection->getType(); }
  const ELFSection *section() const { return ThisSection; }
  std::vector<LDSymbol *> Symbols;
};

class LayoutInfo {
  static uint32_t LayoutDetail;

public:
  enum LayoutDetail {
    ShowStrings = 0x1,
    ShowAbsolutePath = 0x2,
    OnlyLayout = 0x4,
    NoTimeStats = 0x8,
    ShowHeaderDetails = 0x10,
    ShowTiming = 0x20,
    ShowDebugStrings = 0x40,
    ShowRelativePath = 0x80,
    ShowInitialLayout = 0x100,
    ShowSymbolResolution = 0x200
  };

  enum InputKindPrefix {
    Load,
    Skipped,
    SkippedRescan,
    StartGroup,
    EndGroup,
  };

  struct Stats {
    uint64_t NumElfObjectFiles = 0;
    uint64_t NumElfExecutableFiles = 0;
    uint64_t NumLinkerScripts = 0;
    uint64_t NumSharedObjectFiles = 0;
    uint64_t NumSymDefFiles = 0;
    uint64_t NumArchiveFiles = 0;
    uint64_t NumGroupTraversal = 0;
    uint64_t NumBitCodeFiles = 0;
    uint64_t NumSectionsGarbageCollected = 0;
    uint64_t NumZeroSizedSection = 0;
    uint64_t NumZeroSizedSectionsGarbageCollected = 0;
    uint64_t NumLinkerScriptRules = 0;
    uint64_t NumOutputSections = 0;
    uint64_t NumPlugins = 0;
    uint64_t NumThreads = 0;
    uint64_t NumOrphans = 0;
    uint64_t NumTrampolines = 0;
    uint64_t NumNoRuleMatch = 0;
    uint32_t LinkTime = 0;
    uint64_t NumRetainedSections = 0;
    uint32_t NumBinaryFiles = 0;
    std::optional<uint32_t> OutputFileSize;

    bool hasStats() const {
      return (NumElfObjectFiles + NumElfExecutableFiles + NumLinkerScripts +
                  NumSharedObjectFiles + NumSymDefFiles + NumArchiveFiles +
                  NumGroupTraversal + NumBitCodeFiles +
                  NumSectionsGarbageCollected + NumZeroSizedSection +
                  NumZeroSizedSectionsGarbageCollected + NumLinkerScriptRules +
                  NumOutputSections + NumPlugins + NumOrphans + NumTrampolines +
                  NumNoRuleMatch + LinkTime + NumRetainedSections +
                  NumBinaryFiles >
              0);
    }
  };

  struct InputSequenceT {
    InputKindPrefix Prefix;
    Input *Input;
    std::string ArchFlag;
  };

  // -------------------- Public Typedefs ----------------------------
  typedef std::tuple<Input *, InputFile *, ArchiveFile::Symbol *, LDSymbol *>
      ArchiveReferenceRecordT;

  typedef llvm::DenseMap<ELFSection *, std::vector<Plugin *>>
      PluginInfoSectionMapT;

  typedef llvm::DenseMap<plugin::LinkerWrapper *, std::vector<PluginOp *>>
      PluginOpsMapT;

  typedef llvm::DenseMap<const ResolveInfo *, PluginOp *> RemoveSymbolOpsMapT;

  typedef llvm::DenseMap<ELFSection *, std::vector<PluginOp *>> SectionOpsMapT;

  typedef llvm::DenseMap<const Fragment *, std::vector<PluginOp *>>
      ChunkOpsMapT;

  typedef llvm::DenseMap<const Fragment *, LayoutFragmentInfo *>
      FragmentInfoMapT;
  typedef llvm::DenseMap<ELFSection *, InputFile *> SectionInfoMapT;

  typedef FragmentInfoMapT::iterator FragmentInfoMapIterT;
  typedef std::vector<LayoutFragmentInfo *> FragmentInfoVectorT;
  typedef FragmentInfoVectorT::iterator FragmentInfoVectorTIter;
  typedef std::vector<LDSymbol *> SymVectorT;
  typedef SymVectorT::iterator SymVectorTIterT;
  typedef std::vector<ResolveInfo *> ResolveInfoVectorT;
  typedef ResolveInfoVectorT::iterator ResolveInfoIterT;
  typedef std::vector<ArchiveReferenceRecordT>::iterator
      ArchiveReferenceRecordIteratorT;
  typedef std::vector<InputSequenceT> InputSequenceVectorT;
  typedef std::vector<std::string> StringVectorT;
  typedef StringVectorT::iterator StringVectorIteratorT;

  // Linker Script support
  struct ScriptInputT {
    std::string Include;
    std::string Parent;
    bool Found = false;
    uint32_t Depth = 0;
  };
  typedef std::vector<ScriptInputT> ScriptVectorT;

  // -------------------End Typedefs ---------------------------------

  LayoutInfo(LinkerConfig &PConfig);

  bool showStrings() const { return LayoutDetail & ShowStrings; }

  bool showOnlyLayout() const { return LayoutDetail & OnlyLayout; }

  bool showAbsolutePath() const { return LayoutDetail & ShowAbsolutePath; }

  bool showRelativePath() const { return LayoutDetail & ShowRelativePath; }

  bool dontShowTiming() const { return LayoutDetail & NoTimeStats; }

  bool showDebugStrings() const { return LayoutDetail & ShowDebugStrings; }

  bool showTimers() const {
    return !dontShowTiming() && (LayoutDetail & ShowTiming);
  }

  bool showHeaderDetails() const { return (LayoutDetail & ShowHeaderDetails); }

  bool showInitialLayout() const { return LayoutDetail & ShowInitialLayout; }

  static eld::Expected<void> setLayoutDetail(llvm::StringRef Option,
                                             DiagnosticEngine *);

  void recordFragment(InputFile *Input, const ELFSection *InputElfSection,
                      const Fragment *Frag);

  // Record commandline history string from .comment
  void recordCommentFragment(const std::string &CommentStr) {
    Comments.push_back(CommentStr);
  }

  void recordSymbol(const Fragment *Frag, LDSymbol *Symbol);

  void recordThreadCount();

  void recordSectionStat(const Section *Sect);

  void recordSection(ELFSection *S, InputFile *I) { SectionInfoMap[S] = I; }

  void recordPlugin(ELFSection *S, Plugin *P) {
    ThisPluginInfo[S].push_back(P);
  }

  void recordOutputFileSize(uint32_t Sz) { LinkStats.OutputFileSize = Sz; }

  // FIXME: Destructor is redundant here.
  ~LayoutInfo() { destroy(); }

  // FIXME: This function is not required.
  void destroy() {
    InputActions.clear();
    ScriptIncludes.clear();
    ArchiveRecords.clear();
    FragmentInfoMap.clear();
    FragmentInfoVector.clear();
  }

  void recordArchiveMember(Input *Origin, InputFile *Referred,
                           ArchiveFile::Symbol *ArchSym, LDSymbol *Sym);

  void recordWholeArchiveMember(Input *Origin);

  std::string getPath(const Input *Inp) const {
    if (showAbsolutePath())
      return Inp->getResolvedPath().getFullPath();
    return Inp->getResolvedPath().native();
  }

  std::string getFileTypeStringIfBitcode(InputFile *F) {
    return (F->isBitcode()) ? " (Bitcode type)" : "";
  }

  void resetArchiveRecords() { ArchiveRecords.clear(); }

  std::string getStringFromLoadSequence(InputSequenceT Ist);

  void recordInputActions(InputKindPrefix Prefix, Input *Input,
                          std::string FileType = "");

  void resetInputActions() { InputActions.clear(); }

  std::string infoForFrag(const Fragment *Frag);

  bool isSectionDetailedInfoAvailable(ELFSection *Section);

  void sortFragmentSymbols(LayoutFragmentInfo *FragInfo);

  int64_t calculateSymbolValue(LDSymbol *Symbol, Module &M);

  std::string showSymbolName(llvm::StringRef Name) const;

  void recordInputKind(InputFile::InputFileKind K);

  void recordGroup();

  void recordOutputSection();

  std::string getPath(const std::string &Filename) const;

  void recordLinkerScript(std::string File, bool Found = true);

  void recordLinkerScriptRule();

  void recordOrphanSection();

  void recordGC(const ELFSection *Section);

  void recordPlugin();

  void recordTrampolines();

  void recordRetainedSections();

  void recordNoLinkerScriptRuleMatch();

  void recordFeature(std::string Feature);

  void recordSectionOverride(plugin::LinkerWrapper *W,
                             ChangeOutputSectionPluginOp *O);

  void recordAddChunk(plugin::LinkerWrapper *W, AddChunkPluginOp *O);

  void recordResetOffset(plugin::LinkerWrapper *W, ResetOffsetPluginOp *O);

  void recordRemoveChunk(plugin::LinkerWrapper *W, RemoveChunkPluginOp *O);

  void recordUpdateChunks(plugin::LinkerWrapper *W, UpdateChunksPluginOp *O);

  void recordRemoveSymbol(plugin::LinkerWrapper *W, RemoveSymbolPluginOp *O);

  void recordRelocationData(plugin::LinkerWrapper *W,
                            RelocationDataPluginOp *O);

  void closeLinkerScript() {
    if (LinkerScriptStack.size())
      LinkerScriptStack.pop();
  }

  void recordLinkTime(uint32_t TimeInSeconds) {
    LinkStats.LinkTime = TimeInSeconds;
  }

  void recordVersionScript(std::string VersionScript) {
    VersionScripts.push_back(VersionScript);
  }

  std::pair<std::string, std::string>
  getArchiveRecord(ArchiveReferenceRecordT &Itr) {
    // Only first item in tuple is not null in case of --whole-archive
    Input *orig = std::get<0>(Itr);
    InputFile *ref = std::get<1>(Itr);
    LDSymbol *Sym = std::get<3>(Itr);
    std::string SymName;
    if (Sym)
      SymName = Sym->name();
    else
      SymName = getWholeArchiveString();
    if (ref == nullptr) {
      return std::make_pair(orig->decoratedPath(), SymName);
    }
    std::string referred = ref->getInput()->decoratedPath();
    std::string inputType = getFileTypeStringIfBitcode(ref);
    referred = referred + " (" + SymName + ")" + inputType;
    std::string membType = getFileTypeStringIfBitcode(orig->getInputFile());
    std::string memberPath = orig->decoratedPath() + membType;
    return std::make_pair(memberPath, referred);
  }

  std::string getWholeArchiveString() const { return "-whole-archive"; }

  llvm::DenseSet<plugin::LinkerWrapper *> &getPlugins() { return Plugins; }

  const RemoveSymbolOpsMapT &getRemovedSymbols() { return RemovedSymbols; }

  ChunkOpsMapT &getChunkOps() { return ChunkOps; }

  SectionOpsMapT &getSectionOps() { return ChangeOutputSectionOps; }

  PluginOpsMapT &getPluginOps() { return PluginOps; }

  LinkerConfig &getConfig() const { return ThisConfig; }

  FragmentInfoMapT &getFragmentInfoMap() { return FragmentInfoMap; }

  SectionInfoMapT &getSectionInfoMap() { return SectionInfoMap; }

  PluginInfoSectionMapT &getPluginInfo() { return ThisPluginInfo; }

  ResolveInfoVectorT getAllocatedCommonSymbols(Module &Module);

  ResolveInfoVectorT getCommonsGarbageCollected(Module &Module);

  std::vector<std::string> &getFeatures() { return Features; }

  Stats &getLinkStats() { return LinkStats; }

  std::vector<ArchiveReferenceRecordT> &getArchiveRecords() {
    return ArchiveRecords;
  }

  ScriptVectorT &getLinkerScripts() { return LinkerScripts; }

  std::vector<std::string> &getVersionScripts() { return VersionScripts; }

  InputSequenceVectorT &getInputActions() { return InputActions; }

  std::vector<std::string> &getComments() { return Comments; }

  void buildMergedStringMap(Module &M);

  void addMergedStrings(MergeableString *From, MergeableString *To) {
    assert(From != To);
    if (To->Fragment->getOutputELFSection()->name().starts_with(".debug_str") &&
        !showDebugStrings())
      return;
    MergedStrings[From].push_back(To);
  }

  std::vector<MergeableString *> getMergedStrings(MergeableString *S) const {
    auto Fragments = MergedStrings.find(S);
    if (Fragments == MergedStrings.end())
      return {};
    return MergedStrings.at(S);
  }

  static std::optional<std::string> getBasepath() { return ThisBasepath; }

  // -------------------------- Stats ---------------------------------
  void registerStats(void *H, const LinkStats *R) {
    HandleToStats[H];
    HandleToStats[H].insert(R);
  }

  void printStats(void *H, llvm::raw_ostream &OS) const;

  bool showSymbolResolution() const {
    return LayoutDetail & LayoutDetail::ShowSymbolResolution;
  }

private:
  Stats LinkStats;
  std::vector<std::string> Features;
  PluginInfoSectionMapT ThisPluginInfo;
  PluginOpsMapT PluginOps;
  SectionOpsMapT ChangeOutputSectionOps;
  ChunkOpsMapT ChunkOps;
  RemoveSymbolOpsMapT RemovedSymbols;
  llvm::DenseSet<plugin::LinkerWrapper *> Plugins;
  // FIXME: Why do member names here start from underscore?
  // Names starting from an underscore is generally used to
  // indicate internal implementation use.
  InputSequenceVectorT InputActions;
  StringVectorT ScriptIncludes;
  std::vector<ArchiveReferenceRecordT> ArchiveRecords;
  FragmentInfoMapT FragmentInfoMap;
  SectionInfoMapT SectionInfoMap;
  FragmentInfoVectorT FragmentInfoVector;
  std::stack<std::string> LinkerScriptStack;
  ScriptVectorT LinkerScripts;
  std::vector<std::string> VersionScripts;
  std::vector<std::string> Comments;
  std::unordered_map<MergeableString *, std::vector<MergeableString *>>
      MergedStrings;
  LinkerConfig &ThisConfig;
  /// It is required to compute relative path when -MapDetail
  /// 'show-relative-path=...' is used.
  // It needs to be 'static' because LayoutInfo::setLayoutDetail member
  // function is static.
  static std::optional<std::string> ThisBasepath;
  std::optional<uint32_t> OutputFileSize;
  std::unordered_map<void *, std::unordered_set<const class LinkStats *>>
      HandleToStats;
};

} // namespace eld

#endif
