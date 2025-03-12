//===- FileToken.h---------------------------------------------------------===//
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

#ifndef ELD_SCRIPT_FILETOKEN_H
#define ELD_SCRIPT_FILETOKEN_H

#include "eld/Config/Config.h"
#include "eld/Script/InputToken.h"

namespace eld {

/** \class FileToken
 *  \brief This class defines the interfaces to a filename in INPUT/GROUP
 *         command.
 */

class FileToken : public InputToken {
public:
  explicit FileToken(const std::string &pName, bool pAsNeeded);

  static bool classof(const InputToken *pToken) {
    return pToken->type() == InputToken::File;
  }

  virtual ~FileToken() {}
};

} // namespace eld

#endif
