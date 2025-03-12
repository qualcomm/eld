#include "ELD/PluginAPI/OutputSectionIteratorPlugin.h"
#include "ELD/PluginAPI/PluginVersion.h"

class AddRule : public eld::plugin::OutputSectionIteratorPlugin {
public:
  AddRule() : eld::plugin::OutputSectionIteratorPlugin("AddRule") {}

  // 'Init' callback hook can be used for initialization and preparations.
  // This plugin does not need any initialization or preparation.
  void Init(std::string cfg) override {}

  // 'processOutputSection' callback hook is called once for each output
  // section. In this function, the plugin stores 'var, 'foo' and 'bar' output
  // sections in member variables.
  void processOutputSection(eld::plugin::OutputSection O) override {
    // OutputSectionIterator plugin essentially runs three times.
    // It is run once for each of the following three link states: BeforeLayout,
    // CreatingSections and AfterLayout.
    // We are only interested in one link state, CreatingSections, as chunks can
    // only be moved from one LinkerScriptRule to another in the
    // CreatingSections link state. Thus, we simply return for the other link
    // states. We will do this for each callback hook function.
    if (Linker->getState() != eld::plugin::LinkerWrapper::State::CreatingSections)
      return;
    if (O.getName() == "var") {
      m_Var = O;
    } else if (O.getName() == "foo") {
      m_Foo = O;
    } else if (O.getName() == "bar") {
      m_Bar = O;
    }
  }

  // 'Run' callback hook is called after all the 'processSection' callback hook
  // calls.
  eld::plugin::Plugin::Status Run(bool trace) override {
    if (Linker->getState() != eld::plugin::LinkerWrapper::State::CreatingSections)
      return eld::plugin::Plugin::Status::SUCCESS;

    auto lastRule = m_Var.getLinkerScriptRules().back();
    // Create a new rule for m_Var output section.
    // Annotation is used to name the linker sript rule, and is useful
    // for diagnostic purposes.
    eld::plugin::LinkerScriptRule newRule = Linker->createLinkerScriptRule(
        m_Var, /*Annotation=*/"Move foo and bar chunks to var");
    // Insert the newly created linker script rule in the m_Var output section.
    // We can also insert the newly created rule before some already existing
    // rule using LinkerWrapper::insertBeforeRule API.
    Linker->insertAfterRule(m_Var, lastRule, newRule);
    moveChunks(m_Foo, newRule);
    moveChunks(m_Bar, newRule);

    return eld::plugin::Plugin::Status::SUCCESS;
  }

  // 'Destroy' callback hook can be used for finalization and clean-up tasks.
  // It is called once for each section iterator plugin run.
  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "Success"; }

  std::string GetName() override { return "AddRule"; }

private:
  void moveChunks(eld::plugin::LinkerScriptRule oldRule,
                  eld::plugin::LinkerScriptRule newRule) {
    for (eld::plugin::Chunk C : oldRule.getChunks()) {
      // It is crucial to maintain that no two LinkerScriptRule objects contain
      // the same chunk. It is an undefined behavior for a chunk to be contained
      // by multiple linker script rules.
      Linker->addChunk(newRule, C);
      Linker->removeChunk(oldRule, C);
    }
  }

  void moveChunks(eld::plugin::OutputSection oldSection,
                  eld::plugin::LinkerScriptRule newRule) {
    for (eld::plugin::LinkerScriptRule rule : oldSection.getLinkerScriptRules())
      moveChunks(rule, newRule);
  }

private:
  eld::plugin::OutputSection m_Var = eld::plugin::OutputSection(nullptr);
  eld::plugin::OutputSection m_Foo = eld::plugin::OutputSection(nullptr);
  eld::plugin::OutputSection m_Bar = eld::plugin::OutputSection(nullptr);
};

eld::plugin::Plugin *ThisPlugin = nullptr;

extern "C" {
// RegisterAll should initialize all the plugins that a plugin library aims
// to provide. Linker calls this function before running any plugins provided
// by the library.
bool RegisterAll() {
  ThisPlugin = new AddRule{};
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