//===- GarbageCollection.cpp-----------------------------------------------===//
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

#include "eld/GarbageCollection/GarbageCollection.h"
#include "eld/Config/Config.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Core/LinkerScript.h"
#include "eld/Core/Module.h"
#include "eld/Diagnostics/DiagnosticPrinter.h"
#include "eld/Fragment/Fragment.h"
#include "eld/Input/BitcodeFile.h"
#include "eld/Input/ELFObjectFile.h"
#include "eld/Readers/CommonELFSection.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Readers/Relocation.h"
#include "eld/Script/StrToken.h"
#include "eld/Support/MsgHandling.h"
#include "eld/Support/RegisterTimer.h"
#include "eld/SymbolResolver/IRBuilder.h"
#include "eld/SymbolResolver/LDSymbol.h"
#include "eld/Target/GNULDBackend.h"
#include "eld/Target/LDFileFormat.h"
#include "llvm/Support/Casting.h"
#include <queue>
#include <stdlib.h>
#include <string.h>
#include <vector>

using namespace eld;
using namespace llvm;

//===----------------------------------------------------------------------===//
// Non-member functions
//===----------------------------------------------------------------------===//
namespace {
static inline Section *getInputSectionForSymbol(ResolveInfo &PInfo) {
  if (PInfo.isBitCode())
    return llvm::dyn_cast<eld::BitcodeFile>(PInfo.resolvedOrigin())
        ->getInputSectionForSymbol(PInfo);
  if (PInfo.outSymbol() && PInfo.outSymbol()->hasFragRef())
    return PInfo.getOwningSection();
  return nullptr;
}
} // namespace

/// shouldProcessGC - check if the section kind is handled in GC
bool GarbageCollection::mayProcessGC(ELFSection &CurSection) {
  bool Ret = false;
  if (CurSection.isExcludedFromGC())
    return false;
  std::optional<bool> BackendShouldProcess =
      MBackend.shouldProcessSectionForGC(CurSection);
  if (BackendShouldProcess)
    return *BackendShouldProcess;
  switch (CurSection.getKind()) {
  case LDFileFormat::Ignore:
    Ret = false;
    break;
  // take nullptr and StackNote directly
  case LDFileFormat::Null:
  case LDFileFormat::StackNote:
    Ret = false;
    break;
  case LDFileFormat::Relocation:
    Ret = false;
    break;
  // Make these flags go along with whatever is part of the root set.
  case LDFileFormat::EhFrame:
    Ret = false;
    break;
  case LDFileFormat::Regular:
  case LDFileFormat::Common:
  case LDFileFormat::Internal:
  case LDFileFormat::Target:
  case LDFileFormat::MetaData:
  case LDFileFormat::GCCExceptTable:
    Ret = true;
    break;
  case LDFileFormat::MergeStr:
    if (CurSection.isAlloc())
      return true;
    LLVM_FALLTHROUGH;
  case LDFileFormat::Note:
  case LDFileFormat::Debug:
  case LDFileFormat::Discard:
  case LDFileFormat::NamePool:
  case LDFileFormat::EhFrameHdr:
  case LDFileFormat::Group:
  case LDFileFormat::Version:
  case LDFileFormat::OutputSectData:
  default:
    Ret = false;
    break;
  } // end of switch
  return Ret;
}

//===----------------------------------------------------------------------===//
// GarbageCollection::SectionReachedListMap
//===----------------------------------------------------------------------===//
void GarbageCollection::SectionReachedListMap::addReference(Section &From,
                                                            Section &To) {
  ReachedSections[&From].insert(&To);
}

GarbageCollection::SectionListTy &
GarbageCollection::SectionReachedListMap::getReachedList(Section &CurSection) {
  return ReachedSections[&CurSection];
}

GarbageCollection::SectionListTy *
GarbageCollection::SectionReachedListMap::findReachedList(Section &CurSection) {
  ReachedSectionsTy::iterator It = ReachedSections.find(&CurSection);
  if (It == ReachedSections.end())
    return nullptr;
  return &It->second;
}

GarbageCollection::SymbolListTy &
GarbageCollection::SectionReachedListMap::getReachedSymbolList(
    Section &CurSection) {
  return ReachedSymbols[&CurSection];
}

GarbageCollection::SymbolListTy *
GarbageCollection::SectionReachedListMap::findReachedSymbolList(
    Section &CurSection) {
  ReachedSymbolsTy::iterator It = ReachedSymbols.find(&CurSection);
  if (It == ReachedSymbols.end())
    return nullptr;
  return &It->second;
}

void GarbageCollection::SectionReachedListMap::
    findReachedBitCodeSectionsAndSymbols(Module &ThisModule) {

  while (!InputBitcodeSections.empty()) {
    Section *Sect = InputBitcodeSections.front();
    InputBitcodeSections.pop();
    if (findReachedList(*Sect))
      continue;
    // If the bitcode section has already been traversed, just continue;
    SectionListTy &ReachedSects = getReachedList(*Sect);
    SymbolListTy &ReachedSyms = getReachedSymbolList(*Sect);

    auto ProcessSym = [&](ResolveInfo *SymInfo) {
      InputFile *Input = SymInfo->resolvedOrigin();
      if (!Input->isBitcode()) {
        if (SymInfo->outSymbol()->hasFragRef()) {
          ReachedSects.insert(SymInfo->getOwningSection());
          ReachedSyms.insert(SymInfo->outSymbol());
        }
      } else {
        BitcodeFile *BitcodeFile = llvm::dyn_cast<eld::BitcodeFile>(Input);
        if (Section *SectionForSymbol =
                BitcodeFile->getInputSectionForSymbol(*SymInfo)) {
          ReachedSects.insert(SectionForSymbol);
          // If the bitcode section has already been traversed, just
          // continue;
          if (!findReachedList(*SectionForSymbol))
            addToWorkQ(SectionForSymbol);
        }
      }
    };

    auto RefIt = ThisModule.getBitcodeReferencedSymbols().find(Sect);
    if (RefIt == ThisModule.getBitcodeReferencedSymbols().end())
      continue;

    for (const auto &S : RefIt->second) {
      LDSymbol *Sym = S->outSymbol();
      // check if this sym is defined in linker script
      if (Sym->resolveInfo()->outSymbol() &&
          Sym->resolveInfo()->outSymbol()->scriptDefined()) {
        if (const Assignment *Assignment =
                ThisModule.getAssignmentForSymbol(Sym->name())) {
          std::vector<ResolveInfo *> SymbolsForAssignment;
          Assignment->getSymbols(SymbolsForAssignment);
          for (auto &SymInfo : SymbolsForAssignment)
            ProcessSym(SymInfo);
        }
      } else
        ProcessSym(Sym->resolveInfo());
    }
  }
}

//===----------------------------------------------------------------------===//
// GarbageCollection
//===----------------------------------------------------------------------===//
GarbageCollection::GarbageCollection(LinkerConfig &Config,
                                     const GNULDBackend &PBackend,
                                     Module &ThisModule)
    : ThisConfig(Config), MBackend(PBackend), ThisModule(ThisModule) {}

GarbageCollection::~GarbageCollection() {}

bool GarbageCollection::run(const std::string &Phase, bool CommonSectionsOnly) {
  // 1. traverse all the relocations to set up the reached sections of each
  // section
  {
    eld::RegisterTimer T("Get Reachable Sections", "Garbage Collection",
                         ThisConfig.options().printTimingStats());

    setUpReachedSectionsAndSymbols();
    MBackend.setUpReachedSectionsForGC(MSectionReachedListMap);
  }

  SectionSetTy Entry;
  SectionSetTy LiveSet;
  {
    eld::RegisterTimer T("Compute Entry Sections", "Garbage Collection",
                         ThisConfig.options().printTimingStats());
    // 2. get all sections defined the entry point
    if (!getEntrySections(Entry))
      return false;
  }

  {
    eld::RegisterTimer T("Find Dead Code", "Garbage Collection",
                         ThisConfig.options().printTimingStats());
    // 3. find all the referenced sections those can be reached by entry
    if (ThisModule.getPrinter()->traceGCLive())
      ThisConfig.raise(Diag::tracing_gc_phase) << Phase;
    findReferencedSectionsAndSymbols(Entry, LiveSet);
  }

  // 4. stripSections - set the unreached sections to Ignore
  {
    eld::RegisterTimer T("Apply Dead Code Elimination", "Garbage Collection",
                         ThisConfig.options().printTimingStats());
    stripSections(LiveSet, CommonSectionsOnly);
  }
  return true;
}

void GarbageCollection::setUpReachedSectionsAndSymbols() {
  // traverse all the input relocations to setup the reached sections
  Module::obj_iterator Input, InEnd = ThisModule.objEnd();
  llvm::raw_ostream *stream = nullptr;
  llvm::DenseMap<uint64_t,
                 llvm::DenseSet<std::pair<ELFSection *, ResolveInfo *>>>
      crefMap;
  LinkerScript::Assignments::iterator Symit, Symite;
  Symite = ThisModule.getScript().assignments().end();
  Symit = ThisModule.getScript().assignments().begin();
  for (; Symit != Symite; Symit++) {
    ResolveInfo *Info =
        ThisModule.getNamePool().findInfo((*Symit).second->name().str());
    if (Info != nullptr && Info->outSymbol() != nullptr &&
        (*Symit).second->type() == Assignment::DEFAULT)
      Info->outSymbol()->setScriptDefined();
  }

  size_t column = 54;
  size_t SpaceBwField = 4;
  if (ThisModule.getPrinter()->traceGC()) {
    stream = &(llvm::outs());
    ThisConfig.raise(Diag::trace_gc_header)
        << std::string(column - strlen("Referring site"), ' ');
  }
  for (Input = ThisModule.objBegin(); Input != InEnd; ++Input) {
    ELFObjectFile *ObjFile = llvm::dyn_cast<ELFObjectFile>(*Input);
    if (!ObjFile)
      continue;
    for (auto &RelocSect : ObjFile->getRelocationSections()) {
      // bypass the discarded relocation section
      // 1. its section kind is changed to Ignore. (The target section is a
      // discarded group section.)
      // 2. it has no reloc data. (All symbols in the input relocs are in the
      // discarded group sections)
      auto *ApplySect =
          llvm::dyn_cast_or_null<ELFSection>(RelocSect->getLink());
      if (RelocSect->isIgnore())
        continue;
      if (RelocSect->isDiscard())
        continue;

      std::string name;
      bool FirstField = true;

      if (ThisModule.getPrinter()->traceGC() && ApplySect &&
          mayProcessGC(*ApplySect)) {
        name = ApplySect->getDecoratedName(ThisConfig.options());
        if (name.size() > (column - SpaceBwField))
          name = llvm::StringRef(name)
                     .drop_back(name.size() - (column - SpaceBwField))
                     .str();
        *stream << "\n" << name;
      }

      for (auto &Reloc : ApplySect->getRelocations()) {
        auto RecordOneRef = [&](Relocation *Reloc) -> void {
          ResolveInfo *Sym = Reloc->symInfo();

          // only the target symbols defined in the input fragments can make the
          // reference
          if (nullptr == Sym)
            return;

          // Reached symbols from section.
          SymbolListTy *ReachedSyms =
              &MSectionReachedListMap.getReachedSymbolList(*ApplySect);
          // Reached sections from section.
          SectionListTy *ReachedSects =
              &MSectionReachedListMap.getReachedList(*ApplySect);

          if (Sym->isBitCode()) {
            InputFile *Input = Sym->resolvedOrigin();
            Section *Bs = llvm::dyn_cast<eld::BitcodeFile>(Input)
                              ->getInputSectionForSymbol(*Sym);
            if (!Bs)
              return;
            ReachedSyms->insert(Sym->outSymbol());

            ReachedSects->insert(Bs);

            MSectionReachedListMap.addToWorkQ(Bs);
            return;
          }
          // If symbol is script defined, traverse over the symbols that is
          // referenced by the expression and make them live, if this symbol is
          // live.
          if (!Sym->isDefine() || (!Sym->outSymbol()->scriptDefined() &&
                                   !Sym->outSymbol()->hasFragRef())) {
            if (Sym->isCommon() || Sym->outSymbol()) {
              std::pair<SymbolListTy::iterator, bool> Result =
                  ReachedSyms->insert(Sym->outSymbol());

              // For section magic symbol, we mark every section seen before
              // it refers as reached
              llvm::StringRef SymName(Sym->outSymbol()->name());
              llvm::StringRef SectName;
              if (SymName.starts_with("__start_")) {
                SectName = SymName.drop_front(8);
              } else if (SymName.starts_with("__stop_")) {
                SectName = SymName.drop_front(7);
              }
              if (SectName.size() > 0) {
                // We need keep this section.
                for (auto &O : ThisModule.getObjectList()) {
                  ELFObjectFile *ObjFile = llvm::dyn_cast<ELFObjectFile>(O);
                  if (!ObjFile)
                    continue;
                  for (auto &Sect : ObjFile->getSections()) {
                    ELFSection *Section = llvm::dyn_cast<eld::ELFSection>(Sect);
                    if (Section->name() == SectName)
                      ReachedSects->insert(Section);
                  }
                }
              }

              ELFSection *TargetSec = nullptr;
              if (Sym->outSymbol()->hasFragRef()) {
                TargetSec = Sym->getOwningSection();
                ReachedSects->insert(TargetSec);
              }

              if (Result.second && ThisModule.getPrinter()->traceGC() &&
                  ApplySect && mayProcessGC(*ApplySect)) {
                if (FirstField)
                  stream->indent(column - name.size());
                else
                  stream->indent(column);
                *stream << ThisModule.getNamePool().getDecoratedName(
                               Sym, ThisConfig.options().shouldDemangle())
                        << " (Symbol)\n";
                FirstField = false;
              }
              if ((ThisConfig.options().gcCref() == Sym->name()) &&
                  mayProcessGC(*ApplySect)) {
                crefMap[Sym->outSymbol()->sectionIndex()].insert(
                    std::make_pair(ApplySect, Sym));
              }
            }
            return;
          }

          // only the target symbols defined in the concerned sections can make
          // the reference
          ELFSection *TargetSect = nullptr;

          // check if this sym is defined in linker script
          if (Sym->outSymbol()->scriptDefined()) {
            if (TargetSect)
              ThisConfig.raise(Diag::warn_gc_duplicate_symbol)
                  << Sym->outSymbol()->name();
            std::vector<ResolveInfo *> SymbolsForAssignment;
            const Assignment *Assignment =
                ThisModule.getAssignmentForSymbol(Sym->name());
            if (Assignment) {
              Assignment->getSymbols(SymbolsForAssignment);
              for (auto &S : SymbolsForAssignment) {
                if (S->outSymbol()) {
                  if (S->outSymbol()->hasFragRef()) {
                    ReachedSects->insert(S->getOwningSection());
                    ReachedSyms->insert(S->outSymbol());
                  }
                }
              }
            }
            return;
          }

          TargetSect = Sym->getOwningSection();

          ReachedSects->insert(TargetSect);
          ReachedSyms->insert(Sym->outSymbol());

          if (ThisModule.getPrinter()->traceGC() && ApplySect &&
              mayProcessGC(*ApplySect)) {
            if (FirstField)
              stream->indent(column - name.size());
            else
              stream->indent(column);
            *stream << TargetSect->getDecoratedName(ThisConfig.options())
                    << " (section) \n";
            FirstField = false;
          }
          if ((ThisConfig.options().gcCref() == TargetSect->name()) &&
              mayProcessGC(*ApplySect)) {
            crefMap[TargetSect->getIndex()].insert(
                std::make_pair(ApplySect, Sym));
          }
        };
        RecordOneRef(Reloc);
      }
    }
  }
  MSectionReachedListMap.findReachedBitCodeSectionsAndSymbols(ThisModule);
  if (!ThisConfig.options().gcCref().empty()) {
    for (auto &I : crefMap) {
      ThisConfig.raise(Diag::symbols_referring)
          << ThisConfig.options().gcCref() << I.first;
      for (auto &J : I.second)
        ThisConfig.raise(Diag::referred_symbol)
            << ThisModule.getNamePool().getDecoratedName(
                   J.second, ThisConfig.options().shouldDemangle())
            << J.first->getDecoratedName(ThisConfig.options())
            << J.first->getIndex();
    }
  }
}

bool GarbageCollection::getEntrySections(SectionSetTy &EntrySections) {
  // all the KEEP sections defined in ldscript are entries, traverse all the
  // input sections and check the SectionMap to find the KEEP sections
  bool HasEntrySymbol = false;
  ELFSection *FirstTextSect = nullptr;
  LDSymbol *EntrySym =
      ThisModule.getNamePool().findSymbol(MBackend.getEntry().str());
  if ((EntrySym != nullptr) && (EntrySym->hasFragRef())) {
    EntrySections.insert(EntrySym->fragRef()->frag()->getOwningSection());
    HasEntrySymbol = true;
  } else {
    ResolveInfo *EntryInfo =
        ThisModule.getNamePool().findInfo(MBackend.getEntry().str());
    if (EntryInfo && EntryInfo->isBitCode()) {
      EntryInfo->shouldPreserve(true);
      Section *Sect = getInputSectionForSymbol(*EntryInfo);
      if (Sect) {
        EntrySections.insert(Sect);
        HasEntrySymbol = true;
      }
    }
  }

  if (!HasEntrySymbol) {
    Module::obj_iterator Obj, ObjEnd = ThisModule.objEnd();
    for (Obj = ThisModule.objBegin(); Obj != ObjEnd; ++Obj) {
      ELFObjectFile *ObjFile = llvm::dyn_cast<ELFObjectFile>(*Obj);
      if (!ObjFile)
        continue;
      for (auto &Sect : ObjFile->getSections()) {
        ELFSection *Section = llvm::dyn_cast<eld::ELFSection>(Sect);
        if (FirstTextSect == nullptr) {
          if (Section->size() && Section->isCode()) {
            FirstTextSect = Section;
            break;
          }
        }
      }
      // No point running through the rest of the sections.
      if (FirstTextSect)
        break;
    }
  }
  for (auto &Sec : ThisModule.getScript().sectionMap().getEntrySections()) {
    bool IsCommonSection = llvm::isa<CommonELFSection>(Sec);
    // Common sections can be entry sections for garbage collection even
    // though they are part of an internal input file.
    if (!Sec->getInputFile()->isInternal() || IsCommonSection)
      EntrySections.insert(Sec);
  }

  addRetainSections(EntrySections);

  GeneralOptions::const_undef_sym_iterator UndefSym, UndefSymEnd;
  UndefSymEnd = ThisModule.getConfig().options().undefSymEnd();
  UndefSym = ThisModule.getConfig().options().undefSymBegin();
  auto AddListEntry = [&](llvm::StringRef Name) -> void {
    ResolveInfo *SymInfo = ThisModule.getNamePool().findInfo(Name.str());
    if (!SymInfo)
      return;
    if (SymInfo->isBitCode())
      SymInfo->shouldPreserve(true);
    Section *Sec = getInputSectionForSymbol(*SymInfo);
    if (Sec != nullptr)
      EntrySections.insert(Sec);
    if (SymInfo->outSymbol())
      SymInfo->outSymbol()->setShouldIgnore(false);
  };
  for (; UndefSym != UndefSymEnd; UndefSym++)
    AddListEntry((*UndefSym)->name());

  auto &DynExpSymbols = ThisConfig.options().getExportDynSymList();
  for (const auto &S : DynExpSymbols)
    AddListEntry(S->name());

  const auto &SymScopes = MBackend.symbolScopes();
  LinkerScript::Assignments::iterator Symit, Symite;
  Symite = ThisModule.getScript().assignments().end();
  Symit = ThisModule.getScript().assignments().begin();
  for (; Symit != Symite; Symit++) {
    ResolveInfo *Info =
        ThisModule.getNamePool().findInfo((*Symit).second->name().str());
    // Bitcode symbols that are "Common" doesnot have an outsymbol. They do
    // have an outSymbol only if they are preserved.
    if (Info != nullptr && Info->isCommon() && Info->outSymbol()) {
      ELFSection *S = Info->getOwningSection();
      EntrySections.insert(S);
    }
  }

  // get the sections those the entry symbols defined in
  if (LinkerConfig::Exec == ThisConfig.codeGenType() ||
      ThisConfig.options().isPIE()) {
    // when building executable
    // 1. the entry symbol is the entry
    LDSymbol *EntrySym =
        ThisModule.getNamePool().findSymbol(MBackend.getEntry().str());
    // First, try to use entry_sym specifed by command line option,
    // LD script or target specific symbol. If no success, then try to use the
    // first ".text" section.
    if (EntrySym != nullptr && EntrySym->hasFragRef())
      EntrySections.insert(EntrySym->fragRef()->frag()->getOwningSection());
    else if (FirstTextSect)
      EntrySections.insert(FirstTextSect);
    else if (!EntrySections.size()) {
      ThisConfig.raise(Diag::no_entry_sym_for_GC);
      return false;
    }

    // 2. the symbols have been seen in dynamice objects are entries
    for (auto &G : ThisModule.getNamePool().getGlobals()) {
      ResolveInfo *Info = G.getValue();
      LDSymbol *Sym = Info->outSymbol();
      if (nullptr == Sym || !Sym->hasFragRef())
        continue;

      if (ThisConfig.isCodeDynamic() || ThisConfig.options().forceDynamic()) {
        // Skip entry symbolse that have explicitly been marked as local by
        // version script
        const auto &Found = SymScopes.find(Info);
        if (Found != SymScopes.end() && !(Found->second->isGlobal()))
          continue;
      }

      if (!treatSymbolAsEntry(Info))
        continue;

      // only the target symbols defined in the concerned sections can be
      // entries
      ELFSection *Sect = Sym->fragRef()->frag()->getOwningSection();
      if (!mayProcessGC(*Sect))
        continue;

      EntrySections.insert(Sect);
    }
  } else {
    // when building shared objects, the global define symbols are entries
    for (auto &G : ThisModule.getNamePool().getGlobals()) {
      ResolveInfo *Info = G.getValue();
      // Bitcode symbols that are "Common" doesnot have an outsymbol. They do
      // have an outSymbol only if they are preserved.
      if (Info->isCommon() && Info->outSymbol()) {
        ELFSection *Sect = Info->getOwningSection();
        EntrySections.insert(Sect);
        continue;
      }
      if (!Info->isDefine() || Info->isLocal() ||
          ((Info->visibility() == ResolveInfo::Hidden ||
            Info->visibility() == ResolveInfo::Internal) &&
           (Info->isGlobal() || Info->isWeak()) &&
           (Info->isDefine() || Info->isCommon())))
        continue;

      const auto &Found = SymScopes.find(Info);
      // Skip entry symbolse that have explicitly been marked as local by
      // version script
      if (Found != SymScopes.end() && Found->second->isLocal())
        continue;

      LDSymbol *Sym = Info->outSymbol();
      if (nullptr == Sym || !Sym->hasFragRef())
        continue;

      // only the target symbols defined in the concerned sections can be
      // entries
      ELFSection *Sect = Sym->fragRef()->frag()->getOwningSection();
      if (!mayProcessGC(*Sect))
        continue;
      EntrySections.insert(Sect);
    }
  }
  return true;
}

void GarbageCollection::findReferencedSectionsAndSymbols(
    SectionSetTy &EntrySections, SectionSetTy &LiveSet) {
  // list of sections waiting to be processed
  typedef std::queue<std::pair<Section *, Section *>> WorkListTy;
  WorkListTy WorkList;

  if (!EntrySections.size())
    return;

  // start from each entry, resolve the transitive closure
  for (const auto &EntryIt : EntrySections) {
    // add entry point to work list
    WorkList.push(std::make_pair(EntryIt, nullptr));
    LiveSet.insert(EntryIt);

    // add section from the work_list to the referencedSections until every
    // reached sections are added
    while (!WorkList.empty()) {
      std::pair<Section *, Section *> P = WorkList.front();
      WorkList.pop();

      Section *Sect = P.first;

      if (ELFSection *ElfSect = dyn_cast<ELFSection>(Sect)) {
        // A section listed as KEEP in a discarded rule also needs to be checked
        // for references from that section, and those references may need to be
        // kept
        if (!mayProcessGC(*ElfSect) &&
            !(ElfSect->isIgnore() && EntrySections.count(ElfSect)))
          continue;
      }

      // add section to the ReferencedSections, if the section has been put into
      // referencedSections, skip this section
      if (!MReferencedSections.insert(Sect).second)
        continue;

      if (ThisModule.getPrinter()->traceGCLive()) {
        ThisConfig.raise(Diag::refers_to)
            << Sect->getDecoratedName(ThisConfig.options());
        if (Sect->getInputFile())
          ThisConfig.raise(Diag::referenced_input_file)
              << Sect->getInputFile()->getInput()->decoratedPath();
        if (Sect->getOldInputFile())
          ThisConfig.raise(Diag::referenced_bc_file)
              << Sect->getOldInputFile()->getInput()->decoratedPath();
        if (!P.second)
          ThisConfig.raise(Diag::referenced_by_root_symbol);
        else {
          ThisConfig.raise(Diag::referenced_by)
              << P.second->getDecoratedName(ThisConfig.options());
          if (P.second->getInputFile())
            ThisConfig.raise(Diag::referenced_input_file)
                << P.second->getInputFile()->getInput()->decoratedPath();
          if (P.second->getOldInputFile())
            ThisConfig.raise(Diag::referenced_bc_file)
                << P.second->getOldInputFile()->getInput()->decoratedPath();
        }
      }

      // get the section reached list, if the section do not has one, which
      // means no referenced between it and other sections, then skip it
      SectionListTy *ReachList = MSectionReachedListMap.findReachedList(*Sect);
      if (nullptr == ReachList)
        continue;

      // put the reached sections to work list, skip the one already be in
      // referencedSections
      for (const auto &I : *ReachList) {
        if (MReferencedSections.find(I) == MReferencedSections.end()) {
          WorkList.push(std::make_pair(I, Sect));
          LiveSet.insert(I);
        }
      }
    }
  }
}

void GarbageCollection::stripSections(SectionSetTy &S,
                                      bool CommonSectionsOnly) {
  // Traverse all the input Regular and BSS sections, if a section is not found
  // in the ReferencedSections, then it should be garbage collected
  Module::obj_iterator Obj, ObjEnd = ThisModule.objEnd();

  std::vector<std::pair<ELFSection *, LDFileFormat::Kind>> IgnoredSections;

  LayoutPrinter *Printer = ThisModule.getLayoutPrinter();
  bool ShouldDemangle = ThisConfig.options().shouldDemangle();

  for (Obj = ThisModule.objBegin(); Obj != ObjEnd; ++Obj) {
    ObjectFile *ObjFile = llvm::dyn_cast<ObjectFile>(*Obj);
    if (!ObjFile)
      continue;
    // CommonSymbols internal input file content can be stripped by garbage
    // collection.
    if (ObjFile->isInternal() && ObjFile != ThisModule.getCommonInternalInput())
      continue;
    for (auto &Sect : ObjFile->getSections()) {
      if (Sect->isBitcode())
        continue;
      ELFSection *Section = llvm::dyn_cast<eld::ELFSection>(Sect);
      if (!mayProcessGC(*Section))
        continue;
      if (MReferencedSections.find(Section) == MReferencedSections.end()) {
        bool IsCommonSection = false;
        Input *I = ObjFile->getInput();
        if (CommonELFSection *CommonSection =
                dyn_cast<CommonELFSection>(Section)) {
          I = CommonSection->getOrigin()->getInput();
          IsCommonSection = true;
        }
        // Print the GC collected input section if Tracing is enabled.
        if (ThisConfig.options().printGCSections() ||
            ThisModule.getPrinter()->traceGC()) {
          if (CommonELFSection *CommonSection =
                  dyn_cast<CommonELFSection>(Section)) {
            ResolveInfo *CommResolveInfo =
                MBackend.getCommonSymbol(CommonSection)->resolveInfo();
            assert(CommResolveInfo && "Symbol ResolveInfo is missing!");
            std::string DecoratedSymName =
                ThisModule.getNamePool().getDecoratedName(CommResolveInfo,
                                                          ShouldDemangle);
            ThisConfig.raise(Diag::trace_gc_symbol)
                << I->decoratedPath() << DecoratedSymName;
          } else
            ThisConfig.raise(Diag::trace_gc_section)
                << I->decoratedPath()
                << Section->getDecoratedName(ThisConfig.options());
        }
        if (ThisConfig.options().isSectionTracingRequested() &&
            ThisConfig.options().traceSection(Section->name().str()))
          ThisConfig.raise(Diag::gc_section_info)
              << Section->getDecoratedName(ThisConfig.options())
              << I->decoratedPath();
        if (CommonSectionsOnly && !IsCommonSection)
          IgnoredSections.push_back(
              std::make_pair(Section, Section->getKind()));
        else if (Printer)
          Printer->recordGC(Section);
        Section->setKind(LDFileFormat::Ignore);
      }
    }
  }

  for (Obj = ThisModule.objBegin(); Obj != ObjEnd; ++Obj) {
    ObjectFile *ObjFile = llvm::dyn_cast<ObjectFile>(*Obj);
    if (!ObjFile)
      continue;
    if (ObjFile->isInternal() && ObjFile != ThisModule.getCommonInternalInput())
      continue;
    for (auto &Sect : ObjFile->getSections()) {
      if (Sect->isBitcode())
        continue;
      bool NoAllocSection = false;
      bool IsEntrySection = false;
      ELFSection *Section = llvm::dyn_cast<eld::ELFSection>(Sect);
      // Dont keep references from non allocatable sections. This is ELD
      // optimization to reduce memory at runtime.
      if (!Section->isAlloc())
        NoAllocSection = true;
      if (Section->isIgnore())
        continue;
      if (Section->isDiscard())
        continue;
      if (S.find(Sect) != S.end())
        IsEntrySection = true;
      SymbolListTy *ReferredSyms =
          MSectionReachedListMap.findReachedSymbolList(*Section);
      if (ReferredSyms) {
        SymbolListTy::iterator It, Ite = ReferredSyms->end();
        for (It = ReferredSyms->begin(); It != Ite; It++)
          if (!NoAllocSection || IsEntrySection ||
              ((*It)->binding() == ResolveInfo::Local)) {
            (*It)->setShouldIgnore(false);
          }
      }
    }
  }

  for (auto &Sec : IgnoredSections)
    Sec.first->setKind(Sec.second);
}

bool GarbageCollection::treatSymbolAsEntry(ResolveInfo *R) const {
  // Symbol resolved from shared object.
  if (R->isDyn())
    return true;

  if (!R->isDefine() || R->isLocal())
    return false;

  // We trump hidden and absolute symbols are always trumped.
  if (R->isHidden() || R->isAbsolute())
    return false;

  if (ThisConfig.options().isPIE() && ThisConfig.options().exportDynamic())
    return true;

  if (ThisConfig.options().exportDynamic())
    return true;

  return false;
}

void GarbageCollection::addRetainSections(SectionSetTy &EntrySections) {
  Module::obj_iterator Obj, ObjEnd = ThisModule.objEnd();
  for (Obj = ThisModule.objBegin(); Obj != ObjEnd; ++Obj) {
    ObjectFile *ObjFile = llvm::dyn_cast<ObjectFile>(*Obj);
    if (!ObjFile)
      continue;
    for (Section *Sect : ObjFile->getSections()) {
      ELFSection *S = llvm::dyn_cast<ELFSection>(Sect);
      if (S && S->isRetain()) {
        EntrySections.insert(S);
        if (ThisModule.getLayoutPrinter())
          ThisModule.getLayoutPrinter()->recordRetainedSections();
        if (ThisModule.getPrinter()->isVerbose() ||
            ThisConfig.options().traceSection(S->name().str()))
          ThisConfig.raise(Diag::retaining_section)
              << S->name() << S->originalInput()->getInput()->decoratedPath();
      }
    }
  }
}
