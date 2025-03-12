#include "Defines.h"
#include "PluginBase.h"
#include "LinkerWrapper.h"
#include "OutputSectionIteratorPlugin.h"
#include "PluginADT.h"
#include "PluginVersion.h"
#include <cassert>
#include <map>

class DLL_A_EXPORT UnbalancedChunkRemoves
    : public eld::plugin::OutputSectionIteratorPlugin {
public:
  UnbalancedChunkRemoves()
      : eld::plugin::OutputSectionIteratorPlugin("UnbalancedChunkRemoves") {}

  void Init(std::string options) override {}

  void processOutputSection(eld::plugin::OutputSection O) override {
    if (getLinker()->getState() != eld::plugin::LinkerWrapper::CreatingSections)
      return;
    if (O.getName() == "FOO")
      m_Foo = O;
    else if (O.getName() == "BAR")
      m_Bar = O;
  }

  Status Run(bool trace) override {
    if (getLinker()->getState() != eld::plugin::LinkerWrapper::CreatingSections)
      return Status::SUCCESS;
    assert(m_Foo && m_Bar && "foo and bar output sections must be present!");
    auto barRules = m_Bar.getLinkerScriptRules();

    for (auto rule : barRules) {
      for (auto chunk : rule.getChunks()) {
        // Removes chunk but does not add it.
        eld::Expected<void> expRemoveChunk =
            getLinker()->removeChunk(rule, chunk);
        ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expRemoveChunk);
      }
    }
    return Status::SUCCESS;
  }

  std::string GetName() override { return "UnbalancedChunkRemoves"; }

  std::string GetLastErrorAsString() override { return "Success"; }

  void Destroy() override {
    if (getLinker()->getState() != eld::plugin::LinkerWrapper::CreatingSections)
      return;
    auto unbalancedAdds = getLinker()->getUnbalancedChunkAdds();
    assert(unbalancedAdds.empty() && "No unbalanced adds expected");

    const auto &unbalancedRemoves = getLinker()->getUnbalancedChunkRemoves();
    auto fooRule = m_Foo.getLinkerScriptRules().front();
    for (const auto &item : unbalancedRemoves) {
      auto diagID = getLinker()->getNoteDiagID(
          "Found unbalanced remove chunk '%0'. Adding it back.");
      getLinker()->reportDiagEntry(std::make_unique<eld::plugin::DiagnosticEntry>(
          diagID, std::vector<std::string>{item.chunk.getName()}));
      eld::Expected<void> expAddChunk =
          getLinker()->addChunk(fooRule, item.chunk);
      ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(
          getLinker(), expAddChunk);
    }
  }

  uint32_t GetLastError() override { return 0; }

private:
  eld::plugin::OutputSection m_Foo{nullptr};
  eld::plugin::OutputSection m_Bar{nullptr};
};

class DLL_A_EXPORT UnbalancedChunkAdds
    : public eld::plugin::OutputSectionIteratorPlugin {
public:
  UnbalancedChunkAdds()
      : eld::plugin::OutputSectionIteratorPlugin("UnbalancedChunkAdds") {}

  void Init(std::string options) override {}

  void processOutputSection(eld::plugin::OutputSection O) override {
    if (getLinker()->getState() != eld::plugin::LinkerWrapper::CreatingSections)
      return;
    if (O.getName() == "FOO")
      m_Foo = O;
    else if (O.getName() == "BAR")
      m_Bar = O;
  }

  Status Run(bool trace) override {
    if (getLinker()->getState() != eld::plugin::LinkerWrapper::CreatingSections)
      return Status::SUCCESS;
    assert(m_Foo && m_Bar && "foo and bar output sections must be present!");
    auto barRules = m_Bar.getLinkerScriptRules();
    auto fooRules = m_Foo.getLinkerScriptRules();

    for (auto rule : barRules) {
      for (auto chunk : rule.getChunks()) {
        eld::Expected<void> expAddChunk1 =
            getLinker()->addChunk(fooRules.front(), chunk);
        ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expAddChunk1);
      }
    }
    return Status::SUCCESS;
  }

  std::string GetName() override { return "UnbalancedChunkAdds"; }

  std::string GetLastErrorAsString() override { return "Success"; }

  void Destroy() override {
    if (getLinker()->getState() != eld::plugin::LinkerWrapper::CreatingSections)
      return;
    [[maybe_unused]] const auto &unbalancedChunkRemoves =
        getLinker()->getUnbalancedChunkRemoves();
    assert(unbalancedChunkRemoves.empty() && "No unbalanced removes expected!");
    const auto &unbalancedChunkAdds = getLinker()->getUnbalancedChunkAdds();
    for (const auto &item : unbalancedChunkAdds) {
      auto diagID = getLinker()->getNoteDiagID(
          "Found unbalanced add chunk '%0'. Removing it.");
      getLinker()->reportDiagEntry(std::make_unique<eld::plugin::DiagnosticEntry>(
          diagID, std::vector<std::string>{item.chunk.getName()}));
      auto expChunkRemove = getLinker()->removeChunk(item.rule, item.chunk);
      ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(
          getLinker(), expChunkRemove);
    }
  }

  uint32_t GetLastError() override { return 0; }

private:
  eld::plugin::OutputSection m_Foo{nullptr};
  eld::plugin::OutputSection m_Bar{nullptr};
};

std::map<std::string, eld::plugin::Plugin *> plugins;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  plugins["UnbalancedChunkRemoves"] = new UnbalancedChunkRemoves();
  plugins["UnbalancedChunkAdds"] = new UnbalancedChunkAdds();
  return true;
}

eld::plugin::PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return plugins[T]; }

void DLL_A_EXPORT Cleanup() {
  for (auto plugin : plugins) {
    delete plugin.second;
    plugin.second = nullptr;
  }
}
}