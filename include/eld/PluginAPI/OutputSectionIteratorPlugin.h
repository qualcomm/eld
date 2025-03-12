//===- OutputSectionIteratorPlugin.h---------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_PLUGINAPI_OUTPUTSECTIONITERATORPLUGIN_H
#define ELD_PLUGINAPI_OUTPUTSECTIONITERATORPLUGIN_H

#include "PluginBase.h"

namespace eld::plugin {

class DLL_A_EXPORT OutputSectionIteratorPlugin : public Plugin {
public:
  /* Constructor */
  OutputSectionIteratorPlugin(std::string Name)
      : Plugin(Plugin::Type::OutputSectionIterator, Name) {}

  static bool classof(const PluginBase *P) {
    return P->getType() == plugin::Plugin::Type::OutputSectionIterator;
  }

  virtual void processOutputSection(OutputSection O) = 0;
};
} // namespace eld::plugin
#endif
