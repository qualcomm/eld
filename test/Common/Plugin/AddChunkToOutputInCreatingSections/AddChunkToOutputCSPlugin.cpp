#include "LinkerWrapper.h"
#include "PluginVersion.h"
#include "LinkerPlugin.h"
#include <iostream>

class AddChunkToOutputCSPlugin : public eld::plugin::LinkerPlugin {
public:
  AddChunkToOutputCSPlugin()
      : eld::plugin::LinkerPlugin("AddChunkToOutputCSPlugin") {}
  void ActBeforePerformingLayout() override {
    const char *buf = getLinker()->getUninitBuffer(10);
    auto expBarChunk = getLinker()->createDataChunkWithCustomName(
        ".plugin.bar", /*Alignment=*/1, buf, /*Sz=*/10);
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(
        getLinker(), expBarChunk);
    auto expAddBar = getLinker()->addChunkToOutput(expBarChunk.value());
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(
        getLinker(), expAddBar);

    auto expBazChunk = getLinker()->createDataChunkWithCustomName(
        ".plugin.baz", /*Alignment=*/1, buf, /*Sz=*/10);
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(
        getLinker(), expBazChunk);
    auto expAddBaz = getLinker()->addChunkToOutput(expBazChunk.value());
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(
        getLinker(), expAddBaz);
  }
};

eld::plugin::PluginBase *ThisPlugin = nullptr;

extern "C" {
bool RegisterAll() {
  ThisPlugin = new AddChunkToOutputCSPlugin();
  return true;
}

eld::plugin::PluginBase *getPlugin(const char *T) { return ThisPlugin; }

void Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
  ThisPlugin = nullptr;
}
}
