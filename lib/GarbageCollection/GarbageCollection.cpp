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
static inline Section *getInputSectionForSymbol(ResolveInfo &pInfo) {
  if (pInfo.isBitCode())
    return llvm::dyn_cast<eld::BitcodeFile>(pInfo.resolvedOrigin())
        ->getInputSectionForSymbol(pInfo);
  if (pInfo.outSymbol() && pInfo.outSymbol()->hasFragRef())
    return pInfo.getOwningSection();
  return nullptr;
}
} // namespace

/// shouldProcessGC - check if the section kind is handled in GC
bool GarbageCollection::mayProcessGC(ELFSection &pSection) {
  bool ret = false;
  if (pSection.isExcludedFromGC())
    return false;
  std::optional<bool> backendShouldProcess =
      m_Backend.shouldProcessSectionForGC(pSection);
  if (backendShouldProcess)
    return *backendShouldProcess;
  switch (pSection.getKind()) {
  case LDFileFormat::Ignore:
    ret = false;
    break;
  // take nullptr and StackNote directly
  case LDFileFormat::Null:
  case LDFileFormat::StackNote:
    ret = false;
    break;
  case LDFileFormat::Relocation:
    ret = false;
    break;
  // Make these flags go along with whatever is part of the root set.
  case LDFileFormat::EhFrame:
    ret = false;
    break;
  case LDFileFormat::Regular:
  case LDFileFormat::Common:
  case LDFileFormat::Internal:
  case LDFileFormat::Target:
  case LDFileFormat::MetaData:
  case LDFileFormat::GCCExceptTable:
    ret = true;
    break;
  case LDFileFormat::MergeStr:
    if (pSection.isAlloc())
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
    ret = false;
    break;
  } // end of switch
  return ret;
}

//===----------------------------------------------------------------------===//
// GarbageCollection::SectionReachedListMap
//===----------------------------------------------------------------------===//
void GarbageCollection::SectionReachedListMap::addReference(Section &pFrom,
                                                            Section &pTo) {
  m_ReachedSections[&pFrom].insert(&pTo);
}

GarbageCollection::SectionListTy &
GarbageCollection::SectionReachedListMap::getReachedList(Section &pSection) {
  return m_ReachedSections[&pSection];
}

GarbageCollection::SectionListTy *
GarbageCollection::SectionReachedListMap::findReachedList(Section &pSection) {
  ReachedSectionsTy::iterator it = m_ReachedSections.find(&pSection);
  if (it == m_ReachedSections.end())
    return nullptr;
  return &it->second;
}

GarbageCollection::SymbolListTy &
GarbageCollection::SectionReachedListMap::getReachedSymbolList(
    Section &pSection) {
  return m_ReachedSymbols[&pSection];
}

GarbageCollection::SymbolListTy *
GarbageCollection::SectionReachedListMap::findReachedSymbolList(
    Section &pSection) {
  ReachedSymbolsTy::iterator it = m_ReachedSymbols.find(&pSection);
  if (it == m_ReachedSymbols.end())
    return nullptr;
  return &it->second;
}

void GarbageCollection::SectionReachedListMap::
    findReachedBitCodeSectionsAndSymbols(Module &pModule) {

  while (!m_InputBitcodeSections.empty()) {
    Section *sect = m_InputBitcodeSections.front();
    m_InputBitcodeSections.pop();
    if (findReachedList(*sect))
      continue;
    // If the bitcode section has already been traversed, just continue;
    SectionListTy &reached_sects = getReachedList(*sect);
    SymbolListTy &reached_syms = getReachedSymbolList(*sect);

    auto processSym = [&](ResolveInfo *symInfo) {
      InputFile *input = symInfo->resolvedOrigin();
      if (!input->isBitcode()) {
        if (symInfo->outSymbol()->hasFragRef()) {
          reached_sects.insert(symInfo->getOwningSection());
          reached_syms.insert(symInfo->outSymbol());
        }
      } else {
        BitcodeFile *BitcodeFile = llvm::dyn_cast<eld::BitcodeFile>(input);
        if (Section *sectionForSymbol =
                BitcodeFile->getInputSectionForSymbol(*symInfo)) {
          reached_sects.insert(sectionForSymbol);
          // If the bitcode section has already been traversed, just
          // continue;
          if (!findReachedList(*sectionForSymbol))
            addToWorkQ(sectionForSymbol);
        }
      }
    };

    auto RefIt = pModule.getBitcodeReferencedSymbols().find(sect);
    if (RefIt == pModule.getBitcodeReferencedSymbols().end())
      continue;

    for (const auto &S : RefIt->second) {
      LDSymbol *sym = S->outSymbol();
      // check if this sym is defined in linker script
      if (sym->resolveInfo()->outSymbol() &&
          sym->resolveInfo()->outSymbol()->scriptDefined()) {
        if (const Assignment *Assignment =
                pModule.getAssignmentForSymbol(sym->name())) {
          std::vector<ResolveInfo *> SymbolsForAssignment;
          Assignment->getSymbols(SymbolsForAssignment);
          for (auto &symInfo : SymbolsForAssignment)
            processSym(symInfo);
        }
      } else
        processSym(sym->resolveInfo());
    }
  }
}

//===----------------------------------------------------------------------===//
// GarbageCollection
//===----------------------------------------------------------------------===//
GarbageCollection::GarbageCollection(LinkerConfig &pConfig,
                                     const GNULDBackend &pBackend,
                                     Module &pModule)
    : m_Config(pConfig), m_Backend(pBackend), m_Module(pModule) {}

GarbageCollection::~GarbageCollection() {}

bool GarbageCollection::run(const std::string &Phase, bool CommonSectionsOnly) {
  // 1. traverse all the relocations to set up the reached sections of each
  // section
  {
    eld::RegisterTimer T("Get Reachable Sections", "Garbage Collection",
                         m_Config.options().printTimingStats());

    setUpReachedSectionsAndSymbols();
    m_Backend.setUpReachedSectionsForGC(m_SectionReachedListMap);
  }

  SectionSetTy entry;
  SectionSetTy LiveSet;
  {
    eld::RegisterTimer T("Compute Entry Sections", "Garbage Collection",
                         m_Config.options().printTimingStats());
    // 2. get all sections defined the entry point
    if (!getEntrySections(entry))
      return false;
  }

  {
    eld::RegisterTimer T("Find Dead Code", "Garbage Collection",
                         m_Config.options().printTimingStats());
    // 3. find all the referenced sections those can be reached by entry
    if (m_Module.getPrinter()->traceGCLive())
      m_Config.raise(diag::tracing_gc_phase) << Phase;
    findReferencedSectionsAndSymbols(entry, LiveSet);
  }

  // 4. stripSections - set the unreached sections to Ignore
  {
    eld::RegisterTimer T("Apply Dead Code Elimination", "Garbage Collection",
                         m_Config.options().printTimingStats());
    stripSections(LiveSet, CommonSectionsOnly);
  }
  return true;
}

void GarbageCollection::setUpReachedSectionsAndSymbols() {
  // traverse all the input relocations to setup the reached sections
  Module::obj_iterator input, inEnd = m_Module.obj_end();
  llvm::raw_ostream *stream = nullptr;
  llvm::DenseMap<uint64_t,
                 llvm::DenseSet<std::pair<ELFSection *, ResolveInfo *>>>
      crefMap;
  LinkerScript::Assignments::iterator symit, symite;
  symite = m_Module.getScript().assignments().end();
  symit = m_Module.getScript().assignments().begin();
  for (; symit != symite; symit++) {
    ResolveInfo *info =
        m_Module.getNamePool().findInfo((*symit).second->name().str());
    if (info != nullptr && info->outSymbol() != nullptr &&
        (*symit).second->type() == Assignment::DEFAULT)
      info->outSymbol()->setScriptDefined();
  }

  size_t column = 54;
  size_t space_bw_field = 4;
  if (m_Module.getPrinter()->traceGC()) {
    stream = &(llvm::outs());
    m_Config.raise(diag::trace_gc_header)
        << std::string(column - strlen("Referring site"), ' ');
  }
  for (input = m_Module.obj_begin(); input != inEnd; ++input) {
    ELFObjectFile *ObjFile = llvm::dyn_cast<ELFObjectFile>(*input);
    if (!ObjFile)
      continue;
    for (auto &reloc_sect : ObjFile->getRelocationSections()) {
      // bypass the discarded relocation section
      // 1. its section kind is changed to Ignore. (The target section is a
      // discarded group section.)
      // 2. it has no reloc data. (All symbols in the input relocs are in the
      // discarded group sections)
      auto *apply_sect =
          llvm::dyn_cast_or_null<ELFSection>(reloc_sect->getLink());
      if (reloc_sect->isIgnore())
        continue;
      if (reloc_sect->isDiscard())
        continue;

      std::string name;
      bool first_field = true;

      if (m_Module.getPrinter()->traceGC() && apply_sect &&
          mayProcessGC(*apply_sect)) {
        name = apply_sect->getDecoratedName(m_Config.options());
        if (name.size() > (column - space_bw_field))
          name = llvm::StringRef(name)
                     .drop_back(name.size() - (column - space_bw_field))
                     .str();
        *stream << "\n" << name;
      }

      for (auto &reloc : apply_sect->getRelocations()) {
        auto recordOneRef = [&](Relocation *reloc) -> void {
          ResolveInfo *sym = reloc->symInfo();

          // only the target symbols defined in the input fragments can make the
          // reference
          if (nullptr == sym)
            return;

          // Reached symbols from section.
          SymbolListTy *reached_syms =
              &m_SectionReachedListMap.getReachedSymbolList(*apply_sect);
          // Reached sections from section.
          SectionListTy *reached_sects =
              &m_SectionReachedListMap.getReachedList(*apply_sect);

          if (sym->isBitCode()) {
            InputFile *input = sym->resolvedOrigin();
            Section *bs = llvm::dyn_cast<eld::BitcodeFile>(input)
                              ->getInputSectionForSymbol(*sym);
            if (!bs)
              return;
            reached_syms->insert(sym->outSymbol());

            reached_sects->insert(bs);

            m_SectionReachedListMap.addToWorkQ(bs);
            return;
          }
          // If symbol is script defined, traverse over the symbols that is
          // referenced by the expression and make them live, if this symbol is
          // live.
          if (!sym->isDefine() || (!sym->outSymbol()->scriptDefined() &&
                                   !sym->outSymbol()->hasFragRef())) {
            if (sym->isCommon() || sym->outSymbol()) {
              std::pair<SymbolListTy::iterator, bool> result =
                  reached_syms->insert(sym->outSymbol());

              // For section magic symbol, we mark every section seen before
              // it refers as reached
              llvm::StringRef sym_name(sym->outSymbol()->name());
              llvm::StringRef sect_name;
              if (sym_name.starts_with("__start_")) {
                sect_name = sym_name.drop_front(8);
              } else if (sym_name.starts_with("__stop_")) {
                sect_name = sym_name.drop_front(7);
              }
              if (sect_name.size() > 0) {
                // We need keep this section.
                for (auto &O : m_Module.getObjectList()) {
                  ELFObjectFile *ObjFile = llvm::dyn_cast<ELFObjectFile>(O);
                  if (!ObjFile)
                    continue;
                  for (auto &sect : ObjFile->getSections()) {
                    ELFSection *section = llvm::dyn_cast<eld::ELFSection>(sect);
                    if (section->name() == sect_name)
                      reached_sects->insert(section);
                  }
                }
              }

              ELFSection *target_sec = nullptr;
              if (sym->outSymbol()->hasFragRef()) {
                target_sec = sym->getOwningSection();
                reached_sects->insert(target_sec);
              }

              if (result.second && m_Module.getPrinter()->traceGC() &&
                  apply_sect && mayProcessGC(*apply_sect)) {
                if (first_field)
                  stream->indent(column - name.size());
                else
                  stream->indent(column);
                *stream << m_Module.getNamePool().getDecoratedName(
                               sym, m_Config.options().shouldDemangle())
                        << " (Symbol)\n";
                first_field = false;
              }
              if ((m_Config.options().gcCref() == sym->name()) &&
                  mayProcessGC(*apply_sect)) {
                crefMap[sym->outSymbol()->sectionIndex()].insert(
                    std::make_pair(apply_sect, sym));
              }
            }
            return;
          }

          // only the target symbols defined in the concerned sections can make
          // the reference
          ELFSection *target_sect = nullptr;

          // check if this sym is defined in linker script
          if (sym->outSymbol()->scriptDefined()) {
            if (target_sect)
              m_Config.raise(diag::warn_gc_duplicate_symbol)
                  << sym->outSymbol()->name();
            std::vector<ResolveInfo *> SymbolsForAssignment;
            const Assignment *Assignment =
                m_Module.getAssignmentForSymbol(sym->name());
            if (Assignment) {
              Assignment->getSymbols(SymbolsForAssignment);
              for (auto &S : SymbolsForAssignment) {
                if (S->outSymbol()) {
                  if (S->outSymbol()->hasFragRef()) {
                    reached_sects->insert(S->getOwningSection());
                    reached_syms->insert(S->outSymbol());
                  }
                }
              }
            }
            return;
          }

          target_sect = sym->getOwningSection();

          reached_sects->insert(target_sect);
          reached_syms->insert(sym->outSymbol());

          if (m_Module.getPrinter()->traceGC() && apply_sect &&
              mayProcessGC(*apply_sect)) {
            if (first_field)
              stream->indent(column - name.size());
            else
              stream->indent(column);
            *stream << target_sect->getDecoratedName(m_Config.options())
                    << " (section) \n";
            first_field = false;
          }
          if ((m_Config.options().gcCref() == target_sect->name()) &&
              mayProcessGC(*apply_sect)) {
            crefMap[target_sect->getIndex()].insert(
                std::make_pair(apply_sect, sym));
          }
        };
        recordOneRef(reloc);
      }
    }
  }
  m_SectionReachedListMap.findReachedBitCodeSectionsAndSymbols(m_Module);
  if (!m_Config.options().gcCref().empty()) {
    for (auto &i : crefMap) {
      m_Config.raise(diag::symbols_referring)
          << m_Config.options().gcCref() << i.first;
      for (auto &j : i.second)
        m_Config.raise(diag::referred_symbol)
            << m_Module.getNamePool().getDecoratedName(
                   j.second, m_Config.options().shouldDemangle())
            << j.first->getDecoratedName(m_Config.options())
            << j.first->getIndex();
    }
  }
}

bool GarbageCollection::getEntrySections(SectionSetTy &pEntry) {
  // all the KEEP sections defined in ldscript are entries, traverse all the
  // input sections and check the SectionMap to find the KEEP sections
  bool hasEntrySymbol = false;
  ELFSection *firstTextSect = nullptr;
  LDSymbol *entry_sym =
      m_Module.getNamePool().findSymbol(m_Backend.getEntry().str());
  if ((entry_sym != nullptr) && (entry_sym->hasFragRef())) {
    pEntry.insert(entry_sym->fragRef()->frag()->getOwningSection());
    hasEntrySymbol = true;
  } else {
    ResolveInfo *entryInfo =
        m_Module.getNamePool().findInfo(m_Backend.getEntry().str());
    if (entryInfo && entryInfo->isBitCode()) {
      entryInfo->shouldPreserve(true);
      Section *sect = getInputSectionForSymbol(*entryInfo);
      if (sect) {
        pEntry.insert(sect);
        hasEntrySymbol = true;
      }
    }
  }

  if (!hasEntrySymbol) {
    Module::obj_iterator obj, objEnd = m_Module.obj_end();
    for (obj = m_Module.obj_begin(); obj != objEnd; ++obj) {
      ELFObjectFile *ObjFile = llvm::dyn_cast<ELFObjectFile>(*obj);
      if (!ObjFile)
        continue;
      for (auto &sect : ObjFile->getSections()) {
        ELFSection *section = llvm::dyn_cast<eld::ELFSection>(sect);
        if (firstTextSect == nullptr) {
          if (section->size() && section->isCode()) {
            firstTextSect = section;
            break;
          }
        }
      }
      // No point running through the rest of the sections.
      if (firstTextSect)
        break;
    }
  }
  for (auto &sec : m_Module.getScript().sectionMap().getEntrySections()) {
    bool isCommonSection = llvm::isa<CommonELFSection>(sec);
    // Common sections can be entry sections for garbage collection even
    // though they are part of an internal input file.
    if (!sec->getInputFile()->isInternal() || isCommonSection)
      pEntry.insert(sec);
  }

  addRetainSections(pEntry);

  GeneralOptions::const_undef_sym_iterator undef_sym, undef_symEnd;
  undef_symEnd = m_Module.getConfig().options().undef_sym_end();
  undef_sym = m_Module.getConfig().options().undef_sym_begin();
  auto add_list_entry = [&](llvm::StringRef name) -> void {
    ResolveInfo *symInfo = m_Module.getNamePool().findInfo(name.str());
    if (!symInfo)
      return;
    if (symInfo->isBitCode())
      symInfo->shouldPreserve(true);
    Section *sec = getInputSectionForSymbol(*symInfo);
    if (sec != nullptr)
      pEntry.insert(sec);
    if (symInfo->outSymbol())
      symInfo->outSymbol()->setShouldIgnore(false);
  };
  for (; undef_sym != undef_symEnd; undef_sym++)
    add_list_entry((*undef_sym)->name());

  auto &DynExpSymbols = m_Config.options().getExportDynSymList();
  for (const auto &S : DynExpSymbols)
    add_list_entry(S->name());

  const auto &symScopes = m_Backend.symbolScopes();
  LinkerScript::Assignments::iterator symit, symite;
  symite = m_Module.getScript().assignments().end();
  symit = m_Module.getScript().assignments().begin();
  for (; symit != symite; symit++) {
    ResolveInfo *info =
        m_Module.getNamePool().findInfo((*symit).second->name().str());
    // Bitcode symbols that are "Common" doesnot have an outsymbol. They do
    // have an outSymbol only if they are preserved.
    if (info != nullptr && info->isCommon() && info->outSymbol()) {
      ELFSection *S = info->getOwningSection();
      pEntry.insert(S);
    }
  }

  // get the sections those the entry symbols defined in
  if (LinkerConfig::Exec == m_Config.codeGenType() ||
      m_Config.options().isPIE()) {
    // when building executable
    // 1. the entry symbol is the entry
    LDSymbol *entry_sym =
        m_Module.getNamePool().findSymbol(m_Backend.getEntry().str());
    // First, try to use entry_sym specifed by command line option,
    // LD script or target specific symbol. If no success, then try to use the
    // first ".text" section.
    if (entry_sym != nullptr && entry_sym->hasFragRef())
      pEntry.insert(entry_sym->fragRef()->frag()->getOwningSection());
    else if (firstTextSect)
      pEntry.insert(firstTextSect);
    else if (!pEntry.size()) {
      m_Config.raise(diag::no_entry_sym_for_GC);
      return false;
    }

    // 2. the symbols have been seen in dynamice objects are entries
    for (auto &G : m_Module.getNamePool().getGlobals()) {
      ResolveInfo *info = G.getValue();
      LDSymbol *sym = info->outSymbol();
      if (nullptr == sym || !sym->hasFragRef())
        continue;

      if (m_Config.isCodeDynamic() || m_Config.options().forceDynamic()) {
        // Skip entry symbolse that have explicitly been marked as local by
        // version script
        const auto &found = symScopes.find(info);
        if (found != symScopes.end() && !(found->second->isGlobal()))
          continue;
      }

      if (!treatSymbolAsEntry(info))
        continue;

      // only the target symbols defined in the concerned sections can be
      // entries
      ELFSection *sect = sym->fragRef()->frag()->getOwningSection();
      if (!mayProcessGC(*sect))
        continue;

      pEntry.insert(sect);
    }
  } else {
    // when building shared objects, the global define symbols are entries
    for (auto &G : m_Module.getNamePool().getGlobals()) {
      ResolveInfo *info = G.getValue();
      // Bitcode symbols that are "Common" doesnot have an outsymbol. They do
      // have an outSymbol only if they are preserved.
      if (info->isCommon() && info->outSymbol()) {
        ELFSection *sect = info->getOwningSection();
        pEntry.insert(sect);
        continue;
      }
      if (!info->isDefine() || info->isLocal() ||
          ((info->visibility() == ResolveInfo::Hidden ||
            info->visibility() == ResolveInfo::Internal) &&
           (info->isGlobal() || info->isWeak()) &&
           (info->isDefine() || info->isCommon())))
        continue;

      const auto &found = symScopes.find(info);
      // Skip entry symbolse that have explicitly been marked as local by
      // version script
      if (found != symScopes.end() && found->second->isLocal())
        continue;

      LDSymbol *sym = info->outSymbol();
      if (nullptr == sym || !sym->hasFragRef())
        continue;

      // only the target symbols defined in the concerned sections can be
      // entries
      ELFSection *sect = sym->fragRef()->frag()->getOwningSection();
      if (!mayProcessGC(*sect))
        continue;
      pEntry.insert(sect);
    }
  }
  return true;
}

void GarbageCollection::findReferencedSectionsAndSymbols(
    SectionSetTy &pEntry, SectionSetTy &LiveSet) {
  // list of sections waiting to be processed
  typedef std::queue<std::pair<Section *, Section *>> WorkListTy;
  WorkListTy work_list;

  if (!pEntry.size())
    return;

  // start from each entry, resolve the transitive closure
  for (const auto &entry_it : pEntry) {
    // add entry point to work list
    work_list.push(std::make_pair(entry_it, nullptr));
    LiveSet.insert(entry_it);

    // add section from the work_list to the referencedSections until every
    // reached sections are added
    while (!work_list.empty()) {
      std::pair<Section *, Section *> P = work_list.front();
      work_list.pop();

      Section *sect = P.first;

      if (ELFSection *elfSect = dyn_cast<ELFSection>(sect)) {
        // A section listed as KEEP in a discarded rule also needs to be checked
        // for references from that section, and those references may need to be
        // kept
        if (!mayProcessGC(*elfSect) &&
            !(elfSect->isIgnore() && pEntry.count(elfSect)))
          continue;
      }

      // add section to the ReferencedSections, if the section has been put into
      // referencedSections, skip this section
      if (!m_ReferencedSections.insert(sect).second)
        continue;

      if (m_Module.getPrinter()->traceGCLive()) {
        m_Config.raise(diag::refers_to)
            << sect->getDecoratedName(m_Config.options());
        if (sect->getInputFile())
          m_Config.raise(diag::referenced_input_file)
              << sect->getInputFile()->getInput()->decoratedPath();
        if (sect->getOldInputFile())
          m_Config.raise(diag::referenced_bc_file)
              << sect->getOldInputFile()->getInput()->decoratedPath();
        if (!P.second)
          m_Config.raise(diag::referenced_by_root_symbol);
        else {
          m_Config.raise(diag::referenced_by)
              << P.second->getDecoratedName(m_Config.options());
          if (P.second->getInputFile())
            m_Config.raise(diag::referenced_input_file)
                << P.second->getInputFile()->getInput()->decoratedPath();
          if (P.second->getOldInputFile())
            m_Config.raise(diag::referenced_bc_file)
                << P.second->getOldInputFile()->getInput()->decoratedPath();
        }
      }

      // get the section reached list, if the section do not has one, which
      // means no referenced between it and other sections, then skip it
      SectionListTy *reach_list =
          m_SectionReachedListMap.findReachedList(*sect);
      if (nullptr == reach_list)
        continue;

      // put the reached sections to work list, skip the one already be in
      // referencedSections
      for (const auto &i : *reach_list) {
        if (m_ReferencedSections.find(i) == m_ReferencedSections.end()) {
          work_list.push(std::make_pair(i, sect));
          LiveSet.insert(i);
        }
      }
    }
  }
}

void GarbageCollection::stripSections(SectionSetTy &S,
                                      bool CommonSectionsOnly) {
  // Traverse all the input Regular and BSS sections, if a section is not found
  // in the ReferencedSections, then it should be garbage collected
  Module::obj_iterator obj, objEnd = m_Module.obj_end();

  std::vector<std::pair<ELFSection *, LDFileFormat::Kind>> _ignoredSections;

  LayoutPrinter *printer = m_Module.getLayoutPrinter();
  bool shouldDemangle = m_Config.options().shouldDemangle();

  for (obj = m_Module.obj_begin(); obj != objEnd; ++obj) {
    ObjectFile *ObjFile = llvm::dyn_cast<ObjectFile>(*obj);
    if (!ObjFile)
      continue;
    // CommonSymbols internal input file content can be stripped by garbage
    // collection.
    if (ObjFile->isInternal() && ObjFile != m_Module.getCommonInternalInput())
      continue;
    for (auto &sect : ObjFile->getSections()) {
      if (sect->isBitcode())
        continue;
      ELFSection *section = llvm::dyn_cast<eld::ELFSection>(sect);
      if (!mayProcessGC(*section))
        continue;
      if (m_ReferencedSections.find(section) == m_ReferencedSections.end()) {
        bool isCommonSection = false;
        Input *I = ObjFile->getInput();
        if (CommonELFSection *commonSection =
                dyn_cast<CommonELFSection>(section)) {
          I = commonSection->getOrigin()->getInput();
          isCommonSection = true;
        }
        // Print the GC collected input section if Tracing is enabled.
        if (m_Config.options().printGCSections() ||
            m_Module.getPrinter()->traceGC()) {
          if (CommonELFSection *commonSection =
                  dyn_cast<CommonELFSection>(section)) {
            ResolveInfo *commResolveInfo =
                m_Backend.getCommonSymbol(commonSection)->resolveInfo();
            assert(commResolveInfo && "Symbol ResolveInfo is missing!");
            std::string decoratedSymName =
                m_Module.getNamePool().getDecoratedName(commResolveInfo,
                                                        shouldDemangle);
            m_Config.raise(diag::trace_gc_symbol)
                << I->decoratedPath() << decoratedSymName;
          } else
            m_Config.raise(diag::trace_gc_section)
                << I->decoratedPath()
                << section->getDecoratedName(m_Config.options());
        }
        if (m_Config.options().isSectionTracingRequested() &&
            m_Config.options().traceSection(section->name().str()))
          m_Config.raise(diag::gc_section_info)
              << section->getDecoratedName(m_Config.options())
              << I->decoratedPath();
        if (CommonSectionsOnly && !isCommonSection)
          _ignoredSections.push_back(
              std::make_pair(section, section->getKind()));
        else if (printer)
          printer->recordGC(section);
        section->setKind(LDFileFormat::Ignore);
      }
    }
  }

  for (obj = m_Module.obj_begin(); obj != objEnd; ++obj) {
    ObjectFile *ObjFile = llvm::dyn_cast<ObjectFile>(*obj);
    if (!ObjFile)
      continue;
    if (ObjFile->isInternal() && ObjFile != m_Module.getCommonInternalInput())
      continue;
    for (auto &sect : ObjFile->getSections()) {
      if (sect->isBitcode())
        continue;
      bool noAllocSection = false;
      bool isEntrySection = false;
      ELFSection *section = llvm::dyn_cast<eld::ELFSection>(sect);
      // Dont keep references from non allocatable sections. This is ELD
      // optimization to reduce memory at runtime.
      if (!section->isAlloc())
        noAllocSection = true;
      if (section->isIgnore())
        continue;
      if (section->isDiscard())
        continue;
      if (S.find(sect) != S.end())
        isEntrySection = true;
      SymbolListTy *referred_syms =
          m_SectionReachedListMap.findReachedSymbolList(*section);
      if (referred_syms) {
        SymbolListTy::iterator it, ite = referred_syms->end();
        for (it = referred_syms->begin(); it != ite; it++)
          if (!noAllocSection || isEntrySection ||
              ((*it)->binding() == ResolveInfo::Local)) {
            (*it)->setShouldIgnore(false);
          }
      }
    }
  }

  for (auto &sec : _ignoredSections)
    sec.first->setKind(sec.second);
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

  if (m_Config.options().isPIE() && m_Config.options().exportDynamic())
    return true;

  if (m_Config.options().exportDynamic())
    return true;

  return false;
}

void GarbageCollection::addRetainSections(SectionSetTy &entrySections) {
  Module::obj_iterator obj, objEnd = m_Module.obj_end();
  for (obj = m_Module.obj_begin(); obj != objEnd; ++obj) {
    ObjectFile *objFile = llvm::dyn_cast<ObjectFile>(*obj);
    if (!objFile)
      continue;
    for (Section *sect : objFile->getSections()) {
      ELFSection *S = llvm::dyn_cast<ELFSection>(sect);
      if (S && S->isRetain()) {
        entrySections.insert(S);
        if (m_Module.getLayoutPrinter())
          m_Module.getLayoutPrinter()->recordRetainedSections();
        if (m_Module.getPrinter()->isVerbose() ||
            m_Config.options().traceSection(S->name().str()))
          m_Config.raise(diag::retaining_section)
              << S->name() << S->originalInput()->getInput()->decoratedPath();
      }
    }
  }
}
