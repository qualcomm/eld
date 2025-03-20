//===- NameSpec.h----------------------------------------------------------===//
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

#ifndef ELD_SCRIPT_NAMESPEC_H
#define ELD_SCRIPT_NAMESPEC_H

#include "eld/Config/Config.h"
#include "eld/Script/InputToken.h"

namespace eld {

/** \class NameSpec
 *  \brief This class defines the interfaces to a namespec in INPUT/GROUP
 *         command.
 */

class NameSpec : public InputToken {
public:
  explicit NameSpec(const std::string &PName, bool PAsNeeded);

  ~NameSpec() {}

  static bool classof(const InputToken *ThisInputToken) {
    return ThisInputToken->type() == InputToken::NameSpec;
  }
};

} // namespace eld

#endif
