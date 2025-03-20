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

  size_t size() const { return MemoryDescriptors.size(); }

  bool empty() const { return MemoryDescriptors.empty(); }

  void dump(llvm::raw_ostream &Outs) const override;

  static bool classof(const ScriptCommand *LinkerScriptCommand) {
    return LinkerScriptCommand->getKind() == ScriptCommand::MEMORY;
  }

  void dumpOnlyThis(llvm::raw_ostream &Outs) const override;

  eld::Expected<void> activate(Module &CurModule) override;

  void pushBack(ScriptCommand *Cmd) override;

  const std::vector<MemoryDesc *> &getMemoryDescriptors() const {
    return MemoryDescriptors;
  }

private:
  std::vector<MemoryDesc *> MemoryDescriptors;
};

} // namespace eld

#endif
