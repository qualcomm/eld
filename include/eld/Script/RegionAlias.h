//===- RegionAlias.h-------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_SCRIPT_REGION_ALIAS_H
#define ELD_SCRIPT_REGION_ALIAS_H

#include "eld/Script/ScriptCommand.h"
#include "eld/Script/StrToken.h"

#include <string>

namespace eld {

class Module;
class StrToken;

/** \class RegionAlias

    Alias names can be added to existing memory regions created with the MEMORY
    command. Each name corresponds to at most one memory region.

    REGION_ALIAS(alias, region)

    The REGION_ALIAS function creates an alias name alias for the memory region
    region. This allows a flexible mapping of output sections to memory regions.
 */

class RegionAlias : public ScriptCommand {
public:
  explicit RegionAlias(const StrToken *alias, const StrToken *region);

  ~RegionAlias();

  void dump(llvm::raw_ostream &outs) const override;

  static bool classof(const ScriptCommand *pCmd) {
    return pCmd->getKind() == ScriptCommand::REGION_ALIAS;
  }

  eld::Expected<void> activate(Module &pModule) override;

private:
  const StrToken *m_Alias = nullptr;
  const StrToken *m_Region = nullptr;
};

} // namespace eld

#endif
