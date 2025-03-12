//===- Plugin.h------------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_SCRIPT_PLUGIN_H
#define ELD_SCRIPT_PLUGIN_H

#include "eld/PluginAPI/LinkerPluginConfig.h"
#include "eld/PluginAPI/LinkerWrapper.h"
#include "eld/PluginAPI/PluginBase.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/StringMap.h"
#include <functional>
#include <unordered_map>

namespace eld {

class LinkerConfig;
class Relocation;
class SearchDirs;
class OutputTarWriter;
class Module;

class Plugin {
public:
  explicit Plugin(plugin::Plugin::Type T, std::string Name, std::string R,
                  std::string O, bool Stats, Module &module);

  ~Plugin();

  struct CommandLineOptionSpec {
    using OptionHandlerType =
        plugin::LinkerWrapper::CommandLineOptionHandlerType;

    CommandLineOptionSpec(const std::string &option, bool hasValue,
                          const OptionHandlerType &optionHandler)
        : Option(option), HasValue(hasValue), OptionHandler(optionHandler) {}

    std::string Option;
    bool HasValue;
    OptionHandlerType OptionHandler;

    /// Returns true if `optionName` and `val` matches the option spec.
    bool match(const std::string &optionName,
               const std::optional<std::string> &val) const {
      return Option == optionName && HasValue == val.has_value();
    }
  };

  // -------------- Diagnostic Functions ------------------------
  plugin::Plugin::Type getType() const { return m_Type; }

  std::string getLibraryName() const;

  std::string getName() const { return m_Name; }

  std::string getPluginType() const { return m_PluginType; }

  std::string getPluginOptions() const { return m_Options; }

  plugin::PluginBase *getLinkerPlugin() const { return m_UserPlugin; }

  void *getLibraryHandle() const { return m_LibraryHandle; }
  /// Set plugin library handle.
  void setLibraryHandle(void *handle) { m_LibraryHandle = handle; }

  // -------------- Search Plugin --------------------------------
  std::string resolvePath(const LinkerConfig &Config);

  void setResolvedPath(std::string ResolvedPath) {
    m_LibraryName = ResolvedPath;
  }

  void setID(uint32_t ID) { m_ID = ID; }

  uint32_t getID() const { return m_ID; }

  // -------------- Register Plugin --------------------------------

  /// Set functions -- 'RegisterAll', 'getPlugin', 'getPluginConfig', and
  /// 'Cleanup' -- provided by the plugin library.
  ///
  /// \note This function should only be called after setting
  /// m_LibraryHandle member.
  bool SetFunctions();

  /// Call the 'RegisterAll' function if provided by the plugin
  /// library.
  ///
  /// \note This function should only be called after setting all
  /// plugin functions using 'Plugin::SetFunctions'.
  bool RegisterAll() const;
  bool RegisterPlugin(void *Handle);

  // -------------- Load/Unload/Reset Plugin ------------------------
  static void *LoadPlugin(std::string Name, Module *Module);

  static bool Unload(std::string Name, void *LibraryHandle, Module *Module);

  void Reset();

  // -------------- Run -------------------------------------------
  bool Run(std::vector<Plugin *> &L);

  // -------------- GetUserPlugin --------------------------------
  bool GetUserPlugin();

  // -------------- GetUserPluginConfig --------------------------
  void GetUserPluginConfig();

  // --------------Destroy the Plugin----------------------------
  bool Destroy();

  // --------------Cleanup the Plugin----------------------------
  bool Cleanup();

  // --------------Initialize the Plugin-----------------------
  bool Init(eld::OutputTarWriter *outputTar);

  /// ----------------User Plugin functions --------------------
  std::string getPluginName() const;

  std::string GetDescription() const;

  //  -------------- Relocation Callback support ----------------
  void initializeLinkerPluginConfig();

  void createRelocationVector(uint32_t Num, bool State = false);

  void callReloc(uint32_t RelocType, Relocation *R);

  void registerRelocType(uint32_t RelocType, std::string Name);

  bool isRelocTypeRegistered(uint32_t RelocType, Relocation *R);

  plugin::LinkerPluginConfig *getLinkerPluginConfig() const;

  // -----------------Check if timing is enabled ---------------
  bool isTimingEnabled() const { return Stats; }

  /// -----------------Handle crash -------------------
  bool isRunning() const { return m_isRunning; }

  void setRunning(bool isRunning) { m_isRunning = isRunning; }

  plugin::LinkerWrapper *getLinkerWrapper() {
    return getLinkerPlugin()->getLinker();
  }

  /// Stores excess chunk/fragment movements. It is used to
  /// verify chunk movement operations by a plugin. For example:
  /// - A chunk that is removed must be put back into the image.
  /// - A chunk must not be added multiple times.
  class UnbalancedFragmentMoves {
  public:
    using TrackingDataType = std::unordered_map<Fragment *, RuleContainer *>;

    TrackingDataType UnbalancedRemoves;

    // There cannot be more than one unbalanced add for a fragment because
    // repeated adds of the same fragment using LinkerWrapper APIs gives
    // immediate errors.
    TrackingDataType UnbalancedAdds;
  };

  /// Record fragment add operation for fragment movement verification.
  void recordFragmentAdd(RuleContainer *R, Fragment *F);

  /// Record fragment remove operation for fragment movement verification.
  void recordFragmentRemove(RuleContainer *R, Fragment *F);

  eld::Expected<void> verifyFragmentMovements() const;

  const UnbalancedFragmentMoves &getUnbalancedFragmentMoves() const {
    return m_UnbalancedFragmentMoves;
  }

  eld::Expected<std::pair<void *, std::string>>
  loadLibrary(const std::string &LibraryName);

  void callInitHook();

  void callDestroyHook();

  void registerCommandLineOption(
      const std::string &option, bool hasValue,
      const CommandLineOptionSpec::OptionHandlerType &optionHandler);

  const std::vector<CommandLineOptionSpec> &
  getPluginCommandLineOptions() const {
    return PluginCommandLineOptions;
  }

  void callCommandLineOptionHandler(
      const std::string &option, const std::optional<std::string> &val,
      const CommandLineOptionSpec::OptionHandlerType &optionHandler);

  /// Calls VisitSections hook handler for input file IF.
  void callVisitSectionsHook(InputFile &IF);

  void callVisitSymbolHook(LDSymbol *sym, llvm::StringRef symName,
                           const SymbolInfo &symInfo);

  /// Calls ActBeforeSectionMerging hook handler.
  void callActBeforeSectionMergingHook();

  void callActBeforePerformingLayoutHook();

  void callActBeforeWritingOutputHook();

  /// Calls ActBeforeRuleMatching hook handler.
  void callActBeforeRuleMatchingHook();

private:
  bool Check();

  std::string findInRPath(llvm::StringRef LibraryName, llvm::StringRef RPath);

private:
  plugin::Plugin::Type m_Type;
  uint32_t m_ID;
  std::string m_Name;
  std::string m_LibraryName;
  std::string m_PluginType;
  std::string m_Options;
  void *m_LibraryHandle = nullptr;
  plugin::RegisterAllFuncT *m_RegisterFunc = nullptr;
  plugin::PluginFuncT *m_PluginFunc = nullptr;
  plugin::PluginBase *m_UserPlugin = nullptr;
  plugin::PluginCleanupFuncT *m_PluginCleanupFunc = nullptr;
  plugin::PluginConfigFuncT *m_PluginConfigFunc = nullptr;
  plugin::LinkerPluginConfig *m_LinkerPluginConfig = nullptr;
  bool m_isRunning = false;
  llvm::BitVector *RelocBitVector = nullptr;
  llvm::BitVector *SlowPathRelocBitVector = nullptr;
  std::unordered_map<uint32_t, std::string> RelocPayLoadMap;
  bool Stats = false;
  Module &m_Module;
  LinkerConfig &m_Config;
  UnbalancedFragmentMoves m_UnbalancedFragmentMoves;
  std::vector<void *> LibraryHandles;
  std::vector<CommandLineOptionSpec> PluginCommandLineOptions;
};
} // namespace eld

#endif
