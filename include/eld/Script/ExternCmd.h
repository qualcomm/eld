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
  ExternCmd(StringList &pExtern);
  ~ExternCmd();

  void dump(llvm::raw_ostream &outs) const override;

  static bool classof(const ScriptCommand *pCmd) {
    return pCmd->getKind() == ScriptCommand::EXTERN;
  }

  void addExternCommand(StrToken *S) { m_Extern.push_back(S); }

  const StringList &getExternList() const { return m_Extern; }

  eld::Expected<void> activate(Module &pModule) override;

  void addSymbolContainer(SymbolContainer *symContainer) {
    assert(symContainer);
    m_SymbolContainers.push_back(symContainer);
  }

  llvm::ArrayRef<SymbolContainer *> getSymbolContainers() const {
    return m_SymbolContainers;
  }

private:
  StringList m_Extern;
  llvm::SmallVector<SymbolContainer *> m_SymbolContainers;
};

} // namespace eld

#endif
