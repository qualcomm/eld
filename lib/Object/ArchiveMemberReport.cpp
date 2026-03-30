//===- ArchiveMemberReport.cpp---------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Object/ArchiveMemberReport.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Input/ArchiveMemberInput.h"
#include "eld/Input/ObjectFile.h"
#include "eld/Object/ObjectLinker.h"
#include "eld/SymbolResolver/LDSymbol.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace eld;

// clang-format off
// JSON schema emitted by --archive-member-report:
//
// {
//   "ArchiveMembers": [
//     {
//       "MemberPath": "libc.a(malloc.o)",
//       "Archive": "libc.a",
//       "Member": "malloc.o",
//
//       // Exactly one referrer form is normally present.
//       "ReferrerObjectFile": "crt0.o",
//       "ReferrerArchive": "libstandalone.a",
//       "ReferrerMember": "sys_init_tls.o",
//
//       // Pull-in cause for non-whole-archive inclusion.
//       "Symbol": "malloc",
//
//       // Present for -whole-archive inclusion.
//       "WholeArchive": true,
//       "Reason": "-whole-archive",
//
//       // Metadata.
//       "MemberIsBitcode": false,
//       "ReferrerIsBitcode": false,
//       "MemberIsArchiveMember": true,
//       "ReferrerIsArchiveMember": false,
//
//       // Symbol context.
//       "MemberSymbols": ["malloc", "free"],
//       "MemberReferencedSymbols": ["errno", "sbrk"]
//     }
//   ]
// }
//
// Notes:
// - Non-whole-archive pull-ins have Symbol + one referrer form.
// - Whole-archive records set WholeArchive/Reason and may not carry Symbol.
// - MemberReferencedSymbols includes pull-in edges and undefined refs seen in
//   the origin object so symbol-chain tracing can continue across members.
// clang-format on

static bool emitArchiveMemberReportImpl(const ObjectLinker &ObjLinker,
                                        llvm::StringRef Filename,
                                        DiagnosticEngine *DiagEngine) {
  const auto &ArchiveRecords = ObjLinker.getArchiveRecordsForReport();
  std::error_code EC;
  llvm::raw_fd_ostream OS(Filename, EC);
  if (EC) {
    if (DiagEngine) {
      DiagEngine->raise(Diag::unable_to_write_json_file)
          << Filename << EC.message();
    }
    return false;
  }

  std::unordered_map<std::string, std::unordered_set<std::string>>
      MemberToReferencedSymbols;
  for (const auto &Record : ArchiveRecords) {
    InputFile *Referrer = Record.Referrer;
    LDSymbol *Symbol = Record.Symbol;
    if (!Referrer || !Symbol)
      continue;
    std::string ReferrerPath = Referrer->getInput()->decoratedPath(false);
    std::string SymbolName = Symbol->name();
    if (ReferrerPath.empty() || SymbolName.empty())
      continue;
    MemberToReferencedSymbols[ReferrerPath].insert(std::move(SymbolName));
  }

  llvm::json::Array Records;
  for (const auto &Record : ArchiveRecords) {
    Input *Origin = Record.Origin;
    InputFile *Referrer = Record.Referrer;
    LDSymbol *Symbol = Record.Symbol;
    if (!Origin)
      continue;

    llvm::json::Object Entry;
    std::string MemberPath = Origin->decoratedPath(false);
    Entry["MemberPath"] = MemberPath;
    if (auto *ArchiveMember = llvm::dyn_cast<ArchiveMemberInput>(Origin)) {
      auto Pair = ArchiveMember->decoratedPathPair(false);
      Entry["Archive"] = Pair.first;
      Entry["Member"] = Pair.second;
    }
    if (Referrer) {
      const std::string ReferrerPath =
          Referrer->getInput()->decoratedPath(false);
      if (auto *RefArchiveMember =
              llvm::dyn_cast<ArchiveMemberInput>(Referrer->getInput())) {
        auto Pair = RefArchiveMember->decoratedPathPair(false);
        Entry["ReferrerArchive"] = Pair.first;
        Entry["ReferrerMember"] = Pair.second;
      } else {
        Entry["ReferrerObjectFile"] = ReferrerPath;
      }
    }
    if (Symbol)
      Entry["Symbol"] = Symbol->name();
    const bool IsWholeArchive = (Referrer == nullptr && Symbol == nullptr);
    if (IsWholeArchive) {
      Entry["Reason"] = ObjLinker.getWholeArchiveStringForReport();
      Entry["WholeArchive"] = true;
    }
    if (InputFile *OriginFile = Origin->getInputFile())
      Entry["MemberIsBitcode"] = OriginFile->isBitcode();
    if (Referrer)
      Entry["ReferrerIsBitcode"] = Referrer->isBitcode();
    else
      Entry["ReferrerIsBitcode"] = false;
    Entry["MemberIsArchiveMember"] = llvm::isa<ArchiveMemberInput>(Origin);
    Entry["ReferrerIsArchiveMember"] =
        Referrer ? llvm::isa<ArchiveMemberInput>(Referrer->getInput()) : false;

    std::unordered_set<std::string> ReferencedSymbolsSet;
    auto RefSymbolsIt = MemberToReferencedSymbols.find(MemberPath);
    if (RefSymbolsIt != MemberToReferencedSymbols.end())
      ReferencedSymbolsSet.insert(RefSymbolsIt->second.begin(),
                                  RefSymbolsIt->second.end());

    if (InputFile *OriginFile = Origin->getInputFile()) {
      if (auto *Obj = llvm::dyn_cast<ObjectFile>(OriginFile)) {
        llvm::json::Array Symbols;
        std::unordered_set<std::string> Seen;
        for (auto *Sym : Obj->getSymbols()) {
          if (!Sym)
            continue;
          const ResolveInfo *RI = Sym->resolveInfo();
          if (!RI)
            continue;
          if (RI->isFile() || RI->isSection())
            continue;
          std::string Name = Sym->name();
          if (Name.empty())
            continue;
          if (Sym->sectionIndex() == llvm::ELF::SHN_UNDEF)
            ReferencedSymbolsSet.insert(Name);
          if ((RI->isDefine() || RI->isCommon()) && Seen.insert(Name).second)
            Symbols.push_back(Name);
        }
        if (!Symbols.empty())
          Entry["MemberSymbols"] = std::move(Symbols);
      }
    }
    if (!ReferencedSymbolsSet.empty()) {
      std::vector<std::string> SortedSymbols(ReferencedSymbolsSet.begin(),
                                             ReferencedSymbolsSet.end());
      std::sort(SortedSymbols.begin(), SortedSymbols.end());
      llvm::json::Array ReferencedSymbols;
      for (const std::string &Name : SortedSymbols)
        ReferencedSymbols.push_back(Name);
      Entry["MemberReferencedSymbols"] = std::move(ReferencedSymbols);
    }
    Records.push_back(std::move(Entry));
  }

  llvm::json::Object Root;
  Root["ArchiveMembers"] = std::move(Records);
  OS << llvm::formatv("{0:2}\n", llvm::json::Value(std::move(Root)));
  return true;
}

bool eld::emitArchiveMemberReport(const ObjectLinker &ObjLinker,
                                  llvm::StringRef Filename,
                                  DiagnosticEngine *DiagEngine) {
  return emitArchiveMemberReportImpl(ObjLinker, Filename, DiagEngine);
}
