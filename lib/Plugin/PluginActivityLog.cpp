// PluginActivityLog.cpp------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===-------------------------------------------------------------------------===//
#include "eld/Plugin/PluginActivityLog.h"
#include "eld/Config/Version.h"
#include "eld/Core/LinkState.h"
#include "eld/Core/LinkerScript.h"
#include "eld/Fragment/Fragment.h"
#include "eld/Object/RuleContainer.h"
#include "eld/Plugin/PluginOp.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Script/Plugin.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/JSON.h"

using namespace eld;

bool PluginActivityLog::print(llvm::StringRef Filename) const {
  std::error_code EC;
  llvm::raw_fd_ostream OS(Filename, EC, llvm::sys::fs::OF_None);
  if (EC)
    return false;
  llvm::json::Array Ops = getPluginActivities();
  llvm::json::Array PluginsInfo = getPluginsInfo();
  llvm::json::Object Root{{"PluginActivities", std::move(Ops)}};
  Root["LinkerRevision"] = getELDRevision();
  Root["Plugins"] = std::move(PluginsInfo);
  OS << llvm::formatv("{0:2}\n", llvm::json::Value(std::move(Root)));
  return true;
}

llvm::json::Object PluginActivityLog::toJSON(const PluginOp &Op) const {
  switch (Op.getPluginOpType()) {
  case PluginOp::ChangeOutputSection:
    return PluginActivityLog::toJSON(
        static_cast<const ChangeOutputSectionPluginOp &>(Op));
  case PluginOp::AddChunk:
    return PluginActivityLog::toJSON(static_cast<const AddChunkPluginOp &>(Op));
  case PluginOp::RemoveChunk:
    return PluginActivityLog::toJSON(
        static_cast<const RemoveChunkPluginOp &>(Op));
  case PluginOp::UpdateChunks:
    return PluginActivityLog::toJSON(
        static_cast<const UpdateChunksPluginOp &>(Op));
  case PluginOp::RemoveSymbol:
    return PluginActivityLog::toJSON(
        static_cast<const RemoveSymbolPluginOp &>(Op));
  case PluginOp::RelocationData:
    return PluginActivityLog::toJSON(
        static_cast<const RelocationDataPluginOp &>(Op));
  case PluginOp::UpdateLinkStat:
    return PluginActivityLog::toJSON(
        static_cast<const UpdateLinkStatsPluginOp &>(Op));
  case PluginOp::UpdateRule:
    return PluginActivityLog::toJSON(
        static_cast<const UpdateRulePluginOp &>(Op));
  case PluginOp::ResetOffset:
    return PluginActivityLog::toJSON(
        static_cast<const ResetOffsetPluginOp &>(Op));
  case PluginOp::UpdateLinkState:
    return PluginActivityLog::toJSON(
        static_cast<const UpdateLinkStateOp &>(Op));
  default:
    return llvm::json::Object{{"type", "Unknown"}};
  }
}

// Overloads for each concrete PluginOp to extract a minimal JSON payload.
llvm::json::Object
PluginActivityLog::toJSON(const ChangeOutputSectionPluginOp &Op) const {
  auto JSONObj = getBaseActivityJSONObject(Op);
  return JSONObj;
}

llvm::json::Object PluginActivityLog::toJSON(const AddChunkPluginOp &Op) const {
  auto JSONObj = getBaseActivityJSONObject(Op);
  JSONObj["Rule"] = Op.getRule()->getAsString();
  JSONObj["Fragment"] = Op.getFrag()->str(Options);
  return JSONObj;
}

llvm::json::Object
PluginActivityLog::toJSON(const RemoveChunkPluginOp &Op) const {
  auto JSONObj = getBaseActivityJSONObject(Op);
  JSONObj["Rule"] = Op.getRule()->getAsString();
  JSONObj["Fragment"] = Op.getFrag()->str(Options);
  return JSONObj;
}

llvm::json::Object
PluginActivityLog::toJSON(const UpdateChunksPluginOp &Op) const {
  auto JSONObj = getBaseActivityJSONObject(Op);
  JSONObj["Info"] =
      (Op.getType() == UpdateChunksPluginOp::Type::Start ? "Start" : "End");
  return JSONObj;
}

llvm::json::Object
PluginActivityLog::toJSON(const RemoveSymbolPluginOp &Op) const {
  auto JSONObj = getBaseActivityJSONObject(Op);
  JSONObj["Symbol"] = Op.getRemovedSymbol()->getName();
  return JSONObj;
}

// FIXME: This needs to be completed.
llvm::json::Object
PluginActivityLog::toJSON(const RelocationDataPluginOp &Op) const {
  auto JSONObj = getBaseActivityJSONObject(Op);
  return JSONObj;
}

llvm::json::Object
PluginActivityLog::toJSON(const UpdateLinkStatsPluginOp &Op) const {
  auto JSONObj = getBaseActivityJSONObject(Op);
  JSONObj["Stat"] = Op.getStatName();
  return JSONObj;
}

llvm::json::Object
PluginActivityLog::toJSON(const UpdateRulePluginOp &Op) const {
  auto JSONObj = getBaseActivityJSONObject(Op);
  JSONObj["Rule"] = Op.getRule()->getAsString();
  JSONObj["Section"] = Op.getSection()->getDecoratedName(Options);
  return JSONObj;
}

llvm::json::Object
PluginActivityLog::toJSON(const ResetOffsetPluginOp &Op) const {
  auto JSONObj = getBaseActivityJSONObject(Op);
  JSONObj["OutputSection"] = Op.getOutputSection()->name();
  JSONObj["OldOffset"] = Op.getOldOffset();
  return JSONObj;
}

llvm::json::Object
PluginActivityLog::toJSON(const UpdateLinkStateOp &Op) const {
  auto JSONObj = getBaseActivityJSONObject(Op);
  JSONObj["NewState"] = getLinkStateStrRef(Op.getNewState());
  return JSONObj;
}

llvm::json::Object
PluginActivityLog::getBaseActivityJSONObject(const PluginOp &Op) const {
  return llvm::json::Object{{"Plugin", Op.getPluginName()},
                            {"Type", Op.getPluginOpTypeStrRef()},
                            {"Annotation", Op.getAnnotation()}};
}

llvm::json::Array PluginActivityLog::getPluginActivities() const {
  llvm::json::Array Ops;
  for (const auto &OpRef : PluginOperations) {
    const PluginOp &Op = OpRef.get();
    Ops.push_back(toJSON(Op));
  }
  return Ops;
}

llvm::json::Array PluginActivityLog::getPluginsInfo() const {
  llvm::json::Array PluginsInfo;
  for (const auto *P : AllPlugins) {
    llvm::errs() << "P->getPluginName(): " << P->getPluginName() << "\n";
    llvm::json::Object PInfo;
    PInfo["Name"] = P->getPluginName();
    PInfo["Autloaded"] = P->isDefaultPlugin();
    PInfo["Library"] = getPluginLibraryPath(P->getLibraryHandle());
    PInfo["RegisteredCommandLineOptions"] = llvm::json::Array();
    for (const auto &Opt : P->getPluginCommandLineOptions()) {
      llvm::json::Object OptInfo;
      OptInfo["Option"] = Opt.Option;
      OptInfo["HasValue"] = Opt.HasValue;
      auto &PluginCommandLineOptionsInfo =
          *PInfo.getArray("RegisteredCommandLineOptions");
      PluginCommandLineOptionsInfo.push_back(std::move(OptInfo));
    }
    PluginsInfo.push_back(std::move(PInfo));
  }
  return PluginsInfo;
}
