//===- ExternCmd.h---------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_SCRIPT_EXTERNCMD_H
#define ELD_SCRIPT_EXTERNCMD_H

#include "eld/Script/ScriptCommand.h"
#include "eld/Script/StrToken.h"
#include "eld/Script/StringList.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include <string>

namespace eld {

class SymbolContainer;
class Module;

/** \class ExternCmd
 *  \brief This class defines the interfaces to Extern command.
 */

class ExternCmd : public ScriptCommand {
public:
  ExternCmd(StringList &PExtern);
  ~ExternCmd();

  void dump(llvm::raw_ostream &Outs) const override;

  static bool classof(const ScriptCommand *LinkerScriptCommand) {
    return LinkerScriptCommand->getKind() == ScriptCommand::EXTERN;
  }

  void addExternCommand(StrToken *S) { ExternSymbolList.pushBack(S); }

  const StringList &getExternList() const { return ExternSymbolList; }

  eld::Expected<void> activate(Module &CurModule) override;

  void addSymbolContainer(SymbolContainer *SymContainer) {
    assert(SymContainer);
    ThisSymbolContainers.push_back(SymContainer);
  }

  llvm::ArrayRef<SymbolContainer *> getSymbolContainers() const {
    return ThisSymbolContainers;
  }

private:
  StringList ExternSymbolList;
  llvm::SmallVector<SymbolContainer *> ThisSymbolContainers;
};

} // namespace eld

#endif
