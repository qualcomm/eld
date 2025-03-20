//===- OutputCmd.h---------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_SCRIPT_OUTPUTCMD_H
#define ELD_SCRIPT_OUTPUTCMD_H

#include "eld/Script/ScriptCommand.h"
#include <string>

namespace eld {

class Module;

/** \class OutputCmd
 *  \brief This class defines the interfaces to Output command.
 */

class OutputCmd : public ScriptCommand {
public:
  OutputCmd(const std::string &POutputFile);

  ~OutputCmd();

  void dump(llvm::raw_ostream &Outs) const override;

  static bool classof(const ScriptCommand *LinkerScriptCommand) {
    return LinkerScriptCommand->getKind() == ScriptCommand::OUTPUT;
  }

  eld::Expected<void> activate(Module &CurModule) override;

private:
  std::string OutputFileName;
};

} // namespace eld

#endif
