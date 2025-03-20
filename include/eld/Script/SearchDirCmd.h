//===- SearchDirCmd.h------------------------------------------------------===//
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

//===- SearchDirCmd.h -----------------------------------------------------===//
//===----------------------------------------------------------------------===//
#ifndef ELD_SCRIPT_SEARCHDIRCMD_H
#define ELD_SCRIPT_SEARCHDIRCMD_H

#include "eld/Script/ScriptCommand.h"
#include <string>

namespace eld {

class Module;

/** \class SearchDirCmd
 *  \brief This class defines the interfaces to SEARCH_DIR command.
 */

class SearchDirCmd : public ScriptCommand {
public:
  SearchDirCmd(const std::string &PPath);
  ~SearchDirCmd();

  void dump(llvm::raw_ostream &Outs) const override;

  eld::Expected<void> activate(Module &CurModule) override;

  static bool classof(const ScriptCommand *LinkerScriptCommand) {
    return LinkerScriptCommand->getKind() == ScriptCommand::SEARCH_DIR;
  }

private:
  std::string MPath;
};

} // namespace eld

#endif
