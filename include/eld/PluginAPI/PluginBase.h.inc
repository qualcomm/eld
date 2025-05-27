//===- PluginBase.h.inc----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef LINKER_PLUGIN_H
#define LINKER_PLUGIN_H

#include "Defines.h"
#include "LinkerPluginConfig.h"
#include "LinkerWrapper.h"
#include "PluginADT.h"
#include <string>
#include <vector>

namespace eld {
  class LinkerScript;
}

namespace eld::plugin {

class DLL_A_EXPORT LinkerPluginConfig;

// PluginType.
typedef const char *PluginType;

// Register Function.
typedef bool RegisterAllFuncT();

class LinkerWrapper;

#define LINKER_PLUGIN_API_MAJOR_VERSION @LINKER_PLUGIN_API_MAJOR_VERSION@
#define LINKER_PLUGIN_API_MINOR_VERSION @LINKER_PLUGIN_API_MINOR_VERSION@

#ifdef _WIN32
#pragma comment(user, "Plugin built with tools @LLVM_VENDOR_VERSION@")
#else
__asm__(".section .comment, \"MS\",@progbits,1\n\t"
        ".string \"Plugin built with tools @LLVM_VENDOR_VERSION@\"");
#endif

class DLL_A_EXPORT PluginBase {
public:
  /// Plugin interface types.
  enum Type : uint8_t {
    ControlFileSize,
    ControlMemorySize,
    SectionIterator,
    SectionMatcher,
    OutputSectionIterator,
    LinkerPlugin
  };

  explicit PluginBase(Type pluginType, const std::string &pluginName)
      : PluginType(pluginType), PluginName(pluginName) {}

  virtual ~PluginBase() {}

  /// Returns the unique ID associated with the plugin.
  uint32_t getPluginID() const;

  /// Returns the type of the plugin.
  Type getType() const { return PluginType; }

  LinkerWrapper *getLinker() const { return Linker; }

  /// Returns the name of the plugin.
  virtual std::string GetName() = 0;

  /// Returns plugin description.
  virtual std::string GetDescription() { return GetName(); }

protected:
  /* Plugin Type */
  Type PluginType;

  /* Plugin Name */
  std::string PluginName;

private:
  /// Set LinkerWrapper for the plugin. This is set for all the plugins
  /// by the linker during loading plugins. LinkerWrapper is the communication
  /// bridge between the linker plugin and the linker.
  ///
  /// Each plugin is assigned it's own LinkerWrapper that is not shared by any
  /// other plugin. Assigning a LinkerWrapper of a plugin to some other plugin
  /// can result in unexpected results and misleading diagnostics. Therefore,
  /// this option should be used carefully.
  friend class eld::LinkerScript;
  void setLinkerWrapper(plugin::LinkerWrapper *L) { Linker = L; }

  /* Linker Wrapper */
  plugin::LinkerWrapper *Linker = nullptr;
};

class DLL_A_EXPORT Plugin : public PluginBase {
public:
  /// Default error codes to signify SUCCESS or FAILURE. 0 indicates success and
  /// any other integer signifies failure.
  enum Status : uint8_t { SUCCESS = 0, ERROR = 1 };

  /* Constructor */
  explicit Plugin(Type T, std::string Name)
      : PluginBase(T, Name), EC(SUCCESS) {}

  static bool classof(const PluginBase *P) {
    return P->getType() != PluginBase::Type::LinkerPlugin;
  }

  /// Init callback hook is generally used for initialization and preparation purposes.
  /// It is typically used to read config file that is passed as \em Option and prepare
  /// the plugin for its operation.
  ///
  /// Options is (optionally) specified by the user at a plugin invocation
  /// command.
  ///
  /// Using linker script plugin invocation command, 'Options' is passed as follows:
  ///
  /// \code
  /// <PluginInterfaceKeyword>("LibraryName", "PluginName" [, "Option"])
  /// \endcode
  ///
  /// Using plugin configuration file, 'Options' is passed as follows:
  ///
  /// \code
  /// ---
  /// GlobalPlugins:
  ///  - Type: PluginInterfaceType
  ///    Name: PluginName
  ///    Library: LibraryName
  ///    Options: Option
  /// \endcode
  virtual void Init(std::string Option) = 0;

  /// The bulk of the plugin functionality is generally carried out by this
  /// function.
  virtual Status Run(bool Verbose) = 0;

  /// Destroy callback hook is generally used for finalization and clean-up
  /// tasks. Destroy hook is the last hook that is called for any plugin. Any
  /// data managed by the plugin should be freed here.
  virtual void Destroy() = 0;

  /// Returns the last error, a value of 0 denotes there was no error.
  virtual uint32_t GetLastError() = 0;

  /// Returns the last error as string.
  virtual std::string GetLastErrorAsString() = 0;

  /* Error code */
  uint32_t EC;
};

// Plugin API check
typedef void PluginGetAPIVersionFuncT(unsigned int *, unsigned int *);

// Handler
typedef Plugin *PluginFuncT(PluginType P);

// Cleanup Handler
typedef void *PluginCleanupFuncT();

// LinkerPluginConfig
typedef LinkerPluginConfig *PluginConfigFuncT(PluginType T);
}

/// A helper macro for registering a plugin.
/// It simply provides good defaults for RegisterAll, getPlugin
/// and Cleanup functions.
/// \note It can be only be used once in a source file.
#define ELD_REGISTER_PLUGIN(PluginType)                                        \
  PluginType *ThisPlugin = nullptr;                                            \
                                                                               \
  extern "C" {                                                                 \
  bool DLL_A_EXPORT RegisterAll() {                                            \
    ThisPlugin = new PluginType();                                             \
    return true;                                                               \
  }                                                                            \
                                                                               \
  eld::plugin::PluginBase DLL_A_EXPORT *getPlugin(const char *T) {             \
    return ThisPlugin;                                                         \
  }                                                                            \
                                                                               \
  void DLL_A_EXPORT Cleanup() {                                                \
    if (ThisPlugin)                                                            \
      delete ThisPlugin;                                                       \
    ThisPlugin = nullptr;                                                      \
  }                                                                            \
  }
#endif
