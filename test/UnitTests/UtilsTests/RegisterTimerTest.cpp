#include "eld/Support/RegisterTimer.h"
#include "gtest/gtest.h"

TEST(RegisterTimerTest, TimerStopsAtScopeExit) {
  llvm::Timer *OperationTimer = nullptr;
  {
    eld::RegisterTimer OperationScope("Flat Operation", "Read", true);
    OperationTimer = OperationScope.getTimer();
    EXPECT_TRUE(OperationTimer->isRunning());
  }
  EXPECT_FALSE(OperationTimer->isRunning());
}

TEST(RegisterTimerTest, SequentialPhasesDoNotOverlap) {
  llvm::Timer *ReadTimer = nullptr;
  {
    eld::RegisterTimer ReadScope("Sequential Read", "Read", true);
    ReadTimer = ReadScope.getTimer();
    EXPECT_TRUE(ReadTimer->isRunning());
  }
  {
    eld::RegisterTimer ResolveScope("Sequential Resolve", "Resolve", true);
    EXPECT_FALSE(ReadTimer->isRunning());
    EXPECT_TRUE(ResolveScope.getTimer()->isRunning());
  }
}

TEST(RegisterTimerTest, DisabledTimerIsNoOp) {
  eld::RegisterTimer OperationTimer("Enabled Operation", "Read", true);
  {
    eld::RegisterTimer DisabledTimer("Disabled Operation", "Resolve", false);
    EXPECT_EQ(DisabledTimer.getTimer(), nullptr);
    EXPECT_TRUE(OperationTimer.getTimer()->isRunning());
  }
  EXPECT_TRUE(OperationTimer.getTimer()->isRunning());
}

TEST(RegisterTimerTest, RepeatedNamesReuseStoppedTimer) {
  llvm::Timer *FirstTimer = nullptr;
  {
    eld::RegisterTimer FirstScope("Repeated Operation", "Read", true);
    FirstTimer = FirstScope.getTimer();
    EXPECT_TRUE(FirstTimer->isRunning());
  }
  EXPECT_FALSE(FirstTimer->isRunning());
  {
    eld::RegisterTimer SecondScope("Repeated Operation", "Read", true);
    EXPECT_EQ(FirstTimer, SecondScope.getTimer());
    EXPECT_TRUE(FirstTimer->isRunning());
  }
  EXPECT_FALSE(FirstTimer->isRunning());
}

#ifndef NDEBUG
TEST(RegisterTimerDeathTest, RejectsNestedTimersAcrossPhases) {
  EXPECT_DEATH(
      {
        eld::RegisterTimer ReadTimer("Nested Read", "Read", true);
        eld::RegisterTimer ResolveTimer("Nested Resolve", "Resolve", true);
      },
      "Timers must not overlap");
}

TEST(RegisterTimerDeathTest, RejectsNestedTimersWithSameName) {
  EXPECT_DEATH(
      {
        eld::RegisterTimer FirstTimer("Nested Duplicate", "Plugins", true);
        eld::RegisterTimer SecondTimer("Nested Duplicate", "Plugins", true);
      },
      "Timers must not overlap");
}
#endif
