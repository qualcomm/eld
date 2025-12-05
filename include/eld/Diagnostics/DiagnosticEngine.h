//===- DiagnosticEngine.h--------------------------------------------------===//
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
#ifndef ELD_DIAGNOSTICS_DIAGNOSTICENGINE_H
#define ELD_DIAGNOSTICS_DIAGNOSTICENGINE_H
#include "eld/PluginAPI/DiagnosticEntry.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/DataTypes.h"
#include "llvm/Support/Error.h"
#include <condition_variable>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

/// If eld::Expected contains an error, then returns the associated diagnostic
/// entry; Otherwise does nothing.
#define ELDEXP_RETURN_DIAGENTRY_IF_ERROR(eldExp)                               \
  if (!eldExp)                                                                 \
    return std::move(eldExp.error());

#define DG_ELDEXP_REPORT_AND_RETURN_FALSE_IF_ERROR(DG, ELDExp)                 \
  if (!ELDExp) {                                                               \
    (DG).raiseDiagEntry(std::move(ELDExp.error()));                              \
    return false;                                                              \
  }

/// If llvm::Error contains an error, then returns diagnostic entry
/// created from llvm::Error; Otherwise does nothing.
#define LLVMEXP_RETURN_DIAGENTRY_IF_ERROR(llvmExp)                             \
  if (auto E = llvmExp.takeError()) {                                          \
    plugin::DiagnosticEntry diagEntry =                                        \
        this->m_Module.getConfig().getDiagEngine()->convertToDiagEntry(        \
            std::move(E));                                                     \
    return std::make_unique<plugin::DiagnosticEntry>(diagEntry);               \
  }

namespace eld {

class DiagnosticInfos;
class DiagnosticPrinter;
class Input;
class LinkerConfig;
class MsgHandler;
class Plugin;

using DiagnosticEntry = plugin::DiagnosticEntry;

/** \class DiagnosticEngine
 *  \brief DiagnosticEngine is used to report problems and issues.
 *
 *  DiagnosticEngine is used to report problems and issues. It creates the
 *  Diagnostics and passes them to the DiagnosticPrinter for reporting to the
 *  user.
 *
 *  DiagnosticEngine is a complex class, it is responsible for
 *  - remember the argument string for MsgHandler
 *  - choice the severity of a message by options
 *
 * Emitting diagnostics functionality of DiagnosticEnginge object is
 * thread-safe. Thread safety for this functionality is provided using
 * lock-based concurrency techniques. Note that the class does not provide
 * thread-safety for all of its operations. In particular, public thread-safe
 * functions are:
 * - raise
 * - raiseDiagEntry
 * - raisePluginDiag
 * - getCustomDiagID
 * - diagnose
 * - finalize
 *
 * All other public functions do not provide any thread-safety.
 *
 * DiagnosticEngine and DiagnosticInfos are tightly coupled data structures.
 * DiagnosticInfos functionality should only ever be accessed by the outside
 * world through DiagnosticEngine interface. Here outside world is any code that
 * lies outside of the linker diagnostic framework. This is important to
 * maintain thread-safety of diagnostic functions.
 */
class DiagnosticEngine {
public:
  enum Severity {
    // None must be 0 because currently 0 severity value represent
    // no severity at all.
    None = 0,
    Debug,
    Error,
    CriticalWarning,
    Fatal,
    Ignore,
    Note,
    Remark,
    Unreachable,
    Verbose,
    Warning,
    Trace,
    InternalError
  };

  enum ArgumentKind {
    ak_std_string, // std::string
    ak_c_string,   // const char *
    ak_sint,       // int
    ak_slonglong,  // signed long long
    ak_uint,       // unsigned int
    ak_ulonglong,  // unsigned long long
    ak_bool        // bool
  };

  using DiagIDType = uint32_t;

public:
  DiagnosticEngine(bool UseColor);

  ~DiagnosticEngine();

  // There should only be one instance of a DiagnosticEngine throughout the link
  // process. Deleting copy-constructor and copy-assigment operator makes it
  // difficult to accidentally copy the DiagnosticEngine object.
  DiagnosticEngine(const DiagnosticEngine &) = delete;
  DiagnosticEngine &operator=(const DiagnosticEngine &) = delete;

  void setInfoMap(std::unique_ptr<DiagnosticInfos> PInfo);

  DiagnosticPrinter *getPrinter() const { return Printer; }

  // report - issue the message to the printer
  MsgHandler raise(DiagIDType PId);

  /// Raise a plugin diagnostic.
  /// Plugin diagnostic function has two differences from the already
  /// existing 'raise' function:
  /// - It allows setting plugin name. All plugin diagnositcs are
  ///   prefixed with the plugin name.
  /// - It returns std::unique_ptr<MsgHandler> instead of MsgHandler.
  ///   This is because, in case of plugin diagnostics, the lifecycle
  ///   of MsgHandler needs to be managed by plugin::DiagnosticBuilder.
  MsgHandler *raisePluginDiag(DiagIDType ID, const Plugin *Plugin);

  /// Raise diagnostic from a DiagnosticEntry object.
  void raiseDiagEntry(std::unique_ptr<plugin::DiagnosticEntry> DiagEntry);

  /// Raise a plugin diagnostic from a DiagnosticEntry object.
  void raisePluginDiag(std::unique_ptr<plugin::DiagnosticEntry> DiagEntry,
                       const Plugin *Plugin);
  enum {
    /// MaxArguments - The maximum number of arguments we can hold. We currently
    /// only support up to 10 arguments (%0-%9).
    MaxArguments = 10
  };

  struct State {
  public:
    State() = default;
    ~State() {}

    void reset() {
      NumArgs = 0;
      ID.reset();
      Severity = None;
      File = nullptr;
      Plugin = nullptr;
      ThreadID = std::thread::id();
    }

  public:
    std::string ArgumentStrs[MaxArguments];
    intptr_t ArgumentVals[MaxArguments];
    uint8_t ArgumentKinds[MaxArguments];
    int8_t NumArgs = 0;
    std::optional<DiagIDType> ID;
    Severity Severity = Severity::None;
    Input *File = nullptr;
    const eld::Plugin *Plugin = nullptr;
    std::thread::id ThreadID;
  };

  DiagnosticInfos &infoMap();

  bool diagnose();
  void finalize();
  /// Returns an ID for a diagnostic with the specified severity and
  /// format string.
  ///
  /// If this is the first request for this diagnostic, it is created and
  /// registered; otherwise the existing ID is returned.
  DiagIDType getCustomDiagID(DiagnosticEngine::Severity Severity,
                             llvm::StringRef FormatStr) const;

  /// Converts a llvm::Error to plugin::DiagnosticEntry.
  /// DiagnosticEntry message is: "LLVM: " + error message from llvm::Error.
  plugin::DiagnosticEntry convertToDiagEntry(llvm::Error Err) const;

  /// Resets the severity component of diagnostic ID to
  /// DiagnosticEngine::Severity::None.
  static void resetSeverity(DiagIDType &Id);

  /// Returns the diagnostic severity.
  static Severity getSeverity(DiagIDType Id);

  /// Returns the base diagnostic ID.
  ///
  /// Diagnostic ID is composed of two main components:
  /// diagnostic severity and base diagnostic ID. Base diagnostic ID associates
  /// a diagnostic with it's corresponding format string.
  static DiagnosticEngine::DiagIDType getBaseDiagID(DiagIDType Id);

  /// Updates the diagnostic severity to the specified severity,
  /// and returns the diagnostic ID with the updated severity.
  static DiagIDType updateSeverity(DiagIDType Id,
                                   DiagnosticEngine::Severity Severity);

  /// Returns the corresponding DiagnosticEngine::Severity for the
  /// DiagnosticEntry::Severity.
  static eld::DiagnosticEngine::Severity
  getDiagEngineSeverity(plugin::DiagnosticEntry::Severity Severity);

  /// Returns the corresponding DiagnosticEntry::Severity for the
  /// DiagnosticEngine::Severity.
  static plugin::DiagnosticEntry::Severity
  getDiagEntrySeverity(eld::DiagnosticEngine::Severity Severity);

  /// Number of bits that are used to represent diagnostic severity.
  static constexpr uint32_t NumOfSeverityBits = 4;

  /// Number of bits that are used to represent base diagnostic ID.
  static constexpr uint32_t NumOfBaseDiagBits =
      std::numeric_limits<DiagIDType>::digits - NumOfSeverityBits;

  std::thread::id getThreadID() const {
    return State.ThreadID;
  }

  /// Should be used very carefully!!
  /// This function aborts the diagnostic in-flight.
  /// The only safe case of using this function is when a signal, whose default
  /// disposition is to terminate/stop the program, is delivered to the thread
  /// that is currently emitting a diagnostic.
  void abortDiagInFlight() {
    Mutex.unlock();
    State.reset();
  }

  /// Checks that if the diagnostic engine is usable. It is useful
  /// when we are not sure of the program state and thus cannot be
  /// certain that diagnostic engine is usable. For example, a kill
  /// signal by out-of-memory killer.
  bool isUsable();

private:
  // -----  emission  ----- //
  // emit - process the message to printer
  bool emit(std::unique_lock<std::timed_mutex> Lock);

  State &state() { return State; }

  const State &state() const { return State; }

private:
  DiagnosticPrinter *Printer = nullptr;
  std::unique_ptr<DiagnosticInfos> InfoMap;
  State State;
  mutable std::timed_mutex Mutex;

  /// In severity mask, the severity associated bits are 1 valued and
  /// the other bits are 0 valued.
  static constexpr DiagIDType SeverityMask = ((1 << NumOfSeverityBits) - 1)
                                             << NumOfBaseDiagBits;
  friend class MsgHandler;
  friend class Diagnostic;
};

class Diag {
  static DiagnosticEngine::DiagIDType Counter;

public:
#define DIAG(diagName, severity, formatStr)                                    \
  static const DiagnosticEngine::DiagIDType diagName;

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
#include "eld/Diagnostics/PluginDiags.inc"
#undef DIAG
  static DiagnosticEngine::DiagIDType const NumOfBuildinDiagnosticInfo;
}; // struct diag
} // namespace eld

#endif
