#include "Defines.h"
#include "PluginBase.h"
#include "LinkerWrapper.h"
#include "OutputSectionIteratorPlugin.h"
#include "PluginADT.h"
#include "PluginVersion.h"
#include <cassert>
#include <map>

class DLL_A_EXPORT ChunkRemoveButNotAdd
    : public eld::plugin::OutputSectionIteratorPlugin {
public:
  ChunkRemoveButNotAdd()
      : eld::plugin::OutputSectionIteratorPlugin("ChunkRemoveButNotAdd") {}

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
    auto fooRules = m_Foo.getLinkerScriptRules();
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

  std::string GetName() override { return "ChunkRemoveButNotAdd"; }

  std::string GetLastErrorAsString() override { return "Success"; }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

private:
  eld::plugin::OutputSection m_Foo{nullptr};
  eld::plugin::OutputSection m_Bar{nullptr};
};

class DLL_A_EXPORT ChunkRemoveUsingUpdate
    : public eld::plugin::OutputSectionIteratorPlugin {
public:
  ChunkRemoveUsingUpdate()
      : eld::plugin::OutputSectionIteratorPlugin("ChunkRemoveUsingUpdate") {}

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
    auto fooRules = m_Foo.getLinkerScriptRules();
    auto barRules = m_Bar.getLinkerScriptRules();

    getLinker()->updateChunks(barRules.front(), {});
    getLinker()->updateChunks(fooRules.front(), {});

    return Status::SUCCESS;
  }

  std::string GetName() override { return "ChunkRemoveUsingUpdate"; }

  std::string GetLastErrorAsString() override { return "Success"; }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

private:
  eld::plugin::OutputSection m_Foo{nullptr};
  eld::plugin::OutputSection m_Bar{nullptr};
};

class DLL_A_EXPORT MultipleChunkAdds
    : public eld::plugin::OutputSectionIteratorPlugin {
public:
  MultipleChunkAdds()
      : eld::plugin::OutputSectionIteratorPlugin("MultipleChunkAdds") {}

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
    auto fooRules = m_Foo.getLinkerScriptRules();
    auto barRules = m_Bar.getLinkerScriptRules();

    for (auto rule : barRules) {
      for (auto chunk : rule.getChunks()) {
        eld::Expected<void> expAddChunk1 =
            getLinker()->addChunk(fooRules.front(), chunk);
        ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expAddChunk1);
        eld::Expected<void> expAddChunk2 =
            getLinker()->addChunk(fooRules.front(), chunk);
        ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expAddChunk2);
      }
    }
    return Status::SUCCESS;
  }

  std::string GetName() override { return "MultipleChunkAdds"; }

  std::string GetLastErrorAsString() override { return "Success"; }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

private:
  eld::plugin::OutputSection m_Foo{nullptr};
  eld::plugin::OutputSection m_Bar{nullptr};
};

class DLL_A_EXPORT ChunkInsertButNotRemove
    : public eld::plugin::OutputSectionIteratorPlugin {
public:
  ChunkInsertButNotRemove()
      : eld::plugin::OutputSectionIteratorPlugin("ChunkInsertButNotRemove") {}

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
    auto fooRules = m_Foo.getLinkerScriptRules();
    auto barRules = m_Bar.getLinkerScriptRules();

    for (auto rule : barRules) {
      for (auto chunk : rule.getChunks()) {
        eld::Expected<void> expAddChunk =
            getLinker()->addChunk(fooRules.front(), chunk);
        ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expAddChunk);
      }
    }
    return Status::SUCCESS;
  }

  std::string GetName() override { return "ChunkInsertButNotRemove"; }

  std::string GetLastErrorAsString() override { return "Success"; }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

private:
  eld::plugin::OutputSection m_Foo{nullptr};
  eld::plugin::OutputSection m_Bar{nullptr};
};

std::map<std::string, eld::plugin::Plugin *> plugins;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  plugins["ChunkRemoveButNotAdd"] = new ChunkRemoveButNotAdd();
  plugins["ChunkRemoveUsingUpdate"] = new ChunkRemoveUsingUpdate();
  plugins["MultipleChunkAdds"] = new MultipleChunkAdds();
  plugins["ChunkInsertButNotRemove"] = new ChunkInsertButNotRemove();
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