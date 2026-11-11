//===- RelocationDataTest.cpp ---------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Config/LinkerConfig.h"
#include "eld/Core/LinkerScript.h"
#include "eld/Core/Module.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Fragment/FillFragment.h"
#include "eld/Fragment/FragmentRef.h"
#include "eld/LayoutMap/LayoutInfo.h"
#include "eld/PluginAPI/LinkerWrapper.h"
#include "eld/Readers/Relocation.h"
#include "gtest/gtest.h"
#include <atomic>
#include <thread>
#include <vector>

namespace {

class RelocationDataTest : public ::testing::Test {
protected:
  eld::DiagnosticEngine Diag{false};
  eld::LinkerConfig Config{&Diag};
  eld::LinkerScript Script{&Diag};
  eld::LayoutInfo Info{Config};
  eld::Module Module{Script, Config, &Info};
  eld::plugin::LinkerWrapper Writer{nullptr, Module};
  eld::plugin::LinkerWrapper OtherWriter{nullptr, Module};
  eld::FillFragment Fragment{Module, 0, 4096 * sizeof(uint64_t)};
};

TEST_F(RelocationDataTest, RepeatedSamePluginCallsAreLogged) {
  eld::FragmentRef Ref(Fragment);
  eld::Relocation R(0, &Ref, 0, 0);
  eld::plugin::Use U(&R);
  // Equal values from one plugin keep this test independent of replacement
  // update and ownership policies.
  ASSERT_TRUE(Writer.setTargetDataForUse(U, 0x12345678));
  ASSERT_TRUE(Writer.setTargetDataForUse(U, 0x12345678));
  uint64_t Data = 0;
  ASSERT_TRUE(Writer.getTargetDataForUse(U, Data));
  EXPECT_EQ(Data, 0x12345678U);
  EXPECT_EQ(Info.getPlugins().size(), 1U);
  EXPECT_EQ(Info.getPluginOps().size(), 1U);
  EXPECT_EQ(Info.getPluginOps()[&Writer].size(), 2U);
  EXPECT_EQ(Info.getChunkOps()[&Fragment].size(), 2U);
}

TEST_F(RelocationDataTest, NullUseIsRejectedWithoutLogging) {
  eld::plugin::Use U(nullptr);
  EXPECT_FALSE(Writer.setTargetDataForUse(U, 0x12345678));
  EXPECT_TRUE(Info.getPlugins().empty());
  EXPECT_TRUE(Info.getPluginOps().empty());
  EXPECT_TRUE(Info.getChunkOps().empty());
}

TEST_F(RelocationDataTest, ConcurrentWritersRetainAllDataAndMapOperations) {
  constexpr size_t Count = 4096;
  constexpr size_t NumWorkers = 8;
  std::vector<eld::FragmentRef> Refs;
  std::vector<eld::Relocation> Relocs;
  Refs.reserve(Count);
  Relocs.reserve(Count);
  for (size_t I = 0; I < Count; ++I) {
    Refs.emplace_back(Fragment, I * sizeof(uint64_t));
    Relocs.emplace_back(0, &Refs.back(), 0, 0);
  }
  std::atomic<size_t> Ready{0};
  std::atomic<bool> Start{false};
  std::vector<std::thread> Workers;
  for (size_t W = 0; W < NumWorkers; ++W)
    Workers.emplace_back([&, W] {
      ++Ready;
      while (!Start.load())
        std::this_thread::yield();
      auto &LW = W % 2 ? Writer : OtherWriter;
      for (size_t I = W; I < Count; I += NumWorkers) {
        eld::plugin::Use U(&Relocs[I]);
        EXPECT_TRUE(LW.setTargetDataForUse(U, I + 1));
      }
    });
  while (Ready.load() != NumWorkers)
    std::this_thread::yield();
  Start = true;
  for (auto &W : Workers)
    W.join();

  for (size_t I = 0; I < Count; ++I) {
    uint64_t Data = 0;
    ASSERT_TRUE(Module.getRelocationData(&Relocs[I], Data));
    EXPECT_EQ(Data, I + 1);
  }
  EXPECT_EQ(Info.getPlugins().size(), 2U);
  EXPECT_EQ(Info.getPluginOps()[&Writer].size(), Count / 2);
  EXPECT_EQ(Info.getPluginOps()[&OtherWriter].size(), Count / 2);
  const auto &Ops = Info.getChunkOps()[&Fragment];
  ASSERT_EQ(Ops.size(), Count);
  std::vector<bool> Seen(Count);
  for (const auto *Op : Ops) {
    ASSERT_TRUE(eld::RelocationDataPluginOp::classof(Op));
    const auto *R =
        static_cast<const eld::RelocationDataPluginOp *>(Op)->getRelocation();
    size_t I = R - Relocs.data();
    ASSERT_LT(I, Count);
    EXPECT_FALSE(Seen[I]);
    Seen[I] = true;
  }
}

} // namespace
