//===- PluginData.h--------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
#ifndef ELD_PLUGIN_PLUGINDATA_H
#define ELD_PLUGIN_PLUGINDATA_H

#include "llvm/Support/DataTypes.h"
#include <string>

namespace eld {

class PluginData {
public:
  explicit PluginData(uint32_t Key, void *Data, std::string Annotation);

  uint32_t getKey() const { return Key; }

  std::string getAnnotation() const { return Annotation; }

  void *getData() const { return Data; }

  virtual ~PluginData() {}

protected:
  uint32_t Key;
  void *Data;
  std::string Annotation;
};

} // namespace eld

#endif
