//===- InputToken.cpp------------------------------------------------------===//
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

#include "eld/Script/InputToken.h"

using namespace eld;

//===----------------------------------------------------------------------===//
// InputToken
//===----------------------------------------------------------------------===//
InputToken::InputToken(Type AssignmentType, const std::string &PName,
                       bool PAsNeeded)
    : StrToken(PName, StrToken::Input), ThisType(AssignmentType),
      LinkerScriptHasAsNeeded(PAsNeeded) {}
