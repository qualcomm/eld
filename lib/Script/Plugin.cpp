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
  explicit Running(Plugin *p) : P(p) { P->setRunning(true); }
  ~Running() { P->setRunning(false); }

private:
  Plugin *P;
};

Plugin::Plugin(plugin::Plugin::Type T, std::string libraryName,
               std::string pluginName, std::string O, bool Stats,
               Module &module)
    : m_Type(T), m_ID(0), m_Name(libraryName), m_PluginType(pluginName),
      m_Options(O), m_LibraryHandle(nullptr), m_RegisterFunc(nullptr),
      m_PluginFunc(nullptr), m_UserPlugin(nullptr),
      m_PluginCleanupFunc(nullptr), Stats(Stats), m_Module(module),
      m_Config(module.getConfig()) {}

std::string Plugin::resolvePath(const LinkerConfig &pConfig) {
  // Library already loaded!
  if (m_LibraryHandle)
    return m_LibraryName;

  if (!m_LibraryName.empty())
    return m_LibraryName;

  if (pConfig.hasMappingForFile(m_Name)) {
    m_LibraryName = pConfig.getHashFromFile(m_Name);
    return m_LibraryName;
  }

  std::string m_LibraryName = DynamicLibrary::getLibraryName(m_Name);

  auto &pSearchDirs = pConfig.directories();

  // Find the library in the list of directories specified in the search path.
  const sys::fs::Path *NS =
      pSearchDirs.findLibrary("linker plugin", m_LibraryName, m_Config);

  // If it cannot find a library, fail the link.
  if (!NS) {
    m_Config.raise(diag::unable_to_find_library) << m_LibraryName;
    m_Module.setFailure(true);
    return "";
  }

  if (nullptr != NS) {
    m_LibraryName = NS->getFullPath();
    if (m_Module.getPrinter()->tracePlugins())
      m_Config.raise(diag::using_plugin) << m_LibraryName << m_Name;
  }
  return m_LibraryName;
}

void *Plugin::LoadPlugin(std::string LibraryName, Module *Module) {
  void *LibraryHandle = DynamicLibrary::Load(LibraryName);
  DiagnosticEngine *DiagEngine = Module->getConfig().getDiagEngine();
  if (!LibraryHandle) {
    DiagEngine->raise(diag::unable_to_load_library)
        << LibraryName << DynamicLibrary::GetLastError();
    return nullptr;
  }

  if (Module->getPrinter()->tracePlugins())
    DiagEngine->raise(diag::loaded_library) << LibraryName;

  return LibraryHandle;
}

bool Plugin::RegisterPlugin(void *Handle) {
  if (!GetUserPlugin())
    return false;

  GetUserPluginConfig();

  if (!Check())
    return false;

  return true;
}

std::string Plugin::getLibraryName() const { return m_LibraryName; }

bool Plugin::SetFunctions() {
  std::string Register = "RegisterAll";
  std::string LibraryName = DynamicLibrary::getLibraryName(m_Name);

  void *R = DynamicLibrary::GetFunction(m_LibraryHandle, Register);
  if (!R) {
    std::string LastError = DynamicLibrary::GetLastError();
    m_Config.raise(diag::unable_to_find_plugin_func)
        << Register << LibraryName << LastError;
    return false;
  }

  std::string PluginFunc = "getPlugin";
  void *F = DynamicLibrary::GetFunction(m_LibraryHandle, PluginFunc);
  if (!F) {
    std::string LastError = DynamicLibrary::GetLastError();
    m_Config.raise(diag::unable_to_find_plugin_func)
        << PluginFunc << LibraryName << LastError;
    return false;
  }

  if (m_Module.getPrinter()->tracePlugins()) {
    m_Config.raise(diag::found_register_function) << Register << LibraryName;
    m_Config.raise(diag::found_function_for_plugintype)
        << PluginFunc << LibraryName;
  }

  std::string PluginCleanupFunc = "Cleanup";
  void *C = DynamicLibrary::GetFunction(m_LibraryHandle, PluginCleanupFunc);
  if (C && m_Module.getPrinter()->tracePlugins()) {
    m_Config.raise(diag::found_cleanup_function)
        << PluginCleanupFunc << LibraryName;
  }

  std::string PluginConfigFunc = "getPluginConfig";
  void *ConfigFunc =
      DynamicLibrary::GetFunction(m_LibraryHandle, PluginConfigFunc);
  if (ConfigFunc)
    m_Config.raise(diag::found_config_function)
        << PluginConfigFunc << LibraryName;

  m_RegisterFunc = (plugin::RegisterAllFuncT *)R;
  m_PluginFunc = (plugin::PluginFuncT *)F;
  m_PluginCleanupFunc = (plugin::PluginCleanupFuncT *)C;
  m_PluginConfigFunc = (plugin::PluginConfigFuncT *)ConfigFunc;

  return true;
}

bool Plugin::GetUserPlugin() {
  std::string LibraryName = DynamicLibrary::getLibraryName(m_Name);

  if (m_Module.getPrinter()->tracePlugins())
    m_Config.raise(diag::registering_all_functions);

  m_UserPlugin = (*m_PluginFunc)(getPluginType().c_str());
  if (!m_UserPlugin) {
    m_Config.raise(diag::unable_to_find_plugin_for_type)
        << getPluginType() << LibraryName;
    return false;
  }

  if (m_Module.getPrinter()->tracePlugins())
    m_Config.raise(diag::found_plugin_handler)
        << getPluginType() << LibraryName;

  return true;
}

void Plugin::GetUserPluginConfig() {
  if (!m_PluginConfigFunc)
    return;
  m_LinkerPluginConfig = (*m_PluginConfigFunc)(getPluginType().c_str());
}

bool Plugin::Init(eld::OutputTarWriter *outputTar) {
  Running r(this);
  if (!m_UserPlugin)
    return false;
  eld::RegisterTimer T("Init", m_Module.saveString(m_UserPlugin->GetName()),
                       Stats);
  if (m_Module.getPrinter()->tracePlugins())
    m_Config.raise(diag::initializing_plugin)
        << getPluginType() << DynamicLibrary::getLibraryName(m_Name)
        << m_UserPlugin->GetName();
  if (m_Module.getConfig().options().hasMappingFile())
    m_Options = m_Module.getConfig().getHashFromFile(m_Options);
  plugin::Plugin *P = llvm::cast<plugin::Plugin>(m_UserPlugin);
  P->Init(m_Options);
  if (outputTar && llvm::sys::fs::exists(m_Options))
    outputTar->createAndAddConfigFile(m_Options, m_Options);
  if (P->GetLastError()) {
    m_Config.raise(diag::plugin_has_error)
        << getPluginType() << DynamicLibrary::getLibraryName(m_Name)
        << P->GetLastErrorAsString() << m_Module.getStateStr() << "\n";
    return false;
  }
  return true;
}

bool Plugin::Run(std::vector<Plugin *> &Plugins) {
  if (!m_UserPlugin)
    return false;
  eld::RegisterTimer T("Run", m_Module.saveString(m_UserPlugin->GetName()),
                       Stats);
  if (m_Module.getPrinter()->tracePlugins())
    m_Config.raise(diag::running_plugin)
        << getPluginType() << DynamicLibrary::getLibraryName(m_Name)
        << m_UserPlugin->GetName();
  Plugins.push_back(this);
  Running r(this);
  plugin::Plugin *P = llvm::cast<plugin::Plugin>(m_UserPlugin);
  plugin::Plugin::Status S = P->Run(m_Module.getPrinter()->tracePlugins());
  if (S == plugin::Plugin::Status::ERROR || P->GetLastError()) {
    m_Config.raise(diag::plugin_has_error)
        << getPluginType() << DynamicLibrary::getLibraryName(m_Name)
        << P->GetLastErrorAsString() << m_Module.getStateStr() << "\n";
    return false;
  }
  return true;
}

bool Plugin::Cleanup() {
  Running r(this);
  if (!m_LibraryHandle)
    return false;
  if (!m_UserPlugin)
    return false;
  eld::RegisterTimer T("Cleanup", m_Module.saveString(m_UserPlugin->GetName()),
                       Stats);
  if (m_PluginCleanupFunc)
    (*m_PluginCleanupFunc)();
  return true;
}

bool Plugin::Unload(std::string LibraryName, void *Handle, Module *Module) {
  if (Handle) {
    DynamicLibrary::Unload(Handle);

    if (Module->getPrinter()->tracePlugins())
      Module->getConfig().raise(diag::unloaded_library) << LibraryName;
  }

  return true;
}

void Plugin::Reset() {
  m_LibraryHandle = nullptr;
  m_RegisterFunc = nullptr;
  m_PluginCleanupFunc = nullptr;
  m_PluginFunc = nullptr;
}

bool Plugin::Destroy() {
  Running r(this);
  if (!m_UserPlugin)
    return false;
  eld::RegisterTimer T("Destroy", m_Module.saveString(m_UserPlugin->GetName()),
                       Stats);
  if (m_Module.getPrinter()->tracePlugins())
    m_Config.raise(diag::plugin_destroy) << getPluginType();
  plugin::Plugin *P = llvm::cast<plugin::Plugin>(m_UserPlugin);
  P->Destroy();
  if (P->GetLastError()) {
    m_Config.raise(diag::plugin_has_error)
        << getPluginType() << DynamicLibrary::getLibraryName(m_Name)
        << P->GetLastErrorAsString() << m_Module.getStateStr() << "\n";
    return false;
  }

  return true;
}

bool Plugin::Check() {
  Running r(this);
  if (!m_UserPlugin)
    return false;
  eld::RegisterTimer T("Check", m_Module.saveString(m_UserPlugin->GetName()),
                       Stats);
  if (m_UserPlugin->getType() != m_Type) {
    m_Config.raise(diag::plugin_mismatch)
        << DynamicLibrary::getLibraryName(m_Name) << getPluginType();
    return false;
  }

  // Check for plugin version.
  unsigned int pluginMajor = 0, pluginMinor = 0;

  void *R = DynamicLibrary::GetFunction(m_LibraryHandle, "getPluginAPIVersion");
  if (R) {
    plugin::PluginGetAPIVersionFuncT *GetAPIVersionFunc =
        (plugin::PluginGetAPIVersionFuncT *)R;
    GetAPIVersionFunc(&pluginMajor, &pluginMinor);
  } else {
    m_Config.raise(diag::error_plugin_no_api_version)
        << m_UserPlugin->GetName() << DynamicLibrary::getLibraryName(m_Name)
        << pluginMajor << pluginMinor;
    return false;
  }

  if (m_Module.getPrinter()->tracePlugins())
    m_Config.raise(diag::note_plugin_version)
        << pluginMajor << pluginMinor << DynamicLibrary::getLibraryName(m_Name)
        << getPluginType();

  if (pluginMajor != LINKER_PLUGIN_API_MAJOR_VERSION ||
      pluginMinor > LINKER_PLUGIN_API_MINOR_VERSION) {
    m_Config.raise(diag::wrong_version)
        << DynamicLibrary::getLibraryName(m_Name) << getPluginType()
        << pluginMajor << pluginMinor << LINKER_PLUGIN_API_MAJOR_VERSION
        << LINKER_PLUGIN_API_MINOR_VERSION;
    return false;
  }
  return true;
}

void Plugin::createRelocationVector(uint32_t Num, bool State) {
  // If there is no plugin config, there is no need to set the
  // relocation vector.
  if (!m_LinkerPluginConfig)
    return;
  RelocBitVector = eld::make<llvm::BitVector>(Num, State);
  SlowPathRelocBitVector = eld::make<llvm::BitVector>(Num, State);
  RelocPayLoadMap.reserve(Num);
}

void Plugin::initializeLinkerPluginConfig() {
  if (!m_LinkerPluginConfig)
    return;
  auto InitCallBack =
      std::bind(&plugin::LinkerPluginConfig::Init, m_LinkerPluginConfig);
  InitCallBack();
}

void Plugin::callReloc(uint32_t RelocType, Relocation *R) {
  if (!m_LinkerPluginConfig)
    return;
  if (!isRelocTypeRegistered(RelocType, R))
    return;
  auto RelocCallBack = std::bind(&plugin::LinkerPluginConfig::RelocCallBack,
                                 m_LinkerPluginConfig, std::placeholders::_1);
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
  return m_LinkerPluginConfig;
}

std::string Plugin::getPluginName() const {
  if (!m_UserPlugin)
    return "";
  return m_UserPlugin->GetName();
}

std::string Plugin::GetDescription() const {
  if (!m_UserPlugin)
    return "";
  return m_UserPlugin->GetDescription();
}

bool Plugin::RegisterAll() const {
  if (!m_RegisterFunc)
    return false;
  std::string libraryName = DynamicLibrary::getLibraryName(m_Name);
  m_Config.raise(diag::calling_function_from_dynamic_lib)
      << "RegisterAll" << libraryName;
  (*m_RegisterFunc)();
  return true;
}

Plugin::~Plugin() {
  for (void *Handle : LibraryHandles)
    DynamicLibrary::Unload(Handle);
}

eld::Expected<void> Plugin::verifyFragmentMovements() const {
  for (const auto &unbalancedRemove :
       m_UnbalancedFragmentMoves.UnbalancedRemoves) {
    const Fragment *F = unbalancedRemove.first;
    const RuleContainer *R = unbalancedRemove.second;
    // FIXME: We return on first error currently. It will be nice if we can
    // report all the errors at-once. For example, if there are 5 fragments
    // which are removed but not inserted, then it would be nice if we see
    // 5 errors once for each such fragment, but currently, we will only see
    // the error for the first fragment.
    return std::make_unique<plugin::DiagnosticEntry>(
        diag::err_chunk_removed_but_not_inserted,
        std::vector<std::string>{
            F->getOwningSection()->getDecoratedName(m_Config.options()),
            R->getAsString(), getPluginName()});
  }
  for (const auto &unbalancedAdd : m_UnbalancedFragmentMoves.UnbalancedAdds) {
    const Fragment *F = unbalancedAdd.first;
    if (F->originatesFromPlugin(m_Module))
      continue;
    const RuleContainer *R = unbalancedAdd.second;
    return std::make_unique<plugin::DiagnosticEntry>(
        diag::err_chunk_inserted_but_not_removed,
        std::vector<std::string>{
            F->getOwningSection()->getDecoratedName(m_Config.options()),
            R->getAsString(), getPluginName()});
  }
  return {};
}

void Plugin::recordFragmentAdd(RuleContainer *R, Fragment *F) {
  if (m_UnbalancedFragmentMoves.UnbalancedRemoves.count(F)) {
    m_UnbalancedFragmentMoves.UnbalancedRemoves.erase(F);
    return;
  }
  ASSERT(!m_UnbalancedFragmentMoves.UnbalancedAdds.count(F),
         "Immediate error must be returned by the LinkerWrapper for Repeated "
         "adds of the same fragment");
  m_UnbalancedFragmentMoves.UnbalancedAdds[F] = R;
}

void Plugin::recordFragmentRemove(RuleContainer *R, Fragment *F) {
  if (m_UnbalancedFragmentMoves.UnbalancedAdds.count(F)) {
    m_UnbalancedFragmentMoves.UnbalancedAdds.erase(F);
    return;
  }
  m_UnbalancedFragmentMoves.UnbalancedRemoves[F] = R;
}

eld::Expected<std::pair<void *, std::string>>
Plugin::loadLibrary(const std::string &LibraryName) {
  const eld::sys::fs::Path *Library =
      m_Module.getConfig().searchDirs().findLibrary(
          "plugin loadLibrary", LibraryName, m_Module.getConfig());
  if (!Library) {
    Library = m_Module.getConfig().searchDirs().findLibrary(
        "plugin loadLibrary", DynamicLibrary::getLibraryName(LibraryName),
        m_Module.getConfig());
    if (!Library)
      return std::make_unique<plugin::DiagnosticEntry>(
          diag::unable_to_find_library, std::vector<std::string>{LibraryName});
  }

  std::string LibraryPath = Library->getFullPath();

  void *LibraryHandle = DynamicLibrary::Load(LibraryPath);

  if (!LibraryHandle) {
    return std::make_unique<plugin::DiagnosticEntry>(
        diag::unable_to_load_library,
        std::vector<std::string>{DynamicLibrary::GetLastError()});
  }

  LibraryHandles.push_back(LibraryHandle);

  if (m_Module.getOutputTarWriter()) {
    m_Module.getOutputTarWriter()->createAndAddFile(
        LibraryPath, LibraryPath, MappingFile::Kind::SharedLibrary,
        /*isLTOObject*/ false);
  }
  return std::make_pair(LibraryHandle, LibraryPath);
}

void Plugin::callInitHook() {
  plugin::LinkerPlugin *P = llvm::cast<plugin::LinkerPlugin>(m_UserPlugin);
  RegisterTimer T("Init", m_Module.saveString(m_UserPlugin->GetName()), Stats);
  if (m_Module.getPrinter()->tracePlugins())
    m_Config.raise(diag::trace_plugin_init) << getPluginName();
  P->Init(m_Options);
}

void Plugin::callDestroyHook() {
  plugin::LinkerPlugin *P = llvm::cast<plugin::LinkerPlugin>(m_UserPlugin);
  RegisterTimer T("Destroy", m_Module.saveString(m_UserPlugin->GetName()), Stats);
  if (m_Module.getPrinter()->tracePlugins())
    m_Config.raise(diag::trace_plugin_destroy) << getPluginName();
  P->Destroy();
}

void Plugin::registerCommandLineOption(
    const std::string &option, bool hasValue,
    const CommandLineOptionSpec::OptionHandlerType &optionHandler) {
  if (m_Module.getPrinter()->tracePlugins()) {
    if (hasValue)
      m_Config.raise(diag::trace_plugin_register_opt_with_val)
          << getPluginName() << option;
    else
      m_Config.raise(diag::trace_plugin_register_opt_without_val)
          << getPluginName() << option;
  }
  PluginCommandLineOptions.push_back({option, hasValue, optionHandler});
}

void Plugin::callCommandLineOptionHandler(
    const std::string &option, const std::optional<std::string> &val,
    const CommandLineOptionSpec::OptionHandlerType &optionHandler) {
  RegisterTimer T("Command-line option handler",
                  m_Module.saveString(m_UserPlugin->GetName()), Stats);
  if (val)
    m_Config.raise(diag::verbose_calling_plugin_opt_handler_with_val)
        << getPluginName() << option << val.value();
  else
    m_Config.raise(diag::verbose_calling_plugin_opt_handler_without_val)
        << getPluginName() << option;
  optionHandler(option, val);
}

void Plugin::callVisitSectionsHook(InputFile &IF) {
  plugin::LinkerPlugin *P = llvm::cast<plugin::LinkerPlugin>(m_UserPlugin);
  RegisterTimer T("VisitSections", m_Module.saveString(m_UserPlugin->GetName()), Stats);
  if (m_Module.getPrinter()->tracePlugins())
    m_Config.raise(diag::trace_plugin_visit_sections)
        << getPluginName() << IF.getInput()->decoratedPath();
  P->VisitSections(plugin::InputFile(&IF));
}

void Plugin::callVisitSymbolHook(LDSymbol *sym, llvm::StringRef symName,
                                 const SymbolInfo &symInfo) {
  plugin::LinkerPlugin *P = llvm::cast<plugin::LinkerPlugin>(m_UserPlugin);
  RegisterTimer T("VisitSymbol", m_Module.saveString(m_UserPlugin->GetName()),
                  Stats);
  if (m_Module.getPrinter()->tracePlugins())
    m_Config.raise(diag::trace_plugin_visit_symbol)
        << getPluginName() << symName;
  std::unique_ptr<SymbolInfo> upSymInfo = std::make_unique<SymbolInfo>(symInfo);
  P->VisitSymbol(
      plugin::InputSymbol(sym, std::string_view(symName.data(), symName.size()),
                          std::move(upSymInfo)));
}

void Plugin::callActBeforeRuleMatchingHook() {
  llvm::StringRef hookName = "ActBeforeRuleMatching";
  plugin::LinkerPlugin *P = llvm::cast<plugin::LinkerPlugin>(m_UserPlugin);
  RegisterTimer T(hookName, m_Module.saveString(m_UserPlugin->GetName()),
                  Stats);
  if (m_Module.getPrinter()->tracePlugins())
    m_Config.raise(diag::trace_plugin_hook) << getPluginName() << hookName;
  P->ActBeforeRuleMatching();
}

void Plugin::callActBeforeSectionMergingHook() {
  llvm::StringRef hookName = "ActBeforeSectionMerging";
  plugin::LinkerPlugin *P = llvm::cast<plugin::LinkerPlugin>(m_UserPlugin);
  RegisterTimer T(hookName, m_Module.saveString(m_UserPlugin->GetName()),
                  Stats);
  if (m_Module.getPrinter()->tracePlugins())
    m_Config.raise(diag::trace_plugin_hook) << getPluginName() << hookName;
  P->ActBeforeSectionMerging();
}

void Plugin::callActBeforePerformingLayoutHook() {
  llvm::StringRef hookName = "ActBeforePerformingLayout";
  plugin::LinkerPlugin *P = llvm::cast<plugin::LinkerPlugin>(m_UserPlugin);
  RegisterTimer T(hookName, m_Module.saveString(m_UserPlugin->GetName()),
                  Stats);
  if (m_Module.getPrinter()->tracePlugins())
    m_Config.raise(diag::trace_plugin_hook) << getPluginName() << hookName;
  P->ActBeforePerformingLayout();
}

void Plugin::callActBeforeWritingOutputHook() {
  llvm::StringRef hookName = "ActBeforeWritingOutput";
  plugin::LinkerPlugin *P = llvm::cast<plugin::LinkerPlugin>(m_UserPlugin);
  RegisterTimer T(hookName, m_Module.saveString(m_UserPlugin->GetName()),
                  Stats);
  if (m_Module.getPrinter()->tracePlugins())
    m_Config.raise(diag::trace_plugin_hook) << getPluginName() << hookName;
  P->ActBeforeWritingOutput();
}
