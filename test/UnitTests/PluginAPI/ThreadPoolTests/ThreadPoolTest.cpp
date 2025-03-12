#include "eld/PluginAPI/LinkerWrapper.h"
#include "gtest/gtest.h"
#include <numeric>
#include <vector>

class ThreadPoolTest : public ::testing::Test {
  void SetUp() override {}
  void TearDown() override {}
};

void fn(long long i, long long *sq) { *sq = i * i; }

TEST_F(ThreadPoolTest, MoveOpTest) {

  eld::plugin::ThreadPool threadPool(32);
  std::vector<long long> squares(5000, 0);
  for (int i = 1; i < 2000; ++i)
    threadPool.run(fn, i, &squares[i]);

  eld::plugin::ThreadPool anotherThreadPool = std::move(threadPool);
  for (int i = 2000; i < 4000; ++i)
    anotherThreadPool.run(fn, i, &squares[i]);

  threadPool = std::move(anotherThreadPool);
  for (int i = 4000; i < 5000; ++i)
    threadPool.run(fn, i, &squares[i]);

  threadPool.wait();
  long long sum = std::accumulate(squares.begin(), squares.end(), 0LL);
  EXPECT_EQ(sum, 41654167500LL);
}
