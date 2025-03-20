//===- StrToken.cpp--------------------------------------------------------===//
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
#include "eld/Script/StrToken.h"

using namespace eld;

//===----------------------------------------------------------------------===//
// StrToken
//===----------------------------------------------------------------------===//
StrToken::StrToken(const std::string &PString, StrToken::Kind K)
    : Name(PString), Quoted(false), ScriptFileKind(K) {}

std::string StrToken::getDecoratedName() const {
  std::string R = "";
  if (isQuoted())
    R = "\"";
  R += Name;
  if (isQuoted())
    R += "\"";
  return R;
}
