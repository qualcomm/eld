//===- MemoryCmd.h---------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_SCRIPT_MEMORYCMD_H
#define ELD_SCRIPT_MEMORYCMD_H

#include "eld/Script/MemoryDesc.h"
#include "eld/Script/ScriptCommand.h"
#include "llvm/Support/DataTypes.h"
#include <vector>

namespace eld {

class Module;
class MemoryDesc;
struct MemorySpec;

/** \class MemoryCmd
 *  \brief This class defines the interfaces to MEMORY command.
 */

class MemoryCmd : public ScriptCommand {
public:
  MemoryCmd();
  ~MemoryCmd();

  size_t size() const { return m_MemoryDescriptors.size(); }

  bool empty() const { return m_MemoryDescriptors.empty(); }

  void dump(llvm::raw_ostream &outs) const override;

  static bool classof(const ScriptCommand *pCmd) {
    return pCmd->getKind() == ScriptCommand::MEMORY;
  }

  void dumpOnlyThis(llvm::raw_ostream &outs) const override;

  eld::Expected<void> activate(Module &pModule) override;

  void push_back(ScriptCommand *Cmd) override;

  const std::vector<MemoryDesc *> &getMemoryDescriptors() const {
    return m_MemoryDescriptors;
  }

private:
  std::vector<MemoryDesc *> m_MemoryDescriptors;
};

} // namespace eld

#endif
