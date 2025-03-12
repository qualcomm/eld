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
WildcardPattern::WildcardPattern(StrToken *pPattern, SortPolicy pPolicy,
                                 ExcludeFiles *pExcludeFileList)
    : StrToken(pPattern->name(), StrToken::Wildcard), m_SortPolicy(pPolicy),
      m_ExcudeFiles(pExcludeFileList), m_bhasHash(false),
      m_bPatternIsPrefix(false), m_bPatternIsSuffix(false), m_ID(0) {
  if (pPattern->isQuoted())
    setQuoted();
  createGlobPattern(llvm::StringRef(pPattern->name()));
}

WildcardPattern::WildcardPattern(llvm::StringRef Pattern, SortPolicy pPolicy,
                                 ExcludeFiles *pExcludeFileList)
    : StrToken(std::string(Pattern), StrToken::Wildcard), m_SortPolicy(pPolicy),
      m_ExcudeFiles(pExcludeFileList), m_bhasHash(false),
      m_bPatternIsPrefix(false), m_bPatternIsSuffix(false), m_ID(0) {
  createGlobPattern(Pattern);
}

WildcardPattern::~WildcardPattern() {}

bool WildcardPattern::hasGlob() const {
  if (name().find_first_of("*?[]\\") == std::string::npos)
    return false;
  return true;
}

bool WildcardPattern::matched(llvm::StringRef pName, uint64_t hash) const {
  if (m_bhasHash)
    return m_hashValue == hash;
  return matched(pName);
}

bool WildcardPattern::matched(llvm::StringRef pName) const {
  if (pName.empty())
    return false;

  if (m_Name == "*")
    return true;

  // It is added for GNU-compatiblity. A single backslash is an invalid glob
  // pattern and hence it should not match anything. Please note that the
  // '\' glob pattern should not even match '\'. To match '\', the glob
  // pattern should be '\\' (backslash needs to be escaped).
  if (m_Name == "\\")
    return false;

  return !name().empty() && (m_Pattern.match(pName));
}

void WildcardPattern::createGlobPattern(llvm::StringRef Pattern) {
  // A single backslash is an invalid glob pattern. It is supported for
  // GNU-compatibility.
  if (Pattern == "\\")
    return;
  auto E = llvm::GlobPattern::create(Pattern);
  if (!E)
    ASSERT(0, "Pattern cannot be created for " + Pattern.str());
  m_Pattern = std::move(*E);
  if (!hasGlob())
    setHash(llvm::hash_combine(Pattern));
}
