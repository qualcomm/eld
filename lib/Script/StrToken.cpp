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
StrToken::StrToken(const std::string &pString, StrToken::Kind K)
    : m_Name(pString), Quoted(false), m_Kind(K) {}

std::string StrToken::getDecoratedName() const {
  std::string R = "";
  if (isQuoted())
    R = "\"";
  R += m_Name;
  if (isQuoted())
    R += "\"";
  return R;
}
