//===- WildcardPattern.h---------------------------------------------------===//
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

//===- WildcardPattern.h --------------------------------------------------===//
//===----------------------------------------------------------------------===//
#ifndef ELD_SCRIPT_WILDCARDPATTERN_H
#define ELD_SCRIPT_WILDCARDPATTERN_H

#include "eld/Script/StrToken.h"
#include "eld/Script/StringList.h"
#include "eld/SymbolResolver/ResolveInfo.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/GlobPattern.h"

#include <unordered_map>

namespace eld {

class ExcludeFiles;
/** \class WildcardPattern
 *  \brief This class defines the interfaces to Input Section Wildcard Patterns
 */

class WildcardPattern : public StrToken {
public:
  enum SortPolicy {
    SORT_NONE,
    SORT_BY_NAME,
    SORT_BY_ALIGNMENT,
    SORT_BY_NAME_ALIGNMENT,
    SORT_BY_ALIGNMENT_NAME,
    SORT_BY_INIT_PRIORITY,
    EXCLUDE
  };

public:
  explicit WildcardPattern(llvm::StringRef Pattern,
                           SortPolicy pPolicy = SORT_NONE,
                           ExcludeFiles *pExcludeFileList = nullptr);

  explicit WildcardPattern(StrToken *S, SortPolicy pPolicy = SORT_NONE,
                           ExcludeFiles *pExcludeFileList = nullptr);

  ~WildcardPattern();

  SortPolicy sortPolicy() const { return m_SortPolicy; }

  bool hasGlob() const;

  llvm::StringRef prefix() const {
    if (m_bPatternIsPrefix)
      return llvm::StringRef(m_Name.c_str(), m_Name.length() - 1);
    return "";
  }

  bool isPrefix() const { return m_bPatternIsPrefix; }

  bool isSuffix() const { return m_bPatternIsSuffix; }

  llvm::StringRef suffix() const {
    if (m_bPatternIsSuffix)
      return llvm::StringRef(m_Name.c_str() + 1, m_Name.length() - 1);
    return "";
  }

  llvm::StringRef noPrefixSuffix() const {
    llvm::StringRef Suffix = suffix();
    return llvm::StringRef(Suffix.data() + 1, Suffix.size() - 1);
  }

  static bool classof(const StrToken *pToken) {
    return pToken->kind() == StrToken::Wildcard;
  }

  const ExcludeFiles *excludeFiles() const { return m_ExcudeFiles; }

  void setHash(uint64_t hashValue) {
    m_bhasHash = true;
    m_hashValue = hashValue;
  }

  uint64_t hashValue() const { return m_hashValue; }

  bool hasHash() const { return m_bhasHash; }

  void setPrefix() { m_bPatternIsPrefix = true; }

  void setSuffix() { m_bPatternIsSuffix = true; }

  bool matched(llvm::StringRef pName) const;

  bool matched(llvm::StringRef pName, uint64_t hash) const;

  void setID(size_t ID) { m_ID = ID; }

  bool isMatchAll() const { return m_Name == "*"; }

private:
  void createGlobPattern(llvm::StringRef Name);

private:
  SortPolicy m_SortPolicy;
  ExcludeFiles *m_ExcudeFiles;
  uint64_t m_hashValue;
  bool m_bhasHash;
  bool m_bPatternIsPrefix;
  bool m_bPatternIsSuffix;
  size_t m_ID;
  llvm::GlobPattern m_Pattern;
};

} // namespace eld

#endif
