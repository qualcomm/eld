//===- StringList.cpp------------------------------------------------------===//
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
#include "eld/Script/StringList.h"
#include "eld/Script/StrToken.h"
#include "llvm/Support/raw_ostream.h"

using namespace eld;

//===----------------------------------------------------------------------===//
// StringList
//===----------------------------------------------------------------------===//
StringList::StringList() {}

void StringList::push_back(StrToken *pToken) { m_Tokens.push_back(pToken); }

void StringList::dump(llvm::raw_ostream &outs) const {
  for (const auto &elem : *this)
    outs << (elem)->name() << "\t";
  outs << "\n";
}

StringList::iterator StringList::find(StrToken *token) {
  iterator it, ite = end();
  for (it = begin(); it != ite; it++) {
    if ((*it)->name() == token->name())
      return it;
  }
  return end();
}

StringList::const_iterator StringList::find(StrToken *token) const {
  const_iterator it, ite = end();
  for (it = begin(); it != ite; it++) {
    if ((*it)->name() == token->name())
      return it;
  }
  return end();
}
