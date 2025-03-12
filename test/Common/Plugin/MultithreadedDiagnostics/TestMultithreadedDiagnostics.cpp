#include "LinkerWrapper.h"
#include "OutputSectionIteratorPlugin.h"
#include "PluginVersion.h"
#include <iostream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

using namespace eld::plugin;

class CustomThreadPool {
public:
  CustomThreadPool(std::size_t concurrency = 0)
      : m_Concurrency(concurrency), m_Index(0) {
    if (m_Concurrency == 0)
      m_Concurrency = std::thread::hardware_concurrency();
    if (m_Concurrency == 0)
      m_Concurrency = 2;
    m_Threads.resize(m_Concurrency);
  }

  template <class F> void run(F &&f) {
    if (m_Threads[m_Index].joinable())
      m_Threads[m_Index].join();
    m_Threads[m_Index] = std::thread(f);
    m_Index = (m_Index + 1) % m_Concurrency;
  }

  void join() {
    for (auto &thread : m_Threads) {
      if (thread.joinable())
        thread.join();
    }
  }

  ~CustomThreadPool() { join(); }

private:
  std::vector<std::thread> m_Threads;
  std::size_t m_Concurrency;
  std::size_t m_Index = 0;
};

class DLL_A_EXPORT MultithreadedDiagnostics
    : public OutputSectionIteratorPlugin {
public:
  MultithreadedDiagnostics()
      : OutputSectionIteratorPlugin("MultithreadedDiagnostics") {}

  void Init(std::string Options) override {}

  void processOutputSection(OutputSection S) override { return; }

  Status Run(bool Trace) override {
    if (getLinker()->getState() != LinkerWrapper::State::BeforeLayout)
      return Plugin::Status::SUCCESS;

    std::vector<std::size_t> concurrencies = {1, 2, 4, 8, 16, 32, 64, 128};

    for (auto concurrency : concurrencies) {
      CustomThreadPool threadPool(concurrency);
      for (std::size_t i = 0; i < 100; ++i) {
        auto emitDiags = [this, concurrency, i]() -> void {
          for (std::size_t j = 0; j < 20; ++j) {
            std::string t = std::to_string(concurrency) + "." +
                            std::to_string(i) + "." + std::to_string(j);
            auto id = getLinker()->getWarningDiagID("Warning " + t + ": %0");
            getLinker()->reportDiag(id, "Warning message " + t);
          }
        };
        threadPool.run(emitDiags);
      }
    }

    return Plugin::Status::SUCCESS;
  }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "SUCCESS"; }

  std::string GetName() override { return "MultithreadedDiagnostics"; }
};

class DLL_A_EXPORT MultithreadedDiagnosticsUsingELDTP
    : public OutputSectionIteratorPlugin {
public:
  MultithreadedDiagnosticsUsingELDTP()
      : OutputSectionIteratorPlugin("MultithreadedDiagnosticsUsingELDTP") {}

  void Init(std::string Options) override {}

  void processOutputSection(OutputSection S) override { return; }

  Status Run(bool Trace) override {
    if (getLinker()->getState() != LinkerWrapper::State::BeforeLayout)
      return Plugin::Status::SUCCESS;

    std::vector<std::size_t> concurrencies = {1, 2, 4, 8, 16, 32, 64, 128};

    for (auto concurrency : concurrencies) {
      eld::plugin::ThreadPool threadPool(concurrency);
      for (std::size_t i = 0; i < 100; ++i) {
        auto emitDiags = [this, concurrency, i]() -> void {
          for (std::size_t j = 0; j < 20; ++j) {
            std::string t = std::to_string(concurrency) + "." +
                            std::to_string(i) + "." + std::to_string(j);
            auto id = getLinker()->getWarningDiagID("Warning " + t + ": %0");
            getLinker()->reportDiag(id, "Warning message " + t);
          }
        };
        threadPool.run(emitDiags);
      }
    }

    return Plugin::Status::SUCCESS;
  }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "SUCCESS"; }

  std::string GetName() override {
    return "MultithreadedDiagnosticsUsingELDTP";
  }
};

std::unordered_map<std::string, Plugin *> Plugins;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  Plugins["MultithreadedDiagnostics"] = new MultithreadedDiagnostics();
  Plugins["MultithreadedDiagnosticsUsingELDTP"] =
      new MultithreadedDiagnosticsUsingELDTP();
  return true;
}

PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return Plugins[T]; }
void DLL_A_EXPORT Cleanup() {
  for (auto &item : Plugins) {
    if (item.second)
      delete item.second;
  }
  Plugins.clear();
}
}
