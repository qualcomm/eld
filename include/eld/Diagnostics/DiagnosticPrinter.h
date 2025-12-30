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

/** \class DiagnosticPrinter
 *  \brief DiagnosticPrinter provides the interface to customize diagnostic
 *  messages and output.
 */
class DiagnosticPrinter {
public:
  enum TraceType {
    TraceFiles = 0x1,
    TraceTrampolines = 0x2,
    TraceSymbols = 0x4,
    TraceCommandLine = 0x8,
    TraceGC = 0x10,
    TraceSym = 0x20,
    TraceLTO = 0x40,
    TraceAssignments = 0x80,
    TraceReloc = 0x100,
    TraceThreads = 0x200,
    TracePlugin = 0x400,
    TraceGCLive = 0x800,
    TraceWrap = 0x1000,
    TraceSection = 0x2000,
    TraceDynamicLinking = 0x4000,
    TraceMergeStrings = 0x8000,
    TraceLinkerScript = 0x10000,
    TraceSymDef = 0x100000,
    TracePendingAssignments = 0x200000
  };

  enum Verbose : uint16_t { None = 0x0, Default = 0x1 };

  enum VerifyType { VerifyReloc = 0x1 };

  DiagnosticPrinter(llvm::raw_ostream &Ostream, bool UseColor = false)
      : OStream(Ostream), NumErrors(0), NumFatalErrors(0), NumWarnings(0),
        NumCriticalWarnings(0), NumInternalErrors(0), UseColor(UseColor),
        VerboseLevel(Verbose::None) {}

  ~DiagnosticPrinter();

  enum StatsType { AllStats = 0xFF };

  void setTrace(uint32_t PTrace) { Trace |= PTrace; }

  bool traceTrampolines() { return Trace & TraceTrampolines; }

  bool traceAssignments() { return Trace & TraceAssignments; }

  bool tracePendingAssignments() { return Trace & TracePendingAssignments; }

  bool traceFiles() { return Trace & TraceFiles; }

  bool traceSymbols() { return Trace & TraceSymbols; }

  bool traceDynamicLinking() { return Trace & TraceDynamicLinking; }

  bool traceCommandLine() { return Trace & TraceCommandLine; }

  bool traceGC() { return Trace & TraceGC; }

  bool traceGCLive() { return Trace & TraceGCLive; }

  bool traceSym() { return Trace & TraceSym; }

  bool traceReloc() { return Trace & TraceReloc; }

  bool traceThreads() { return Trace & TraceThreads; }

  bool tracePlugins() { return Trace & TracePlugin; }

  bool traceWrapSymbols() { return Trace & TraceWrap; }

  bool traceSection() { return Trace & TraceSection; }

  bool traceMergeStrings() { return Trace & TraceMergeStrings; }

  bool traceLinkerScript() { return (Trace & TraceLinkerScript); }

  bool traceSymDef() { return (Trace & TraceSymDef); }

  uint32_t trace() { return Trace; }

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

protected:
  llvm::raw_ostream &OStream;
  std::atomic<unsigned int> NumErrors = 0;
  std::atomic<unsigned int> NumFatalErrors = 0;
  std::atomic<unsigned int> NumWarnings = 0;
  std::atomic<unsigned int> NumCriticalWarnings = 0;
  std::atomic<unsigned int> NumInternalErrors = 0;
  bool UseColor = false;
  Verbose VerboseLevel;
  uint32_t Trace = 0;
  uint32_t Verify = 0;
  uint32_t Stats = 0;
  uint32_t UserErrorLimit = 10;
  uint32_t UserWarningLimit = 10;
  bool IsNoInhibitExec = false;
};

} // namespace eld

#endif
