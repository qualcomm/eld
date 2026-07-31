//===- SymbolResolutionInfo.cpp--------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#include "eld/SymbolResolver/SymbolResolutionInfo.h"
#include "eld/Config/GeneralOptions.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Core/Module.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Fragment/FragmentRef.h"
#include "eld/Input/BitcodeFile.h"
#include "eld/Script/Plugin.h"
#include "eld/SymbolResolver/NamePool.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/raw_ostream.h"
#include <optional>

using namespace eld;

void SymbolResolutionInfo::setupCandidatesInfo(NamePool &NP,
                                               const LinkerScript &Script) {
  std::vector<const LDSymbol *> LTOObjSyms;
  using SymNameToBitcodeSym = llvm::StringMap<const LDSymbol *>;
  // The mapping is this way so that it is efficient to find the bitcode
  // symbol corresponding to a LTO-object symbol.
  std::unordered_map<const InputFile *, SymNameToBitcodeSym> BitcodeSymbolsMap;

  for (const auto &[sym, symInfo] : SymbolInfoMap) {
    const InputFile *IF = symInfo.getInputFile();
    if (IF->isLTOObject()) {
      LTOObjSyms.push_back(sym);
      continue;
    }
    llvm::StringRef SymName = sym->resolveInfo()->getName();
    Candidates[SymName].push_back(sym);

    if (symInfo.isBitcodeSymbol()) {
      BitcodeSymbolsMap[IF][SymName] = sym;
    }
  }
  for (const LDSymbol *S : LTOObjSyms) {
    if (SymbolInfoMap[S].getSymbolSectionIndexKind() ==
        SymbolInfo::SectionIndexKind::Undef)
      continue;
    // Ideally, all the non-undef LTO-object symbols must have a corresponding
    // section. This could be an ASSERT as well, however, we should not fail the
    // link due to a failed condition in a diagnostics part.
    if (!S->fragRef() || !S->fragRef()->frag() ||
        !S->fragRef()->frag()->getOwningSection())
      continue;
    InputFile *OriginalInput =
        S->fragRef()->frag()->getOwningSection()->originalInput();
    llvm::StringRef SymName = S->resolveInfo()->getName();
    auto BitcodeSymIter = BitcodeSymbolsMap[OriginalInput].find(SymName);
    BitcodeSymToLtoObjectSymMap[BitcodeSymIter->second] = S;
  }
}

const SymbolResolutionInfo::CandidatesType &
SymbolResolutionInfo::getCandidates(llvm::StringRef SymName) {
  static const CandidatesType Empty{};
  auto Iter = Candidates.find(SymName);
  if (Iter == Candidates.end())
    return Empty;
  return Iter->second;
}

std::string SymbolResolutionInfo::getSymbolSectionName(
    const LDSymbol *Sym, const SymbolInfo &SymInfo,
    const GeneralOptions &Options) const {
  if (SymInfo.getSymbolSectionIndexKind() != SymbolInfo::SectionIndexKind::Def)
    return "";
  std::string SectName;
  if (SymInfo.isBitcodeSymbol()) {
    const BitcodeFile *BitcodeInputFile =
        llvm::cast<const BitcodeFile>(SymInfo.getInputFile());
    Section *BitcodeSect =
        BitcodeInputFile->getInputSectionForSymbol(*Sym->resolveInfo());
    // The check is required here because it is undefined behavior to
    // initialize std::string with nullptr.
    // bitcodeSect can be nullptr in cases where bitcode section cannot be
    // determined. For example: we cannot know input sections for asm symbols.
    if (BitcodeSect)
      SectName = BitcodeSect->name();
  } else {
    const FragmentRef *FragRef = Sym->fragRef();
    if (FragRef != FragmentRef::null() && FragRef != FragmentRef::discard() &&
        FragRef != nullptr && FragRef->frag()) {
      Section *S = FragRef->frag()->getOwningSection();
      // Ideally, we should never have a fragment without an owning section.
      // Thus, this can be an assert. However, if in some corner case this
      // condition is not satisfied, then I don't think we should fail
      // the link because of some information required for diagnostics.
      if (S)
        SectName = S->getDecoratedName(Options);
    }
  }
  return SectName;
}

void SymbolResolutionInfo::recordSymbolInfo(const LDSymbol *Sym,
                                            SymbolInfo SymInfo) {
  SymbolInfoMap[Sym] = SymInfo;
}

const LDSymbol *SymbolResolutionInfo::getCorrespondingLTOObjectSymIfAny(
    const LDSymbol *S) const {
  auto It = BitcodeSymToLtoObjectSymMap.find(S);
  if (It != BitcodeSymToLtoObjectSymMap.end())
    return It->second;
  return nullptr;
}

/// clang-format off
/// JSON schema emitted by --emit-symbol-resolution-report:
///
/// {
///   "SymbolResolutionReportVersion": 1,
///   "Symbols": [
///     {
///       "Name": "foo",
///       "Selected": "libc.a(malloc.o)(foo)",  // <InputFile>(<SymbolName>)
///       "Candidates": [
///         {
///           "Name": "foo",
///           "InputFile": "libc.a(malloc.o)",
///           "Section": ".text",             // omitted when no section
///           "Plugin": "MyPlugin",           // only for plugin-created symbols
///           "Size": 4,
///           "Bitcode": false,
///           "SectionIndexKind": "Def",
///           "Binding": "Global",            // always present
///           "Type": "Object",
///           "Visibility": "Default",        // always present
///           "IsSelected": true,
///           "LTOObjectSymbol": { ...same shape... } // bitcode w/ post-LTO sym
///         }
///       ]
///     }
///   ]
/// }
/// clang-format on
llvm::json::Object SymbolResolutionInfo::buildCandidateObject(
    const LDSymbol *Candidate, const SymbolInfo &SymInfo,
    const GeneralOptions &Options, bool IsSelected) {
  llvm::json::Object Obj;
  Obj["Name"] =
      Candidate->resolveInfo()->getDecoratedName(/*DoDemangle=*/false);
  Obj["InputFile"] = SymInfo.getInputFile()->getInput()->decoratedPath();
  std::string SectName = getSymbolSectionName(Candidate, SymInfo, Options);
  if (!SectName.empty())
    Obj["Section"] = SectName;
  if (const Plugin *P = getSymbolPlugin(Candidate))
    Obj["Plugin"] = P->getPluginName();
  Obj["Size"] = static_cast<int64_t>(SymInfo.getSize());
  Obj["Bitcode"] = SymInfo.isBitcodeSymbol();
  Obj["SectionIndexKind"] = SymInfo.getSymbolSectionIndexKindAsStr();
  Obj["Binding"] = SymInfo.getSymbolBindingAsStr();
  Obj["Type"] = SymInfo.getSymbolTypeAsStr();
  Obj["Visibility"] = SymInfo.getSymbolVisibilityAsStr();
  Obj["IsSelected"] = IsSelected;
  return Obj;
}

bool SymbolResolutionInfo::emitSymbolResolutionReport(
    Module &CurModule, llvm::StringRef Filename) {
  DiagnosticEngine *DiagEngine = CurModule.getConfig().getDiagEngine();
  std::error_code EC;
  llvm::raw_fd_ostream OS(Filename, EC);
  if (EC) {
    if (DiagEngine)
      DiagEngine->raise(Diag::unable_to_write_json_file)
          << Filename << EC.message();
    return false;
  }

  NamePool &NP = CurModule.getNamePool();
  const GeneralOptions &Options = CurModule.getConfig().options();
  setupCandidatesInfo(NP, CurModule.getScript());

  llvm::json::Array SymbolsArray;
  for (const auto *RI : CurModule.getSymbols()) {
    if (RI->isLocal() &&
        RI->resolvedOrigin() !=
            CurModule.getInternalInput(Module::InternalInputType::Plugin))
      continue;

    llvm::StringRef SymName = RI->getName();
    const SymbolResolutionInfo::CandidatesType &Candidates =
        getCandidates(SymName);

    llvm::json::Object SymEntry;
    SymEntry["Name"] = SymName;

    llvm::json::Array CandidatesArray;
    std::string Selected;
    for (const LDSymbol *Candidate : Candidates) {
      std::optional<SymbolInfo> OptSymbolInfo = getSymbolInfo(Candidate);
      if (!OptSymbolInfo)
        continue;
      SymbolInfo CandidateInfo = OptSymbolInfo.value();

      bool IsSelected = Candidate->resolveInfo()->outSymbol() == Candidate ||
                        (CandidateInfo.isBitcodeSymbol() &&
                         CandidateInfo.getInputFile() ==
                             Candidate->resolveInfo()->resolvedOrigin());

      llvm::json::Object CandObj =
          buildCandidateObject(Candidate, CandidateInfo, Options, IsSelected);

      if (CandidateInfo.isBitcodeSymbol()) {
        if (const LDSymbol *LTOSym =
                getCorrespondingLTOObjectSymIfAny(Candidate)) {
          if (std::optional<SymbolInfo> LTOInfo = getSymbolInfo(LTOSym))
            CandObj["LTOObjectSymbol"] = buildCandidateObject(
                LTOSym, LTOInfo.value(), Options, IsSelected);
        }
      }

      if (IsSelected)
        Selected =
            (CandidateInfo.getInputFile()->getInput()->decoratedPath() + "(" +
             Candidate->resolveInfo()->getDecoratedName(/*DoDemangle=*/false) +
             ")");

      CandidatesArray.push_back(std::move(CandObj));
    }

    if (!Selected.empty())
      SymEntry["Selected"] = Selected;
    SymEntry["Candidates"] = std::move(CandidatesArray);
    SymbolsArray.push_back(std::move(SymEntry));
  }

  llvm::json::Object Root;
  Root["SymbolResolutionReportVersion"] = 1;
  Root["Symbols"] = std::move(SymbolsArray);
  OS << llvm::formatv("{0:2}\n", llvm::json::Value(std::move(Root)));
  return true;
}
