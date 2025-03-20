//===- InputToken.h--------------------------------------------------------===//
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
#ifndef ELD_SCRIPT_INPUTTOKEN_H
#define ELD_SCRIPT_INPUTTOKEN_H

#include "eld/Script/StrToken.h"

namespace eld {

/** \class InputToken
 *  \brief This class defines the interfaces to a file/namespec token.
 */

class InputToken : public StrToken {
public:
  enum Type { Unknown, File, NameSpec };

protected:
  InputToken();
  InputToken(Type AssignmentType, const std::string &PName, bool PAsNeeded);

public:
  Type type() const { return ThisType; }

  bool asNeeded() const { return LinkerScriptHasAsNeeded; }

  static bool classof(const StrToken *ThisInputToken) {
    return ThisInputToken->kind() == StrToken::Input;
  }

  virtual ~InputToken() {}

private:
  Type ThisType;
  bool LinkerScriptHasAsNeeded;
};

} // namespace eld

#endif
