//===- PluginData.h--------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
#ifndef ELD_PLUGIN_PLUGINACTIVITYLOG_H
#define ELD_PLUGIN_PLUGINACTIVITYLOG_H
#include "eld/Plugin/PluginOp.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/JSON.h"
#include <functional>

namespace eld {
class GeneralOptions;

class PluginActivityLog {
public:
  PluginActivityLog(const GeneralOptions &GO) : Options (GO) {}

  void addPluginOperation(PluginOp &Op) {
    PluginOperations.push_back(std::ref(Op));
  }

  // Emit a JSON dump of collected plugin operations to the given file.
  bool print(llvm::StringRef Filename) const;

private:
  // Convert a PluginOp to JSON based on its dynamic type.
  llvm::json::Object toJSON(const PluginOp &Op) const;

  // Overloads for each concrete PluginOp to extract a minimal JSON payload.
  llvm::json::Object toJSON(const ChangeOutputSectionPluginOp &P) const;

  llvm::json::Object toJSON(const AddChunkPluginOp &P) const;

  llvm::json::Object toJSON(const RemoveChunkPluginOp &P) const;

  llvm::json::Object toJSON(const UpdateChunksPluginOp &P) const;

  llvm::json::Object toJSON(const RemoveSymbolPluginOp &P) const;

  llvm::json::Object toJSON(const RelocationDataPluginOp &P) const;

  llvm::json::Object toJSON(const UpdateLinkStatsPluginOp &P) const;

  llvm::json::Object toJSON(const UpdateRulePluginOp &P) const;

  llvm::json::Object toJSON(const ResetOffsetPluginOp &P) const;

  llvm::json::Object toJSON(const UpdateLinkStateOp &P) const;

  llvm::json::Object getBaseActivityJSONObject(const PluginOp &Op) const;

private:
  std::vector<std::reference_wrapper<PluginOp>> PluginOperations;
  const GeneralOptions &Options;
};
} // namespace eld

#endif
