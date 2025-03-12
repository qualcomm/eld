//===- LinkerPlugin.cpp----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/PluginAPI/PluginBase.h"
#include "eld/Script/Plugin.h"

using namespace eld::plugin;

uint32_t PluginBase::getPluginID() const {
  return Linker->getPlugin()->getID();
}
