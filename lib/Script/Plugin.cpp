//===- Plugin.cpp----------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#include "eld/Script/Plugin.h"
#include "eld/Core/Module.h"
#include "eld/Diagnostics/DiagnosticPrinter.h"
#include "eld/Input/SearchDirs.h"
#include "eld/PluginAPI/DiagnosticEntry.h"
#include "eld/PluginAPI/LinkerPlugin.h"
#include "eld/PluginAPI/LinkerPluginConfig.h"
#include "eld/PluginAPI/PluginADT.h"
#include "eld/PluginAPI/PluginBase.h"
#include "eld/Support/DynamicLibrary.h"
#include "eld/Support/MsgHandling.h"
#include "eld/Support/RegisterTimer.h"
#include "eld/Support/StringUtils.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/PrettyStackTrace.h"
#include <functional>

#include <memory>
#include <string>
#include <string_view>

using namespace eld;

struct Running {
  explicit Running(Plugin *P) : P(P) { P->setRunning(true); }
  ~Running() { P->setRunning(false); }

private:
  Plugin *P;
};

Plugin::Plugin(plugin::Plugin::Type T, std::string LibraryName,
               std::string PluginName, std::string O, bool Stats,
               Module &Module)
    : ThisType(T), CurID(0), Name(LibraryName), PluginType(PluginName),
      PluginOptions(O), PluginLibraryHandle(nullptr),
      PluginRegisterFunction(nullptr), GetPluginFunction(nullptr),
      UserPluginHandle(nullptr), PluginCleanupFunction(nullptr), Stats(Stats),
      ThisModule(Module), ThisConfig(Module.getConfig()) {}

std::string Plugin::resolvePath(const LinkerConfig &PConfig) {
  // Library already loaded!
  if (PluginLibraryHandle)
    return PluginLibraryName;

  if (!PluginLibraryName.empty())
    return PluginLibraryName;

  if (PConfig.hasMappingForFile(Name)) {
    PluginLibraryName = PConfig.getHashFromFile(Name);
    return PluginLibraryName;
  }

  std::string PluginLibraryName = DynamicLibrary::getLibraryName(Name);

  auto &PSearchDirs = PConfig.directories();

  // Find the library in the list of directories specified in the search path.
  const sys::fs::Path *NS =
      PSearchDirs.findLibrary("linker plugin", PluginLibraryName, ThisConfig);

  // If it cannot find a library, fail the link.
  if (!NS) {
    ThisConfig.raise(Diag::unable_to_find_library) << PluginLibraryName;
    ThisModule.setFailure(true);
    return "";
  }

  if (nullptr != NS) {
    PluginLibraryName = NS->getFullPath();
    if (ThisModule.getPrinter()->tracePlugins())
      ThisConfig.raise(Diag::using_plugin) << PluginLibraryName << Name;
  }
  return PluginLibraryName;
}

void *Plugin::loadPlugin(std::string LibraryName, Module *Module) {
  void *LibraryHandle = DynamicLibrary::Load(LibraryName);
  DiagnosticEngine *DiagEngine = Module->getConfig().getDiagEngine();
  if (!LibraryHandle) {
    DiagEngine->raise(Diag::unable_to_load_library)
        << LibraryName << DynamicLibrary::GetLastError();
    return nullptr;
  }

  if (Module->getPrinter()->tracePlugins())
    DiagEngine->raise(Diag::loaded_library) << LibraryName;

  return LibraryHandle;
}

bool Plugin::registerPlugin(void *Handle) {
  if (!getUserPlugin())
    return false;

  getUserPluginConfig();

  if (!check())
    return false;

  return true;
}

std::string Plugin::getLibraryName() const { return PluginLibraryName; }

bool Plugin::setFunctions() {
  std::string Register = "RegisterAll";
  std::string LibraryName = DynamicLibrary::getLibraryName(Name);

  void *R = DynamicLibrary::GetFunction(PluginLibraryHandle, Register);
  if (!R) {
    std::string LastError = DynamicLibrary::GetLastError();
    ThisConfig.raise(Diag::unable_to_find_plugin_func)
        << Register << LibraryName << LastError;
    return false;
  }

  std::string PluginFunc = "getPlugin";
  void *F = DynamicLibrary::GetFunction(PluginLibraryHandle, PluginFunc);
  if (!F) {
    std::string LastError = DynamicLibrary::GetLastError();
    ThisConfig.raise(Diag::unable_to_find_plugin_func)
        << PluginFunc << LibraryName << LastError;
    return false;
  }

  if (ThisModule.getPrinter()->tracePlugins()) {
    ThisConfig.raise(Diag::found_register_function) << Register << LibraryName;
    ThisConfig.raise(Diag::found_function_for_plugintype)
        << PluginFunc << LibraryName;
  }

  std::string PluginCleanupFunc = "Cleanup";
  void *C = DynamicLibrary::GetFunction(PluginLibraryHandle, PluginCleanupFunc);
  if (C && ThisModule.getPrinter()->tracePlugins()) {
    ThisConfig.raise(Diag::found_cleanup_function)
        << PluginCleanupFunc << LibraryName;
  }

  std::string PluginConfigFunc = "getPluginConfig";
  void *ConfigFunc =
      DynamicLibrary::GetFunction(PluginLibraryHandle, PluginConfigFunc);
  if (ConfigFunc)
    ThisConfig.raise(Diag::found_config_function)
        << PluginConfigFunc << LibraryName;

  PluginRegisterFunction = (plugin::RegisterAllFuncT *)R;
  GetPluginFunction = (plugin::PluginFuncT *)F;
  PluginCleanupFunction = (plugin::PluginCleanupFuncT *)C;
  PluginConfigFunction = (plugin::PluginConfigFuncT *)ConfigFunc;

  return true;
}

bool Plugin::getUserPlugin() {
  std::string LibraryName = DynamicLibrary::getLibraryName(Name);

  if (ThisModule.getPrinter()->tracePlugins())
    ThisConfig.raise(Diag::registering_all_functions);

  UserPluginHandle = (*GetPluginFunction)(getPluginType().c_str());
  if (!UserPluginHandle) {
    ThisConfig.raise(Diag::unable_to_find_plugin_for_type)
        << getPluginType() << LibraryName;
    return false;
  }

  if (ThisModule.getPrinter()->tracePlugins())
    ThisConfig.raise(Diag::found_plugin_handler)
        << getPluginType() << LibraryName;

  return true;
}

void Plugin::getUserPluginConfig() {
  if (!PluginConfigFunction)
    return;
  LinkerPluginConfigHandle = (*PluginConfigFunction)(getPluginType().c_str());
}

bool Plugin::init(eld::OutputTarWriter *OutputTar) {
  Running R(this);
  if (!UserPluginHandle)
    return false;
  eld::RegisterTimer T(
      "Init", ThisModule.saveString(UserPluginHandle->GetName()), Stats);
  if (ThisModule.getPrinter()->tracePlugins())
    ThisConfig.raise(Diag::initializing_plugin)
        << getPluginType() << DynamicLibrary::getLibraryName(Name)
        << UserPluginHandle->GetName();
  if (ThisModule.getConfig().options().hasMappingFile())
    PluginOptions = ThisModule.getConfig().getHashFromFile(PluginOptions);
  plugin::Plugin *P = llvm::cast<plugin::Plugin>(UserPluginHandle);
  P->Init(PluginOptions);
  if (OutputTar && llvm::sys::fs::exists(PluginOptions))
    OutputTar->createAndAddConfigFile(PluginOptions, PluginOptions);
  if (P->GetLastError()) {
    ThisConfig.raise(Diag::plugin_has_error)
        << getPluginType() << DynamicLibrary::getLibraryName(Name)
        << P->GetLastErrorAsString() << ThisModule.getStateStr() << "\n";
    return false;
  }
  return true;
}

bool Plugin::run(std::vector<Plugin *> &Plugins) {
  if (!UserPluginHandle)
    return false;
  eld::RegisterTimer T(
      "Run", ThisModule.saveString(UserPluginHandle->GetName()), Stats);
  if (ThisModule.getPrinter()->tracePlugins())
    ThisConfig.raise(Diag::running_plugin)
        << getPluginType() << DynamicLibrary::getLibraryName(Name)
        << UserPluginHandle->GetName();
  Plugins.push_back(this);
  Running R(this);
  plugin::Plugin *P = llvm::cast<plugin::Plugin>(UserPluginHandle);
  plugin::Plugin::Status S = P->Run(ThisModule.getPrinter()->tracePlugins());
  if (S == plugin::Plugin::Status::ERROR || P->GetLastError()) {
    ThisConfig.raise(Diag::plugin_has_error)
        << getPluginType() << DynamicLibrary::getLibraryName(Name)
        << P->GetLastErrorAsString() << ThisModule.getStateStr() << "\n";
    return false;
  }
  return true;
}

bool Plugin::cleanup() {
  Running R(this);
  if (!PluginLibraryHandle)
    return false;
  if (!UserPluginHandle)
    return false;
  eld::RegisterTimer T(
      "Cleanup", ThisModule.saveString(UserPluginHandle->GetName()), Stats);
  if (PluginCleanupFunction)
    (*PluginCleanupFunction)();
  clearResources();
  return true;
}

bool Plugin::unload(std::string LibraryName, void *Handle, Module *Module) {
  if (Handle) {
    DynamicLibrary::Unload(Handle);

    if (Module->getPrinter()->tracePlugins())
      Module->getConfig().raise(Diag::unloaded_library) << LibraryName;
  }

  return true;
}

void Plugin::reset() {
  PluginLibraryHandle = nullptr;
  PluginRegisterFunction = nullptr;
  PluginCleanupFunction = nullptr;
  GetPluginFunction = nullptr;
}

bool Plugin::destroy() {
  Running R(this);
  if (!UserPluginHandle)
    return false;
  eld::RegisterTimer T(
      "Destroy", ThisModule.saveString(UserPluginHandle->GetName()), Stats);
  if (ThisModule.getPrinter()->tracePlugins())
    ThisConfig.raise(Diag::plugin_destroy) << getPluginType();
  plugin::Plugin *P = llvm::cast<plugin::Plugin>(UserPluginHandle);
  P->Destroy();
  if (P->GetLastError()) {
    ThisConfig.raise(Diag::plugin_has_error)
        << getPluginType() << DynamicLibrary::getLibraryName(Name)
        << P->GetLastErrorAsString() << ThisModule.getStateStr() << "\n";
    return false;
  }

  return true;
}

bool Plugin::check() {
  Running Run(this);
  if (!UserPluginHandle)
    return false;
  eld::RegisterTimer T(
      "Check", ThisModule.saveString(UserPluginHandle->GetName()), Stats);
  if (UserPluginHandle->getType() != ThisType) {
    ThisConfig.raise(Diag::plugin_mismatch)
        << DynamicLibrary::getLibraryName(Name) << getPluginType();
    return false;
  }

  // Check for plugin version.
  unsigned int PluginMajor = 0, PluginMinor = 0;

  void *R =
      DynamicLibrary::GetFunction(PluginLibraryHandle, "getPluginAPIVersion");
  if (R) {
    plugin::PluginGetAPIVersionFuncT *GetAPIVersionFunc =
        (plugin::PluginGetAPIVersionFuncT *)R;
    GetAPIVersionFunc(&PluginMajor, &PluginMinor);
  } else {
    ThisConfig.raise(Diag::error_plugin_no_api_version)
        << UserPluginHandle->GetName() << DynamicLibrary::getLibraryName(Name)
        << PluginMajor << PluginMinor;
    return false;
  }

  if (ThisModule.getPrinter()->tracePlugins())
    ThisConfig.raise(Diag::note_plugin_version)
        << PluginMajor << PluginMinor << DynamicLibrary::getLibraryName(Name)
        << getPluginType();

  if (PluginMajor != LINKER_PLUGIN_API_MAJOR_VERSION ||
      PluginMinor > LINKER_PLUGIN_API_MINOR_VERSION) {
    ThisConfig.raise(Diag::wrong_version)
        << DynamicLibrary::getLibraryName(Name) << getPluginType()
        << PluginMajor << PluginMinor << LINKER_PLUGIN_API_MAJOR_VERSION
        << LINKER_PLUGIN_API_MINOR_VERSION;
    return false;
  }
  return true;
}

void Plugin::createRelocationVector(uint32_t Num, bool State) {
  // If there is no plugin config, there is no need to set the
  // relocation vector.
  if (!LinkerPluginConfigHandle)
    return;
  RelocBitVector = eld::make<llvm::BitVector>(Num, State);
  SlowPathRelocBitVector = eld::make<llvm::BitVector>(Num, State);
  RelocPayLoadMap.reserve(Num);
}

void Plugin::initializeLinkerPluginConfig() {
  if (!LinkerPluginConfigHandle)
    return;
  auto InitCallBack =
      std::bind(&plugin::LinkerPluginConfig::Init, LinkerPluginConfigHandle);
  InitCallBack();
}

void Plugin::callReloc(uint32_t RelocType, Relocation *R) {
  if (!LinkerPluginConfigHandle)
    return;
  if (!isRelocTypeRegistered(RelocType, R))
    return;
  auto RelocCallBack =
      std::bind(&plugin::LinkerPluginConfig::RelocCallBack,
                LinkerPluginConfigHandle, std::placeholders::_1);
  RelocCallBack(plugin::Use(R));
}

void Plugin::registerRelocType(uint32_t RelocType, std::string Name) {
  if (!RelocBitVector)
    return;
  // Fast Path.
  if (Name.empty()) {
    (*RelocBitVector)[RelocType] = true;
    return;
  }
  (*SlowPathRelocBitVector)[RelocType] = true;
  RelocPayLoadMap[RelocType] = Name;
}

bool Plugin::isRelocTypeRegistered(uint32_t RelocType, Relocation *R) {
  if (!RelocBitVector)
    return false;
  if ((*RelocBitVector)[RelocType])
    return true;
  if ((*SlowPathRelocBitVector)[RelocType] == false)
    return false;
  llvm::StringRef SymbolName = R->symInfo()->name();
  llvm::StringRef RelocPayLoadStr = RelocPayLoadMap[RelocType];
  if (SymbolName.starts_with(RelocPayLoadStr))
    return true;
  return false;
}

plugin::LinkerPluginConfig *Plugin::getLinkerPluginConfig() const {
  return LinkerPluginConfigHandle;
}

std::string Plugin::getPluginName() const {
  if (!UserPluginHandle)
    return "";
  return UserPluginHandle->GetName();
}

std::string Plugin::getDescription() const {
  if (!UserPluginHandle)
    return "";
  return UserPluginHandle->GetDescription();
}

bool Plugin::registerAll() const {
  if (!PluginRegisterFunction)
    return false;
  std::string LibraryName = DynamicLibrary::getLibraryName(Name);
  ThisConfig.raise(Diag::calling_function_from_dynamic_lib)
      << "RegisterAll" << LibraryName;
  (*PluginRegisterFunction)();
  return true;
}

Plugin::~Plugin() {
  for (void *Handle : LibraryHandles)
    DynamicLibrary::Unload(Handle);
}

eld::Expected<void> Plugin::verifyFragmentMovements() const {
  for (const auto &UnbalancedRemove :
       PluginUnbalancedFragmentMoves.UnbalancedRemoves) {
    const Fragment *F = UnbalancedRemove.first;
    const RuleContainer *R = UnbalancedRemove.second;
    // FIXME: We return on first error currently. It will be nice if we can
    // report all the errors at-once. For example, if there are 5 fragments
    // which are removed but not inserted, then it would be nice if we see
    // 5 errors once for each such fragment, but currently, we will only see
    // the error for the first fragment.
    return std::make_unique<plugin::DiagnosticEntry>(
        Diag::err_chunk_removed_but_not_inserted,
        std::vector<std::string>{
            F->getOwningSection()->getDecoratedName(ThisConfig.options()),
            R->getAsString(), getPluginName()});
  }
  for (const auto &UnbalancedAdd :
       PluginUnbalancedFragmentMoves.UnbalancedAdds) {
    const Fragment *F = UnbalancedAdd.first;
    if (F->originatesFromPlugin(ThisModule))
      continue;
    const RuleContainer *R = UnbalancedAdd.second;
    return std::make_unique<plugin::DiagnosticEntry>(
        Diag::err_chunk_inserted_but_not_removed,
        std::vector<std::string>{
            F->getOwningSection()->getDecoratedName(ThisConfig.options()),
            R->getAsString(), getPluginName()});
  }
  return {};
}

void Plugin::recordFragmentAdd(RuleContainer *R, Fragment *F) {
  if (PluginUnbalancedFragmentMoves.UnbalancedRemoves.count(F)) {
    PluginUnbalancedFragmentMoves.UnbalancedRemoves.erase(F);
    return;
  }
  ASSERT(!PluginUnbalancedFragmentMoves.UnbalancedAdds.count(F),
         "Immediate error must be returned by the LinkerWrapper for Repeated "
         "adds of the same fragment");
  PluginUnbalancedFragmentMoves.UnbalancedAdds[F] = R;
}

void Plugin::recordFragmentRemove(RuleContainer *R, Fragment *F) {
  if (PluginUnbalancedFragmentMoves.UnbalancedAdds.count(F)) {
    PluginUnbalancedFragmentMoves.UnbalancedAdds.erase(F);
    return;
  }
  PluginUnbalancedFragmentMoves.UnbalancedRemoves[F] = R;
}

eld::Expected<std::pair<void *, std::string>>
Plugin::loadLibrary(const std::string &LibraryName) {
  const eld::sys::fs::Path *Library =
      ThisModule.getConfig().searchDirs().findLibrary(
          "plugin loadLibrary", LibraryName, ThisModule.getConfig());
  if (!Library) {
    Library = ThisModule.getConfig().searchDirs().findLibrary(
        "plugin loadLibrary", DynamicLibrary::getLibraryName(LibraryName),
        ThisModule.getConfig());
    if (!Library)
      return std::make_unique<plugin::DiagnosticEntry>(
          Diag::unable_to_find_library, std::vector<std::string>{LibraryName});
  }

  std::string LibraryPath = Library->getFullPath();

  void *LibraryHandle = DynamicLibrary::Load(LibraryPath);

  if (!LibraryHandle) {
    return std::make_unique<plugin::DiagnosticEntry>(
        Diag::unable_to_load_library,
        std::vector<std::string>{DynamicLibrary::GetLastError()});
  }

  LibraryHandles.push_back(LibraryHandle);

  if (ThisModule.getOutputTarWriter()) {
    ThisModule.getOutputTarWriter()->createAndAddFile(
        LibraryPath, LibraryPath, MappingFile::Kind::SharedLibrary,
        /*isLTOObject*/ false);
  }
  return std::make_pair(LibraryHandle, LibraryPath);
}

void Plugin::callInitHook() {
  plugin::LinkerPlugin *P = llvm::cast<plugin::LinkerPlugin>(UserPluginHandle);
  RegisterTimer T("Init", ThisModule.saveString(UserPluginHandle->GetName()),
                  Stats);
  if (ThisModule.getPrinter()->tracePlugins())
    ThisConfig.raise(Diag::trace_plugin_init) << getPluginName();
  P->Init(PluginOptions);
}

void Plugin::callDestroyHook() {
  plugin::LinkerPlugin *P = llvm::cast<plugin::LinkerPlugin>(UserPluginHandle);
  RegisterTimer T("Destroy", ThisModule.saveString(UserPluginHandle->GetName()),
                  Stats);
  if (ThisModule.getPrinter()->tracePlugins())
    ThisConfig.raise(Diag::trace_plugin_destroy) << getPluginName();
  P->Destroy();
}

void Plugin::registerCommandLineOption(
    const std::string &Option, bool HasValue,
    const CommandLineOptionSpec::OptionHandlerType &OptionHandler) {
  if (ThisModule.getPrinter()->tracePlugins()) {
    if (HasValue)
      ThisConfig.raise(Diag::trace_plugin_register_opt_with_val)
          << getPluginName() << Option;
    else
      ThisConfig.raise(Diag::trace_plugin_register_opt_without_val)
          << getPluginName() << Option;
  }
  PluginCommandLineOptions.push_back({Option, HasValue, OptionHandler});
}

void Plugin::callCommandLineOptionHandler(
    const std::string &Option, const std::optional<std::string> &Val,
    const CommandLineOptionSpec::OptionHandlerType &OptionHandler) {
  RegisterTimer T("Command-line option handler",
                  ThisModule.saveString(UserPluginHandle->GetName()), Stats);
  if (Val)
    ThisConfig.raise(Diag::verbose_calling_plugin_opt_handler_with_val)
        << getPluginName() << Option << Val.value();
  else
    ThisConfig.raise(Diag::verbose_calling_plugin_opt_handler_without_val)
        << getPluginName() << Option;
  OptionHandler(Option, Val);
}

void Plugin::callVisitSectionsHook(InputFile &IF) {
  plugin::LinkerPlugin *P = llvm::cast<plugin::LinkerPlugin>(UserPluginHandle);
  RegisterTimer T("VisitSections",
                  ThisModule.saveString(UserPluginHandle->GetName()), Stats);
  if (ThisModule.getPrinter()->tracePlugins())
    ThisConfig.raise(Diag::trace_plugin_visit_sections)
        << getPluginName() << IF.getInput()->decoratedPath();
  P->VisitSections(plugin::InputFile(&IF));
}

void Plugin::callVisitSymbolHook(LDSymbol *Sym, llvm::StringRef SymName,
                                 const SymbolInfo &SymInfo) {
  plugin::LinkerPlugin *P = llvm::cast<plugin::LinkerPlugin>(UserPluginHandle);
  RegisterTimer T("VisitSymbol",
                  ThisModule.saveString(UserPluginHandle->GetName()), Stats);
  if (ThisModule.getPrinter()->tracePlugins())
    ThisConfig.raise(Diag::trace_plugin_visit_symbol)
        << getPluginName() << SymName;
  std::unique_ptr<SymbolInfo> UpSymInfo = std::make_unique<SymbolInfo>(SymInfo);
  P->VisitSymbol(
      plugin::InputSymbol(Sym, std::string_view(SymName.data(), SymName.size()),
                          std::move(UpSymInfo)));
}

void Plugin::callActBeforeRuleMatchingHook() {
  llvm::StringRef HookName = "ActBeforeRuleMatching";
  plugin::LinkerPlugin *P = llvm::cast<plugin::LinkerPlugin>(UserPluginHandle);
  RegisterTimer T(HookName, ThisModule.saveString(UserPluginHandle->GetName()),
                  Stats);
  if (ThisModule.getPrinter()->tracePlugins())
    ThisConfig.raise(Diag::trace_plugin_hook) << getPluginName() << HookName;
  P->ActBeforeRuleMatching();
}

void Plugin::callActBeforeSectionMergingHook() {
  llvm::StringRef HookName = "ActBeforeSectionMerging";
  plugin::LinkerPlugin *P = llvm::cast<plugin::LinkerPlugin>(UserPluginHandle);
  RegisterTimer T(HookName, ThisModule.saveString(UserPluginHandle->GetName()),
                  Stats);
  if (ThisModule.getPrinter()->tracePlugins())
    ThisConfig.raise(Diag::trace_plugin_hook) << getPluginName() << HookName;
  P->ActBeforeSectionMerging();
}

void Plugin::callActBeforePerformingLayoutHook() {
  llvm::StringRef HookName = "ActBeforePerformingLayout";
  plugin::LinkerPlugin *P = llvm::cast<plugin::LinkerPlugin>(UserPluginHandle);
  RegisterTimer T(HookName, ThisModule.saveString(UserPluginHandle->GetName()),
                  Stats);
  if (ThisModule.getPrinter()->tracePlugins())
    ThisConfig.raise(Diag::trace_plugin_hook) << getPluginName() << HookName;
  P->ActBeforePerformingLayout();
}

void Plugin::callActBeforeWritingOutputHook() {
  llvm::StringRef HookName = "ActBeforeWritingOutput";
  plugin::LinkerPlugin *P = llvm::cast<plugin::LinkerPlugin>(UserPluginHandle);
  RegisterTimer T(HookName, ThisModule.saveString(UserPluginHandle->GetName()),
                  Stats);
  if (ThisModule.getPrinter()->tracePlugins())
    ThisConfig.raise(Diag::trace_plugin_hook) << getPluginName() << HookName;
  P->ActBeforeWritingOutput();
}

void Plugin::clearResources() {
  PluginCommandLineOptions.clear();
}