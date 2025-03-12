#include "Defines.h"
#include "LinkerPlugin.h"
#include "LinkerScript.h"
#include "LinkerWrapper.h"
#include "PluginVersion.h"
#include <iostream>

using namespace eld::plugin;

class DLL_A_EXPORT BasicLinkerScriptGenerator : public LinkerPlugin {
public:
  BasicLinkerScriptGenerator() : LinkerPlugin("BasicLinkerScriptGenerator") {}

  void ActBeforeSectionMerging() override {
    LinkerScript LS = getLinker()->getLinkerScript();

    for (const Script::ScriptCommand *cmd : LS.getCommands())
      printCommand(*cmd);
  }

private:
  void printCommand(const Script::ScriptCommand &cmd) {
    if (isInternal(cmd))
      return;

    std::cout << cmd;
    for (const Script::ScriptCommand *subCmd : cmd.getCommands())
      printCommand(*subCmd);
  }

  bool isInternal(const Script::ScriptCommand &cmd) {
    if (cmd.isInputSectionSpec()) {
      Script::InputSectionSpec spec = cmd.getInputSectionSpec();
      if (spec.isInternal())
        return true;
    }
    return false;
  }
};

std::unordered_map<std::string, PluginBase *> Plugins;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  Plugins["BasicLinkerScriptGenerator"] = new BasicLinkerScriptGenerator();
  return true;
}

PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return Plugins[T]; }
void DLL_A_EXPORT Cleanup() {
  for (auto &item : Plugins) {
    if (item.second)
      delete item.second;
  }
  Plugins.clear();
}
}