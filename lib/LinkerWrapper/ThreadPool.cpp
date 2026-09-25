//===- ThreadPool.cpp------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "llvm/Support/ThreadPool.h"
#include "eld/PluginAPI/ThreadPool.h"

using namespace eld;
using namespace eld::plugin;

plugin::ThreadPool::ThreadPool(uint32_t NumThreads)
    : TPool(new llvm::StdThreadPool(llvm::hardware_concurrency(NumThreads))) {}

ThreadPool::ThreadPool(ThreadPool &&other) noexcept : TPool(nullptr) {
  *this = std::move(other);
}

ThreadPool &ThreadPool::operator=(ThreadPool &&other) noexcept {
  if (this == &other)
    return *this;
  if (TPool) {
    wait();
    delete TPool;
  }
  TPool = other.TPool;
  Futures = std::move(other.Futures);
  other.TPool = nullptr;
  return *this;
}

std::shared_future<void>
ThreadPool::asyncImpl(plugin::ThreadPool::TaskTy Task) {
  std::shared_future<void> Future = TPool->async(Task);
  Futures.push_back(Future);
  return Future;
}

void plugin::ThreadPool::wait() {
  if (!TPool)
    return;
  TPool->wait();
  Futures.clear();
}

plugin::ThreadPool::~ThreadPool() {
  if (TPool) {
    wait();
    delete TPool;
  }
}
