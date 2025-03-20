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
    : LayoutFile(nullptr), TrampolineLayoutFile(nullptr), ThisLayoutPrinter(P) {
}

eld::Expected<void> YamlLayoutPrinter::init() {
  if (ThisLayoutPrinter->getConfig().options().printMap())
    return {};
  std::string SuffixExtension = "";
  if (!ThisLayoutPrinter->getConfig().options().isDefaultMapStyleYAML())
    SuffixExtension = ".yaml";
  if (!ThisLayoutPrinter->getConfig().options().layoutFile().empty()) {
    std::string LayoutFileName =
        ThisLayoutPrinter->getConfig().options().layoutFile();
    if (!SuffixExtension.empty())
      LayoutFileName = ThisLayoutPrinter->getConfig().options().layoutFile() +
                       SuffixExtension;
    std::error_code Error;
    LayoutFile =
        new llvm::raw_fd_ostream(LayoutFileName, Error, llvm::sys::fs::OF_None);
    if (Error) {
      return std::make_unique<plugin::DiagnosticEntry>(
          plugin::FatalDiagnosticEntry(Diag::fatal_unwritable_output,
                                       {LayoutFileName, Error.message()}));
    }
    Defaults.push_back(
        {"NoVerify",
         std::to_string(ThisLayoutPrinter->getConfig().options().verifyLink()),
         "Enable Linker verification"});
    Defaults.push_back(
        {"MaxGPSize",
         std::to_string(ThisLayoutPrinter->getConfig().options().getGPSize()),
         "GP Size value for Small Data"});
    char const *Separator = "";
    for (const auto *Arg : ThisLayoutPrinter->getConfig().options().args()) {
      CommandLine.append(Separator);
      Separator = " ";
      if (Arg)
        CommandLine.append(std::string(Arg));
    }
  }
  ThisLayoutPrinter->getConfig().options().setBuildCRef();
  return {};
}

llvm::raw_ostream &YamlLayoutPrinter::outputStream() const {
  if (LayoutFile) {
    LayoutFile->flush();
    return *LayoutFile;
  }
  return llvm::errs();
}

void YamlLayoutPrinter::insertCommons(std::vector<eld::LDYAML::Common> &Commons,
                                      const std::vector<ResolveInfo *> &Infos) {
  for (auto &I : Infos) {
    eld::LDYAML::Common Common;
    Common.Name =
        ThisLayoutPrinter->showSymbolName(StringRef(I->name(), I->nameSize()));
    Common.Size = I->size();
    InputFile *Input = I->resolvedOrigin();
    if (Input != nullptr)
      Common.InputPath = ThisLayoutPrinter->getPath(Input->getInput());
    if (Input->getInput()->isArchiveMember() > 0)
      Common.InputName = Input->getInput()->getName();
    Commons.push_back(Common);
  }
}

void YamlLayoutPrinter::addInputs(
    std::vector<std::shared_ptr<eld::LDYAML::InputFile>> &Inputs,
    eld::Module &Module) {
  InputBuilder &InputBuilder = Module.getIRBuilder()->getInputBuilder();
  const std::vector<Node *> &InputVector = InputBuilder.getInputs();
  eld::LDYAML::RegularInput I;
  eld::LDYAML::Archive A;
  for (auto &Input : InputVector) {
    FileNode *Node = llvm::dyn_cast<FileNode>(Input);
    if (!Node)
      continue;
    eld::Input *Inp = Node->getInput();
    InputFile *InpFile = Inp->getInputFile();
    if (!InpFile)
      continue;
    if (InpFile->isArchive()) {
      A.Name = ThisLayoutPrinter->getPath(Inp);
      A.Used = InpFile->isUsed();
      for (auto &M :
           llvm::dyn_cast<eld::ArchiveFile>(InpFile)->getAllMembers()) {
        I.Name = llvm::dyn_cast<eld::ArchiveMemberInput>(M)->getName();
        I.Used = M->getInputFile()->isUsed();
        A.ArchiveMembers.push_back(
            std::make_shared<eld::LDYAML::RegularInput>(std::move(I)));
      }
      Inputs.push_back(std::make_shared<eld::LDYAML::Archive>(std::move(A)));
      continue;
    }
    I.Name = ThisLayoutPrinter->getPath(Inp);
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
  S.push_back(addStat("ObjectFiles", L.NumElfObjectFiles));
  S.push_back(addStat("LinkerScripts", L.NumLinkerScripts));
  S.push_back(addStat("SharedObjects", L.NumSharedObjectFiles));
  S.push_back(addStat("SymDefFiles", L.NumSymDefFiles));
  S.push_back(addStat("ThreadCount", L.NumThreads));
  S.push_back(addStat("ArchiveFiles", L.NumArchiveFiles));
  S.push_back(addStat("GroupTraversals", L.NumGroupTraversal));
  S.push_back(addStat("BitcodeFiles", L.NumBitCodeFiles));
  S.push_back(addStat("ZeroSizedSections", L.NumZeroSizedSection));
  S.push_back(
      addStat("SectionsGarbageCollected", L.NumSectionsGarbageCollected));
  S.push_back(addStat("ZeroSizedSectionsGarbageCollected",
                      L.NumZeroSizedSectionsGarbageCollected));
  S.push_back(addStat("NumLinkerScriptRules", L.NumLinkerScriptRules));
  S.push_back(addStat("NumOutputSections", L.NumOutputSections));
  S.push_back(addStat("NumPlugins", L.NumPlugins));
  S.push_back(addStat("NumOrphans", L.NumOrphans));
  S.push_back(addStat("NumTrampolines", L.NumTrampolines));
  S.push_back(addStat("NoRuleMatches", L.NumNoRuleMatch));
  S.push_back(addStat("LinkTime", L.LinkTime));
}

eld::LDYAML::Module
eld::YamlLayoutPrinter::buildYaml(eld::Module &Module,
                                  GNULDBackend const &Backend) {
  bool HasSectionsCmd = Module.getScript().linkerScriptHasSectionsCommand();
  eld::LDYAML::Module Result;
  Result.ModuleHeader.Architecture =
      Module.getConfig().targets().triple().getArchName().str();
  std::string FlagString =
      Backend.getInfo().flagString(Backend.getInfo().flags());
  if (!FlagString.empty())
    Result.ModuleHeader.Emulation = FlagString;

  Result.ModuleHeader.AddressSize =
      Module.getConfig().targets().is32Bits() ? "32bit" : "64bit";
  Result.ModuleHeader.ABIPageSize = Backend.abiPageSize();
  for (auto const &I : Defaults) {
    Result.CommandLineDefaults.push_back({I.Name, I.Value, I.Desc.str()});
  }
  Result.ModuleVersion.VendorVersion = eld::getVendorVersion().str();
  Result.ModuleVersion.ELDVersion = eld::getELDVersion().str();

  for (auto &I : ThisLayoutPrinter->getArchiveRecords()) {
    std::pair<std::string, std::string> Ret =
        ThisLayoutPrinter->getArchiveRecord(I);
    Result.ArchiveRecords.push_back({Ret.first, Ret.second});
  }
  insertCommons(Result.Commons,
                ThisLayoutPrinter->getAllocatedCommonSymbols(Module));
  addStats(ThisLayoutPrinter->getLinkStats(), Result.Stats);
  Result.Features = ThisLayoutPrinter->getFeatures();
  addInputs(Result.InputFileInfo, Module);
  for (auto &I : ThisLayoutPrinter->getInputActions()) {
    std::string Action = ThisLayoutPrinter->getStringFromLoadSequence(I);
    Result.InputActions.push_back(Action);
  }
  for (auto &Script : ThisLayoutPrinter->getLinkerScripts()) {
    std::string Indent = "";
    for (uint32_t I = 0; I < Script.Depth; ++I)
      Indent += "\t";
    std::string LinkerScriptName = Script.Include;
    if (ThisLayoutPrinter->getConfig().options().hasMappingFile())
      LinkerScriptName = ThisLayoutPrinter->getPath(LinkerScriptName) + "(" +
                         LinkerScriptName + ")";
    if (!Script.Found)
      LinkerScriptName += "(NOTFOUND)";
    Result.LinkerScriptsUsed.push_back(Indent + LinkerScriptName);
  }
  Result.BuildType = Module.getConfig().codeGenType();
  Result.OutputFile = Module.getConfig().options().outputFileName();
  Result.CommandLine = CommandLine;
  Result.EntryAddress = getEntryAddress(Module, Backend);
  for (auto *I : Module.getScript().sectionMap()) {
    auto *Section = I->getSection();
    if (!(ThisLayoutPrinter->showHeaderDetails() &&
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
    for (const GNULDBackend::PaddingT &P : Backend.getPaddingBetweenFragments(
             Section, nullptr, I->getFirstFrag())) {
      eld::LDYAML::Padding Padding;
      Padding.Name = "LINKER_SCRIPT_PADDING";
      Padding.Offset = P.startOffset;
      Padding.PaddingValue = P.endOffset - P.startOffset;
      Value.Inputs.push_back(
          std::make_shared<eld::LDYAML::Padding>(std::move(Padding)));
    }
    for (auto J = I->begin(), N = I->end(); J != N; ++J) {
      ELFSection *IS = (*J)->getSection();
      for (auto K = (*J)->symBegin(), M = (*J)->symEnd(); K != M; ++K) {
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
        auto *FragCur = Frag->getIterator();
        auto *FragEnd = Frag->getOwningSection()
                            ->getMatchedLinkerScriptRule()
                            ->getSection()
                            ->getFragmentList()
                            .end();
        DiagnosticEngine *DiagEngine = Module.getConfig().getDiagEngine();
        for (; FragCur != FragEnd; ++FragCur) {
          auto *Frag = *FragCur;
          if (Frag->paddingSize() > 0) {
            eld::LDYAML::Padding Padding;
            Padding.Name = "ALIGNMENT_PADDING";
            Padding.Offset = Frag->getOffset(DiagEngine) - Frag->paddingSize();
            Padding.PaddingValue = Frag->paddingSize();
            Value.Inputs.push_back(
                std::make_shared<eld::LDYAML::Padding>(std::move(Padding)));
          }
          auto Existing = ThisLayoutPrinter->getFragmentInfoMap().find(Frag);
          if (Existing != ThisLayoutPrinter->getFragmentInfoMap().end()) {
            eld::LDYAML::InputSection Input;
            eld::LDYAML::InputBitcodeSection BCInput;
            eld::LDYAML::InputSection *ELFInputSection = &Input;
            bool IsBitcode = false;

            auto *Info = Existing->second;
            if (Info->section()->hasOldInputFile())
              IsBitcode = true;

            if (!IsBitcode) {
              ELFInputSection->Name = Info->name();
              ELFInputSection->Offset = Frag->getOffset(DiagEngine);
              ELFInputSection->Size = Frag->size();
            } else {
              BCInput.Name = Info->name();
              BCInput.Offset = Frag->getOffset(DiagEngine);
              BCInput.Size = Frag->size();
            }
            const auto *Desc = (*J)->desc();
            if (Desc != nullptr) {
              if (!IsBitcode) {
                llvm::raw_string_ostream Stream(Input.LinkerScript);
                Desc->dumpMap(Stream, false, false);
                Stream.flush();
              } else {
                llvm::raw_string_ostream Stream(BCInput.LinkerScript);
                Desc->dumpMap(Stream, false, false);
                Stream.flush();
              }
            }
            if (!IsBitcode) {
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
            if (IsBitcode)
              BCInput.BitcodeOrigin =
                  Info->section()->getOldInputFile()->getInput()->decoratedPath(
                      ThisLayoutPrinter->showAbsolutePath());
            ThisLayoutPrinter->sortFragmentSymbols(Info);
            for (auto *K : Info->Symbols) {
              eld::InputFile *RO = K->resolveInfo()
                                       ? K->resolveInfo()->resolvedOrigin()
                                       : nullptr;
              if (!RO->isBitcode() &&
                  (RO->getInput()->decoratedPath(
                       ThisLayoutPrinter->showAbsolutePath()) !=
                   Info->getResolvedPath()))
                continue;
              if (IsBitcode) {
                BCInput.Symbols.push_back(
                    {K->name(), K->type(), K->binding(), K->size(),
                     (llvm::yaml::Hex64)ThisLayoutPrinter->calculateSymbolValue(
                         K, Module)});
                continue;
              }
              ELFInputSection->Symbols.push_back(
                  {K->name(), K->type(), K->binding(), K->size(),
                   (llvm::yaml::Hex64)ThisLayoutPrinter->calculateSymbolValue(
                       K, Module)});
            }
            if (IsBitcode && !BCInput.Symbols.empty())
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
        const auto *Desc = (*J)->desc();
        llvm::raw_string_ostream Stream(EmptyRule.LinkerScript);
        Desc->dumpMap(Stream, false, false);
        Stream.flush();
        Value.Inputs.push_back(std::make_shared<eld::LDYAML::LinkerScriptRule>(
            std::move(EmptyRule)));
      }
      // print padding between current rule and next rule or end
      const RuleContainer *NextRuleWithContent = (*J)->getNextRuleWithContent();
      if ((*J)->hasContent())
        for (const GNULDBackend::PaddingT &P :
             Backend.getPaddingBetweenFragments(
                 Section, (*J)->getLastFrag(),
                 NextRuleWithContent ? NextRuleWithContent->getFirstFrag()
                                     : nullptr)) {
          eld::LDYAML::Padding Padding;
          Padding.Name = "LINKER_SCRIPT_PADDING";
          Padding.Offset = P.startOffset;
          Padding.PaddingValue = P.endOffset - P.startOffset;
          Value.Inputs.push_back(
              std::make_shared<eld::LDYAML::Padding>(std::move(Padding)));
        }
    }
    Result.OutputSections.emplace_back(
        std::make_shared<eld::LDYAML::OutputSection>(std::move(Value)));
    for (OutputSectionEntry::sym_iterator It = I->sectionendsymBegin(),
                                          Ie = I->sectionendsymEnd();
         It != Ie; ++It) {
      eld::LDYAML::Assignment Assignment;
      Assignment.Name = (*It)->name();
      Assignment.Value = (*It)->value();
      {
        raw_string_ostream Stream(Assignment.Text);
        (*It)->getExpression()->dump(Stream);
        Stream.flush();
      }
      Result.OutputSections.emplace_back(
          std::make_shared<eld::LDYAML::Assignment>(std::move(Assignment)));
    }
  }
  Module::const_obj_iterator Obj, ObjEnd = Module.objEnd();
  for (Obj = Module.objBegin(); Obj != ObjEnd; ++Obj) {
    ObjectFile *ObjFile = llvm::dyn_cast<ObjectFile>(*Obj);
    if (ObjFile->isInternal())
      continue;
    for (auto &Sect : ObjFile->getSections()) {
      if (Sect->isBitcode())
        continue;
      ELFSection *Section = llvm::dyn_cast<eld::ELFSection>(Sect);
      if (!Section->isIgnore() && !Section->isDiscard())
        continue;
      if (!Section->hasSectionData()) {
        auto Existing = ThisLayoutPrinter->getSectionInfoMap().find(Section);
        if (Existing == ThisLayoutPrinter->getSectionInfoMap().end())
          continue;
        eld::LDYAML::DiscardedSection DS;
        eld::LDYAML::DiscardedSection *ELFDiscardedSection = &DS;
        ELFDiscardedSection->Name = Section->name().str();
        ELFDiscardedSection->Size = Section->size();
        ELFDiscardedSection->Type = Section->getType();
        ELFDiscardedSection->Origin =
            Existing->second->getInput()->decoratedPath(
                ThisLayoutPrinter->showAbsolutePath());
        ELFDiscardedSection->Alignment = Section->getAddrAlign();
        ELFDiscardedSection->InputPermissions = Section->getFlags();
        Result.DiscardedSectionGroups.emplace_back(
            std::make_shared<eld::LDYAML::DiscardedSection>(std::move(DS)));
        continue;
      }
      for (auto &F : Section->getFragmentList()) {
        auto Existing = ThisLayoutPrinter->getFragmentInfoMap().find(F);
        if (Existing == ThisLayoutPrinter->getFragmentInfoMap().end())
          continue;
        eld::LDYAML::DiscardedSection DS;
        eld::LDYAML::DiscardedSection *ELFDiscardedSection = &DS;
        auto *Info = Existing->second;
        ELFDiscardedSection->Name = Info->name();
        ELFDiscardedSection->Size = F->size();
        ELFDiscardedSection->Type = Info->type();
        ELFDiscardedSection->Origin = Info->getResolvedPath();
        ELFDiscardedSection->Alignment = F->alignment();
        ELFDiscardedSection->InputPermissions = Info->flag();
        ThisLayoutPrinter->sortFragmentSymbols(Info);
        for (auto *K : Info->Symbols)
          ELFDiscardedSection->Symbols.push_back(
              {K->name(), K->type(), K->binding(), K->size(), 0});
        Result.DiscardedSections.emplace_back(
            std::make_shared<eld::LDYAML::DiscardedSection>(std::move(DS)));
      }
    }
  }
  for (auto &G : Module.getNamePool().getGlobals()) {
    ResolveInfo *Info = G.getValue();
    if (Info->isCommon() && (Info->outSymbol()->shouldIgnore())) {
      eld::LDYAML::Common S;
      S.Name = ThisLayoutPrinter->showSymbolName(Info->name());
      S.Size = Info->size();
      InputFile *Input = Info->resolvedOrigin();
      if (Input != nullptr)
        S.InputPath = ThisLayoutPrinter->getPath(Input->getInput());
      if (Input->getInput()->isArchiveMember())
        S.InputName = Input->getInput()->getName();
      Result.DiscardedCommons.emplace_back(S);
    }
  }
  const std::vector<ELFSegment *> &Segments =
      Backend.elfSegmentTable().segments();
  int SegNum = 0;
  for (auto &Seg : Segments) {
    ++SegNum;
    eld::LDYAML::LoadRegion L;
    L.SegmentName = Seg->name();
    if (L.SegmentName.empty())
      L.SegmentName = "Segment #" + std::to_string(SegNum);
    L.Type = Seg->type();
    L.VirtualAddress = Seg->vaddr();
    L.PhysicalAddress = Seg->paddr();
    L.FileOffset = Seg->offset();
    L.SegPermission = Seg->flag();
    L.FileSize = Seg->filesz();
    L.MemorySize = Seg->memsz();
    L.Alignment = Seg->align();
    for (auto &Sec : Seg->sections())
      L.Sections.push_back(Sec->name().str());
    Result.LoadRegions.push_back(L);
  }
  const GeneralOptions::CrefTableType &Table =
      ThisLayoutPrinter->getConfig().options().crefTable();
  std::vector<const ResolveInfo *> SymVector;
  GeneralOptions::CrefTableType::const_iterator Itr = Table.begin(),
                                                Ite = Table.end();
  for (; Itr != Ite; Itr++)
    SymVector.push_back(Itr->first);

  std::sort(SymVector.begin(), SymVector.end(),
            [](const ResolveInfo *A, const ResolveInfo *B) {
              return (std::string(A->name()) < std::string(B->name()));
            });

  for (const auto *I : SymVector) {
    eld::LDYAML::CRef C;

    const std::vector<std::pair<const InputFile *, bool>> RefSites =
        Table.find(I)->second;

    C.SymbolName = I->name();

    for (auto J : RefSites) {
      if (J.second)
        continue;
      C.FileRefs.push_back(J.first->getInput()->decoratedPath());
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
    bool HasBranchIsland = false;
    branch_island_iter Bi = OutputSection->islandsBegin();
    branch_island_iter Be = OutputSection->islandsEnd();
    for (; Bi != Be; ++Bi) {
      eld::LDYAML::Trampoline T;
      // ARMv7 uses the same branch island interface, but some of the stuff
      // that exists in branch island interface are not stored by the ARM
      // backend.
      if (!(*Bi)->stub())
        continue;

      if (!(*Bi)->stub()->symInfo())
        continue;

      if (!(*Bi)->stub()->savedSymInfo())
        continue;
      HasBranchIsland = true;

      // Name of the trampoline.
      T.Name = (*Bi)->stub()->symInfo()->name();
      // The file where the source is located.
      eld::Relocation *OrigRelocation = (*Bi)->getOrigRelocation();
      Fragment *PatchFrag = OrigRelocation->targetRef()->frag();
      T.From = PatchFrag->getOwningSection()->name().str();
      {
        auto Existing = ThisLayoutPrinter->getFragmentInfoMap().find(PatchFrag);
        if (Existing != ThisLayoutPrinter->getFragmentInfoMap().end()) {
          auto *Info = Existing->second;
          for (auto *K : Info->Symbols) {
            if (!K->isSection())
              T.FromSymbols.push_back({K->name(), K->type(), K->size()});
          }
        }
      }
      T.FromFile = PatchFrag->getOwningSection()
                       ->getInputFile()
                       ->getInput()
                       ->decoratedPath(ThisLayoutPrinter->showAbsolutePath());
      T.To = (*Bi)->stub()->savedSymInfo()->name();
      if (!(*Bi)->stub()->savedSymInfo()->isAbsolute()) {
        T.ToSection = (*Bi)
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
      T.ToFile = (*Bi)
                     ->stub()
                     ->savedSymInfo()
                     ->resolvedOrigin()
                     ->getInput()
                     ->decoratedPath(ThisLayoutPrinter->showAbsolutePath());
      T.Addend = (*Bi)->getAddend();
      {
        if ((*Bi)->stub()->savedSymInfo()->isLocal()) {
          Fragment *ToFrag =
              (*Bi)->stub()->savedSymInfo()->outSymbol()->fragRef()->frag();
          auto Existing = ThisLayoutPrinter->getFragmentInfoMap().find(ToFrag);
          if (Existing != ThisLayoutPrinter->getFragmentInfoMap().end()) {
            auto *Info = Existing->second;
            for (auto *K : Info->Symbols) {
              if (!K->isSection())
                T.ToSymbols.push_back({K->name(), K->type(), K->size()});
            }
          }
        }
      }
      std::set<Fragment *> RSet;

      for (auto &Reloc : (*Bi)->getReuses()) {
        eld::LDYAML::Reuse R;
        Fragment *F = Reloc->targetRef()->frag();
        if (RSet.find(F) != RSet.end())
          continue;
        RSet.insert(F);
        R.From = F->getOwningSection()->name().str();
        auto Existing = ThisLayoutPrinter->getFragmentInfoMap().find(F);
        if (Existing != ThisLayoutPrinter->getFragmentInfoMap().end()) {
          auto *Info = Existing->second;
          for (auto *K : Info->Symbols) {
            if (!K->isSection())
              R.Symbols.push_back({K->name(), K->type(), K->size()});
          }
        }
        R.FromFile =
            F->getOwningSection()->getInputFile()->getInput()->decoratedPath(
                ThisLayoutPrinter->showAbsolutePath());
        R.Addend = Reloc->addend();
        T.Uses.push_back(R);
      }
      TInfo.Trampolines.push_back(T);
    }
    if (!HasBranchIsland)
      continue;
    R.push_back(TInfo);
  }
}

uint64_t YamlLayoutPrinter::getEntryAddress(eld::Module const &CurModule,
                                            GNULDBackend const &Backend) {
  /// getEntryPoint
  llvm::StringRef EntryName = Backend.getEntry();
  uint64_t Result = 0x0;

  if (string::isValidCIdentifier(EntryName)) {
    const LDSymbol *EntrySymbol =
        CurModule.getNamePool().findSymbol(EntryName.str());
    if (!EntrySymbol)
      return (Result = 0);
    Result = EntrySymbol->value();
  } else {
    if (EntryName.getAsInteger(0, Result)) {
      return (Result = 0);
    }
  }
  return Result;
}

void YamlLayoutPrinter::printLayout(eld::Module &Module,
                                    GNULDBackend const &Backend) {
  DiagnosticEngine *DiagEngine = Backend.config().getDiagEngine();
  // Open Trampoline Map file.
  if (!ThisLayoutPrinter->getConfig()
           .options()
           .getTrampolineMapFile()
           .empty()) {
    std::string File =
        ThisLayoutPrinter->getConfig().options().getTrampolineMapFile().str();
    std::error_code Error;
    TrampolineLayoutFile =
        new llvm::raw_fd_ostream(File, Error, llvm::sys::fs::OF_None);
    if (Error) {
      DiagEngine->raise(Diag::fatal_unwritable_output)
          << File << Error.message();
      exit(1);
    }
  }

  auto Yaml = buildYaml(Module, Backend);
  llvm::ArrayRef<std::string> MapStyles =
      ThisLayoutPrinter->getConfig().options().mapStyle();
  if (std::find(MapStyles.begin(), MapStyles.end(), "compressed") ==
      MapStyles.end()) {
    yaml::Output Yout(outputStream());
    Yout << Yaml;
    outputStream().flush();
    if (TrampolineLayoutFile) {
      yaml::Output Tout(*TrampolineLayoutFile);
      Tout << Yaml.TrampolinesGenerated;
      TrampolineLayoutFile->flush();
    }
    return;
  }

  if (TrampolineLayoutFile) {
    yaml::Output Tout(*TrampolineLayoutFile);
    Tout << Yaml.TrampolinesGenerated;
    TrampolineLayoutFile->flush();
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
    DiagEngine->raise(Diag::unable_to_compress)
        << m_Config.options().layoutFile();
    return;
  }
  outputStream() << CompressedData;
#else
  yaml::Output Yout(outputStream());
  Yout << Yaml;
#endif

  outputStream().flush();
}
