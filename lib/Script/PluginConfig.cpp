//===- PluginConfig.cpp----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#include "eld/PluginAPI/PluginConfig.h"
#include "eld/Config/LinkerConfig.h"

using namespace llvm::yaml;
using namespace eld;
using namespace eld::LinkerPlugin;

namespace llvm {
namespace yaml {
template <> struct ScalarEnumerationTraits<plugin::Plugin::Type> {
  static void enumeration(IO &Io, plugin::Plugin::Type &T) {
    Io.enumCase(T, "SectionMatcher", plugin::Plugin::Type::SectionMatcher);
    Io.enumCase(T, "SectionIterator", plugin::Plugin::Type::SectionIterator);
    Io.enumCase(T, "OutputSectionIterator",
                plugin::Plugin::Type::OutputSectionIterator);
    Io.enumCase(T, "", plugin::Plugin::Type::OutputSectionIterator);
    Io.enumCase(T, "ControlFileSize", plugin::Plugin::Type::ControlFileSize);
    Io.enumCase(T, "ControlMemorySize",
                plugin::Plugin::Type::ControlMemorySize);
    Io.enumCase(T, "LinkerPlugin", plugin::Plugin::Type::LinkerPlugin);
  }
};

void MappingTraits<Config>::mapping(IO &IO, Config &C) {
  IO.mapOptional("GlobalPlugins", C.GlobalPlugins);
  IO.mapOptional("OutputSectionPlugins", C.OutputSectionPlugins);
}

void MappingTraits<GlobalPlugin>::mapping(IO &IO, GlobalPlugin &G) {
  IO.mapRequired("Type", G.PluginType);
  IO.mapRequired("Name", G.PluginName);
  IO.mapRequired("Library", G.LibraryName);
  IO.mapOptional("Options", G.Options);
}

void MappingTraits<OutputSectionPlugin>::mapping(IO &IO,
                                                 OutputSectionPlugin &S) {
  IO.mapRequired("OutputSection", S.OutputSection);
  IO.mapRequired("Type", S.PluginType);
  IO.mapRequired("Name", S.PluginName);
  IO.mapRequired("Library", S.LibraryName);
  IO.mapOptional("Options", S.Options);
}

} // namespace yaml
} // namespace llvm
