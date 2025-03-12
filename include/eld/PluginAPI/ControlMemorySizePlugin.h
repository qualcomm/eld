//===- ControlMemorySizePlugin.h-------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_PLUGINAPI_CONTROLMEMORYSIZEPLUGIN_H
#define ELD_PLUGINAPI_CONTROLMEMORYSIZEPLUGIN_H

#include "PluginBase.h"

namespace eld::plugin {

class DLL_A_EXPORT ControlMemorySizePlugin : public Plugin {
public:
  /* Constructor */
  ControlMemorySizePlugin(std::string Name)
      : Plugin(Plugin::ControlMemorySize, Name) {}

  static bool classof(const PluginBase *P) {
    return P->getType() == plugin::Plugin::Type::ControlMemorySize;
  }

  /* Memory blocks that the linker will call the client with */
  virtual void AddBlocks(Block memBlock) = 0;

  /* Return memory blocks to the client */
  virtual std::vector<Block> GetBlocks() = 0;
};
} // namespace eld::plugin
#endif
