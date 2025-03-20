//===- IncludeCmd.h--------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_SCRIPT_INCLUDECMD_H
#define ELD_SCRIPT_INCLUDECMD_H

#include "eld/Script/ScriptCommand.h"

namespace eld {
/** \class IncludeCmd
 *  \brief This class defines the interfaces to INCLUDE command.
 */

class IncludeCmd : public ScriptCommand {
public:
  IncludeCmd(const std::string FileName, bool IsOptional);

  ~IncludeCmd() {};

  void dump(llvm::raw_ostream &Outs) const override;

  void dumpOnlyThis(llvm::raw_ostream &Outs) const override;

  static bool classof(const ScriptCommand *LinkerScriptCommand) {
    return LinkerScriptCommand->getKind() == ScriptCommand::INCLUDE;
  }

  eld::Expected<void> activate(Module &CurModule) override;

  bool isOptional() const { return IsOptional; }

  std::string getFileName() const { return FileName; }

private:
  const std::string FileName;
  bool IsOptional;
};

} // namespace eld

#endif
