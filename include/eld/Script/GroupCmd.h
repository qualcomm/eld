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
  GroupCmd(const LinkerConfig &config, StringList &pStringList,
           const Attribute &attr, ScriptFile &pScriptFile);
  ~GroupCmd();

  void dump(llvm::raw_ostream &outs) const override;

  static bool classof(const ScriptCommand *pCmd) {
    return pCmd->getKind() == ScriptCommand::GROUP;
  }

  eld::Expected<void> activate(Module &pModule) override;

private:
  StringList &m_StringList;
  InputBuilder m_InputBuilder;
  ScriptFile &m_ScriptFile;
};

} // namespace eld

#endif
