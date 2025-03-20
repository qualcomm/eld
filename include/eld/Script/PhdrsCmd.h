//===- PhdrsCmd.h----------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_SCRIPT_PHDRSCMD_H
#define ELD_SCRIPT_PHDRSCMD_H

#include "eld/Script/ScriptCommand.h"
#include "llvm/Support/DataTypes.h"
#include <vector>

namespace eld {

class Module;

class PhdrDesc;

/** \class PhdrsCmd
 *  \brief This class defines the interfaces to PHDRS command.
 */

class PhdrsCmd : public ScriptCommand {
public:
  PhdrsCmd();
  ~PhdrsCmd();

  size_t size() const { return MPhdrs.size(); }

  bool empty() const { return MPhdrs.empty(); }

  void dump(llvm::raw_ostream &Outs) const override;

  static bool classof(const ScriptCommand *LinkerScriptCommand) {
    return LinkerScriptCommand->getKind() == ScriptCommand::PHDRS;
  }

  void dumpOnlyThis(llvm::raw_ostream &Outs) const override;

  eld::Expected<void> activate(Module &CurModule) override;

  void pushBack(ScriptCommand *Cmd) override;

  const std::vector<ScriptCommand *> &getPhdrDescriptors() const {
    return MPhdrs;
  }

private:
  std::vector<ScriptCommand *> MPhdrs;
};

} // namespace eld

#endif
