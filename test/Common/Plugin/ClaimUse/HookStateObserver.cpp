//===- HookStateObserver.cpp ----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "LinkerPlugin.h"
#include "LinkerWrapper.h"
#include "PluginVersion.h"
#include <iostream>

using namespace eld::plugin;

class HookStateObserver : public LinkerPlugin {
public:
  HookStateObserver() : LinkerPlugin("HookStateObserver") {}
  void ActBeforeRuleMatching() override { observe("rule matching"); }
  void ActBeforeRelocationScan() override { observe("relocation scan"); }
  void ActBeforeSectionMerging() override { observe("section merging"); }
  void ActBeforePerformingLayout() override { observe("layout"); }
  void ActBeforeWritingOutput() override { observe("output"); }

private:
  void observe(const char *Hook) {
    std::cout << "linker " << Hook << ": "
              << getLinker()->getCurrentLinkStateAsStr() << "\n";
  }
};

ELD_REGISTER_PLUGIN(HookStateObserver)
