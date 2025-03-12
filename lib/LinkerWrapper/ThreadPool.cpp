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
  std::swap(TPool, other.TPool);
  return *this;
}

std::shared_future<void>
ThreadPool::asyncImpl(plugin::ThreadPool::TaskTy Task) {
  return TPool->async(Task);
}

void plugin::ThreadPool::wait() { TPool->wait(); }

plugin::ThreadPool::~ThreadPool() {
  if (TPool)
    delete TPool;
}
