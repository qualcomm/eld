//===- PluginCmd.h---------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_SCRIPT_PLUGINCMD_H
#define ELD_SCRIPT_PLUGINCMD_H

#include "eld/PluginAPI/PluginBase.h"
#include "eld/Script/ScriptCommand.h"
#include <string>

namespace eld {

class Module;
class Plugin;

/** \class PluginCmd
 *  \brief This class defines the interfaces to Linker Plugins being requested
 *  by the user.
 */

class PluginCmd : public ScriptCommand {
public:
  explicit PluginCmd(plugin::Plugin::Type T, const std::string &Name,
                     const std::string &R, const std::string &O);

  eld::Expected<void> activate(Module &pModule) override;

  Plugin *getPlugin() const { return Plugin; }

  static bool classof(const ScriptCommand *pCmd) {
    return pCmd->getKind() == ScriptCommand::PLUGIN;
  }

  void dump(llvm::raw_ostream &outs) const override;

  void dumpPluginInfo(llvm::raw_ostream &outs) const;

  bool hasOutputSection() const { return m_HasOutputSection; }

  void setHasOutputSection() { m_HasOutputSection = true; }

private:
  std::string getPluginType() const;

private:
  plugin::Plugin::Type T;
  std::string Name;
  std::string R;
  std::string Options;
  eld::Plugin *Plugin = nullptr;
  bool m_HasOutputSection = false;
};

} // namespace eld

#endif
