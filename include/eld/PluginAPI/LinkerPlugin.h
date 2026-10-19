//===- LinkerPlugin.h------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#ifndef ELD_PLUGINAPI_LINKERPLUGIN_H
#define ELD_PLUGINAPI_LINKERPLUGIN_H
#include "PluginBase.h"

namespace llvm {

namespace lto {
struct Config;
}
} // namespace llvm

namespace eld::plugin {

class DLL_A_EXPORT LinkerPlugin : public PluginBase {
public:
  LinkerPlugin(const std::string &name)
      : PluginBase(PluginBase::Type::LinkerPlugin, name) {}

  static bool classof(const PluginBase *P) {
    return P->getType() == plugin::PluginBase::Type::LinkerPlugin;
  }

  std::string GetName() override final { return PluginName; }

  virtual void Init(const std::string &options) {}

  virtual void Destroy() {}

  /// The VisitSections hook is invoked for each input-file. It is called
  /// immediately after the sections of an InputFile are created.
  ///
  /// Use InputFile::getSections() to get the sections.
  virtual void VisitSections(plugin::InputFile IF) {}

  /// The VisitSymbol hook is invoked for each non-local symbol from a
  /// relocatable object file.
  ///
  /// This hook is disabled by default. Use 'LinkerWrapper::enableVisitSymbol()'
  /// to enable this hook.
  virtual void VisitSymbol(plugin::InputSymbol S) {}

  /// The ActBeforeRuleMatching is invoked just before linker script
  /// rule-matching.
  ///
  /// Link pipeline flow:
  /// Read inputs --> linker script rule-matching

  virtual void ActBeforeRuleMatching() {}

  /// The ActBeforeSectionMerging is invoked just before section merging step in
  /// the link pipeline.
  ///
  /// Section merging merges the input sections content and puts them into their
  /// corresponding output sections. One important thing to note is that chunks
  /// cannot be moved around after section merging.
  ///
  ///
  /// Link pipeline flow:
  /// Read inputs --> linker script rule-matching --> garbage-collection -->
  /// section merging.
  virtual void ActBeforeSectionMerging() {}

  /// The ActBeforePerformingLayout is invoked just before performing layout
  /// step in the link pipeline.
  ///
  /// The ActBeforePerformingLayout step is responsible for assigning addresses
  /// to output sections.
  ///
  /// Link pipeline flow:
  /// Read inputs --> linker script rule-matching --> garbage-collection -->
  /// section merging --> performing layout.
  virtual void ActBeforePerformingLayout() {}

  /// The ActBeforeWritingOutput is invoked just before writing the output
  /// image.
  ///
  /// Link pipeline flow:
  /// Read inputs --> linker script rule-matching --> garbage-collection -->
  /// section merging --> performing layout --> writing output.
  virtual void ActBeforeWritingOutput() {}

  typedef uint64_t LTOModuleHash;

  /// Give the LTO plugin a chance to rewrite the module hash.
  ///
  /// This hook is called during reading bitcode files (Initializing state).
  /// This hook is called for the LTO plugin only.
  virtual std::optional<uint64_t>
  OverrideLTOModuleHash(plugin::BitcodeFile BitcodeFile,
                        const std::string &Name) {
    return {};
  }

  /// Override reading symbols for bitcode files. The linker will
  /// skip reading bitcode symbols and call this hook instead if
  /// the LTO plugin is loaded.
  ///
  /// This hook is called during reading bitcode files, after bitcode sections
  /// and input file creation (Initializing state).
  ///
  /// This hook is called for the LTO plugin only.
  virtual void ReadSymbols(plugin::InputFile IF) {}

  /// Modify compile options represented as strings. The LTO plugin
  /// can add new or update existing options.
  ///
  /// This hook is called in the beginning of the LTO compilation (BeforeLayout
  /// state). This hook is called for the LTO plugin only.
  virtual void ModifyLTOOptions(llvm::lto::Config &Config,
                                std::vector<std::string> &Options) {}

  /// Do any action before LTO compilation is invoked. The LTO plugin
  /// can also modify the LTO configuration.
  ///
  /// This hook is called in the beginning of the LTO compilation,
  /// after calling ModifyLTOOptions (BeforeLayout state).
  /// This hook is called for the LTO plugin only.
  virtual void ActBeforeLTO(llvm::lto::Config &Config) {}

  /// The VisitInput hook is invoked for each input file. It is called
  /// immediately after an InputFile component is created and passes initial
  /// validation, before its sections or symbols are parsed.
  ///
  /// Use plugin::InputFile methods to inspect file properties, resolved path,
  /// size, format kind, and raw buffer contents.
  /// Note: getSections() and getSymbols() are empty during VisitInput.
  virtual void VisitInput(plugin::InputFile IF) {}
};
} // namespace eld::plugin
#endif