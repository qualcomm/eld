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

eld::Expected<void> PluginCmd::activate(Module &pModule) {
  auto PrintTimingStats =
      pModule.getConfig().options().printTimingStats("Plugin") ||
      pModule.getConfig().options().printTimingStats(Name.c_str()) ||
      pModule.getConfig().options().allUserPluginStatsRequested();
  Plugin = pModule.getScript().addPlugin(T, Name, R, Options, PrintTimingStats,
                                         pModule);
  return eld::Expected<void>();
}

void PluginCmd::dump(llvm::raw_ostream &outs) const {
  if (!Options.empty()) {
    outs << getPluginType() << "(\"" << Name << "\", "
         << "\"" << R << "\", "
         << "\"" << Options << "\");";
    outs << "\n";
    return;
  }
  outs << getPluginType() << "(\"" << Name << "\", "
       << "\"" << R << "\");";
  outs << "\n";
}

void PluginCmd::dumpPluginInfo(llvm::raw_ostream &outs) const {
  if (!Options.empty()) {
    outs << getPluginType() << "(\"" << Name << "\", "
         << "\"" << R << "\", "
         << "\"" << Options << "\")";
    return;
  }
  outs << getPluginType() << "(\"" << Name << "\", "
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
