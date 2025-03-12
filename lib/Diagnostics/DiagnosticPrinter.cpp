//===- DiagnosticPrinter.cpp-----------------------------------------------===//
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
#include "eld/Diagnostics/DiagnosticPrinter.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/Signals.h"

using namespace eld;

namespace {
const enum llvm::raw_ostream::Colors UnreachableColor = llvm::raw_ostream::RED;
const enum llvm::raw_ostream::Colors FatalColor = llvm::raw_ostream::YELLOW;
const enum llvm::raw_ostream::Colors ErrorColor = llvm::raw_ostream::RED;
const enum llvm::raw_ostream::Colors WarningColor = llvm::raw_ostream::MAGENTA;
const enum llvm::raw_ostream::Colors DebugColor = llvm::raw_ostream::CYAN;
const enum llvm::raw_ostream::Colors NoteColor = llvm::raw_ostream::GREEN;
const enum llvm::raw_ostream::Colors IgnoreColor = llvm::raw_ostream::BLUE;
const enum llvm::raw_ostream::Colors RemarkColor = llvm::raw_ostream::BLUE;
const enum llvm::raw_ostream::Colors VerboseColor = llvm::raw_ostream::CYAN;
const enum llvm::raw_ostream::Colors TraceColor = llvm::raw_ostream::CYAN;
} // namespace

DiagnosticPrinter::~DiagnosticPrinter() {
  // If error count equals error limit print a message to make user aware
  if (m_UserErrorLimit && (m_NumErrors.load() >= m_UserErrorLimit))
    printDiagnostic(
        ErrorColor, "Error",
        "Too many errors emitted, stopping now (use --error-limit=0 "
        "to see all errors)",
        /*pluginName=*/"");
  // If warning count equals warn limit print a message to make user aware
  if (m_UserWarningLimit && (m_NumWarnings.load() >= m_UserWarningLimit))
    printDiagnostic(WarningColor, "Warning",
                    "Too many warnings emitted (use --warn-limit=0 "
                    "to see all warnings)",
                    /*pluginName=*/"");
  // FIXME: Why clear the diagnostics stats in destructor?
  clear();
}

void DiagnosticPrinter::printDiagnostic(
    const enum llvm::raw_ostream::Colors color, llvm::StringRef type,
    const std::string &outString, llvm::StringRef pluginName) {
  if (m_UseColor)
    m_OStream.changeColor(color, true);
  if (!pluginName.empty())
    m_OStream << pluginName << ":";
  if (!type.empty())
    m_OStream << type << ": ";
  if (m_UseColor)
    m_OStream.resetColor();
  m_OStream << outString << "\n";
}

/// HandleDiagnostic - Handle this diagnostic, reporting it to the user or
/// capturing it to a log as needed.
eld::Expected<void>
DiagnosticPrinter::handleDiagnostic(DiagnosticEngine::Severity pSeverity,
                                    const Diagnostic &pInfo) {
  if (pSeverity == DiagnosticEngine::Warning) {
    ++m_NumWarnings;
    // Skip printing warning messages above warning limit
    // Warning limit 0 is treated as no limit
    if (m_UserWarningLimit && (m_NumWarnings >= m_UserWarningLimit))
      return {};
  }

  if (pSeverity == DiagnosticEngine::CriticalWarning)
    ++m_NumCriticalWarnings;

  if (pSeverity <= DiagnosticEngine::Error) {
    ++m_NumErrors;
    // Skip printing error messages above error limit
    // Error limit 0 is treated as no limit
    if (m_UserErrorLimit && (m_NumErrors.load() >= m_UserErrorLimit))
      return {};
  }

  if (pSeverity == DiagnosticEngine::Fatal)
    ++m_NumFatalErrors;

  if (pSeverity == DiagnosticEngine::InternalError)
    ++m_NumInternalErrors;

  std::string out_string;
  eld::Expected<void> expFormatRes = pInfo.format(out_string);
  if (!expFormatRes)
    return expFormatRes;

  auto PrintDiagnostic = [&](const enum llvm::raw_ostream::Colors Color,
                             llvm::StringRef Type) {
    printDiagnostic(Color, Type, out_string, pInfo.getPluginName());
  };
  switch (pSeverity) {
  case DiagnosticEngine::Unreachable: {
    PrintDiagnostic(UnreachableColor, "Fatal");
    break;
  }
  case DiagnosticEngine::Fatal: {
    PrintDiagnostic(FatalColor, "Fatal");
    break;
  }
  case DiagnosticEngine::Error: {
    PrintDiagnostic(ErrorColor, "Error");
    break;
  }
  case DiagnosticEngine::CriticalWarning: {
    PrintDiagnostic(WarningColor, "CriticalWarning");
    break;
  }
  case DiagnosticEngine::Warning: {
    PrintDiagnostic(WarningColor, "Warning");
    break;
  }
  case DiagnosticEngine::Debug: {
    // show debug message only if verbose >= 0
    if (DiagnosticPrinter::isVerbose())
      PrintDiagnostic(DebugColor, "Debug");
    break;
  }
  case DiagnosticEngine::Note: {
    PrintDiagnostic(NoteColor, "Note");
    break;
  }
  case DiagnosticEngine::Remark: {
    PrintDiagnostic(RemarkColor, "Remark");
    break;
  }
  case DiagnosticEngine::Ignore: {
    if (DiagnosticPrinter::isVerbose())
      PrintDiagnostic(IgnoreColor, "Ignore");
    break;
  }
  case DiagnosticEngine::Verbose: {
    if (!DiagnosticPrinter::isVerbose())
      return {};
    PrintDiagnostic(VerboseColor, "Verbose");
    break;
  }
  case DiagnosticEngine::Trace: {
    PrintDiagnostic(TraceColor, "Trace");
    break;
  }
  case DiagnosticEngine::InternalError: {
    PrintDiagnostic(ErrorColor, "InternalError");
    break;
  }
  default:
    break;
  }

  switch (pSeverity) {
  case DiagnosticEngine::Unreachable: {
    m_OStream << "\n\n";
    printDiagnostic(FatalColor, "", out_string, /*pluginName=*/"");
  }
    LLVM_FALLTHROUGH;

  /** fall through **/
  case DiagnosticEngine::Fatal: {
    // If we reached here, we are failing ungracefully. Run the interrupt
    // handlers
    // to make sure any special cleanups get done, in particular that we remove
    // files registered with RemoveFileOnSignal.
    break;
  }
  case DiagnosticEngine::Error:
  case DiagnosticEngine::Warning:
  default:
    break;
  }
  return {};
}
