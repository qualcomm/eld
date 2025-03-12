//===- LinkerPluginConfig.cpp----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/PluginAPI/LinkerPluginConfig.h"
#include "eld/PluginAPI/PluginBase.h"

using namespace eld;
using namespace eld::plugin;

LinkerPluginConfig::LinkerPluginConfig(plugin::Plugin *Plugin)
    : Plugin(Plugin) {}
