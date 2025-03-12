//===- ControlFileSizePlugin.h---------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_PLUGINAPI_CONTROLFILESIZEPLUGIN_H
#define ELD_PLUGINAPI_CONTROLFILESIZEPLUGIN_H

#include "PluginBase.h"

namespace eld::plugin {

class DLL_A_EXPORT ControlFileSizePlugin : public Plugin {
public:
  /* Constructor */
  ControlFileSizePlugin(std::string Name)
      : Plugin(Plugin::ControlFileSize, Name) {}

  static bool classof(const PluginBase *P) {
    return P->getType() == plugin::Plugin::Type::ControlFileSize;
  }

  /* Memory blocks that the linker will call the client with */
  virtual void AddBlocks(Block memBlock) = 0;

  /* Return memory blocks to the client */
  virtual std::vector<Block> GetBlocks() = 0;
};
} // namespace eld::plugin
#endif
