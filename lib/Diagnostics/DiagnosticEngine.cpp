//===- DiagnosticEngine.cpp------------------------------------------------===//
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
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Diagnostics/DiagnosticInfos.h"
#include "eld/Diagnostics/DiagnosticPrinter.h"
#include "eld/Diagnostics/MsgHandler.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/ErrorHandling.h"
#include <cassert>
#include <limits>
#include <mutex>

using namespace eld;

//===----------------------------------------------------------------------===//
// DiagnosticEngine
//===----------------------------------------------------------------------===//
DiagnosticEngine::DiagnosticEngine(bool useColor) {
  m_pPrinter = new DiagnosticPrinter(llvm::errs(), useColor);
}

DiagnosticEngine::~DiagnosticEngine() { delete m_pPrinter; }

void DiagnosticEngine::setInfoMap(std::unique_ptr<DiagnosticInfos> pInfo) {
  m_pInfoMap = std::move(pInfo);
}

// emit - process current diagnostic.
bool DiagnosticEngine::emit(std::unique_lock<std::mutex> lock) {
  if (!m_pInfoMap)
    return true;

  eld::Expected<void> expRes = m_pInfoMap->process(*this);
  m_State.reset();
  if (!expRes) {
    lock.unlock();
    raiseDiagEntry(std::move(expRes.error()));
    return false;
  }
  return true;
}

MsgHandler DiagnosticEngine::raise(DiagIDType pID) {
  std::unique_lock<std::mutex> lock(m_Mutex);
  m_State.ID = pID;
  return MsgHandler(*this, std::move(lock));
}

MsgHandler *DiagnosticEngine::raisePluginDiag(DiagIDType ID,
                                              const Plugin *plugin) {
  std::unique_lock<std::mutex> lock(m_Mutex);
  // FIXME: Verify that diagnostic ID is valid. ID should be less than
  // diag::NUM_OF_BUILDIN_DIAGNOSTIC_INFO + m_CustomDiags.size()
  m_State.ID = ID;
  m_State.plugin = plugin;
  return new MsgHandler(*this, std::move(lock));
}

bool DiagnosticEngine::diagnose() {
  if (m_pPrinter->getNumErrors() > 0 || m_pPrinter->getNumFatalErrors()) {
    if (!m_pPrinter->isNoInhibitExec())
      return false;
  }
  return !m_pPrinter->getNumFatalErrors() && true;
}

void DiagnosticEngine::finalize() {
  raise(diag::linker_run_summary)
      << m_pPrinter->getNumWarnings() << m_pPrinter->getNumErrors()
      << m_pPrinter->getNumFatalErrors();
}

DiagnosticEngine::DiagIDType
DiagnosticEngine::getCustomDiagID(DiagnosticEngine::Severity severity,
                                  llvm::StringRef formatStr) const {
  std::lock_guard<std::mutex> lock(m_Mutex);
  ASSERT(m_pInfoMap, "Diagnostics info map is not initialized!");
  DiagnosticEngine::DiagIDType res =
      m_pInfoMap->getOrCreateCustomDiagID(severity, formatStr);
  return res;
}

DiagnosticInfos &DiagnosticEngine::infoMap() {
  assert(nullptr != m_pInfoMap && "DiagnosticEngine was not initialized!");
  return *m_pInfoMap.get();
}

void DiagnosticEngine::raiseDiagEntry(
    std::unique_ptr<plugin::DiagnosticEntry> diagEntry) {
  // If diagEntry does not contain any diagnostic, then simply return.
  if (!diagEntry)
    return;
  MsgHandler diagnostic = raise(diagEntry->diagID());
  for (const auto &arg : diagEntry->args())
    diagnostic << arg;
}

void DiagnosticEngine::raisePluginDiag(
    std::unique_ptr<plugin::DiagnosticEntry> diagEntry, const Plugin *plugin) {
  if (!diagEntry)
    return;
  MsgHandler *diagnostic = raisePluginDiag(diagEntry->diagID(), plugin);
  for (const auto &arg : diagEntry->args())
    *diagnostic << arg;
  delete diagnostic;
}

plugin::DiagnosticEntry
DiagnosticEngine::convertToDiagEntry(llvm::Error err) const {
  std::string errMessage;
  llvm::raw_string_ostream sstream(errMessage);
  sstream << err;
  DiagnosticEngine::DiagIDType diagID = getCustomDiagID(
      DiagnosticEngine::Severity::Fatal, "LLVM: " + sstream.str());

  llvm::Error ignoreErr = llvm::handleErrors(
      std::move(err),
      [](std::unique_ptr<llvm::ErrorInfoBase> payload) -> llvm::Error {
        return llvm::Error::success();
      });

  // llvm::Error must always be checked.
  if (ignoreErr) {
    ASSERT(!ignoreErr, "ignoreErr must always be false");
    return plugin::DiagnosticEntry{};
  }

  return plugin::DiagnosticEntry(diagID);
}

void DiagnosticEngine::resetSeverity(DiagIDType &id) { id &= ~m_SeverityMask; }

DiagnosticEngine::Severity DiagnosticEngine::getSeverity(DiagIDType id) {
  DiagIDType severityVal = (id & m_SeverityMask) >> NumOfBaseDiagBits;
  switch (severityVal) {
#define ADD_CASE(severity)                                                     \
  case Severity::severity:                                                     \
    return Severity::severity;
    ADD_CASE(None);
    ADD_CASE(Debug);
    ADD_CASE(Error);
    ADD_CASE(CriticalWarning);
    ADD_CASE(Fatal);
    ADD_CASE(Ignore);
    ADD_CASE(Note);
    ADD_CASE(Remark);
    ADD_CASE(Unreachable);
    ADD_CASE(Verbose);
    ADD_CASE(Warning);
    ADD_CASE(Trace);
    ADD_CASE(InternalError);
  default:
    llvm_unreachable("Invalid severity value!");
#undef ADD_CASE
  }
}

DiagnosticEngine::DiagIDType
DiagnosticEngine::updateSeverity(DiagIDType id, Severity severity) {
  DiagIDType severityValue = static_cast<DiagIDType>(severity);
  severityValue = severityValue << NumOfBaseDiagBits;
  resetSeverity(id);
  id |= severityValue;
  return id;
}

DiagnosticEngine::DiagIDType DiagnosticEngine::getBaseDiagID(DiagIDType id) {
  resetSeverity(id);
  return id;
}

DiagnosticEngine::Severity DiagnosticEngine::getDiagEngineSeverity(
    plugin::DiagnosticEntry::Severity severity) {
  switch (severity) {
#define ADD_CASE(severity)                                                     \
  case plugin::DiagnosticEntry::Severity::severity:                            \
    return DiagnosticEngine::Severity::severity;
    ADD_CASE(None);
    ADD_CASE(Verbose);
    ADD_CASE(Note);
    ADD_CASE(Warning);
    ADD_CASE(Error);
    ADD_CASE(Fatal);
#undef ADD_CASE
  }
}

plugin::DiagnosticEntry::Severity DiagnosticEngine::getDiagEntrySeverity(
    eld::DiagnosticEngine::Severity severity) {
  switch (severity) {
#define ADD_CASE(severity)                                                     \
  case eld::DiagnosticEngine::Severity::severity:                              \
    return plugin::DiagnosticEntry::Severity::severity;
    ADD_CASE(None);
    ADD_CASE(Verbose);
    ADD_CASE(Note);
    ADD_CASE(Warning);
    ADD_CASE(Error);
    ADD_CASE(Fatal);
  default:
    // FIXME: This is required because currently there are fewer diagnostic
    // severity in DiagnosticEntry::Severity than the
    // DiagnosticEngine::Severity.
    llvm_unreachable("Unexpected severity!");
#undef ADD_CASE
  }
}

// Initializes the default diagnostic IDs.
// Diagnostic IDs are formed of 2 components: diagnostic severity and base ID.
// Both diagnostic severity and the base diagnostic ID can be extract out of the
// complete diagnostic ID by using the severity mask
// (DiagnosticEngine::m_SeverityMask).
//
// Base diagnostic IDs and complete diagnostic IDs are unique for each
// diagnostic.
#define DIAG(diagName, severity, formatStr)                                    \
  const DiagnosticEngine::DiagIDType diag::diagName =                          \
      DiagnosticEngine::updateSeverity(++m_Counter, severity);

DiagnosticEngine::DiagIDType diag::m_Counter = 0;
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
// This is used to determine where to start base IDs for custom diagnostics.
const DiagnosticEngine::DiagIDType diag::NUM_OF_BUILDIN_DIAGNOSTIC_INFO =
    ++m_Counter;
