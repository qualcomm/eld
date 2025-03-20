//===- PluginData.cpp------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Plugin/PluginData.h"

using namespace eld;

PluginData::PluginData(uint32_t Key, void *Data, std::string Annotation)
    : Key(Key), Data(Data), Annotation(Annotation) {}
