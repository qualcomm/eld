//===- ObjectFile.h--------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_INPUT_OBJECTFILE_H
#define ELD_INPUT_OBJECTFILE_H

#include "eld/Input/InputFile.h"
#include "llvm/ADT/Hashing.h"
#include <optional>
#include <unordered_map>
#include <vector>

namespace eld {

// A hash function used to hash a pair.
// Used to hash elfsection and addend to indentify local symbols uniquely
struct HashPair {
  template <class T1, class T2>
  size_t operator()(const std::pair<T1, T2> &P) const {
    return llvm::hash_combine(P.first, P.second);
  }
};

class Section;
class ELFSection;
class ResolveInfo;
class LDSymbol;
class DiagnosticPrinter;

/** \class InputFile
 *  \brief InputFile represents a real object file, a linker script or anything
 *  that the rest of the linker can work with.
 */
class ObjectFile : public InputFile {
public:
  using RuleMatchingSectionNameMap = std::unordered_map<uint64_t, std::string>;
  using AuxiliarySymbolNameMap = std::unordered_map<uint64_t, std::string>;

  ObjectFile(Input *I, InputFile::InputFileKind K, DiagnosticEngine *DiagEngine)
      : InputFile(I, DiagEngine, K) {}

  /// Casting support.
  static bool classof(const InputFile *E);

  bool isInputUsed() const { return BUsed; }

  void setInputUsed() { BUsed = true; }

  /// -------------- Symbol Helpers -------------------------------
  LDSymbol *getSymbol(unsigned int PIdx) const;

  LDSymbol *getSymbol(std::string PName) const;

  void addSymbol(LDSymbol *S);

  /// -------------- Local Symbol Helpers -------------------------------
  LDSymbol *getLocalSymbol(unsigned int PIdx) const;

  void addLocalSymbol(LDSymbol *PSym) { MLocalSymTab.push_back(PSym); }

  LDSymbol *getLocalSymbol(llvm::StringRef PName) const;

  /// -------------- SymbolTable Helpers -------------------------------
  const std::vector<LDSymbol *> &getLocalSymbols() const {
    return MLocalSymTab;
  }

  const std::vector<LDSymbol *> &getSymbols() const { return SymTab; }

  /// ----------------- Section Helpers ----------------------------
  void addSection(Section *S);

  std::vector<Section *> &getSections() { return MSectionTable; }

  const std::vector<Section *> &getSections() const { return MSectionTable; }

  Section *getSection(uint32_t Idx) const {
    if (Idx >= MSectionTable.size())
      return nullptr;
    return MSectionTable.at(Idx);
  }

  bool isInputRelocsRead() const { return IsInputRelocsRead; }

  void setInputRelocsRead() { IsInputRelocsRead = true; }

  size_t getSectionSize() const { return MSectionTable.size(); }

  void setHasHighSectionCount() { HasHighSectionCount = true; }

  // FIXME: Can we remove 'HasHighSectionCount' member by
  // checking the existence of extended symbol section index table
  // or by comparing number of section using m_SectionTable.size()?
  bool hasHighSectionCount() const { return HasHighSectionCount; }

  virtual ~ObjectFile() {}

  // --- Add Object File Features ----
  void recordFeature(const std::string &Feature) {
    Features.push_back(Feature);
  }

  std::string getFeaturesStr() const;

  const ResolveInfo *getMatchingLocalSymbol(uint64_t SectionIndex,
                                            uint64_t Value) const;

  size_t getNumSections() const override { return MSectionTable.size(); }

  void setRuleMatchingSectionNameMap(const RuleMatchingSectionNameMap &SM) {
    RMSectNameMap = SM;
  }

  bool hasRuleMatchingSectionNameMap() const {
    return RMSectNameMap.has_value();
  }

  const std::optional<RuleMatchingSectionNameMap> &
  getRuleMatchingSectNameMap() const {
    return RMSectNameMap;
  }

  std::optional<std::string> getRuleMatchingSectName(uint64_t Index) const {
    if (!RMSectNameMap.has_value())
      return std::nullopt;
    auto It = RMSectNameMap->find(Index);
    if (It != RMSectNameMap->end())
      return It->second;
    return std::nullopt;
  }

  /// This function is used to set the auxiliary symbol name map.
  void setAuxiliarySymbolNameMap(const AuxiliarySymbolNameMap &SM) {
    AuxSymbolNameMap = SM;
  }

  bool hasAuxiliarySymbolNameMap() const {
    return AuxSymbolNameMap.has_value();
  }

  std::optional<std::string> getAuxiliarySymbolName(uint64_t Index) const {
    if (!hasAuxiliarySymbolNameMap())
      return std::nullopt;
    auto It = AuxSymbolNameMap->find(Index);
    if (It != AuxSymbolNameMap->end())
      return It->second;
    return std::nullopt;
  }

protected:
  std::vector<Section *> MSectionTable;
  std::vector<LDSymbol *> SymTab;
  std::vector<LDSymbol *> MLocalSymTab;

  /// Stores the mapping of section index to rule-matching section name. If a
  /// section has rule matching section name, then this name is used instead of
  /// the actual section name for linker script rule-matching.
  std::optional<RuleMatchingSectionNameMap> RMSectNameMap;

  std::optional<AuxiliarySymbolNameMap> AuxSymbolNameMap;

private:
  bool BUsed = false;
  bool IsInputRelocsRead = false;
  bool HasHighSectionCount = false;
  std::vector<std::string> Features;
  std::unordered_map<std::pair<uint64_t, uint64_t>, LDSymbol *, HashPair>
      LocalSymbolInfoMap;
};

} // namespace eld

#endif // ELD_INPUT_OBJECTFILE_H
