#include "ControlFileSizePlugin.h"
#include "PluginVersion.h"
#include <cstring>
#include <string>
#include <unordered_map>

using namespace eld::plugin;

class DLL_A_EXPORT LinkerCrashPlugin : public ControlFileSizePlugin {
public:
  LinkerCrashPlugin() : ControlFileSizePlugin("LINKERCRASH"), Blocks() {}

  void Init(std::string Options) override {
    // crash the linker
    *p = 36;
  }

  Status Run(bool Verbose) override { return Plugin::Status::SUCCESS; }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "SUCCESS"; }

  std::string GetName() override { return "LINKERCRASH"; }

  void AddBlocks(Block B) override {
    B.Name = ".pluginfoo";
    Blocks.push_back(B);
  }

  std::vector<Block> GetBlocks() override { return Blocks; }

private:
  std::vector<Block> Blocks;
  // Compiler optimizes away p
  int *p = nullptr;
};

Plugin *ThisPlugin = nullptr;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  ThisPlugin = new LinkerCrashPlugin();
  return true;
}
PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return ThisPlugin; }
void DLL_A_EXPORT Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
}
}
