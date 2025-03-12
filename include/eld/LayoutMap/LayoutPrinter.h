//===- LayoutPrinter.h-----------------------------------------------------===//
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
  LayoutFragmentInfo(InputFile *F, const ELFSection *section)
      : m_InputFile(F), m_Section(section) {}

  LayoutFragmentInfo(const ELFSection *section)
      : m_InputFile(nullptr), m_Section(section) {}

  InputFile *m_InputFile;
  const ELFSection *m_Section;

  std::string getResolvedPath() const {
    if (m_InputFile != nullptr)
      return m_InputFile->getInput()->decoratedPath();
    return "";
  }

  std::string getDecoratedName(const GeneralOptions &options) const {
    return m_Section->getDecoratedName(options);
  }

  std::string name() const { return m_Section->name().str(); }

  std::string getRealName() const { return m_Section->name().str(); }

  uint32_t flag() const { return m_Section->getFlags(); }
  uint32_t type() const { return m_Section->getType(); }
  const ELFSection *section() const { return m_Section; }
  std::vector<LDSymbol *> m_symbols;
};

class LayoutPrinter {
  static uint32_t m_LayoutDetail;

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
    uint64_t numELFObjectFiles = 0;
    uint64_t numELFExecutableFiles = 0;
    uint64_t numLinkerScripts = 0;
    uint64_t numSharedObjectFiles = 0;
    uint64_t numSymDefFiles = 0;
    uint64_t numArchiveFiles = 0;
    uint64_t numGroupTraversal = 0;
    uint64_t numBitCodeFiles = 0;
    uint64_t numSectionsGarbageCollected = 0;
    uint64_t numZeroSizedSection = 0;
    uint64_t numZeroSizedSectionsGarbageCollected = 0;
    uint64_t numLinkerScriptRules = 0;
    uint64_t numOutputSections = 0;
    uint64_t numPlugins = 0;
    uint64_t numThreads = 0;
    uint64_t numOrphans = 0;
    uint64_t numTrampolines = 0;
    uint64_t numNoRuleMatch = 0;
    uint32_t LinkTime = 0;
    uint64_t numRetainedSections = 0;
    uint32_t numBinaryFiles = 0;
    std::optional<uint32_t> OutputFileSize;

    bool hasStats() const {
      return (numELFObjectFiles + numELFExecutableFiles + numLinkerScripts +
                  numSharedObjectFiles + numSymDefFiles + numArchiveFiles +
                  numGroupTraversal + numBitCodeFiles +
                  numSectionsGarbageCollected + numZeroSizedSection +
                  numZeroSizedSectionsGarbageCollected + numLinkerScriptRules +
                  numOutputSections + numPlugins + numOrphans + numTrampolines +
                  numNoRuleMatch + LinkTime + numRetainedSections +
                  numBinaryFiles >
              0);
    }
  };

  struct InputSequenceT {
    InputKindPrefix prefix;
    Input *input;
    std::string archFlag;
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
    std::string include;
    std::string parent;
    bool found = false;
    uint32_t Depth = 0;
  };
  typedef std::vector<ScriptInputT> ScriptVectorT;

  // -------------------End Typedefs ---------------------------------

  LayoutPrinter(LinkerConfig &pConfig);

  bool showStrings() const { return m_LayoutDetail & ShowStrings; }

  bool showOnlyLayout() const { return m_LayoutDetail & OnlyLayout; }

  bool showAbsolutePath() const { return m_LayoutDetail & ShowAbsolutePath; }

  bool showRelativePath() const { return m_LayoutDetail & ShowRelativePath; }

  bool dontShowTiming() const { return m_LayoutDetail & NoTimeStats; }

  bool showDebugStrings() const { return m_LayoutDetail & ShowDebugStrings; }

  bool showTimers() const {
    return !dontShowTiming() && (m_LayoutDetail & ShowTiming);
  }

  bool showHeaderDetails() const {
    return (m_LayoutDetail & ShowHeaderDetails);
  }

  bool showInitialLayout() const { return m_LayoutDetail & ShowInitialLayout; }

  static eld::Expected<void> setLayoutDetail(llvm::StringRef option,
                                             DiagnosticEngine *);

  void recordFragment(InputFile *input, const ELFSection *inputELFSection,
                      const Fragment *frag);

  // Record commandline history string from .comment
  void recordCommentFragment(const std::string &CommentStr) {
    m_Comments.push_back(CommentStr);
  }

  void recordSymbol(const Fragment *frag, LDSymbol *symbol);

  void recordThreadCount();

  void recordSectionStat(const Section *sect);

  void recordSection(ELFSection *s, InputFile *i) { _sectionInfoMap[s] = i; }

  void recordPlugin(ELFSection *S, Plugin *P) { m_PluginInfo[S].push_back(P); }

  void recordOutputFileSize(uint32_t sz) { LinkStats.OutputFileSize = sz; }

  // FIXME: Destructor is redundant here.
  ~LayoutPrinter() { destroy(); }

  // FIXME: This function is not required.
  void destroy() {
    _inputActions.clear();
    _scriptIncludes.clear();
    _archiveRecords.clear();
    _fragmentInfoMap.clear();
    _fragmentInfoVector.clear();
  }

  void recordArchiveMember(Input *origin, InputFile *referred,
                           ArchiveFile::Symbol *archSym, LDSymbol *sym);

  void recordWholeArchiveMember(Input *origin);

  std::string getPath(const Input *inp) const {
    if (showAbsolutePath())
      return inp->getResolvedPath().getFullPath();
    return inp->getResolvedPath().native();
  }

  std::string getFileTypeStringIfBitcode(InputFile *f) {
    return (f->isBitcode()) ? " (Bitcode type)" : "";
  }

  void resetArchiveRecords() { _archiveRecords.clear(); }

  std::string getStringFromLoadSequence(InputSequenceT ist);

  void recordInputActions(InputKindPrefix prefix, Input *input,
                          std::string fileType = "");

  void resetInputActions() { _inputActions.clear(); }

  std::string infoForFrag(const Fragment *frag);

  bool isSectionDetailedInfoAvailable(ELFSection *section);

  void sortFragmentSymbols(LayoutFragmentInfo *FragInfo);

  int64_t calculateSymbolValue(LDSymbol *Symbol, Module &M);

  std::string showSymbolName(llvm::StringRef Name) const;

  void recordInputKind(InputFile::Kind K);

  void recordGroup();

  void recordOutputSection();

  std::string getPath(const std::string &filename) const;

  void recordLinkerScript(std::string File, bool Found = true);

  void recordLinkerScriptRule();

  void recordOrphanSection();

  void recordGC(const ELFSection *section);

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
    m_VersionScripts.push_back(VersionScript);
  }

  std::pair<std::string, std::string>
  getArchiveRecord(ArchiveReferenceRecordT &itr) {
    // Only first item in tuple is not null in case of --whole-archive
    Input *orig = std::get<0>(itr);
    InputFile *ref = std::get<1>(itr);
    LDSymbol *sym = std::get<3>(itr);
    std::string SymName;
    if (sym)
      SymName = sym->name();
    else
      SymName = getWholeArchiveString();
    if (ref == nullptr) {
      return std::make_pair(orig->decoratedPath(), SymName);
    } else {
      std::string referred = ref->getInput()->decoratedPath();
      std::string inputType = getFileTypeStringIfBitcode(ref);
      referred = referred + " (" + SymName + ")" + inputType;
      std::string membType = getFileTypeStringIfBitcode(orig->getInputFile());
      std::string memberPath = orig->decoratedPath() + membType;
      return std::make_pair(memberPath, referred);
    }
  }

  std::string getWholeArchiveString() const { return "-whole-archive"; }

  llvm::DenseSet<plugin::LinkerWrapper *> &getPlugins() { return m_Plugins; }

  const RemoveSymbolOpsMapT &getRemovedSymbols() { return RemovedSymbols; }

  ChunkOpsMapT &getChunkOps() { return m_ChunkOps; }

  SectionOpsMapT &getSectionOps() { return m_ChangeOutputSectionOps; }

  PluginOpsMapT &getPluginOps() { return m_PluginOps; }

  LinkerConfig &getConfig() const { return m_Config; }

  FragmentInfoMapT &getFragmentInfoMap() { return _fragmentInfoMap; }

  SectionInfoMapT &getSectionInfoMap() { return _sectionInfoMap; }

  PluginInfoSectionMapT &getPluginInfo() { return m_PluginInfo; }

  ResolveInfoVectorT getAllocatedCommonSymbols(Module &module);

  ResolveInfoVectorT getCommonsGarbageCollected(Module &module);

  std::vector<std::string> &getFeatures() { return Features; }

  Stats &getLinkStats() { return LinkStats; }

  std::vector<ArchiveReferenceRecordT> &getArchiveRecords() {
    return _archiveRecords;
  }

  ScriptVectorT &getLinkerScripts() { return m_LinkerScripts; }

  std::vector<std::string> &getVersionScripts() { return m_VersionScripts; }

  InputSequenceVectorT &getInputActions() { return _inputActions; }

  std::vector<std::string> &getComments() { return m_Comments; }

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

  static std::optional<std::string> getBasepath() { return m_Basepath; }

  // -------------------------- Stats ---------------------------------
  void registerStats(void *H, const LinkStats *R) {
    HandleToStats[H];
    HandleToStats[H].insert(R);
  }

  void printStats(void *H, llvm::raw_ostream &OS) const;

  bool showSymbolResolution() const {
    return m_LayoutDetail & LayoutDetail::ShowSymbolResolution;
  }

private:
  Stats LinkStats;
  std::vector<std::string> Features;
  PluginInfoSectionMapT m_PluginInfo;
  PluginOpsMapT m_PluginOps;
  SectionOpsMapT m_ChangeOutputSectionOps;
  ChunkOpsMapT m_ChunkOps;
  RemoveSymbolOpsMapT RemovedSymbols;
  llvm::DenseSet<plugin::LinkerWrapper *> m_Plugins;
  // FIXME: Why do member names here start from underscore?
  // Names starting from an underscore is generally used to
  // indicate internal implementation use.
  InputSequenceVectorT _inputActions;
  StringVectorT _scriptIncludes;
  std::vector<ArchiveReferenceRecordT> _archiveRecords;
  FragmentInfoMapT _fragmentInfoMap;
  SectionInfoMapT _sectionInfoMap;
  FragmentInfoVectorT _fragmentInfoVector;
  std::stack<std::string> LinkerScriptStack;
  ScriptVectorT m_LinkerScripts;
  std::vector<std::string> m_VersionScripts;
  std::vector<std::string> m_Comments;
  std::unordered_map<MergeableString *, std::vector<MergeableString *>>
      MergedStrings;
  LinkerConfig &m_Config;
  /// It is required to compute relative path when -MapDetail
  /// 'show-relative-path=...' is used.
  // It needs to be 'static' because LayoutPrinter::setLayoutDetail member
  // function is static.
  static std::optional<std::string> m_Basepath;
  std::optional<uint32_t> m_OutputFileSize;
  std::unordered_map<void *, std::unordered_set<const class LinkStats *>>
      HandleToStats;
};

} // namespace eld

#endif
