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
                                               const LinkerScript &script) {
  std::vector<const LDSymbol *> LTOObjSyms;
  using SymNameToBitcodeSym = llvm::StringMap<const LDSymbol *>;
  // The mapping is this way so that it is efficient to find the bitcode
  // symbol corresponding to a LTO-object symbol.
  std::unordered_map<const InputFile *, SymNameToBitcodeSym> bitcodeSymbolsMap;

  for (const auto &[sym, symInfo] : m_SymbolInfoMap) {
    const InputFile *IF = symInfo.getInputFile();
    if (IF->isLTOObject()) {
      LTOObjSyms.push_back(sym);
      continue;
    }
    llvm::StringRef symName = sym->resolveInfo()->getName();
    m_Candidates[symName].push_back(sym);

    if (symInfo.isBitcodeSymbol()) {
      bitcodeSymbolsMap[IF][symName] = sym;
    }
  }
  for (const LDSymbol *S : LTOObjSyms) {
    if (m_SymbolInfoMap[S].getSymbolSectionIndexKind() ==
        SymbolInfo::SectionIndexKind::Undef)
      continue;
    // Ideally, all the non-undef LTO-object symbols must have a corresponding
    // section. This could be an ASSERT as well, however, we should not fail the
    // link due to a failed condition in a diagnostics part.
    if (!S->fragRef() || !S->fragRef()->frag() ||
        !S->fragRef()->frag()->getOwningSection())
      continue;
    InputFile *originalInput =
        S->fragRef()->frag()->getOwningSection()->originalInput();
    llvm::StringRef symName = S->resolveInfo()->getName();
    auto bitcodeSymIter = bitcodeSymbolsMap[originalInput].find(symName);
    m_BitcodeSymToLTOObjectSymMap[bitcodeSymIter->second] = S;
  }
}

const SymbolResolutionInfo::CandidatesType &
SymbolResolutionInfo::getCandidates(llvm::StringRef symName) {
  static const CandidatesType empty{};
  auto iter = m_Candidates.find(symName);
  if (iter == m_Candidates.end())
    return empty;
  return iter->second;
}

std::string
SymbolResolutionInfo::getSymbolInfoAsString(const LDSymbol *sym,
                                            const GeneralOptions &options) {
  std::optional<SymbolInfo> optSymInfo = getSymbolInfo(sym);
  if (!optSymInfo)
    return "";
  SymbolInfo symInfo = optSymInfo.value();
  std::string inputFile = symInfo.getInputFile()->getInput()->decoratedPath();

  auto pluginSymIter = m_SymbolToPluginMap.find(sym);
  if (pluginSymIter != m_SymbolToPluginMap.end()) {
    const Plugin *P = pluginSymIter->second;
    inputFile += "[" + P->getPluginName() + "]";
  }

  std::string symName =
      sym->resolveInfo()->getDecoratedName(/*DoDemangle=*/false);
  std::string symbolInfo = symName + "(" + inputFile;
  if (symInfo.getSymbolSectionIndexKind() ==
      SymbolInfo::SectionIndexKind::Def) {
    std::string sectName;
    if (symInfo.isBitcodeSymbol()) {
      const BitcodeFile *bitcodeInputFile =
          llvm::cast<const BitcodeFile>(symInfo.getInputFile());
      Section *bitcodeSect =
          bitcodeInputFile->getInputSectionForSymbol(*sym->resolveInfo());
      // The check is required here because it is undefined behavior to
      // initialize std::string with nullptr.
      // bitcodeSect can be nullptr in cases where bitcode section cannot be
      // determined. For example: we cannot know input sections for asm symbols.
      if (bitcodeSect)
        sectName = bitcodeSect->name();
    } else {
      const FragmentRef *fragRef = sym->fragRef();
      if (fragRef != FragmentRef::Null() && fragRef != FragmentRef::Discard() &&
          fragRef != nullptr && fragRef->frag()) {
        Section *S = fragRef->frag()->getOwningSection();
        // Ideally, we should never have a fragment without an owning section.
        // Thus, this can be an assert. However, if in some corner case this
        // condition is not satisfied, then I don't think we should fail
        // the link because of some information required for diagnostics.
        if (S)
          sectName = S->getDecoratedName(options);
      }
    }
    if (!sectName.empty())
      symbolInfo += ":" + sectName;
  }
  symbolInfo += ")";
  std::vector<std::string> symbolAttributes;
  symbolInfo += " [";
  symbolAttributes.push_back("Size=" + std::to_string(symInfo.getSize()));
  if (symInfo.isBitcodeSymbol())
    symbolAttributes.push_back("bitcode");
  symbolAttributes.push_back(symInfo.getSymbolSectionIndexKindAsStr().str());
  if (symInfo.getSymbolSectionIndexKind() != SymbolInfo::SectionIndexKind::Abs)
    symbolAttributes.push_back(symInfo.getSymbolBindingAsStr().str());
  symbolAttributes.push_back(symInfo.getSymbolTypeAsStr().str());
  if (symInfo.getSymbolVisibility() != ResolveInfo::Visibility::Default)
    symbolAttributes.push_back(symInfo.getSymbolVisibilityAsStr().str());
  for (size_t i = 0; i < symbolAttributes.size(); ++i) {
    symbolInfo += symbolAttributes[i];
    if (i != symbolAttributes.size() - 1)
      symbolInfo += ", ";
  }
  symbolInfo += "]";
  return symbolInfo;
}

void SymbolResolutionInfo::recordSymbolInfo(const LDSymbol *sym,
                                            SymbolInfo symInfo) {
  m_SymbolInfoMap[sym] = symInfo;
}

const LDSymbol *SymbolResolutionInfo::getCorrespondingLTOObjectSymIfAny(
    const LDSymbol *S) const {
  auto it = m_BitcodeSymToLTOObjectSymMap.find(S);
  if (it != m_BitcodeSymToLTOObjectSymMap.end())
    return it->second;
  return nullptr;
}
