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

  ~StringList() { m_Tokens.clear(); }

  const_iterator begin() const { return m_Tokens.begin(); }
  iterator begin() { return m_Tokens.begin(); }
  const_iterator end() const { return m_Tokens.end(); }
  iterator end() { return m_Tokens.end(); }
  const_iterator find(StrToken *token) const;
  iterator find(StrToken *token);

  const_reference front() const { return m_Tokens.front(); }
  reference front() { return m_Tokens.front(); }
  const_reference back() const { return m_Tokens.back(); }
  reference back() { return m_Tokens.back(); }

  bool empty() const { return m_Tokens.empty(); }

  size_t size() const { return m_Tokens.size(); }

  void push_back(StrToken *pToken);

  void dump(llvm::raw_ostream &outs) const;

  const Tokens &getTokens() const { return m_Tokens; }

private:
  Tokens m_Tokens;
};

} // namespace eld

#endif
