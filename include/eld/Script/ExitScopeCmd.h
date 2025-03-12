//===- ExitScopeCmd.h------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_SCRIPT_EXITSCOPECMD_H
#define ELD_SCRIPT_EXITSCOPECMD_H

#include "eld/PluginAPI/PluginBase.h"
#include "eld/Script/ScriptCommand.h"

namespace eld {

class Module;

/** \class ExitScopeCmd
 *  \brief This opens a scope
 *  by the user.
 */

class ExitScopeCmd : public ScriptCommand {
public:
  ExitScopeCmd();

  eld::Expected<void> activate(Module &pModule) override;

  static bool classof(const ScriptCommand *pCmd) {
    return pCmd->getKind() == ScriptCommand::EXIT_SCOPE;
  }

  void dump(llvm::raw_ostream &outs) const override;

  void dumpOnlyThis(llvm::raw_ostream &outs) const override;

  uint32_t getDepth() const override;
};

} // namespace eld

#endif
