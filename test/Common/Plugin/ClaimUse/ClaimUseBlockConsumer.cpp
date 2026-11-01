//===- ClaimUseBlockConsumer.cpp ------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "ControlFileSizePlugin.h"
#include "ControlMemorySizePlugin.h"
#include "PluginVersion.h"
#include <iostream>
#include <string>
#include <vector>

using namespace eld::plugin;

#ifdef MEMORY_SIZE
using ConsumerBase = ControlMemorySizePlugin;
#else
using ConsumerBase = ControlFileSizePlugin;
#endif

class ClaimUseBlockConsumer : public ConsumerBase {
public:
  ClaimUseBlockConsumer() : ConsumerBase("ClaimUseBlockConsumer") {}
  void Init(std::string) override {}
  void AddBlocks(Block B) override {
    std::cerr << "consumed block\n";
    B.Name = ".consumed";
    Blocks.push_back(B);
  }
  std::vector<Block> GetBlocks() override { return Blocks; }
  Status Run(bool) override { return Status::SUCCESS; }
  void Destroy() override {}
  uint32_t GetLastError() override { return 0; }
  std::string GetLastErrorAsString() override { return ""; }
  std::string GetName() override { return "ClaimUseBlockConsumer"; }

private:
  std::vector<Block> Blocks;
};

ELD_REGISTER_PLUGIN(ClaimUseBlockConsumer)
