//===- PluginRelocationTest.cpp -------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Config/LinkerConfig.h"
#include "eld/Core/LinkerScript.h"
#include "eld/Core/Module.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Diagnostics/DiagnosticInfos.h"
#include "eld/Diagnostics/DiagnosticPrinter.h"
#include "eld/PluginAPI/LinkerWrapper.h"
#include "eld/Readers/Relocation.h"
#include "gtest/gtest.h"
#include <atomic>
#include <memory>
#include <thread>
#include <vector>

namespace {

class PluginRelocationTest : public ::testing::Test {
protected:
  void SetUp() override {
    Diag.setInfoMap(std::make_unique<eld::DiagnosticInfos>(Config));
  }

  eld::DiagnosticEngine Diag{false};
  eld::LinkerConfig Config{&Diag};
  eld::LinkerScript Script{&Diag};
  eld::Module Module{Script, Config, nullptr};
  eld::plugin::LinkerWrapper Owner{nullptr, Module};
  eld::plugin::LinkerWrapper Other{nullptr, Module};
};

TEST_F(PluginRelocationTest, ClaimDoesNotSupplyReplacementData) {
  eld::Relocation R, Unclaimed;
  R.setTargetData(0x12345678);
  uint64_t Data = 0x87654321;
  EXPECT_FALSE(Module.getRelocationData(&Unclaimed, Data));
  EXPECT_EQ(Data, 0x87654321U);
  ASSERT_TRUE(Module.claimRelocation(&R, &Owner));
  EXPECT_TRUE(R.isPluginOwned());
  EXPECT_FALSE(Module.getRelocationData(&R, Data));
  EXPECT_EQ(Data, 0x87654321U);
  EXPECT_FALSE(Module.getRelocationDataForSync(&R, Data));
  EXPECT_EQ(Data, 0x87654321U);
  eld::plugin::Use U(&R);
  ASSERT_TRUE(Owner.getTargetDataForUse(U, Data));
  EXPECT_EQ(Data, 0x12345678U);
  ASSERT_TRUE(Owner.setTargetDataForUse(U, 0));
  ASSERT_TRUE(Module.getRelocationData(&R, Data));
  EXPECT_EQ(Data, 0U);
  Data = 1;
  ASSERT_TRUE(Module.getRelocationDataForSync(&R, Data));
  EXPECT_EQ(Data, 0U);
}

TEST_F(PluginRelocationTest, ZeroOverrideHasReplacementData) {
  eld::Relocation R;
  R.setTargetData(0x12345678);
  eld::plugin::Use U(&R);
  ASSERT_TRUE(Owner.setTargetDataForUse(U, 0));
  EXPECT_FALSE(R.isPluginOwned());
  uint64_t Data = 1;
  ASSERT_TRUE(Module.getRelocationData(&R, Data));
  EXPECT_EQ(Data, 0U);
  Data = 1;
  ASSERT_TRUE(Owner.getTargetDataForUse(U, Data));
  EXPECT_EQ(Data, 0U);
  EXPECT_TRUE(Module.validatePluginRelocations());
}

TEST_F(PluginRelocationTest, OnlyOwnerCanSupplyClaimedData) {
  eld::Relocation R;
  eld::plugin::Use U(&R);
  ASSERT_TRUE(Module.claimRelocation(&R, &Owner));
  EXPECT_FALSE(Module.claimRelocation(&R, &Owner));
  EXPECT_FALSE(Module.claimRelocation(&R, &Other));
  EXPECT_FALSE(Other.setTargetDataForUse(U, 0xaaaaaaaaaaaaaaaa));
  ASSERT_TRUE(Owner.setTargetDataForUse(U, 0x123456789abcdef0));
  uint64_t Data = 0;
  ASSERT_TRUE(Owner.getTargetDataForUse(U, Data));
  EXPECT_EQ(Data, 0x123456789abcdef0ULL);
}

TEST_F(PluginRelocationTest, ExistingOverridePreventsClaim) {
  eld::Relocation R;
  eld::plugin::Use U(&R);
  ASSERT_TRUE(Other.setTargetDataForUse(U, 0x12345678));
  EXPECT_FALSE(Module.claimRelocation(&R, &Owner));
  EXPECT_FALSE(Owner.setTargetDataForUse(U, 0xaaaaaaaa));
  EXPECT_FALSE(Owner.setTargetDataForUse(U, 0x12345678));
  EXPECT_TRUE(Other.setTargetDataForUse(U, 0x87654321));
  EXPECT_FALSE(Owner.setTargetDataForUse(U, 0x87654321));
  EXPECT_FALSE(R.isPluginOwned());
  uint64_t Data = 0;
  ASSERT_TRUE(Other.getTargetDataForUse(U, Data));
  EXPECT_EQ(Data, 0x87654321U);
  EXPECT_FALSE(Module.validatePluginRelocations());
  EXPECT_FALSE(Diag.diagnose());
  EXPECT_EQ(Diag.getPrinter()->getNumErrors(), 2U);
}

TEST_F(PluginRelocationTest, SamePluginCanUpdateClaimedData) {
  eld::Relocation R;
  eld::plugin::Use U(&R);
  ASSERT_TRUE(Module.claimRelocation(&R, &Owner));
  for (uint64_t Expected : {0x123456789abcdef0ULL, 0xabcdef0123456789ULL,
                            0xabcdef0123456789ULL, 0ULL}) {
    ASSERT_TRUE(Owner.setTargetDataForUse(U, Expected));
    uint64_t Data = 0;
    ASSERT_TRUE(Owner.getTargetDataForUse(U, Data));
    EXPECT_EQ(Data, Expected);
  }
  EXPECT_FALSE(Other.setTargetDataForUse(U, 0));
  EXPECT_FALSE(Other.setTargetDataForUse(U, 0x12345678));
  uint64_t Data = 1;
  ASSERT_TRUE(Owner.getTargetDataForUse(U, Data));
  EXPECT_EQ(Data, 0U);
}

TEST_F(PluginRelocationTest, ConcurrentForeignOverridesAreExclusive) {
  constexpr size_t Count = 4096;
  std::vector<eld::Relocation> Relocs(Count);
  std::vector<uint8_t> First(Count), Second(Count);
  std::atomic<unsigned> Ready{0};
  std::atomic<bool> Start{false};
  auto Write = [&](eld::plugin::LinkerWrapper &Writer,
                   std::vector<uint8_t> &Written, uint64_t Data) {
    ++Ready;
    while (!Start.load())
      std::this_thread::yield();
    for (size_t I = 0; I < Count; ++I) {
      eld::plugin::Use U(&Relocs[I]);
      Written[I] = bool(Writer.setTargetDataForUse(U, Data));
      if (I % 7 == 0)
        std::this_thread::yield();
    }
  };
  std::thread FirstWriter([&] { Write(Owner, First, 0x12345678); });
  std::thread SecondWriter([&] { Write(Other, Second, 0x87654321); });
  while (Ready.load() != 2)
    std::this_thread::yield();
  Start = true;
  FirstWriter.join();
  SecondWriter.join();
  for (size_t I = 0; I < Count; ++I) {
    EXPECT_NE(First[I], Second[I]) << I;
    uint64_t Data = 0;
    ASSERT_TRUE(Module.getRelocationData(&Relocs[I], Data));
    EXPECT_EQ(Data, First[I] ? 0x12345678U : 0x87654321U) << I;
  }
  EXPECT_FALSE(Module.validatePluginRelocations());
  EXPECT_FALSE(Diag.diagnose());
  EXPECT_EQ(Diag.getPrinter()->getNumErrors(), 1U);
}

TEST_F(PluginRelocationTest, ConcurrentClaimAndForeignOverrideAreExclusive) {
  constexpr size_t Count = 4096;
  std::vector<eld::Relocation> Relocs(Count);
  std::vector<uint8_t> Claimed(Count), Written(Count);
  std::atomic<unsigned> Ready{0};
  std::atomic<bool> Start{false};
  auto AwaitStart = [&] {
    ++Ready;
    while (!Start.load())
      std::this_thread::yield();
  };
  std::thread Claimant([&] {
    AwaitStart();
    for (size_t I = 0; I < Count; ++I) {
      Claimed[I] = bool(Module.claimRelocation(&Relocs[I], &Owner));
      if (Claimed[I]) {
        eld::plugin::Use U(&Relocs[I]);
        EXPECT_TRUE(Owner.setTargetDataForUse(U, 0x123456789abcdef0));
      }
      if (I % 7 == 0)
        std::this_thread::yield();
    }
  });
  std::thread Writer([&] {
    AwaitStart();
    for (size_t I = 0; I < Count; ++I) {
      eld::plugin::Use U(&Relocs[I]);
      Written[I] = bool(Other.setTargetDataForUse(U, 0xaaaaaaaaaaaaaaaa));
      if (I % 7 == 0)
        std::this_thread::yield();
    }
  });
  while (Ready.load() != 2)
    std::this_thread::yield();
  Start = true;
  Claimant.join();
  Writer.join();
  for (size_t I = 0; I < Count; ++I) {
    EXPECT_NE(Claimed[I], Written[I]) << I;
    uint64_t Data = 0;
    ASSERT_TRUE(Module.getRelocationData(&Relocs[I], Data));
    EXPECT_EQ(Data, Claimed[I] ? 0x123456789abcdef0ULL : 0xaaaaaaaaaaaaaaaaULL)
        << I;
  }
}

TEST_F(PluginRelocationTest, ClaimsCloseBeforeScanning) {
  eld::Relocation Claimed, Late;
  ASSERT_TRUE(Module.claimRelocation(&Claimed, &Owner));
  Module.closePluginRelocationClaims();
  EXPECT_FALSE(Module.claimRelocation(&Late, &Owner));
  EXPECT_FALSE(Late.isPluginOwned());
  eld::plugin::Use U(&Claimed);
  ASSERT_TRUE(Owner.setTargetDataForUse(U, 0x12345678));
  ASSERT_TRUE(Owner.setTargetDataForUse(U, 0x87654321));
  uint64_t Data = 0;
  ASSERT_TRUE(Module.getRelocationData(&Claimed, Data));
  EXPECT_EQ(Data, 0x87654321U);
}

TEST_F(PluginRelocationTest, ValidationClosesReplacementWrites) {
  eld::Relocation R, Late;
  eld::plugin::Use U(&R);
  ASSERT_TRUE(Owner.setTargetDataForUse(U, 0x12345678));
  EXPECT_TRUE(Module.validatePluginRelocations());
  EXPECT_FALSE(Owner.setTargetDataForUse(U, 0x87654321));
  EXPECT_FALSE(Module.claimRelocation(&Late, &Owner));
  EXPECT_FALSE(Late.isPluginOwned());
  uint64_t Data = 0;
  ASSERT_TRUE(Module.getRelocationData(&R, Data));
  EXPECT_EQ(Data, 0x12345678U);
  EXPECT_FALSE(Module.validatePluginRelocations());
  EXPECT_FALSE(Diag.diagnose());
  EXPECT_GE(Diag.getPrinter()->getNumErrors(), 2U);
}

TEST_F(PluginRelocationTest, NullUseCannotBeClaimed) {
  EXPECT_FALSE(Owner.claimUse(eld::plugin::Use(nullptr)));
}

} // namespace
