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
  OutputCmd(const std::string &pOutputFile);

  ~OutputCmd();

  void dump(llvm::raw_ostream &outs) const override;

  static bool classof(const ScriptCommand *pCmd) {
    return pCmd->getKind() == ScriptCommand::OUTPUT;
  }

  eld::Expected<void> activate(Module &pModule) override;

private:
  std::string m_OutputFile;
};

} // namespace eld

#endif
