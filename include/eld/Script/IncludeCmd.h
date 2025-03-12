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
  IncludeCmd(const std::string FileName, bool isOptional);

  ~IncludeCmd() {};

  void dump(llvm::raw_ostream &outs) const override;

  void dumpOnlyThis(llvm::raw_ostream &outs) const override;

  static bool classof(const ScriptCommand *pCmd) {
    return pCmd->getKind() == ScriptCommand::INCLUDE;
  }

  eld::Expected<void> activate(Module &pModule) override;

  bool isOptional() const { return m_isOptional; }

  std::string getFileName() const { return FileName; }

private:
  const std::string FileName;
  bool m_isOptional;
};

} // namespace eld

#endif
