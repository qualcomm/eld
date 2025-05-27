#include "Defines.h"
#include "LinkerScript.h"
#include "LinkerWrapper.h"
#include "OutputSectionIteratorPlugin.h"
#include "PluginADT.h"
#include "PluginBase.h"
#include "PluginVersion.h"
#include <cassert>

class DLL_A_EXPORT BasicChunkMover : public eld::plugin::OutputSectionIteratorPlugin {
public:
  BasicChunkMover() : eld::plugin::OutputSectionIteratorPlugin("BasicChunkMover") {}

  void Init(std::string options) override {}

  void processOutputSection(eld::plugin::OutputSection O) override {
    if (getLinker()->getState() != eld::plugin::LinkerWrapper::CreatingSections)
      return;
    if (O.getName() == "foo")
      m_Foo = O;
    else if (O.getName() == "bar")
      m_Bar = O;
  }

  Status Run(bool trace) override {
    if (getLinker()->getState() != eld::plugin::LinkerWrapper::CreatingSections)
      return Status::SUCCESS;
    assert(m_Foo && m_Bar && "foo and bar output sections must be present!");
    auto fooRules = m_Foo.getLinkerScriptRules();
    auto barRules = m_Bar.getLinkerScriptRules();
    eld::Expected<eld::plugin::LinkerScriptRule> expMoveChunksRule =
        getLinker()->createLinkerScriptRule(m_Foo,
                                            "Move chunks from bar to foo");
    ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expMoveChunksRule);
    eld::plugin::LinkerScriptRule moveChunksRule =
        std::move(expMoveChunksRule.value());

    for (auto rule : barRules) {
      for (auto chunk : rule.getChunks()) {
        eld::Expected<void> expAddChunk =
            getLinker()->addChunk(moveChunksRule, chunk);
        ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expAddChunk);
        eld::Expected<void> expRemoveChunk =
            getLinker()->removeChunk(rule, chunk);
        ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expRemoveChunk);
      }
    }
    eld::Expected<void> expInsertRule =
        getLinker()->insertAfterRule(m_Foo, fooRules.front(), moveChunksRule);
    ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expInsertRule);
    return Status::SUCCESS;
  }

  std::string GetName() override { return "BasicChunkMover"; }

  std::string GetLastErrorAsString() override { return "Success"; }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

private:
  eld::plugin::OutputSection m_Foo{nullptr};
  eld::plugin::OutputSection m_Bar{nullptr};
};

ELD_REGISTER_PLUGIN(BasicChunkMover)