//===- GroupCmd.h----------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_SCRIPT_GROUPCMD_H
#define ELD_SCRIPT_GROUPCMD_H

#include "eld/Input/InputBuilder.h"
#include "eld/Script/ScriptCommand.h"
#include "eld/Script/ScriptFile.h"

namespace eld {

class StringList;
class InputTree;
class InputBuilder;
class LinkerConfig;

/** \class GroupCmd
 *  \brief This class defines the interfaces to Group command.
 */

class GroupCmd : public ScriptCommand {
public:
  GroupCmd(const LinkerConfig &Config, StringList &PStringList,
           const Attribute &Attr, ScriptFile &PScriptFile);
  ~GroupCmd();

  void dump(llvm::raw_ostream &Outs) const override;

  static bool classof(const ScriptCommand *LinkerScriptCommand) {
    return LinkerScriptCommand->getKind() == ScriptCommand::GROUP;
  }

  eld::Expected<void> activate(Module &CurModule) override;

private:
  StringList &ThisStringList;
  InputBuilder ThisBuilder;
  ScriptFile &ThisScriptFile;
};

} // namespace eld

#endif
