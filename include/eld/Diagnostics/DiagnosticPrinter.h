//===- DiagnosticPrinter.h-------------------------------------------------===//
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
#ifndef ELD_DIAGNOSTICS_DIAGNOSTICPRINTER_H
#define ELD_DIAGNOSTICS_DIAGNOSTICPRINTER_H
#include "eld/Diagnostics/Diagnostic.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Input/Input.h"
#include "eld/PluginAPI/Expected.h"
#include "llvm/ADT/StringRef.h"
#include <atomic>
#include <mutex>

namespace eld {

class ProgressBar;

/** \class DiagnosticPrinter
 *  \brief DiagnosticPrinter provides the interface to customize diagnostic
 *  messages and output.
 */
class DiagnosticPrinter {
public:
  enum Verbose : uint16_t { None = 0x0, Default = 0x1 };

  enum VerifyType { VerifyReloc = 0x1 };

  DiagnosticPrinter(llvm::raw_ostream &Ostream, bool UseColor = false)
      : OStream(Ostream), NumErrors(0), NumFatalErrors(0), NumWarnings(0),
        NumCriticalWarnings(0), NumInternalErrors(0), UseColor(UseColor),
        VerboseLevel(Verbose::None) {}

  ~DiagnosticPrinter();

  enum StatsType { AllStats = 0xFF };

  uint32_t isVerbose() const { return VerboseLevel & Verbose::Default; }

  void setVerbose(int8_t PVerboseLevel) {
    // All levels of verbose is Default for now.
    VerboseLevel = Verbose::Default;
  }

  void setVerify(uint32_t PVerify) { Verify |= PVerify; }

  uint32_t verify() { return Verify; }

  bool verifyReloc() { return Verify & VerifyReloc; }

  void setStats(uint32_t PStats) { Stats = PStats; }

  bool allStats() { return (Stats & AllStats); }

  void clear() {
    NumErrors.store(0);
    NumWarnings.store(0);
  }

  /// HandleDiagnostic - Handle this diagnostic, reporting it to the user or
  /// capturing it to a log as needed.
  eld::Expected<void> handleDiagnostic(DiagnosticEngine::Severity PSeverity,
                                       const Diagnostic &PInfo);

  /// Prints a diagnostic.
  ///
  /// Diagnostic is printed as:
  /// pluginName:type: outString
  void printDiagnostic(const enum llvm::raw_ostream::Colors Color,
                       llvm::StringRef Type, const std::string &OutString,
                       llvm::StringRef PluginName);

  unsigned int getNumErrors() const { return NumErrors.load(); }
  unsigned int getNumFatalErrors() const { return NumFatalErrors.load(); }
  unsigned int getNumWarnings() const { return NumWarnings.load(); }

  // If we are in NoInhibit exec mode, only fatal errors count.
  void setNoInhibitExec() { IsNoInhibitExec = true; }

  bool isNoInhibitExec() const { return IsNoInhibitExec; }

  void setUserErrorLimit(uint32_t Limit) { UserErrorLimit = Limit; }

  void setUserWarningLimit(uint32_t Limit) { UserWarningLimit = Limit; }

  // Increment fatal errors
  void recordFatalError() { ++NumFatalErrors; }

  void setUseColor(bool PUseColor) { UseColor = PUseColor; }

  void setProgressBar(ProgressBar *PProgressBar) { Progress = PProgressBar; }

protected:
  llvm::raw_ostream &OStream;
  std::atomic<unsigned int> NumErrors = 0;
  std::atomic<unsigned int> NumFatalErrors = 0;
  std::atomic<unsigned int> NumWarnings = 0;
  std::atomic<unsigned int> NumCriticalWarnings = 0;
  std::atomic<unsigned int> NumInternalErrors = 0;
  bool UseColor = false;
  Verbose VerboseLevel;
  uint32_t Verify = 0;
  uint32_t Stats = 0;
  uint32_t UserErrorLimit = 10;
  uint32_t UserWarningLimit = 10;
  bool IsNoInhibitExec = false;
  ProgressBar *Progress = nullptr;
};

} // namespace eld

#endif
