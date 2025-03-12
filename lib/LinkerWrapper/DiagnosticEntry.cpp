//===- DiagnosticEntry.cpp-------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#include "eld/PluginAPI/DiagnosticEntry.h"
#include "eld/Diagnostics/DiagnosticEngine.h"

using namespace eld;
using namespace eld::plugin;

DiagnosticEntry::DiagnosticEntry(DiagIDType id,
                                 const std::vector<std::string> &args,
                                 Severity severity)
    : m_DiagID(eld::DiagnosticEngine::updateSeverity(
          id, eld::DiagnosticEngine::getDiagEngineSeverity(severity))),
      m_DiagArgs(args) {}

DiagnosticEntry::DiagnosticEntry(DiagIDType id, std::vector<std::string> &&args,
                                 Severity severity)
    : m_DiagID(eld::DiagnosticEngine::updateSeverity(
          id, eld::DiagnosticEngine::getDiagEngineSeverity(severity))),
      m_DiagArgs(std::move(args)) {}

DiagnosticEntry::Severity DiagnosticEntry::severity() const {
  eld::DiagnosticEngine::Severity diagEngineSeverity =
      eld::DiagnosticEngine::getSeverity(m_DiagID);
  return eld::DiagnosticEngine::getDiagEntrySeverity(diagEngineSeverity);
}

bool DiagnosticEntry::isWarning() const {
  return eld::DiagnosticEngine::getSeverity(m_DiagID) ==
         eld::DiagnosticEngine::Severity::Warning;
}

bool DiagnosticEntry::isVerbose() const {
  return eld::DiagnosticEngine::getSeverity(m_DiagID) ==
         eld::DiagnosticEngine::Severity::Verbose;
}

bool DiagnosticEntry::isNote() const {
  return eld::DiagnosticEngine::getSeverity(m_DiagID) ==
         eld::DiagnosticEngine::Severity::Note;
}

bool DiagnosticEntry::isError() const {
  return eld::DiagnosticEngine::getSeverity(m_DiagID) ==
         eld::DiagnosticEngine::Severity::Error;
}

bool DiagnosticEntry::isFatal() const {
  return eld::DiagnosticEngine::getSeverity(m_DiagID) ==
         eld::DiagnosticEngine::Severity::Fatal;
}
