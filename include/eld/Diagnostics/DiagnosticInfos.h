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
    CustomDiagInfo(const std::string &PFormatStr) : FormatStr(PFormatStr) {}
    llvm::StringRef getDescription() const { return FormatStr; }
    bool operator==(const CustomDiagInfo &Rhs) const {
      return FormatStr == Rhs.FormatStr;
    }

  private:
    const std::string FormatStr;
  };

  DiagnosticInfos(LinkerConfig &PConfig);

  ~DiagnosticInfos();

  /// Returns the corresponding format string of the diagnostic ID 'pID'.
  ///
  /// \note Function call to this function is thread-safe only if it is called
  /// from within the diagnostic framework.
  eld::Expected<llvm::StringRef>
  getDescription(DiagnosticEngine::DiagIDType PId, bool PLoC) const;

  /// Returns the corresponding severity of the diagnostic ID 'pID'.
  ///
  /// \note Function call to this function is thread-safe only if it is called
  /// from within the diagnostic framework.
  DiagnosticEngine::Severity getSeverity(const Diagnostic &Id, bool Ploc) const;

  /// Returns the diagnostic ID for a diagnostic that has severity and formatStr
  /// attributes same as the ones provided through parameters.
  ///
  /// \note Function call to this function is thread-safe only if it is called
  /// from DiagnosticEngine.
  DiagnosticEngine::DiagIDType
  getOrCreateCustomDiagID(DiagnosticEngine::Severity Severity,
                          llvm::StringRef FormatStr);

  /// Process the diagnostic. This performs basic processing of diagnostic and
  /// then forwards to DiagnosticPrinter to finally print the diagnostic.
  ///
  /// \note Function call to this function is thread-safe only if it is called
  /// from DiagnosticEngine.
  eld::Expected<void> process(DiagnosticEngine &PEngine) const;

  /// Returns total number of diagnostics
  size_t numOfDiags() const;

private:
  LinkerConfig &Config;
  std::vector<CustomDiagInfo> CustomDiags;
};

} // namespace eld

#endif
