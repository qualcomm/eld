//===- DiagnosticInfos.h---------------------------------------------------===//
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
#ifndef ELD_DIAGNOSTICS_DIAGNOSTICINFOS_H
#define ELD_DIAGNOSTICS_DIAGNOSTICINFOS_H
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/PluginAPI/Expected.h"
#include "llvm/ADT/StringRef.h"
#include <vector>

namespace eld {

class LinkerConfig;
class Diagnostic;
class DiagnosticEngine;

/** \class DiagnosticInfos
 *  \brief DiagnosticInfos caches run-time information of DiagnosticInfo.
 *
 * \note This class should only be used from within the diagnostic framework.
 * DiagnosticEngine shall be the only access point to the diagnostic framework
 * for the outside world.
 */
class DiagnosticInfos {
public:
  /// Stores custom diagnostic info
  class CustomDiagInfo {
  public:
    CustomDiagInfo(const std::string &formatStr) : m_FormatStr(formatStr) {}
    llvm::StringRef getDescription() const { return m_FormatStr; }
    bool operator==(const CustomDiagInfo &rhs) const {
      return m_FormatStr == rhs.m_FormatStr;
    }

  private:
    const std::string m_FormatStr;
  };

  DiagnosticInfos(LinkerConfig &pConfig);

  ~DiagnosticInfos();

  /// Returns the corresponding format string of the diagnostic ID 'pID'.
  ///
  /// \note Function call to this function is thread-safe only if it is called
  /// from within the diagnostic framework.
  eld::Expected<llvm::StringRef>
  getDescription(DiagnosticEngine::DiagIDType pID, bool pLoC) const;

  /// Returns the corresponding severity of the diagnostic ID 'pID'.
  ///
  /// \note Function call to this function is thread-safe only if it is called
  /// from within the diagnostic framework.
  DiagnosticEngine::Severity getSeverity(const Diagnostic &id, bool ploc) const;

  /// Returns the diagnostic ID for a diagnostic that has severity and formatStr
  /// attributes same as the ones provided through parameters.
  ///
  /// \note Function call to this function is thread-safe only if it is called
  /// from DiagnosticEngine.
  DiagnosticEngine::DiagIDType
  getOrCreateCustomDiagID(DiagnosticEngine::Severity severity,
                          llvm::StringRef formatStr);

  /// Process the diagnostic. This performs basic processing of diagnostic and
  /// then forwards to DiagnosticPrinter to finally print the diagnostic.
  ///
  /// \note Function call to this function is thread-safe only if it is called
  /// from DiagnosticEngine.
  eld::Expected<void> process(DiagnosticEngine &pEngine) const;

  /// Returns total number of diagnostics
  size_t numOfDiags() const;

private:
  LinkerConfig &m_Config;
  std::vector<CustomDiagInfo> m_CustomDiags;
};

} // namespace eld

#endif
