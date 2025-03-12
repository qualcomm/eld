#include "ELD/PluginAPI/PluginVersion.h"
#include "ELD/PluginAPI/SectionIteratorPlugin.h"
#include <iostream>

class ChangeOutputSection : public eld::plugin::SectionIteratorPlugin {
public:
  ChangeOutputSection() : eld::plugin::SectionIteratorPlugin("ChangeOutputSection") {}

  // 'Init' callback hook can be used for initialization and preparations.
  // This plugin does not need any initialization or preparation.
  void Init(std::string cfg) override {}

  // 'processSection' callback hook of SectionIteratorPlugin is called for
  // each non-garbage collected section.
  void processSection(eld::plugin::Section S) override {
    if (S.matchPattern("*foo")) {
      // Changes the output section of the section S to
      // bar. LinkerWrapper::setOutputSection must only be
      // called in BeforeLayout link state. Section overrides created
      // after BeforeLayout link state do not work and can result in
      // undefined behavior.
      //
      // Annotation is useful for diagnostic purposes. Later, we will see where
      // to find these annotations.
      Linker->setOutputSection(
          S, "bar",
          /*Annotation=*/"Setting output section of '.text.foo' to 'bar'");
    }
  }

  // 'Run' callback hook is called after all the 'processSection' callback hook
  // calls. It is called once for each section iterator plugin run.
  eld::plugin::Plugin::Status Run(bool trace) override {
    return eld::plugin::Plugin::Status::SUCCESS;
  }

  // 'Destroy' callback hook can be used for finalization and clean-up tasks.
  // It is called once for each section iterator plugin run.
  void Destroy() override {
    // LinkerWrapper::finishAssignOutputSections must be called
    // after all section overrides have been created by the plugin.
    // It brings the created section overrides into effect.
    Linker->finishAssignOutputSections();
  }

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "Success"; }

  std::string GetName() override { return "ChangeOutputSection"; }
};

eld::plugin::Plugin *ThisPlugin = nullptr;

extern "C" {
// RegisterAll should initialize all the plugins that a plugin library aims
// to provide. Linker calls this function before running any plugins provided
// by the library.
bool RegisterAll() {
  ThisPlugin = new ChangeOutputSection{};
  return true;
}

// Linker calls this function to request an instance of the plugin
// with the plugin name pluginName. pluginName is provided in the plugin
// invocation command.
eld::plugin::Plugin *getPlugin(const char *pluginName) { return ThisPlugin; }

// Cleanup should free all the resources owned by a plugin library.
// Linker calls this function after all runs of the plugins provided
// by the library have completed.
void Cleanup() {
  if (!ThisPlugin)
    return;
  delete ThisPlugin;
  ThisPlugin = nullptr;
}
}