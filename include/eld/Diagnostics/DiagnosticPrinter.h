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
  };

  enum Verbose : uint16_t { None = 0x0, Default = 0x1 };

  enum VerifyType { VerifyReloc = 0x1 };

  DiagnosticPrinter(llvm::raw_ostream &ostream, bool useColor = false)
      : m_OStream(ostream), m_NumErrors(0), m_NumFatalErrors(0),
        m_NumWarnings(0), m_NumCriticalWarnings(0), m_NumInternalErrors(0),
        m_UseColor(useColor), m_Verbose(Verbose::None) {}

  ~DiagnosticPrinter();

  enum StatsType { AllStats = 0xFF };

  void setTrace(uint32_t pTrace) { m_Trace |= pTrace; }

  bool traceTrampolines() { return m_Trace & TraceTrampolines; }

  bool traceAssignments() { return m_Trace & TraceAssignments; }

  bool traceFiles() { return m_Trace & TraceFiles; }

  bool traceSymbols() { return m_Trace & TraceSymbols; }

  bool traceDynamicLinking() { return m_Trace & TraceDynamicLinking; }

  bool traceCommandLine() { return m_Trace & TraceCommandLine; }

  bool traceGC() { return m_Trace & TraceGC; }

  bool traceGCLive() { return m_Trace & TraceGCLive; }

  bool traceSym() { return m_Trace & TraceSym; }

  bool traceReloc() { return m_Trace & TraceReloc; }

  bool traceThreads() { return m_Trace & TraceThreads; }

  bool tracePlugins() { return m_Trace & TracePlugin; }

  bool traceWrapSymbols() { return m_Trace & TraceWrap; }

  bool traceSection() { return m_Trace & TraceSection; }

  bool traceMergeStrings() { return m_Trace & TraceMergeStrings; }

  bool traceLinkerScript() { return (m_Trace & TraceLinkerScript); }

  bool traceSymDef() { return (m_Trace & TraceSymDef); }

  uint32_t trace() { return m_Trace; }

  uint32_t isVerbose() const { return m_Verbose & Verbose::Default; }

  void setVerbose(int8_t Verbose) {
    // All levels of verbose is Default for now.
    m_Verbose = Verbose::Default;
  }

  void setVerify(uint32_t pVerify) { m_Verify |= pVerify; }

  uint32_t verify() { return m_Verify; }

  bool verifyReloc() { return m_Verify & VerifyReloc; }

  void setStats(uint32_t stats) { m_Stats = stats; }

  bool allStats() { return (m_Stats & AllStats); }

  void clear() {
    m_NumErrors.store(0);
    m_NumWarnings.store(0);
  }

  /// HandleDiagnostic - Handle this diagnostic, reporting it to the user or
  /// capturing it to a log as needed.
  eld::Expected<void> handleDiagnostic(DiagnosticEngine::Severity pSeverity,
                                       const Diagnostic &pInfo);

  /// Prints a diagnostic.
  ///
  /// Diagnostic is printed as:
  /// pluginName:type: outString
  void printDiagnostic(const enum llvm::raw_ostream::Colors Color,
                       llvm::StringRef type, const std::string &outString,
                       llvm::StringRef pluginName);

  unsigned int getNumErrors() const { return m_NumErrors.load(); }
  unsigned int getNumFatalErrors() const { return m_NumFatalErrors.load(); }
  unsigned int getNumWarnings() const { return m_NumWarnings.load(); }

  // If we are in NoInhibit exec mode, only fatal errors count.
  void setNoInhibitExec() { m_isNoInhibitExec = true; }

  bool isNoInhibitExec() const { return m_isNoInhibitExec; }

  void setUserErrorLimit(uint32_t limit) { m_UserErrorLimit = limit; }

  void setUserWarningLimit(uint32_t limit) { m_UserWarningLimit = limit; }

  // Increment fatal errors
  void recordFatalError() { ++m_NumFatalErrors; }

  void setUseColor(bool useColor) { m_UseColor = useColor; }

protected:
  llvm::raw_ostream &m_OStream;
  std::atomic<unsigned int> m_NumErrors = 0;
  std::atomic<unsigned int> m_NumFatalErrors = 0;
  std::atomic<unsigned int> m_NumWarnings = 0;
  std::atomic<unsigned int> m_NumCriticalWarnings = 0;
  std::atomic<unsigned int> m_NumInternalErrors = 0;
  bool m_UseColor = false;
  Verbose m_Verbose;
  uint32_t m_Trace = 0;
  uint32_t m_Verify = 0;
  uint32_t m_Stats = 0;
  uint32_t m_UserErrorLimit = 10;
  uint32_t m_UserWarningLimit = 10;
  bool m_isNoInhibitExec = false;
};

} // namespace eld

#endif
