//===- DiagnosticInfos.cpp-------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
//
//                     The MCLinker Project
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#include "eld/Diagnostics/DiagnosticInfos.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Diagnostics/Diagnostic.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Diagnostics/DiagnosticPrinter.h"
#include "eld/PluginAPI/DiagnosticEntry.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/DataTypes.h"
#include <algorithm>
#include <memory>

using namespace eld;

namespace {

struct DiagStaticInfo {
public:
  DiagnosticEngine::DiagIDType BaseDiagID;
  llvm::StringRef DescriptionStr;

public:
  llvm::StringRef getDescription() const { return DescriptionStr; }

  bool operator<(const DiagStaticInfo &PRhs) const {
    return (BaseDiagID < PRhs.BaseDiagID);
  }
};

} // namespace

static const DiagStaticInfo DiagCommonInfo[] = {
#define DIAG(diagName, severity, formatStr)                                    \
  {DiagnosticEngine::getBaseDiagID(Diag::diagName), formatStr},
// PluginDiags.inc file should always be first!
// This is to ensure that Plugin Diags IDs are consistent in
// eld::Diag:: and plugin::Diagnostic:: namespaces.
// clang-format off
#include "eld/Diagnostics/PluginDiags.inc"
// clang-format on
#include "eld/Diagnostics/DiagAttribute.inc"
#include "eld/Diagnostics/DiagBackends.inc"
#include "eld/Diagnostics/DiagCommonKinds.inc"
#include "eld/Diagnostics/DiagGOTPLT.inc"
#include "eld/Diagnostics/DiagLDScript.inc"
#include "eld/Diagnostics/DiagLTO.inc"
#include "eld/Diagnostics/DiagLayouts.inc"
#include "eld/Diagnostics/DiagPlugin.inc"
#include "eld/Diagnostics/DiagReaders.inc"
#include "eld/Diagnostics/DiagRelocations.inc"
#include "eld/Diagnostics/DiagStats.inc"
#include "eld/Diagnostics/DiagSymbolResolutions.inc"
#include "eld/Diagnostics/DiagTraceAssignments.inc"
#include "eld/Diagnostics/DiagTraceFiles.inc"
#include "eld/Diagnostics/DiagTraceGC.inc"
#include "eld/Diagnostics/DiagTraceSymbols.inc"
#include "eld/Diagnostics/DiagTraceTrampolines.inc"
#include "eld/Diagnostics/DiagSymbolVersioning.inc"
#include "eld/Diagnostics/DiagVerbose.inc"
#include "eld/Diagnostics/DiagWriters.inc"
#undef DIAG
    {0, ""}};

static const unsigned int DiagCommonInfoSize =
    sizeof(DiagCommonInfo) / sizeof(DiagCommonInfo[0]) - 1;

static const DiagStaticInfo DiagLoCInfo[] = {
#define DIAG(diagName, severity, formatStr)                                    \
  {DiagnosticEngine::getBaseDiagID(Diag::diagName), formatStr},
// PluginDiags.inc file should always be first!
// This is to ensure that Plugin Diags IDs are consistent in
// eld::Diag:: and plugin::Diagnostic:: namespaces.
// clang-format off
#include "eld/Diagnostics/PluginDiags.inc"
// clang-format on
#include "eld/Diagnostics/DiagAttribute.inc"
#include "eld/Diagnostics/DiagBackends.inc"
#include "eld/Diagnostics/DiagCommonKinds.inc"
#include "eld/Diagnostics/DiagGOTPLT.inc"
#include "eld/Diagnostics/DiagLDScript.inc"
#include "eld/Diagnostics/DiagLTO.inc"
#include "eld/Diagnostics/DiagLayouts.inc"
#include "eld/Diagnostics/DiagPlugin.inc"
#include "eld/Diagnostics/DiagReaders.inc"
#include "eld/Diagnostics/DiagRelocations.inc"
#include "eld/Diagnostics/DiagStats.inc"
#include "eld/Diagnostics/DiagSymbolResolutions.inc"
#include "eld/Diagnostics/DiagTraceAssignments.inc"
#include "eld/Diagnostics/DiagTraceFiles.inc"
#include "eld/Diagnostics/DiagTraceGC.inc"
#include "eld/Diagnostics/DiagTraceSymbols.inc"
#include "eld/Diagnostics/DiagTraceTrampolines.inc"
#include "eld/Diagnostics/DiagSymbolVersioning.inc"
#include "eld/Diagnostics/DiagVerbose.inc"
#include "eld/Diagnostics/DiagWriters.inc"
#undef DIAG
    {0, ""}};

static const unsigned int DiagLoCInfoSize =
    sizeof(DiagLoCInfo) / sizeof(DiagLoCInfo[0]) - 1;

static const DiagStaticInfo *getDiagInfo(DiagnosticEngine::DiagIDType ID,
                                         bool PInLoC = false) {
  const DiagStaticInfo *StaticInfo = (PInLoC) ? DiagLoCInfo : DiagCommonInfo;
  unsigned int InfoSize = (PInLoC) ? DiagLoCInfoSize : DiagCommonInfoSize;
  DiagnosticEngine::DiagIDType BaseDiagId = DiagnosticEngine::getBaseDiagID(ID);
  DiagStaticInfo Key = {BaseDiagId, ""};
  // FIXME: Why don't we simply use static_info[baseDiagID] here?
  const DiagStaticInfo *Result =
      std::lower_bound(StaticInfo, StaticInfo + InfoSize, Key);

  if (Result == (StaticInfo + InfoSize) || Result->BaseDiagID != ID)
    return nullptr;

  return Result;
}

//===----------------------------------------------------------------------===//
//  DiagnosticInfos
//===----------------------------------------------------------------------===//
DiagnosticInfos::DiagnosticInfos(LinkerConfig &PConfig) : Config(PConfig) {}

DiagnosticInfos::~DiagnosticInfos() {}

eld::Expected<llvm::StringRef>
DiagnosticInfos::getDescription(DiagnosticEngine::DiagIDType PId,
                                bool PInLoC) const {
  DiagnosticEngine::DiagIDType BaseDiagId =
      DiagnosticEngine::getBaseDiagID(PId);
  if (BaseDiagId >= numOfDiags()) {
    return std::make_unique<plugin::DiagnosticEntry>(plugin::DiagnosticEntry(
        Diag::fatal_invalid_diag_id, {std::to_string(PId)}));
  }
  if (BaseDiagId < Diag::NumOfBuildinDiagnosticInfo)
    return getDiagInfo(BaseDiagId, PInLoC)->getDescription();
  return CustomDiags[BaseDiagId - Diag::NumOfBuildinDiagnosticInfo]
      .getDescription();
}

DiagnosticEngine::Severity
DiagnosticInfos::getSeverity(const Diagnostic &Diagnostic, bool Loc) const {
  DiagnosticEngine::DiagIDType Id = Diagnostic.getID();
  return DiagnosticEngine::getSeverity(Id);
}

eld::Expected<void> DiagnosticInfos::process(DiagnosticEngine &PEngine) const {
  Diagnostic Info(PEngine);
  DiagnosticEngine::DiagIDType ID = Info.getID();
  DiagnosticEngine::DiagIDType BaseDiagId = DiagnosticEngine::getBaseDiagID(ID);
  if (BaseDiagId >= numOfDiags()) {
    return std::make_unique<plugin::DiagnosticEntry>(plugin::DiagnosticEntry(
        Diag::fatal_invalid_diag_id, {std::to_string(ID)}));
  }
  DiagnosticEngine::Severity Severity = getSeverity(Info, /*loc=*/false);

  if (BaseDiagId ==
          DiagnosticEngine::getBaseDiagID(Diag::multiple_definitions) &&
      Config.options().isMulDefs())
    Severity = DiagnosticEngine::Ignore;

  // If --fatal-warnings is turned on, then switch warnings and errors to fatal
  if (Config.options().isFatalWarnings()) {
    if (Severity == DiagnosticEngine::Warning ||
        Severity == DiagnosticEngine::CriticalWarning ||
        Severity == DiagnosticEngine::Error ||
        Severity == DiagnosticEngine::InternalError) {
      Severity = DiagnosticEngine::Fatal;
    }
  }

  // If -Werror is turned on, then switch warnings to errors
  if (Config.options().isWarningsAsErrors()) {
    if (Severity == DiagnosticEngine::Warning ||
        Severity == DiagnosticEngine::CriticalWarning)
      Severity = DiagnosticEngine::Error;
  }

  // If --fatal-internal-errors is used, then switch internal errors to fatal
  // errors.
  if (Config.options().isFatalInternalErrors()) {
    if (Severity == DiagnosticEngine::InternalError)
      Severity = DiagnosticEngine::Fatal;
  }

  // finally, report it.
  eld::Expected<void> ExpReportRes =
      PEngine.getPrinter()->handleDiagnostic(Severity, Info);
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(ExpReportRes);
  return {};
}

DiagnosticEngine::DiagIDType
DiagnosticInfos::getOrCreateCustomDiagID(DiagnosticEngine::Severity Severity,
                                         llvm::StringRef FormatStr) {
  DiagnosticEngine::DiagIDType Idx = 0;
  CustomDiagInfo DiagInfo{FormatStr.str()};
  auto It = std::find(CustomDiags.begin(), CustomDiags.end(), DiagInfo);
  if (It != CustomDiags.end())
    Idx = It - CustomDiags.begin();
  else {
    Idx = CustomDiags.size();
    CustomDiags.push_back(DiagInfo);
  }

  DiagnosticEngine::DiagIDType BaseId = Idx + Diag::NumOfBuildinDiagnosticInfo;

  ASSERT(BaseId < (1 << DiagnosticEngine::NumOfBaseDiagBits),
         "Base diagnostic limit exceeded!");

  return DiagnosticEngine::updateSeverity(BaseId, Severity);
}

size_t DiagnosticInfos::numOfDiags() const {
  return Diag::NumOfBuildinDiagnosticInfo + CustomDiags.size();
}
