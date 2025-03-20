//===- WildcardPattern.cpp-------------------------------------------------===//
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

//===- WildcardPattern.cpp ------------------------------------------------===//
//===----------------------------------------------------------------------===//
#include "eld/Script/WildcardPattern.h"
#include "eld/Script/ExcludeFiles.h"
#include "eld/Support/MsgHandling.h"
#include "llvm/ADT/Hashing.h"
#include "llvm/Support/raw_ostream.h"
#include <string>

using namespace eld;

//===----------------------------------------------------------------------===//
// WildcardPattern
//===----------------------------------------------------------------------===//
WildcardPattern::WildcardPattern(StrToken *PPattern, SortPolicy PPolicy,
                                 ExcludeFiles *PExcludeFileList)
    : StrToken(PPattern->name(), StrToken::Wildcard), MSortPolicy(PPolicy),
      MExcudeFiles(PExcludeFileList), MBhasHash(false),
      MBPatternIsPrefix(false), MBPatternIsSuffix(false), CurID(0) {
  if (PPattern->isQuoted())
    setQuoted();
  createGlobPattern(llvm::StringRef(PPattern->name()));
}

WildcardPattern::WildcardPattern(llvm::StringRef Pattern, SortPolicy PPolicy,
                                 ExcludeFiles *PExcludeFileList)
    : StrToken(std::string(Pattern), StrToken::Wildcard), MSortPolicy(PPolicy),
      MExcudeFiles(PExcludeFileList), MBhasHash(false),
      MBPatternIsPrefix(false), MBPatternIsSuffix(false), CurID(0) {
  createGlobPattern(Pattern);
}

WildcardPattern::~WildcardPattern() {}

bool WildcardPattern::hasGlob() const {
  if (name().find_first_of("*?[]\\") == std::string::npos)
    return false;
  return true;
}

bool WildcardPattern::matched(llvm::StringRef PName, uint64_t Hash) const {
  if (MBhasHash)
    return HashValue == Hash;
  return matched(PName);
}

bool WildcardPattern::matched(llvm::StringRef PName) const {
  if (PName.empty())
    return false;

  if (Name == "*")
    return true;

  // It is added for GNU-compatiblity. A single backslash is an invalid glob
  // pattern and hence it should not match anything. Please note that the
  // '\' glob pattern should not even match '\'. To match '\', the glob
  // pattern should be '\\' (backslash needs to be escaped).
  if (Name == "\\")
    return false;

  return !name().empty() && (MPattern.match(PName));
}

void WildcardPattern::createGlobPattern(llvm::StringRef Pattern) {
  // A single backslash is an invalid glob pattern. It is supported for
  // GNU-compatibility.
  if (Pattern == "\\")
    return;
  auto E = llvm::GlobPattern::create(Pattern);
  if (!E)
    ASSERT(0, "Pattern cannot be created for " + Pattern.str());
  MPattern = std::move(*E);
  if (!hasGlob())
    setHash(llvm::hash_combine(Pattern));
}
