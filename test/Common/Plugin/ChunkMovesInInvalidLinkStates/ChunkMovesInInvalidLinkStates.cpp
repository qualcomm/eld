#include "Defines.h"
#include "PluginBase.h"
#include "LinkerWrapper.h"
#include "OutputSectionIteratorPlugin.h"
#include "PluginADT.h"
#include "PluginVersion.h"
#include "SectionIteratorPlugin.h"
#include <cassert>
#include <iostream>
#include <map>

class DLL_A_EXPORT ChunkMovesInBeforeLayout
    : public eld::plugin::SectionIteratorPlugin {
public:
  ChunkMovesInBeforeLayout()
      : eld::plugin::SectionIteratorPlugin("ChunkMovesInBeforeLayout") {}

  void Init(std::string options) override {}

  void processSection(eld::plugin::Section O) override {
    if (O.matchPattern("*foo*"))
      m_Foo = O;
    else if (O.matchPattern("*bar*"))
      m_Bar = O;
  }

  Status Run(bool trace) override {
    assert(m_Foo && m_Bar && "foo and bar output sections must be present!");
    auto fooRule = m_Foo.getLinkerScriptRule();
    auto chunk = m_Bar.getChunks().front();

    eld::Expected<void> expRemoveChunk =
        getLinker()->removeChunk(fooRule, chunk);
    eld::Expected<void> expAddChunk = getLinker()->addChunk(fooRule, chunk);
    eld::Expected<void> expUpdateChunks =
        getLinker()->updateChunks(fooRule, std::vector<eld::plugin::Chunk>{});
    if (!expRemoveChunk.has_value())
      getLinker()->reportDiagEntry(std::move(expRemoveChunk.error()));
    if (!expAddChunk.has_value())
      getLinker()->reportDiagEntry(std::move(expAddChunk.error()));
    if (!expUpdateChunks.has_value())
      getLinker()->reportDiagEntry(std::move(expUpdateChunks.error()));
    return Status::ERROR;
  }

  std::string GetName() override { return "ChunkMovesInBeforeLayout"; }

  std::string GetLastErrorAsString() override { return "Success"; }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

private:
  eld::plugin::Section m_Foo{nullptr};
  eld::plugin::Section m_Bar{nullptr};
};

class DLL_A_EXPORT ChunkMovesInAfterLayout
    : public eld::plugin::OutputSectionIteratorPlugin {
public:
  ChunkMovesInAfterLayout()
      : eld::plugin::OutputSectionIteratorPlugin("ChunkMovesInAfterLayout") {}

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
    if (getLinker()->getState() == eld::plugin::LinkerWrapper::CreatingSections) {
      auto barRules = m_Bar.getLinkerScriptRules();
      m_BarChunk = barRules.front().getChunks().front();
      return Status::SUCCESS;
    }
    if (getLinker()->getState() != eld::plugin::LinkerWrapper::AfterLayout)
      return Status::SUCCESS;

    assert(m_Foo && m_Bar && "foo and bar output sections must be present!");
    auto fooRule = m_Foo.getLinkerScriptRules().front();

    eld::Expected<void> expRemoveChunk =
        getLinker()->removeChunk(fooRule, m_BarChunk);
    eld::Expected<void> expAddChunk =
        getLinker()->addChunk(fooRule, m_BarChunk);
    eld::Expected<void> expUpdateChunks =
        getLinker()->updateChunks(fooRule, std::vector<eld::plugin::Chunk>{});
    if (!expRemoveChunk.has_value())
      getLinker()->reportDiagEntry(std::move(expRemoveChunk.error()));
    if (!expAddChunk.has_value())
      getLinker()->reportDiagEntry(std::move(expAddChunk.error()));
    if (!expUpdateChunks.has_value())
      getLinker()->reportDiagEntry(std::move(expUpdateChunks.error()));
    return Status::ERROR;
  }

  std::string GetName() override { return "ChunkMovesInAfterLayout"; }

  std::string GetLastErrorAsString() override { return "Success"; }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

private:
  eld::plugin::OutputSection m_Foo{nullptr};
  eld::plugin::OutputSection m_Bar{nullptr};
  eld::plugin::Chunk m_BarChunk{nullptr};
};


std::map<std::string, eld::plugin::Plugin *> plugins;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  plugins["ChunkMovesInBeforeLayout"] = new ChunkMovesInBeforeLayout();
  plugins["ChunkMovesInAfterLayout"] = new ChunkMovesInAfterLayout();
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
