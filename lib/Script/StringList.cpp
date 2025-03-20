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

void StringList::pushBack(StrToken *ThisInputToken) {
  TokenList.push_back(ThisInputToken);
}

void StringList::dump(llvm::raw_ostream &Outs) const {
  for (const auto &Elem : *this)
    Outs << (Elem)->name() << "\t";
  Outs << "\n";
}

StringList::iterator StringList::find(StrToken *Token) {
  iterator It, Ite = end();
  for (It = begin(); It != Ite; It++) {
    if ((*It)->name() == Token->name())
      return It;
  }
  return end();
}

StringList::const_iterator StringList::find(StrToken *Token) const {
  const_iterator It, Ite = end();
  for (It = begin(); It != Ite; It++) {
    if ((*It)->name() == Token->name())
      return It;
  }
  return end();
}
