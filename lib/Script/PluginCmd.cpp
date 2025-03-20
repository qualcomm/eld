//===- PluginCmd.cpp-------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Script/PluginCmd.h"
#include "eld/Core/Module.h"

using namespace eld;

PluginCmd::PluginCmd(plugin::Plugin::Type T, const std::string &Name,
                     const std::string &R, const std::string &O)
    : ScriptCommand(ScriptCommand::PLUGIN), T(T), Name(Name), R(R), Options(O) {
}

eld::Expected<void> PluginCmd::activate(Module &CurModule) {
  auto PrintTimingStats =
      CurModule.getConfig().options().printTimingStats("Plugin") ||
      CurModule.getConfig().options().printTimingStats(Name.c_str()) ||
      CurModule.getConfig().options().allUserPluginStatsRequested();
  Plugin = CurModule.getScript().addPlugin(T, Name, R, Options,
                                           PrintTimingStats, CurModule);
  return eld::Expected<void>();
}

void PluginCmd::dump(llvm::raw_ostream &Outs) const {
  if (!Options.empty()) {
    Outs << getPluginType() << "(\"" << Name << "\", "
         << "\"" << R << "\", "
         << "\"" << Options << "\");";
    Outs << "\n";
    return;
  }
  Outs << getPluginType() << "(\"" << Name << "\", "
       << "\"" << R << "\");";
  Outs << "\n";
}

void PluginCmd::dumpPluginInfo(llvm::raw_ostream &Outs) const {
  if (!Options.empty()) {
    Outs << getPluginType() << "(\"" << Name << "\", "
         << "\"" << R << "\", "
         << "\"" << Options << "\")";
    return;
  }
  Outs << getPluginType() << "(\"" << Name << "\", "
       << "\"" << R << "\")";
}

std::string PluginCmd::getPluginType() const {
  switch (T) {
  case plugin::PluginBase::Type::ControlFileSize:
    return "PLUGIN_CONTROL_FILESZ";
  case plugin::PluginBase::Type::ControlMemorySize:
    return "PLUGIN_CONTROL_MEMSZ";
  case plugin::PluginBase::Type::SectionIterator:
    return "PLUGIN_ITER_SECTIONS";
  case plugin::PluginBase::Type::SectionMatcher:
    return "PLUGIN_SECTION_MATCHER";
  case plugin::PluginBase::Type::OutputSectionIterator:
    return "PLUGIN_OUTPUT_SECTION_ITER";
  case plugin::PluginBase::Type::LinkerPlugin:
    return "LINKER_PLUGIN";
  }
}
