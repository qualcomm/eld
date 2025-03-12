//===- YamlLayoutPrinter.cpp-----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#include "eld/LayoutMap/YamlLayoutPrinter.h"
#include "eld/Config/Version.h"
#include "eld/Core/Module.h"
#include "eld/Input/ArchiveMemberInput.h"
#include "eld/Input/ObjectFile.h"
#include "eld/Object/ObjectLinker.h"
#include "eld/Readers/Relocation.h"
#include "eld/Support/StringRefUtils.h"
#include "eld/SymbolResolver/NamePool.h"
#include "eld/Target/ELFSegment.h"
#include "eld/Target/ELFSegmentFactory.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Compression.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Signals.h"
#include <string>

using namespace eld;
using namespace llvm;

YamlLayoutPrinter::YamlLayoutPrinter(LayoutPrinter *P)
    : layoutFile(nullptr), trampolineLayoutFile(nullptr), m_LayoutPrinter(P) {}

eld::Expected<void> YamlLayoutPrinter::init() {
  if (m_LayoutPrinter->getConfig().options().printMap())
    return {};
  std::string SuffixExtension = "";
  if (!m_LayoutPrinter->getConfig().options().isDefaultMapStyleYAML())
    SuffixExtension = ".yaml";
  if (!m_LayoutPrinter->getConfig().options().layoutFile().empty()) {
    std::string layoutFileName =
        m_LayoutPrinter->getConfig().options().layoutFile();
    if (!SuffixExtension.empty())
      layoutFileName =
          m_LayoutPrinter->getConfig().options().layoutFile() + SuffixExtension;
    std::error_code error;
    layoutFile =
        new llvm::raw_fd_ostream(layoutFileName, error, llvm::sys::fs::OF_None);
    if (error) {
      return std::make_unique<plugin::DiagnosticEntry>(
          plugin::FatalDiagnosticEntry(diag::fatal_unwritable_output,
                                       {layoutFileName, error.message()}));
    }
    Defaults.push_back(
        {"NoVerify",
         std::to_string(m_LayoutPrinter->getConfig().options().verifyLink()),
         "Enable Linker verification"});
    Defaults.push_back(
        {"MaxGPSize",
         std::to_string(m_LayoutPrinter->getConfig().options().getGPSize()),
         "GP Size value for Small Data"});
    char const *Separator = "";
    for (auto arg : m_LayoutPrinter->getConfig().options().Args()) {
      CommandLine.append(Separator);
      Separator = " ";
      if (arg)
        CommandLine.append(std::string(arg));
    }
  }
  m_LayoutPrinter->getConfig().options().setBuildCRef();
  return {};
}

llvm::raw_ostream &YamlLayoutPrinter::outputStream() const {
  if (layoutFile) {
    layoutFile->flush();
    return *layoutFile;
  }
  return llvm::errs();
}

void YamlLayoutPrinter::insertCommons(std::vector<eld::LDYAML::Common> &Commons,
                                      const std::vector<ResolveInfo *> &Infos) {
  for (auto &I : Infos) {
    eld::LDYAML::Common Common;
    Common.Name =
        m_LayoutPrinter->showSymbolName(StringRef(I->name(), I->nameSize()));
    Common.Size = I->size();
    InputFile *Input = I->resolvedOrigin();
    if (Input != nullptr)
      Common.InputPath = m_LayoutPrinter->getPath(Input->getInput());
    if (Input->getInput()->isArchiveMember() > 0)
      Common.InputName = Input->getInput()->getName();
    Commons.push_back(Common);
  }
}

void YamlLayoutPrinter::addInputs(
    std::vector<std::shared_ptr<eld::LDYAML::InputFile>> &Inputs,
    eld::Module &Module) {
  InputBuilder &inputBuilder = Module.getIRBuilder()->getInputBuilder();
  const std::vector<Node *> &inputVector = inputBuilder.getInputs();
  eld::LDYAML::RegularInput I;
  eld::LDYAML::Archive A;
  for (auto &input : inputVector) {
    FileNode *node = llvm::dyn_cast<FileNode>(input);
    if (!node)
      continue;
    Input *inp = node->getInput();
    InputFile *InpFile = inp->getInputFile();
    if (!InpFile)
      continue;
    if (InpFile->isArchive()) {
      A.Name = m_LayoutPrinter->getPath(inp);
      A.Used = InpFile->isUsed();
      for (auto &M :
           llvm::dyn_cast<eld::ArchiveFile>(InpFile)->GetAllMembers()) {
        I.Name = llvm::dyn_cast<eld::ArchiveMemberInput>(M)->getName();
        I.Used = M->getInputFile()->isUsed();
        A.ArchiveMembers.push_back(
            std::make_shared<eld::LDYAML::RegularInput>(std::move(I)));
      }
      Inputs.push_back(std::make_shared<eld::LDYAML::Archive>(std::move(A)));
      continue;
    }
    I.Name = m_LayoutPrinter->getPath(inp);
    I.Used = InpFile->isUsed();
    Inputs.push_back(std::make_shared<eld::LDYAML::RegularInput>(std::move(I)));
  }
}

eld::LDYAML::LinkStats YamlLayoutPrinter::addStat(std::string S,
                                                  uint64_t Count) {
  eld::LDYAML::LinkStats L;
  L.Name = S;
  L.Count = Count;
  return L;
}

void YamlLayoutPrinter::addStats(LayoutPrinter::Stats &L,
                                 std::vector<eld::LDYAML::LinkStats> &S) {
  S.push_back(addStat("ObjectFiles", L.numELFObjectFiles));
  S.push_back(addStat("LinkerScripts", L.numLinkerScripts));
  S.push_back(addStat("SharedObjects", L.numSharedObjectFiles));
  S.push_back(addStat("SymDefFiles", L.numSymDefFiles));
  S.push_back(addStat("ThreadCount", L.numThreads));
  S.push_back(addStat("ArchiveFiles", L.numArchiveFiles));
  S.push_back(addStat("GroupTraversals", L.numGroupTraversal));
  S.push_back(addStat("BitcodeFiles", L.numBitCodeFiles));
  S.push_back(addStat("ZeroSizedSections", L.numZeroSizedSection));
  S.push_back(
      addStat("SectionsGarbageCollected", L.numSectionsGarbageCollected));
  S.push_back(addStat("ZeroSizedSectionsGarbageCollected",
                      L.numZeroSizedSectionsGarbageCollected));
  S.push_back(addStat("NumLinkerScriptRules", L.numLinkerScriptRules));
  S.push_back(addStat("NumOutputSections", L.numOutputSections));
  S.push_back(addStat("NumPlugins", L.numPlugins));
  S.push_back(addStat("NumOrphans", L.numOrphans));
  S.push_back(addStat("NumTrampolines", L.numTrampolines));
  S.push_back(addStat("NoRuleMatches", L.numNoRuleMatch));
  S.push_back(addStat("LinkTime", L.LinkTime));
}

eld::LDYAML::Module
eld::YamlLayoutPrinter::buildYaml(eld::Module &Module,
                                  GNULDBackend const &backend) {
  bool HasSectionsCmd = Module.getScript().linkerScriptHasSectionsCommand();
  eld::LDYAML::Module Result;
  Result.ModuleHeader.Architecture =
      Module.getConfig().targets().triple().getArchName().str();
  std::string flagString =
      backend.getInfo().flagString(backend.getInfo().flags());
  if (!flagString.empty())
    Result.ModuleHeader.Emulation = flagString;

  Result.ModuleHeader.AddressSize =
      Module.getConfig().targets().is32Bits() ? "32bit" : "64bit";
  Result.ModuleHeader.ABIPageSize = backend.abiPageSize();
  for (auto const &I : Defaults) {
    Result.CommandLineDefaults.push_back({I.Name, I.Value, I.Desc.str()});
  }
  Result.ModuleVersion.VendorVersion = eld::getVendorVersion().str();
  Result.ModuleVersion.ELDVersion = eld::getELDVersion().str();

  for (auto &I : m_LayoutPrinter->getArchiveRecords()) {
    std::pair<std::string, std::string> ret =
        m_LayoutPrinter->getArchiveRecord(I);
    Result.ArchiveRecords.push_back({ret.first, ret.second});
  }
  insertCommons(Result.Commons,
                m_LayoutPrinter->getAllocatedCommonSymbols(Module));
  addStats(m_LayoutPrinter->getLinkStats(), Result.Stats);
  Result.Features = m_LayoutPrinter->getFeatures();
  addInputs(Result.InputFileInfo, Module);
  for (auto &I : m_LayoutPrinter->getInputActions()) {
    std::string action = m_LayoutPrinter->getStringFromLoadSequence(I);
    Result.InputActions.push_back(action);
  }
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
    Result.LinkerScriptsUsed.push_back(Indent + LinkerScriptName);
  }
  Result.BuildType = Module.getConfig().codeGenType();
  Result.OutputFile = Module.getConfig().options().outputFileName();
  Result.CommandLine = CommandLine;
  Result.EntryAddress = getEntryAddress(Module, backend);
  for (auto I : Module.getScript().sectionMap()) {
    auto Section = I->getSection();
    if (!(m_LayoutPrinter->showHeaderDetails() &&
          (Section->name() == "__ehdr__" || Section->name() == "__pHdr__")) &&
        (Section->isNullType() || (!HasSectionsCmd && !I->size() &&
                                   !Section->isWanted() && !Section->size())))
      continue;
    eld::LDYAML::OutputSection Value;
    Value.Name = Section->name().str();
    Value.Type = Section->getType();
    Value.Address = Section->addr();
    Value.Offset = Section->offset();
    Value.OutputPermissions = Section->getFlags();
    Value.Size = Section->size();

    // print padding from the start of the output seciton to the first fragment
    for (const GNULDBackend::PaddingT &p : backend.getPaddingBetweenFragments(
             Section, nullptr, I->getFirstFrag())) {
      eld::LDYAML::Padding Padding;
      Padding.Name = "LINKER_SCRIPT_PADDING";
      Padding.Offset = p.startOffset;
      Padding.PaddingValue = p.endOffset - p.startOffset;
      Value.Inputs.push_back(
          std::make_shared<eld::LDYAML::Padding>(std::move(Padding)));
    }
    for (auto J = I->begin(), N = I->end(); J != N; ++J) {
      ELFSection *IS = (*J)->getSection();
      for (auto K = (*J)->sym_begin(), M = (*J)->sym_end(); K != M; ++K) {
        std::string Text;
        {
          llvm::raw_string_ostream Stream(Text);
          (*K)->getExpression()->dump(Stream, true);
        }
        eld::LDYAML::Assignment Assignment;
        Assignment.Name = (*K)->name();
        Assignment.Value = (*K)->value();
        Assignment.Text = std::move(Text);
        Value.Inputs.push_back(
            std::make_shared<eld::LDYAML::Assignment>(std::move(Assignment)));
      }
      Fragment *Frag = nullptr;
      bool FoundFrag = false;
      if (IS && IS->getFragmentList().size())
        Frag = IS->getFragmentList().front();
      if (Frag) {
        auto FragCur = Frag->getIterator();
        auto FragEnd = Frag->getOwningSection()
                           ->getMatchedLinkerScriptRule()
                           ->getSection()
                           ->getFragmentList()
                           .end();
        DiagnosticEngine *DiagEngine = Module.getConfig().getDiagEngine();
        for (; FragCur != FragEnd; ++FragCur) {
          auto Frag = *FragCur;
          if (Frag->paddingSize() > 0) {
            eld::LDYAML::Padding Padding;
            Padding.Name = "ALIGNMENT_PADDING";
            Padding.Offset = Frag->getOffset(DiagEngine) - Frag->paddingSize();
            Padding.PaddingValue = Frag->paddingSize();
            Value.Inputs.push_back(
                std::make_shared<eld::LDYAML::Padding>(std::move(Padding)));
          }
          auto Existing = m_LayoutPrinter->getFragmentInfoMap().find(Frag);
          if (Existing != m_LayoutPrinter->getFragmentInfoMap().end()) {
            eld::LDYAML::InputSection Input;
            eld::LDYAML::InputBitcodeSection BCInput;
            eld::LDYAML::InputSection *ELFInputSection = &Input;
            bool isBitcode = false;

            auto Info = Existing->second;
            if (Info->section()->hasOldInputFile())
              isBitcode = true;

            if (!isBitcode) {
              ELFInputSection->Name = Info->name();
              ELFInputSection->Offset = Frag->getOffset(DiagEngine);
              ELFInputSection->Size = Frag->size();
            } else {
              BCInput.Name = Info->name();
              BCInput.Offset = Frag->getOffset(DiagEngine);
              BCInput.Size = Frag->size();
            }
            auto Desc = (*J)->desc();
            if (Desc != nullptr) {
              if (!isBitcode) {
                llvm::raw_string_ostream stream(Input.LinkerScript);
                Desc->dumpMap(stream, false, false);
                stream.flush();
              } else {
                llvm::raw_string_ostream stream(BCInput.LinkerScript);
                Desc->dumpMap(stream, false, false);
                stream.flush();
              }
            }
            if (!isBitcode) {
              ELFInputSection->Type = Info->type();
              ELFInputSection->Origin = Info->getResolvedPath();
              ELFInputSection->Alignment = Frag->alignment();
              ELFInputSection->InputPermissions = Info->flag();
            } else {
              BCInput.Type = Info->type();
              BCInput.Origin = Info->getResolvedPath();
              BCInput.Alignment = Frag->alignment();
              BCInput.InputPermissions = Info->flag();
            }
            if (isBitcode)
              BCInput.BitcodeOrigin =
                  Info->section()->getOldInputFile()->getInput()->decoratedPath(
                      m_LayoutPrinter->showAbsolutePath());
            m_LayoutPrinter->sortFragmentSymbols(Info);
            for (auto K : Info->m_symbols) {
              eld::InputFile *RO = K->resolveInfo()
                                       ? K->resolveInfo()->resolvedOrigin()
                                       : nullptr;
              if (!RO->isBitcode() &&
                  (RO->getInput()->decoratedPath(
                       m_LayoutPrinter->showAbsolutePath()) !=
                   Info->getResolvedPath()))
                continue;
              if (isBitcode) {
                BCInput.Symbols.push_back(
                    {K->name(), K->type(), K->binding(), K->size(),
                     (llvm::yaml::Hex64)m_LayoutPrinter->calculateSymbolValue(
                         K, Module)});
                continue;
              }
              ELFInputSection->Symbols.push_back(
                  {K->name(), K->type(), K->binding(), K->size(),
                   (llvm::yaml::Hex64)m_LayoutPrinter->calculateSymbolValue(
                       K, Module)});
            }
            if (isBitcode && !BCInput.Symbols.empty())
              Value.Inputs.emplace_back(
                  std::make_shared<eld::LDYAML::InputBitcodeSection>(
                      std::move(BCInput)));
            else
              Value.Inputs.emplace_back(
                  std::make_shared<eld::LDYAML::InputSection>(
                      std::move(Input)));
          }
          FoundFrag = true;
        }
      }
      if (!FoundFrag && (*J)->desc()) {
        eld::LDYAML::LinkerScriptRule EmptyRule;
        auto Desc = (*J)->desc();
        llvm::raw_string_ostream stream(EmptyRule.LinkerScript);
        Desc->dumpMap(stream, false, false);
        stream.flush();
        Value.Inputs.push_back(std::make_shared<eld::LDYAML::LinkerScriptRule>(
            std::move(EmptyRule)));
      }
      // print padding between current rule and next rule or end
      const RuleContainer *NextRuleWithContent = (*J)->getNextRuleWithContent();
      if ((*J)->hasContent())
        for (const GNULDBackend::PaddingT &p :
             backend.getPaddingBetweenFragments(
                 Section, (*J)->getLastFrag(),
                 NextRuleWithContent ? NextRuleWithContent->getFirstFrag()
                                     : nullptr)) {
          eld::LDYAML::Padding Padding;
          Padding.Name = "LINKER_SCRIPT_PADDING";
          Padding.Offset = p.startOffset;
          Padding.PaddingValue = p.endOffset - p.startOffset;
          Value.Inputs.push_back(
              std::make_shared<eld::LDYAML::Padding>(std::move(Padding)));
        }
    }
    Result.OutputSections.emplace_back(
        std::make_shared<eld::LDYAML::OutputSection>(std::move(Value)));
    for (OutputSectionEntry::sym_iterator it = I->sectionendsym_begin(),
                                          ie = I->sectionendsym_end();
         it != ie; ++it) {
      eld::LDYAML::Assignment Assignment;
      Assignment.Name = (*it)->name();
      Assignment.Value = (*it)->value();
      {
        raw_string_ostream Stream(Assignment.Text);
        (*it)->getExpression()->dump(Stream);
        Stream.flush();
      }
      Result.OutputSections.emplace_back(
          std::make_shared<eld::LDYAML::Assignment>(std::move(Assignment)));
    }
  }
  Module::const_obj_iterator obj, objEnd = Module.obj_end();
  for (obj = Module.obj_begin(); obj != objEnd; ++obj) {
    ObjectFile *ObjFile = llvm::dyn_cast<ObjectFile>(*obj);
    if (ObjFile->isInternal())
      continue;
    for (auto &sect : ObjFile->getSections()) {
      if (sect->isBitcode())
        continue;
      ELFSection *section = llvm::dyn_cast<eld::ELFSection>(sect);
      if (!section->isIgnore() && !section->isDiscard())
        continue;
      if (!section->hasSectionData()) {
        auto Existing = m_LayoutPrinter->getSectionInfoMap().find(section);
        if (Existing == m_LayoutPrinter->getSectionInfoMap().end())
          continue;
        eld::LDYAML::DiscardedSection DS;
        eld::LDYAML::DiscardedSection *ELFDiscardedSection = &DS;
        ELFDiscardedSection->Name = section->name().str();
        ELFDiscardedSection->Size = section->size();
        ELFDiscardedSection->Type = section->getType();
        ELFDiscardedSection->Origin =
            Existing->second->getInput()->decoratedPath(
                m_LayoutPrinter->showAbsolutePath());
        ELFDiscardedSection->Alignment = section->getAddrAlign();
        ELFDiscardedSection->InputPermissions = section->getFlags();
        Result.DiscardedSectionGroups.emplace_back(
            std::make_shared<eld::LDYAML::DiscardedSection>(std::move(DS)));
        continue;
      }
      for (auto &f : section->getFragmentList()) {
        auto Existing = m_LayoutPrinter->getFragmentInfoMap().find(f);
        if (Existing == m_LayoutPrinter->getFragmentInfoMap().end())
          continue;
        eld::LDYAML::DiscardedSection DS;
        eld::LDYAML::DiscardedSection *ELFDiscardedSection = &DS;
        auto Info = Existing->second;
        ELFDiscardedSection->Name = Info->name();
        ELFDiscardedSection->Size = f->size();
        ELFDiscardedSection->Type = Info->type();
        ELFDiscardedSection->Origin = Info->getResolvedPath();
        ELFDiscardedSection->Alignment = f->alignment();
        ELFDiscardedSection->InputPermissions = Info->flag();
        m_LayoutPrinter->sortFragmentSymbols(Info);
        for (auto K : Info->m_symbols)
          ELFDiscardedSection->Symbols.push_back(
              {K->name(), K->type(), K->binding(), K->size(), 0});
        Result.DiscardedSections.emplace_back(
            std::make_shared<eld::LDYAML::DiscardedSection>(std::move(DS)));
      }
    }
  }
  for (auto &G : Module.getNamePool().getGlobals()) {
    ResolveInfo *info = G.getValue();
    if (info->isCommon() && (info->outSymbol()->shouldIgnore())) {
      eld::LDYAML::Common S;
      S.Name = m_LayoutPrinter->showSymbolName(info->name());
      S.Size = info->size();
      InputFile *Input = info->resolvedOrigin();
      if (Input != nullptr)
        S.InputPath = m_LayoutPrinter->getPath(Input->getInput());
      if (Input->getInput()->isArchiveMember())
        S.InputName = Input->getInput()->getName();
      Result.DiscardedCommons.emplace_back(S);
    }
  }
  const std::vector<ELFSegment *> &segments =
      backend.elfSegmentTable().segments();
  int segNum = 0;
  for (auto &seg : segments) {
    ++segNum;
    eld::LDYAML::LoadRegion L;
    L.SegmentName = seg->name();
    if (L.SegmentName.empty())
      L.SegmentName = "Segment #" + std::to_string(segNum);
    L.Type = seg->type();
    L.VirtualAddress = seg->vaddr();
    L.PhysicalAddress = seg->paddr();
    L.FileOffset = seg->offset();
    L.SegPermission = seg->flag();
    L.FileSize = seg->filesz();
    L.MemorySize = seg->memsz();
    L.Alignment = seg->align();
    for (auto &sec : seg->sections())
      L.Sections.push_back(sec->name().str());
    Result.LoadRegions.push_back(L);
  }
  const GeneralOptions::CrefTable &table =
      m_LayoutPrinter->getConfig().options().crefTable();
  std::vector<const ResolveInfo *> symVector;
  GeneralOptions::CrefTable::const_iterator itr = table.begin(),
                                            ite = table.end();
  for (; itr != ite; itr++)
    symVector.push_back(itr->first);

  std::sort(symVector.begin(), symVector.end(),
            [](const ResolveInfo *A, const ResolveInfo *B) {
              return (std::string(A->name()) < std::string(B->name()));
            });

  for (auto i : symVector) {
    eld::LDYAML::CRef C;

    const std::vector<std::pair<const InputFile *, bool>> refSites =
        table.find(i)->second;

    C.symbolName = i->name();

    for (auto j : refSites) {
      if (j.second)
        continue;
      C.FileRefs.push_back(j.first->getInput()->decoratedPath());
    }
    Result.CrossReferences.push_back(C);
  }

  getTrampolineMap(Module, Result.TrampolinesGenerated);

  return Result;
}

void eld::YamlLayoutPrinter::getTrampolineMap(
    eld::Module &Module, std::vector<eld::LDYAML::TrampolineInfo> &R) {
  for (auto &OutputSection : Module.getScript().sectionMap()) {
    eld::LDYAML::TrampolineInfo TInfo;
    TInfo.OutputSectionName = OutputSection->name().str();
    typedef std::vector<BranchIsland *>::iterator branch_island_iter;
    if (!OutputSection->getNumBranchIslands())
      continue;
    bool hasBranchIsland = false;
    branch_island_iter bi = OutputSection->islands_begin();
    branch_island_iter be = OutputSection->islands_end();
    for (; bi != be; ++bi) {
      eld::LDYAML::Trampoline T;
      // ARMv7 uses the same branch island interface, but some of the stuff
      // that exists in branch island interface are not stored by the ARM
      // backend.
      if (!(*bi)->stub())
        continue;

      if (!(*bi)->stub()->symInfo())
        continue;

      if (!(*bi)->stub()->savedSymInfo())
        continue;
      hasBranchIsland = true;

      // Name of the trampoline.
      T.Name = (*bi)->stub()->symInfo()->name();
      // The file where the source is located.
      eld::Relocation *OrigRelocation = (*bi)->getOrigRelocation();
      Fragment *PatchFrag = OrigRelocation->targetRef()->frag();
      T.From = PatchFrag->getOwningSection()->name().str();
      {
        auto Existing = m_LayoutPrinter->getFragmentInfoMap().find(PatchFrag);
        if (Existing != m_LayoutPrinter->getFragmentInfoMap().end()) {
          auto Info = Existing->second;
          for (auto K : Info->m_symbols) {
            if (!K->isSection())
              T.FromSymbols.push_back({K->name(), K->type(), K->size()});
          }
        }
      }
      T.FromFile = PatchFrag->getOwningSection()
                       ->getInputFile()
                       ->getInput()
                       ->decoratedPath(m_LayoutPrinter->showAbsolutePath());
      T.To = (*bi)->stub()->savedSymInfo()->name();
      if (!(*bi)->stub()->savedSymInfo()->isAbsolute()) {
        T.ToSection = (*bi)
                          ->stub()
                          ->savedSymInfo()
                          ->outSymbol()
                          ->fragRef()
                          ->frag()
                          ->getOutputELFSection()
                          ->name()
                          .str();
      } else {
        T.ToSection = "No Section";
      }
      T.ToFile = (*bi)
                     ->stub()
                     ->savedSymInfo()
                     ->resolvedOrigin()
                     ->getInput()
                     ->decoratedPath(m_LayoutPrinter->showAbsolutePath());
      T.Addend = (*bi)->getAddend();
      {
        if ((*bi)->stub()->savedSymInfo()->isLocal()) {
          Fragment *ToFrag =
              (*bi)->stub()->savedSymInfo()->outSymbol()->fragRef()->frag();
          auto Existing = m_LayoutPrinter->getFragmentInfoMap().find(ToFrag);
          if (Existing != m_LayoutPrinter->getFragmentInfoMap().end()) {
            auto Info = Existing->second;
            for (auto K : Info->m_symbols) {
              if (!K->isSection())
                T.ToSymbols.push_back({K->name(), K->type(), K->size()});
            }
          }
        }
      }
      std::set<Fragment *> RSet;

      for (auto &Reloc : (*bi)->getReuses()) {
        eld::LDYAML::Reuse R;
        Fragment *F = Reloc->targetRef()->frag();
        if (RSet.find(F) != RSet.end())
          continue;
        RSet.insert(F);
        R.From = F->getOwningSection()->name().str();
        auto Existing = m_LayoutPrinter->getFragmentInfoMap().find(F);
        if (Existing != m_LayoutPrinter->getFragmentInfoMap().end()) {
          auto Info = Existing->second;
          for (auto K : Info->m_symbols) {
            if (!K->isSection())
              R.Symbols.push_back({K->name(), K->type(), K->size()});
          }
        }
        R.FromFile =
            F->getOwningSection()->getInputFile()->getInput()->decoratedPath(
                m_LayoutPrinter->showAbsolutePath());
        R.Addend = Reloc->addend();
        T.Uses.push_back(R);
      }
      TInfo.Trampolines.push_back(T);
    }
    if (!hasBranchIsland)
      continue;
    R.push_back(TInfo);
  }
}

uint64_t YamlLayoutPrinter::getEntryAddress(eld::Module const &pModule,
                                            GNULDBackend const &backend) {
  /// getEntryPoint
  llvm::StringRef entry_name = backend.getEntry();
  uint64_t result = 0x0;

  if (string::isValidCIdentifier(entry_name)) {
    const LDSymbol *entry_symbol =
        pModule.getNamePool().findSymbol(entry_name.str());
    if (!entry_symbol)
      return (result = 0);
    result = entry_symbol->value();
  } else {
    if (entry_name.getAsInteger(0, result)) {
      return (result = 0);
    }
  }
  return result;
}

void YamlLayoutPrinter::printLayout(eld::Module &Module,
                                    GNULDBackend const &backend) {
  DiagnosticEngine *DiagEngine = backend.config().getDiagEngine();
  // Open Trampoline Map file.
  if (!m_LayoutPrinter->getConfig().options().getTrampolineMapFile().empty()) {
    std::string File =
        m_LayoutPrinter->getConfig().options().getTrampolineMapFile().str();
    std::error_code error;
    trampolineLayoutFile =
        new llvm::raw_fd_ostream(File, error, llvm::sys::fs::OF_None);
    if (error) {
      DiagEngine->raise(diag::fatal_unwritable_output)
          << File << error.message();
      exit(1);
    }
  }

  auto yaml = buildYaml(Module, backend);
  llvm::ArrayRef<std::string> MapStyles =
      m_LayoutPrinter->getConfig().options().mapStyle();
  if (std::find(MapStyles.begin(), MapStyles.end(), "compressed") ==
      MapStyles.end()) {
    yaml::Output Yout(outputStream());
    Yout << yaml;
    outputStream().flush();
    if (trampolineLayoutFile) {
      yaml::Output Tout(*trampolineLayoutFile);
      Tout << yaml.TrampolinesGenerated;
      trampolineLayoutFile->flush();
    }
    return;
  }

  if (trampolineLayoutFile) {
    yaml::Output Tout(*trampolineLayoutFile);
    Tout << yaml.TrampolinesGenerated;
    trampolineLayoutFile->flush();
  }
#if LLVM_ENABLE_ZLIB == 1 && HAVE_LIBZ
  std::string Buf;
  llvm::SmallString<128> CompressedData;
  llvm::raw_string_ostream OS(Buf);
  yaml::Output Yout(OS);
  Yout << yaml;
  OS.flush();
  auto E = llvm::zlib::compress(OS.str(), CompressedData,
                                llvm::zlib::BestSizeCompression);
  if (E) {
    DiagEngine->raise(diag::unable_to_compress)
        << m_Config.options().layoutFile();
    return;
  }
  outputStream() << CompressedData;
#else
  yaml::Output Yout(outputStream());
  Yout << yaml;
#endif

  outputStream().flush();
}
