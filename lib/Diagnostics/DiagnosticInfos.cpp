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

  bool operator<(const DiagStaticInfo &pRHS) const {
    return (BaseDiagID < pRHS.BaseDiagID);
  }
};

} // namespace

static const DiagStaticInfo DiagCommonInfo[] = {
#define DIAG(diagName, severity, formatStr)                                    \
  {DiagnosticEngine::getBaseDiagID(diag::diagName), formatStr},
// PluginDiags.inc file should always be first!
// This is to ensure that Plugin Diags IDs are consistent in
// eld::diag:: and plugin::Diagnostic:: namespaces.
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
#include "eld/Diagnostics/DiagVerbose.inc"
#include "eld/Diagnostics/DiagWriters.inc"
#undef DIAG
    {0, ""}};

static const unsigned int DiagCommonInfoSize =
    sizeof(DiagCommonInfo) / sizeof(DiagCommonInfo[0]) - 1;

static const DiagStaticInfo DiagLoCInfo[] = {
#define DIAG(diagName, severity, formatStr)                                    \
  {DiagnosticEngine::getBaseDiagID(diag::diagName), formatStr},
// PluginDiags.inc file should always be first!
// This is to ensure that Plugin Diags IDs are consistent in
// eld::diag:: and plugin::Diagnostic:: namespaces.
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
#include "eld/Diagnostics/DiagVerbose.inc"
#include "eld/Diagnostics/DiagWriters.inc"
#undef DIAG
    {0, ""}};

static const unsigned int DiagLoCInfoSize =
    sizeof(DiagLoCInfo) / sizeof(DiagLoCInfo[0]) - 1;

static const DiagStaticInfo *getDiagInfo(DiagnosticEngine::DiagIDType ID,
                                         bool pInLoC = false) {
  const DiagStaticInfo *static_info = (pInLoC) ? DiagLoCInfo : DiagCommonInfo;
  unsigned int info_size = (pInLoC) ? DiagLoCInfoSize : DiagCommonInfoSize;
  DiagnosticEngine::DiagIDType baseDiagID = DiagnosticEngine::getBaseDiagID(ID);
  DiagStaticInfo key = {baseDiagID, ""};
  // FIXME: Why don't we simply use static_info[baseDiagID] here?
  const DiagStaticInfo *result =
      std::lower_bound(static_info, static_info + info_size, key);

  if (result == (static_info + info_size) || result->BaseDiagID != ID)
    return nullptr;

  return result;
}

//===----------------------------------------------------------------------===//
//  DiagnosticInfos
//===----------------------------------------------------------------------===//
DiagnosticInfos::DiagnosticInfos(LinkerConfig &pConfig) : m_Config(pConfig) {}

DiagnosticInfos::~DiagnosticInfos() {}

eld::Expected<llvm::StringRef>
DiagnosticInfos::getDescription(DiagnosticEngine::DiagIDType pID,
                                bool pInLoC) const {
  DiagnosticEngine::DiagIDType baseDiagID =
      DiagnosticEngine::getBaseDiagID(pID);
  if (baseDiagID >= numOfDiags()) {
    return std::make_unique<plugin::DiagnosticEntry>(plugin::DiagnosticEntry(
        diag::fatal_invalid_diag_id, {std::to_string(pID)}));
  }
  if (baseDiagID < diag::NUM_OF_BUILDIN_DIAGNOSTIC_INFO)
    return getDiagInfo(baseDiagID, pInLoC)->getDescription();
  return m_CustomDiags[baseDiagID - diag::NUM_OF_BUILDIN_DIAGNOSTIC_INFO]
      .getDescription();
}

DiagnosticEngine::Severity
DiagnosticInfos::getSeverity(const Diagnostic &diagnostic, bool loc) const {
  DiagnosticEngine::DiagIDType id = diagnostic.getID();
  return DiagnosticEngine::getSeverity(id);
}

eld::Expected<void> DiagnosticInfos::process(DiagnosticEngine &pEngine) const {
  Diagnostic info(pEngine);
  DiagnosticEngine::DiagIDType ID = info.getID();
  DiagnosticEngine::DiagIDType baseDiagID = DiagnosticEngine::getBaseDiagID(ID);
  if (baseDiagID >= numOfDiags()) {
    return std::make_unique<plugin::DiagnosticEntry>(plugin::DiagnosticEntry(
        diag::fatal_invalid_diag_id, {std::to_string(ID)}));
  }
  DiagnosticEngine::Severity severity = getSeverity(info, /*loc=*/false);

  if (baseDiagID ==
          DiagnosticEngine::getBaseDiagID(diag::multiple_definitions) &&
      m_Config.options().isMulDefs())
    severity = DiagnosticEngine::Ignore;

  // If --fatal-warnings is turned on, then switch warnings and errors to fatal
  if (m_Config.options().isFatalWarnings()) {
    if (severity == DiagnosticEngine::Warning ||
        severity == DiagnosticEngine::CriticalWarning ||
        severity == DiagnosticEngine::Error ||
        severity == DiagnosticEngine::InternalError) {
      severity = DiagnosticEngine::Fatal;
    }
  }

  // If --fatal-internal-errors is used, then switch internal errors to fatal
  // errors.
  if (m_Config.options().isFatalInternalErrors()) {
    if (severity == DiagnosticEngine::InternalError)
      severity = DiagnosticEngine::Fatal;
  }

  // finally, report it.
  eld::Expected<void> expReportRes =
      pEngine.getPrinter()->handleDiagnostic(severity, info);
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expReportRes);
  return {};
}

DiagnosticEngine::DiagIDType
DiagnosticInfos::getOrCreateCustomDiagID(DiagnosticEngine::Severity severity,
                                         llvm::StringRef formatStr) {
  DiagnosticEngine::DiagIDType idx = 0;
  CustomDiagInfo diagInfo{formatStr.str()};
  auto it = std::find(m_CustomDiags.begin(), m_CustomDiags.end(), diagInfo);
  if (it != m_CustomDiags.end())
    idx = it - m_CustomDiags.begin();
  else {
    idx = m_CustomDiags.size();
    m_CustomDiags.push_back(diagInfo);
  }

  DiagnosticEngine::DiagIDType baseID =
      idx + diag::NUM_OF_BUILDIN_DIAGNOSTIC_INFO;

  ASSERT(baseID < (1 << DiagnosticEngine::NumOfBaseDiagBits),
         "Base diagnostic limit exceeded!");

  return DiagnosticEngine::updateSeverity(baseID, severity);
}

size_t DiagnosticInfos::numOfDiags() const {
  return diag::NUM_OF_BUILDIN_DIAGNOSTIC_INFO + m_CustomDiags.size();
}
