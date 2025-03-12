//===- PluginVersion.h-----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_PLUGINAPI_PLUGIN_VERSION_H
#define ELD_PLUGINAPI_PLUGIN_VERSION_H

#include "PluginBase.h"

extern "C" void DLL_A_EXPORT getPluginAPIVersion(unsigned int *Major,
                                                 unsigned int *Minor);

void getPluginAPIVersion(unsigned int *Major, unsigned int *Minor) {
  *Major = LINKER_PLUGIN_API_MAJOR_VERSION;
  *Minor = LINKER_PLUGIN_API_MINOR_VERSION;
}

#endif
