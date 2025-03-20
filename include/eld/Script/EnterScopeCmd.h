//===- EnterScopeCmd.h-----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_SCRIPT_ENTERSCOPECMD_H
#define ELD_SCRIPT_ENTERSCOPECMD_H

#include "eld/PluginAPI/PluginBase.h"
#include "eld/Script/ScriptCommand.h"

namespace eld {

class Module;

/** \class EnterScopeCmd
 *  \brief This opens a scope
 *  by the user.
 */

class EnterScopeCmd : public ScriptCommand {
public:
  EnterScopeCmd();

  eld::Expected<void> activate(Module &CurModule) override;

  static bool classof(const ScriptCommand *LinkerScriptCommand) {
    return LinkerScriptCommand->getKind() == ScriptCommand::ENTER_SCOPE;
  }

  void dump(llvm::raw_ostream &Outs) const override;

  void dumpOnlyThis(llvm::raw_ostream &Outs) const override;
};

} // namespace eld

#endif
