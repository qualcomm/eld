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

  size_t size() const { return m_Phdrs.size(); }

  bool empty() const { return m_Phdrs.empty(); }

  void dump(llvm::raw_ostream &outs) const override;

  static bool classof(const ScriptCommand *pCmd) {
    return pCmd->getKind() == ScriptCommand::PHDRS;
  }

  void dumpOnlyThis(llvm::raw_ostream &outs) const override;

  eld::Expected<void> activate(Module &pModule) override;

  void push_back(ScriptCommand *Cmd) override;

  const std::vector<ScriptCommand *> &getPhdrDescriptors() const {
    return m_Phdrs;
  }

private:
  std::vector<ScriptCommand *> m_Phdrs;
};

} // namespace eld

#endif
