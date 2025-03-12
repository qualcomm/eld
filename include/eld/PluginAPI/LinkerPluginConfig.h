//===- LinkerPluginConfig.h------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_PLUGINAPI_LINKERPLUGINCONFIG_H
#define ELD_PLUGINAPI_LINKERPLUGINCONFIG_H

#include "PluginADT.h"
#include <string>
#include <vector>

namespace eld::plugin {

class DLL_A_EXPORT Plugin;
struct DLL_A_EXPORT Use;

/// LinkerPluginConfig allows to inspect and modify relocations.
/// Each LinkerPluginConfig object has a corresponding plugin::Plugin object.
/// LinkerPluginConfig provides callback hooks that are called for each
/// registered relocation type. Relocation types must be registered via
/// LinkerWrapper::RegisterReloc.
class DLL_A_EXPORT LinkerPluginConfig {
public:
  explicit LinkerPluginConfig(plugin::Plugin *);

  /// The Init callback hook is called before any relocation callback hook
  /// call. It is used for initialization purposes. Typically, plugins
  /// register relocation types in the init callback function.
  virtual void Init() = 0;

  /// RelocCallBack is the callback hook function that the linker calls
  /// for each registered relocation.
  ///
  /// This function must be thread-safe as the linker may handle
  /// relocations parallelly and thus may make calls to this function
  /// parallelly as well.
  ///
  /// \note There can be at most one registered relocation handler for each
  /// relocation type per plugin.
  virtual void RelocCallBack(plugin::Use U) = 0;

  /// Return the corresponding plugin.
  plugin::Plugin *getPlugin() const { return Plugin; }

  virtual ~LinkerPluginConfig() {}

protected:
  /// Pointer to the corresponding plugin object.
  /// \warning Please do not modify the plugin object directly.
  plugin::Plugin *Plugin;
};

} // namespace eld::plugin
#endif
