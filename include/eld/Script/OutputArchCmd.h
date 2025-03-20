//===- OutputArchCmd.h-----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_SCRIPT_OUTPUTARCHCMD_H
#define ELD_SCRIPT_OUTPUTARCHCMD_H

#include "eld/Script/ScriptCommand.h"
#include <string>

namespace eld {

class Module;

/** \class OutputArchCmd
 *  \brief This class defines the interfaces to OutputArch command.
 */

class OutputArchCmd : public ScriptCommand {
public:
  OutputArchCmd(const std::string &PArch);
  ~OutputArchCmd();

  void dump(llvm::raw_ostream &Outs) const override;

  static bool classof(const ScriptCommand *LinkerScriptCommand) {
    return LinkerScriptCommand->getKind() == ScriptCommand::OUTPUT_ARCH;
  }

  eld::Expected<void> activate(Module &CurModule) override;

private:
  std::string OutputArch;
};

} // namespace eld

#endif
