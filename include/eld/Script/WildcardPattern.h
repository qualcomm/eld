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
                           SortPolicy PPolicy = SORT_NONE,
                           ExcludeFiles *PExcludeFileList = nullptr);

  explicit WildcardPattern(StrToken *S, SortPolicy PPolicy = SORT_NONE,
                           ExcludeFiles *PExcludeFileList = nullptr);

  ~WildcardPattern();

  SortPolicy sortPolicy() const { return MSortPolicy; }

  bool hasGlob() const;

  llvm::StringRef prefix() const {
    if (MBPatternIsPrefix)
      return llvm::StringRef(Name.c_str(), Name.length() - 1);
    return "";
  }

  bool isPrefix() const { return MBPatternIsPrefix; }

  bool isSuffix() const { return MBPatternIsSuffix; }

  llvm::StringRef suffix() const {
    if (MBPatternIsSuffix)
      return llvm::StringRef(Name.c_str() + 1, Name.length() - 1);
    return "";
  }

  llvm::StringRef noPrefixSuffix() const {
    llvm::StringRef Suffix = suffix();
    return llvm::StringRef(Suffix.data() + 1, Suffix.size() - 1);
  }

  static bool classof(const StrToken *ThisInputToken) {
    return ThisInputToken->kind() == StrToken::Wildcard;
  }

  const ExcludeFiles *excludeFiles() const { return MExcudeFiles; }

  void setHash(uint64_t HashValue) {
    MBhasHash = true;
    this->HashValue = HashValue;
  }

  uint64_t hashValue() const { return HashValue; }

  bool hasHash() const { return MBhasHash; }

  void setPrefix() { MBPatternIsPrefix = true; }

  void setSuffix() { MBPatternIsSuffix = true; }

  bool matched(llvm::StringRef PName) const;

  bool matched(llvm::StringRef PName, uint64_t Hash) const;

  void setID(size_t ID) { CurID = ID; }

  bool isMatchAll() const { return Name == "*"; }

private:
  void createGlobPattern(llvm::StringRef Name);

private:
  SortPolicy MSortPolicy;
  ExcludeFiles *MExcudeFiles;
  uint64_t HashValue;
  bool MBhasHash;
  bool MBPatternIsPrefix;
  bool MBPatternIsSuffix;
  size_t CurID;
  llvm::GlobPattern MPattern;
};

} // namespace eld

#endif
