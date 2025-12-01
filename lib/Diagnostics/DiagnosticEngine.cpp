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
DiagnosticEngine::DiagnosticEngine(bool UseColor) {
  Printer = new DiagnosticPrinter(llvm::errs(), UseColor);
}

DiagnosticEngine::~DiagnosticEngine() { delete Printer; }

void DiagnosticEngine::setInfoMap(std::unique_ptr<DiagnosticInfos> PInfo) {
  InfoMap = std::move(PInfo);
}

// emit - process current diagnostic.
bool DiagnosticEngine::emit(std::unique_lock<std::timed_mutex> Lock) {
  if (!InfoMap)
    return true;

  eld::Expected<void> ExpRes = InfoMap->process(*this);
  State.reset();
  if (!ExpRes) {
    Lock.unlock();
    raiseDiagEntry(std::move(ExpRes.error()));
    return false;
  }
  return true;
}

MsgHandler DiagnosticEngine::raise(DiagIDType PId) {
  std::unique_lock<std::timed_mutex> Lock(Mutex);
  State.ID = PId;
  State.ThreadID = std::this_thread::get_id();
  return MsgHandler(*this, std::move(Lock));
}

MsgHandler *DiagnosticEngine::raisePluginDiag(DiagIDType ID,
                                              const Plugin *Plugin) {
  std::unique_lock<std::timed_mutex> Lock(Mutex);
  // FIXME: Verify that diagnostic ID is valid. ID should be less than
  // Diag::NUM_OF_BUILDIN_DIAGNOSTIC_INFO + CustomDiags.size()
  State.ID = ID;
  State.Plugin = Plugin;
  State.ThreadID = std::this_thread::get_id();
  return new MsgHandler(*this, std::move(Lock));
}

bool DiagnosticEngine::diagnose() {
  if (Printer->getNumErrors() > 0 || Printer->getNumFatalErrors()) {
    if (!Printer->isNoInhibitExec())
      return false;
  }

  // FIXME: Remove redundant '&& true'
  return !Printer->getNumFatalErrors() && true;
}

void DiagnosticEngine::finalize() {
  raise(Diag::linker_run_summary)
      << Printer->getNumWarnings() << Printer->getNumErrors()
      << Printer->getNumFatalErrors();
}

DiagnosticEngine::DiagIDType
DiagnosticEngine::getCustomDiagID(DiagnosticEngine::Severity Severity,
                                  llvm::StringRef FormatStr) const {
  std::lock_guard<std::timed_mutex> Lock(Mutex);
  ASSERT(InfoMap, "Diagnostics info map is not initialized!");
  DiagnosticEngine::DiagIDType Res =
      InfoMap->getOrCreateCustomDiagID(Severity, FormatStr);
  return Res;
}

DiagnosticInfos &DiagnosticEngine::infoMap() {
  assert(nullptr != InfoMap && "DiagnosticEngine was not initialized!");
  return *InfoMap.get();
}

void DiagnosticEngine::raiseDiagEntry(
    std::unique_ptr<plugin::DiagnosticEntry> DiagEntry) {
  // If diagEntry does not contain any diagnostic, then simply return.
  if (!DiagEntry)
    return;
  MsgHandler Diagnostic = raise(DiagEntry->diagID());
  for (const auto &Arg : DiagEntry->args())
    Diagnostic << Arg;
}

void DiagnosticEngine::raisePluginDiag(
    std::unique_ptr<plugin::DiagnosticEntry> DiagEntry, const Plugin *Plugin) {
  if (!DiagEntry)
    return;
  MsgHandler *Diagnostic = raisePluginDiag(DiagEntry->diagID(), Plugin);
  for (const auto &Arg : DiagEntry->args())
    *Diagnostic << Arg;
  delete Diagnostic;
}

plugin::DiagnosticEntry
DiagnosticEngine::convertToDiagEntry(llvm::Error Err) const {
  std::string ErrMessage;
  llvm::raw_string_ostream Sstream(ErrMessage);
  Sstream << Err;
  DiagnosticEngine::DiagIDType DiagId = getCustomDiagID(
      DiagnosticEngine::Severity::Fatal, "LLVM: " + Sstream.str());

  llvm::Error IgnoreErr = llvm::handleErrors(
      std::move(Err),
      [](std::unique_ptr<llvm::ErrorInfoBase> Payload) -> llvm::Error {
        return llvm::Error::success();
      });

  // llvm::Error must always be checked.
  if (IgnoreErr) {
    ASSERT(!IgnoreErr, "ignoreErr must always be false");
    return plugin::DiagnosticEntry{};
  }

  return plugin::DiagnosticEntry(DiagId);
}

void DiagnosticEngine::resetSeverity(DiagIDType &Id) { Id &= ~SeverityMask; }

DiagnosticEngine::Severity DiagnosticEngine::getSeverity(DiagIDType Id) {
  DiagIDType SeverityVal = (Id & SeverityMask) >> NumOfBaseDiagBits;
  switch (SeverityVal) {
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
DiagnosticEngine::updateSeverity(DiagIDType Id, Severity Severity) {
  DiagIDType SeverityValue = static_cast<DiagIDType>(Severity);
  SeverityValue = SeverityValue << NumOfBaseDiagBits;
  resetSeverity(Id);
  Id |= SeverityValue;
  return Id;
}

DiagnosticEngine::DiagIDType DiagnosticEngine::getBaseDiagID(DiagIDType Id) {
  resetSeverity(Id);
  return Id;
}

bool DiagnosticEngine::isUsable() {
  std::unique_lock<std::timed_mutex> Lock(Mutex, std::defer_lock_t{});
  std::chrono::seconds WaitThreshold(2);
  if (Lock.try_lock_for(WaitThreshold))
    return true;
  return false;
}

DiagnosticEngine::Severity DiagnosticEngine::getDiagEngineSeverity(
    plugin::DiagnosticEntry::Severity Severity) {
  switch (Severity) {
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
    eld::DiagnosticEngine::Severity Severity) {
  switch (Severity) {
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
  const DiagnosticEngine::DiagIDType Diag::diagName =                          \
      DiagnosticEngine::updateSeverity(++Counter, severity);

DiagnosticEngine::DiagIDType Diag::Counter = 0;
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
// This is used to determine where to start base IDs for custom diagnostics.
const DiagnosticEngine::DiagIDType Diag::NumOfBuildinDiagnosticInfo = ++Counter;
