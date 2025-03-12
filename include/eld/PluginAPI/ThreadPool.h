//===- ThreadPool.h--------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_PLUGINAPI_THREADPOOL_H
#define ELD_PLUGINAPI_THREADPOOL_H

#include "Defines.h"
#include <functional>
#include <future>

namespace llvm {
class ThreadPoolInterface;
}

namespace eld::plugin {
/// A ThreadPool for asynchronous parallel execution on a defined number
/// of threads.
///
/// The pool keeps a vector of threads alive, waiting on a condition
/// variable for some work to become available.
class DLL_A_EXPORT ThreadPool {
public:
  using TaskTy = std::function<void()>;

  explicit ThreadPool(uint32_t NumThreads);

  // Disable copy operations.
  ThreadPool(const ThreadPool &) = delete;
  ThreadPool &operator=(const ThreadPool &) = delete;

  // ThreadPool should be movable to be consistent with other
  // thread-based data structure types in std and llvm.
  ThreadPool(ThreadPool &&) noexcept;
  ThreadPool &operator=(ThreadPool &&) noexcept;

  /// Run a function F in a thread pool.
  template <typename Function, typename... Args>
  inline std::shared_future<void> run(Function &&F, Args &&...ArgList) {
    auto Task =
        std::bind(std::forward<Function>(F), std::forward<Args>(ArgList)...);
    return asyncImpl(std::move(Task));
  }

  /// Run a function F in a thread pool.
  template <typename Function> std::shared_future<void> run(Function &&F) {
    return asyncImpl(std::forward<Function>(F));
  }

  /// Blocking wait for all the threads to complete and the queue to
  /// be empty.
  void wait();

  ~ThreadPool();

private:
  std::shared_future<void> asyncImpl(TaskTy F);

private:
  llvm::ThreadPoolInterface *TPool;
};

} // namespace eld::plugin
#endif
