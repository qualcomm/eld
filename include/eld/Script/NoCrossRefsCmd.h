//===- NoCrossRefsCmd.h----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_SCRIPT_NOCROSSREFSCMD_H
#define ELD_SCRIPT_NOCROSSREFSCMD_H

#include "eld/Script/ScriptCommand.h"
#include "eld/Script/StringList.h"
#include <string>

namespace eld {

class Module;

/** \class NoCrossRefsCmd
 *  \brief This class defines the interfaces to NOCROSSREFS command.
 */

class NoCrossRefsCmd : public ScriptCommand {
public:
  NoCrossRefsCmd(StringList &pExtern, size_t pID);
  ~NoCrossRefsCmd();

  void dump(llvm::raw_ostream &outs) const override;

  static bool classof(const ScriptCommand *pCmd) {
    return pCmd->getKind() == ScriptCommand::NOCROSSREFS;
  }

  eld::Expected<void> activate(Module &pModule) override;

private:
  StringList m_Sections;
  size_t m_ID;
};

} // namespace eld

#endif
