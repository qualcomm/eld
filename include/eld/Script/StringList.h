//===- StringList.h--------------------------------------------------------===//
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
#ifndef ELD_SCRIPT_STRINGLIST_H
#define ELD_SCRIPT_STRINGLIST_H

#include "llvm/ADT/iterator_range.h"
#include "llvm/Support/raw_ostream.h"
#include <vector>

namespace eld {

class StrToken;

/** \class StringList
 *  \brief This class defines the interfaces to StringList.
 */

class StringList {
public:
  typedef std::vector<StrToken *> Tokens;
  using TokenRange = llvm::iterator_range<Tokens::iterator>;
  using TokenConstRange = llvm::iterator_range<Tokens::const_iterator>;

  StringList();

  ~StringList() { TokenList.clear(); }

  TokenRange tokens() {
    return llvm::make_range(TokenList.begin(), TokenList.end());
  }
  TokenConstRange tokens() const {
    return llvm::make_range(TokenList.begin(), TokenList.end());
  }

  const StrToken *front() const { return TokenList.front(); }
  StrToken *front() { return TokenList.front(); }
  const StrToken *back() const { return TokenList.back(); }
  StrToken *back() { return TokenList.back(); }

  bool empty() const { return TokenList.empty(); }

  size_t size() const { return TokenList.size(); }

  void pushBack(StrToken *ThisInputToken);

  void dump(llvm::raw_ostream &Outs) const;

  const Tokens &getTokens() const { return TokenList; }

private:
  Tokens TokenList;
};

} // namespace eld

#endif
