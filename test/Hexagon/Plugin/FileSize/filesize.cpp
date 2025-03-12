#include "ControlFileSizePlugin.h"
#include "PluginVersion.h"
#include <cstring>
#include <string>
#include <unordered_map>

using namespace eld::plugin;

class DLL_A_EXPORT CopyBlocksPlugin : public ControlFileSizePlugin {
public:
  CopyBlocksPlugin() : ControlFileSizePlugin("COPYBLOCKS"), Blocks() {}

  void Init(std::string Options) override {}

  void AddBlocks(Block B) override {
    B.Name = ".pluginfoo";
    Blocks.push_back(B);
  }

  Status Run(bool Verbose) override {
    uint8_t *Buf = nullptr;
    Buf = reinterpret_cast<uint8_t *>(getLinker()->getUninitBuffer(sizeof("hello")));
    if (Buf)
      std::memcpy(Buf, "Hello", 5);
    return Plugin::Status::SUCCESS;
  }

  std::vector<Block> GetBlocks() override { return Blocks; }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "SUCCESS"; }

  std::string GetName() override { return "COPYBLOCKS"; }

private:
  std::vector<Block> Blocks;
};

Plugin *ThisPlugin = nullptr;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  ThisPlugin = new CopyBlocksPlugin();
  return true;
}
PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return ThisPlugin; }
void DLL_A_EXPORT Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
}
}
