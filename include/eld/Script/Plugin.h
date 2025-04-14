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
                  std::string O, bool Stats, Module &Module);

  ~Plugin();

  struct CommandLineOptionSpec {
    using OptionHandlerType =
        plugin::LinkerWrapper::CommandLineOptionHandlerType;

    CommandLineOptionSpec(const std::string &Option, bool HasValue,
                          const OptionHandlerType &OptionHandler)
        : Option(Option), HasValue(HasValue), OptionHandler(OptionHandler) {}

    std::string Option;
    bool HasValue;
    OptionHandlerType OptionHandler;

    /// Returns true if `optionName` and `val` matches the option spec.
    bool match(const std::string &OptionName,
               const std::optional<std::string> &Val) const {
      return Option == OptionName && HasValue == Val.has_value();
    }
  };

  // -------------- Diagnostic Functions ------------------------
  plugin::Plugin::Type getType() const { return ThisType; }

  std::string getLibraryName() const;

  std::string getName() const { return Name; }

  std::string getPluginType() const { return PluginType; }

  std::string getPluginOptions() const { return PluginOptions; }

  plugin::PluginBase *getLinkerPlugin() const { return UserPluginHandle; }

  void *getLibraryHandle() const { return PluginLibraryHandle; }
  /// Set plugin library handle.
  void setLibraryHandle(void *Handle) { PluginLibraryHandle = Handle; }

  // -------------- Search Plugin --------------------------------
  std::string resolvePath(const LinkerConfig &Config);

  void setResolvedPath(std::string ResolvedPath) {
    PluginLibraryName = ResolvedPath;
  }

  void setID(uint32_t ID) { CurID = ID; }

  uint32_t getID() const { return CurID; }

  // -------------- Register Plugin --------------------------------

  /// Set functions -- 'RegisterAll', 'getPlugin', 'getPluginConfig', and
  /// 'Cleanup' -- provided by the plugin library.
  ///
  /// \note This function should only be called after setting
  /// m_LibraryHandle member.
  bool setFunctions();

  /// Call the 'RegisterAll' function if provided by the plugin
  /// library.
  ///
  /// \note This function should only be called after setting all
  /// plugin functions using 'Plugin::SetFunctions'.
  bool registerAll() const;
  bool registerPlugin(void *Handle);

  // -------------- Load/Unload/Reset Plugin ------------------------
  static void *loadPlugin(std::string Name, Module *Module);

  static bool unload(std::string Name, void *LibraryHandle, Module *Module);

  void reset();

  // -------------- Run -------------------------------------------
  bool run(std::vector<Plugin *> &L);

  // -------------- GetUserPlugin --------------------------------
  bool getUserPlugin();

  // -------------- GetUserPluginConfig --------------------------
  void getUserPluginConfig();

  // --------------Destroy the Plugin----------------------------
  bool destroy();

  // --------------Cleanup the Plugin----------------------------
  bool cleanup();

  // --------------Initialize the Plugin-----------------------
  bool init(eld::OutputTarWriter *OutputTar);

  /// ----------------User Plugin functions --------------------
  std::string getPluginName() const;

  std::string getDescription() const;

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
  bool isRunning() const { return PluginIsRunning; }

  void setRunning(bool IsRunning) { PluginIsRunning = IsRunning; }

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
    return PluginUnbalancedFragmentMoves;
  }

  eld::Expected<std::pair<void *, std::string>>
  loadLibrary(const std::string &LibraryName);

  void callInitHook();

  void callDestroyHook();

  void registerCommandLineOption(
      const std::string &Option, bool HasValue,
      const CommandLineOptionSpec::OptionHandlerType &OptionHandler);

  const std::vector<CommandLineOptionSpec> &
  getPluginCommandLineOptions() const {
    return PluginCommandLineOptions;
  }

  void callCommandLineOptionHandler(
      const std::string &Option, const std::optional<std::string> &Val,
      const CommandLineOptionSpec::OptionHandlerType &OptionHandler);

  /// Calls VisitSections hook handler for input file IF.
  void callVisitSectionsHook(InputFile &IF);

  void callVisitSymbolHook(LDSymbol *Sym, llvm::StringRef SymName,
                           const SymbolInfo &SymInfo);

  /// Calls ActBeforeSectionMerging hook handler.
  void callActBeforeSectionMergingHook();

  void callActBeforePerformingLayoutHook();

  void callActBeforeWritingOutputHook();

  /// Calls ActBeforeRuleMatching hook handler.
  void callActBeforeRuleMatchingHook();

private:
  bool check();

  std::string findInRPath(llvm::StringRef LibraryName, llvm::StringRef RPath);

  void clearResources();

private:
  plugin::Plugin::Type ThisType;
  uint32_t CurID;
  std::string Name;
  std::string PluginLibraryName;
  std::string PluginType;
  std::string PluginOptions;
  void *PluginLibraryHandle = nullptr;
  plugin::RegisterAllFuncT *PluginRegisterFunction = nullptr;
  plugin::PluginFuncT *GetPluginFunction = nullptr;
  plugin::PluginBase *UserPluginHandle = nullptr;
  plugin::PluginCleanupFuncT *PluginCleanupFunction = nullptr;
  plugin::PluginConfigFuncT *PluginConfigFunction = nullptr;
  plugin::LinkerPluginConfig *LinkerPluginConfigHandle = nullptr;
  bool PluginIsRunning = false;
  llvm::BitVector *RelocBitVector = nullptr;
  llvm::BitVector *SlowPathRelocBitVector = nullptr;
  std::unordered_map<uint32_t, std::string> RelocPayLoadMap;
  bool Stats = false;
  Module &ThisModule;
  LinkerConfig &ThisConfig;
  UnbalancedFragmentMoves PluginUnbalancedFragmentMoves;
  std::vector<void *> LibraryHandles;
  std::vector<CommandLineOptionSpec> PluginCommandLineOptions;
};
} // namespace eld

#endif
