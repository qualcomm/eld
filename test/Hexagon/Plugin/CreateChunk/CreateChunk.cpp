#include "LinkerWrapper.h"
#include "OutputSectionIteratorPlugin.h"
#include "PluginVersion.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <queue>
#include <set>
#include <string>
#include <unordered_map>

// namespace {

class DLL_A_EXPORT CreateChunk : public eld::plugin::OutputSectionIteratorPlugin {
public:
  CreateChunk() : eld::plugin::OutputSectionIteratorPlugin("CreateChunk") {}

  void Init(std::string Options) override {}
  void Destroy() override {}
  std::string GetName() override { return PluginName; }

  Status Run(bool Trace) override {
    if (getLinker()->getState() == eld::plugin::LinkerWrapper::CreatingSections) {
      size_t Align = sizeof(uint32_t);

      const unsigned char NOPBytes[] = {0x00, 0xc0, 0x00, 0x7f};
      eld::Expected<eld::plugin::Chunk> expCodeChunk = getLinker()->createCodeChunk(
          "nop", Align, CreateBufCopy(sizeof(NOPBytes), NOPBytes),
          sizeof(NOPBytes));
      ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expCodeChunk);
      eld::plugin::Chunk codeChunk = std::move(expCodeChunk.value());

      addChunkToSection(".text", codeChunk);

      const uint32_t DataVar = 0x12345678;
      eld::Expected<eld::plugin::Chunk> expDataChunk = getLinker()->createDataChunk(
          "datavar", Align, CreateBufCopy(sizeof(DataVar), &DataVar),
          sizeof(DataVar));
      ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expDataChunk);
      eld::plugin::Chunk dataChunk = std::move(expDataChunk.value());
      addChunkToSection(".rodata", dataChunk);

      eld::Expected<eld::plugin::Chunk> expBSSChunk =
          getLinker()->createBSSChunk("bssvar", Align, sizeof(uint32_t));
      ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expBSSChunk);
      eld::plugin::Chunk BSSChunk = std::move(expBSSChunk.value());
      addChunkToSection(".bss", BSSChunk);
    }
    return Plugin::Status::SUCCESS;
  }

  void processOutputSection(eld::plugin::OutputSection O) override {};

  uint32_t GetLastError() override { return 0; }
  std::string GetLastErrorAsString() override { return "SUCCESS"; }

private:
  char *CreateBufCopy(size_t Size, const void *Data) {
    char *Buf = getLinker()->getUninitBuffer(Size);
    std::memcpy(Buf, Data, Size);
    return Buf;
  }

  void addChunkToSection(std::string SectionName, eld::plugin::Chunk C) {
    eld::Expected<eld::plugin::OutputSection> expOutSect =
        getLinker()->getOutputSection(SectionName);
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(
        getLinker(), expOutSect);
    eld::plugin::OutputSection outSect = expOutSect.value();
    for (const auto &R : outSect.getLinkerScriptRules()) {
      eld::Expected<void> expAddChunk =
          getLinker()->addChunk(R.getRuleContainer(), C);
      ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(
          getLinker(), expAddChunk);
      break;
    }
  }
};

std::unique_ptr<CreateChunk> Plugin;

//} // namespace

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  if (!Plugin)
    Plugin = std::make_unique<CreateChunk>();
  return true;
}

void DLL_A_EXPORT Cleanup() { Plugin = nullptr; }

eld::plugin::PluginBase DLL_A_EXPORT *getPlugin(const char *T) {
  return Plugin && Plugin->GetName() == T ? Plugin.get() : nullptr;
}

eld::plugin::LinkerPluginConfig DLL_A_EXPORT *getPluginConfig(const char *T) {
  return nullptr;
}
}
