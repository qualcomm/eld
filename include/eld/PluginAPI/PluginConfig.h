//===- PluginConfig.h------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_PLUGINAPI_PLUGINCONFIG_H
#define ELD_PLUGINAPI_PLUGINCONFIG_H

#include "eld/PluginAPI/PluginBase.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ObjectYAML/YAML.h"

namespace eld {
namespace LinkerPlugin {

struct GlobalPlugin {
  plugin::PluginBase::Type PluginType;
  std::string PluginName;
  std::string LibraryName;
  std::string Options;
};

struct OutputSectionPlugin {
  plugin::PluginBase::Type PluginType;
  std::string OutputSection;
  std::string PluginName;
  std::string LibraryName;
  std::string Options;
};

struct Config {
  std::vector<eld::LinkerPlugin::GlobalPlugin> GlobalPlugins;
  std::vector<eld::LinkerPlugin::OutputSectionPlugin> OutputSectionPlugins;
};
} // namespace LinkerPlugin
} // namespace eld

LLVM_YAML_IS_SEQUENCE_VECTOR(eld::LinkerPlugin::Config)
LLVM_YAML_IS_SEQUENCE_VECTOR(eld::LinkerPlugin::GlobalPlugin)
LLVM_YAML_IS_SEQUENCE_VECTOR(eld::LinkerPlugin::OutputSectionPlugin)

namespace llvm {
namespace yaml {
template <> struct MappingTraits<eld::LinkerPlugin::Config> {
  static void mapping(IO &IO, eld::LinkerPlugin::Config &Config);
};
template <> struct MappingTraits<eld::LinkerPlugin::GlobalPlugin> {
  static void mapping(IO &IO, eld::LinkerPlugin::GlobalPlugin &GlobalPlugin);
};
template <> struct MappingTraits<eld::LinkerPlugin::OutputSectionPlugin> {
  static void
  mapping(IO &IO, eld::LinkerPlugin::OutputSectionPlugin &OutputSectionPlugin);
};
} // namespace yaml
} // namespace llvm

#endif
