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
  size_t operator()(const std::pair<T1, T2> &p) const {
    return llvm::hash_combine(p.first, p.second);
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

  ObjectFile(Input *I, InputFile::Kind K, DiagnosticEngine *diagEngine)
      : InputFile(I, diagEngine, K) {}

  /// Casting support.
  static bool classof(const InputFile *E);

  bool isInputUsed() const { return m_bUsed; }

  void setInputUsed() { m_bUsed = true; }

  /// -------------- Symbol Helpers -------------------------------
  LDSymbol *getSymbol(unsigned int pIdx) const;

  LDSymbol *getSymbol(std::string pName) const;

  void addSymbol(LDSymbol *S);

  /// -------------- Local Symbol Helpers -------------------------------
  LDSymbol *getLocalSymbol(unsigned int pIdx) const;

  void addLocalSymbol(LDSymbol *pSym) { m_LocalSymTab.push_back(pSym); }

  LDSymbol *getLocalSymbol(llvm::StringRef pName) const;

  /// -------------- SymbolTable Helpers -------------------------------
  const std::vector<LDSymbol *> &getLocalSymbols() const {
    return m_LocalSymTab;
  }

  const std::vector<LDSymbol *> &getSymbols() const { return m_SymTab; }

  /// ----------------- Section Helpers ----------------------------
  void addSection(Section *S);

  std::vector<Section *> &getSections() { return m_SectionTable; }

  const std::vector<Section *> &getSections() const { return m_SectionTable; }

  Section *getSection(uint32_t Idx) const {
    if (Idx >= m_SectionTable.size())
      return nullptr;
    return m_SectionTable.at(Idx);
  }

  bool isInputRelocsRead() const { return m_isInputRelocsRead; }

  void setInputRelocsRead() { m_isInputRelocsRead = true; }

  size_t getSectionSize() const { return m_SectionTable.size(); }

  void setHasHighSectionCount() { m_HasHighSectionCount = true; }

  // FIXME: Can we remove 'm_HasHighSectionCount' member by
  // checking the existence of extended symbol section index table
  // or by comparing number of section using m_SectionTable.size()?
  bool hasHighSectionCount() const { return m_HasHighSectionCount; }

  virtual ~ObjectFile() {}

  // --- Add Object File Features ----
  void recordFeature(const std::string &Feature) {
    m_Features.push_back(Feature);
  }

  std::string getFeaturesStr() const;

  const ResolveInfo *getMatchingLocalSymbol(uint64_t sectionIndex,
                                            uint64_t value) const;

  size_t getNumSections() const override { return m_SectionTable.size(); }

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

  std::optional<std::string> getRuleMatchingSectName(uint64_t index) const {
    if (!RMSectNameMap.has_value())
      return std::nullopt;
    auto it = RMSectNameMap->find(index);
    if (it != RMSectNameMap->end())
      return it->second;
    return std::nullopt;
  }

  /// This function is used to set the auxiliary symbol name map.
  void setAuxiliarySymbolNameMap(const AuxiliarySymbolNameMap &SM) {
    AuxSymbolNameMap = SM;
  }

  bool hasAuxiliarySymbolNameMap() const {
    return AuxSymbolNameMap.has_value();
  }

  std::optional<std::string> getAuxiliarySymbolName(uint64_t index) const {
    if (!hasAuxiliarySymbolNameMap())
      return std::nullopt;
    auto it = AuxSymbolNameMap->find(index);
    if (it != AuxSymbolNameMap->end())
      return it->second;
    return std::nullopt;
  }

protected:
  std::vector<Section *> m_SectionTable;
  std::vector<LDSymbol *> m_SymTab;
  std::vector<LDSymbol *> m_LocalSymTab;

  /// Stores the mapping of section index to rule-matching section name. If a
  /// section has rule matching section name, then this name is used instead of
  /// the actual section name for linker script rule-matching.
  std::optional<RuleMatchingSectionNameMap> RMSectNameMap;

  std::optional<AuxiliarySymbolNameMap> AuxSymbolNameMap;

private:
  bool m_bUsed = false;
  bool m_isInputRelocsRead = false;
  bool m_HasHighSectionCount = false;
  std::vector<std::string> m_Features;
  std::unordered_map<std::pair<uint64_t, uint64_t>, LDSymbol *, HashPair>
      m_LocalSymbolInfoMap;
};

} // namespace eld

#endif // ELD_INPUT_OBJECTFILE_H
