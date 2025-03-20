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
  typedef Tokens::const_iterator const_iterator;
  typedef Tokens::iterator iterator;
  typedef Tokens::const_reference const_reference;
  typedef Tokens::reference reference;

  StringList();

  ~StringList() { TokenList.clear(); }

  const_iterator begin() const { return TokenList.begin(); }
  iterator begin() { return TokenList.begin(); }
  const_iterator end() const { return TokenList.end(); }
  iterator end() { return TokenList.end(); }
  const_iterator find(StrToken *Token) const;
  iterator find(StrToken *Token);

  const_reference front() const { return TokenList.front(); }
  reference front() { return TokenList.front(); }
  const_reference back() const { return TokenList.back(); }
  reference back() { return TokenList.back(); }

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
