//===- SectionMatcherPlugin.h----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_PLUGINAPI_SECTIONMATCHERPLUGIN_H
#define ELD_PLUGINAPI_SECTIONMATCHERPLUGIN_H

#include "PluginBase.h"

namespace eld::plugin {

class DLL_A_EXPORT SectionMatcherPlugin : public Plugin {
public:
  /* Constructor */
  SectionMatcherPlugin(std::string Name)
      : Plugin(Plugin::Type::SectionMatcher, Name) {}

  static bool classof(const PluginBase *P) {
    return P->getType() == plugin::Plugin::Type::SectionMatcher;
  }

  /* Chunks that the linker will call the client with */
  virtual void processSection(Section S) = 0;
};
} // namespace eld::plugin
#endif
