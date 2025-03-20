//===- SymbolResolutionInfo.cpp--------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#include "eld/SymbolResolver/SymbolResolutionInfo.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Fragment/FragmentRef.h"
#include "eld/Input/BitcodeFile.h"
#include "eld/Script/Plugin.h"
#include "eld/SymbolResolver/NamePool.h"
#include "llvm/ADT/StringRef.h"
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

std::string
SymbolResolutionInfo::getSymbolInfoAsString(const LDSymbol *Sym,
                                            const GeneralOptions &Options) {
  std::optional<SymbolInfo> OptSymInfo = getSymbolInfo(Sym);
  if (!OptSymInfo)
    return "";
  SymbolInfo SymInfo = OptSymInfo.value();
  std::string InputFile = SymInfo.getInputFile()->getInput()->decoratedPath();

  auto PluginSymIter = SymbolToPluginMap.find(Sym);
  if (PluginSymIter != SymbolToPluginMap.end()) {
    const Plugin *P = PluginSymIter->second;
    InputFile += "[" + P->getPluginName() + "]";
  }

  std::string SymName =
      Sym->resolveInfo()->getDecoratedName(/*DoDemangle=*/false);
  std::string SymbolInfo = SymName + "(" + InputFile;
  if (SymInfo.getSymbolSectionIndexKind() ==
      SymbolInfo::SectionIndexKind::Def) {
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
    if (!SectName.empty())
      SymbolInfo += ":" + SectName;
  }
  SymbolInfo += ")";
  std::vector<std::string> SymbolAttributes;
  SymbolInfo += " [";
  SymbolAttributes.push_back("Size=" + std::to_string(SymInfo.getSize()));
  if (SymInfo.isBitcodeSymbol())
    SymbolAttributes.push_back("bitcode");
  SymbolAttributes.push_back(SymInfo.getSymbolSectionIndexKindAsStr().str());
  if (SymInfo.getSymbolSectionIndexKind() != SymbolInfo::SectionIndexKind::Abs)
    SymbolAttributes.push_back(SymInfo.getSymbolBindingAsStr().str());
  SymbolAttributes.push_back(SymInfo.getSymbolTypeAsStr().str());
  if (SymInfo.getSymbolVisibility() != ResolveInfo::Visibility::Default)
    SymbolAttributes.push_back(SymInfo.getSymbolVisibilityAsStr().str());
  for (size_t I = 0; I < SymbolAttributes.size(); ++I) {
    SymbolInfo += SymbolAttributes[I];
    if (I != SymbolAttributes.size() - 1)
      SymbolInfo += ", ";
  }
  SymbolInfo += "]";
  return SymbolInfo;
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
