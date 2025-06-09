//===- HelloWorldPlugin.h--------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "Defines.h"
#include "PluginBase.h"
#include "PluginVersion.h"
#include "LinkerPlugin.h"
#include <iostream>

class DLL_A_EXPORT HelloWorldPlugin : public eld::plugin::LinkerPlugin {
public:
  HelloWorldPlugin() : eld::plugin::LinkerPlugin("HelloWorldPlugin") {}

  void Init(const std::string &options) override {}

  void VisitSections(eld::plugin::InputFile IF) override {}

  void ActBeforeRuleMatching() override {}

  void ActBeforeSectionMerging() override {}

  void ActBeforePerformingLayout() override {}

  void ActBeforeWritingOutput() override {}

  void Destroy() override {}
};

ELD_REGISTER_PLUGIN(HelloWorldPlugin)
