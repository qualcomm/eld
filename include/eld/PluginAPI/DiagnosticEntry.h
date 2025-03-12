//===- DiagnosticEntry.h---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_PLUGINAPI_DIAGNOSTIC_ENTRY_H
#define ELD_PLUGINAPI_DIAGNOSTIC_ENTRY_H
#include "Defines.h"
#include <limits>
#include <string>
#include <vector>

namespace eld::plugin {
/// DiagnosticEntry represents a diagnostic. It allows to conveniently
/// pass diagnostics from one function to another.
///
/// Each diagnostic ID has an associated severity. 'Severity' specified
/// in DiagnosticEntry overrides the default severity associated with a
/// diagnostic ID.
/// If no 'Severity' is specified, then the default severity associated
/// with a diagnostic ID is used.
class DLL_A_EXPORT DiagnosticEntry {
public:
  // The order of values in Severity enum should be from
  // least severe to most severe.
  enum class Severity { None, Verbose, Note, Warning, Error, Fatal };
  using DiagIDType = uint32_t;

  explicit DiagnosticEntry(
      DiagIDType id = std::numeric_limits<DiagIDType>::max(),
      const std::vector<std::string> &args = {})
      : m_DiagID(id), m_DiagArgs(args) {}

  explicit DiagnosticEntry(DiagIDType id, std::vector<std::string> &&args)
      : m_DiagID(id), m_DiagArgs(std::move(args)) {}

  explicit DiagnosticEntry(DiagIDType id, const std::vector<std::string> &args,
                           Severity severity);

  explicit DiagnosticEntry(DiagIDType id, std::vector<std::string> &&args,
                           Severity severity);

  /// Returns true if the object contains an error.
  explicit operator bool() const { return m_DiagID != NoDiag; }

  bool hasDiagnostic() const { return m_DiagID != NoDiag; }

  /// Returns diagnostic ID.
  DiagIDType diagID() const { return m_DiagID; }
  /// Returns diagnostic arguments.
  std::vector<std::string> &args() { return m_DiagArgs; }
  const std::vector<std::string> &args() const { return m_DiagArgs; }
  Severity severity() const;

  /// Returns true if the diagnostic entry represents a warning
  bool isWarning() const;

  /// Returns true if the diagnostic entry represents a verbose message
  bool isVerbose() const;

  /// Returns true if the diagnostic entry represents a note
  bool isNote() const;

  /// Returns true if the diagnostic entry represents an error
  bool isError() const;

  /// Returns true if the diagnostic entry represents a fatal error or
  /// fatal warning
  bool isFatal() const;

protected:
  DiagIDType m_DiagID = NoDiag;
  std::vector<std::string> m_DiagArgs;

private:
  static constexpr DiagIDType NoDiag = std::numeric_limits<DiagIDType>::max();
};

/// ErrorDiagnosticEntry subclass allows to easily create error diagnostic
/// entry.
class DLL_A_EXPORT ErrorDiagnosticEntry : public DiagnosticEntry {
public:
  ErrorDiagnosticEntry() = default;
  explicit ErrorDiagnosticEntry(DiagIDType id,
                                const std::vector<std::string> &args)
      : DiagnosticEntry(id, args, Severity::Error) {}
  explicit ErrorDiagnosticEntry(DiagIDType id, std::vector<std::string> &&args)
      : DiagnosticEntry(id, std::move(args), Severity::Error) {}
};

/// WarningDiagnosticEntry subclass allows to easily create warning diagnostic
/// entry.
class DLL_A_EXPORT WarningDiagnosticEntry : public DiagnosticEntry {
public:
  WarningDiagnosticEntry() = default;
  explicit WarningDiagnosticEntry(DiagIDType id,
                                  const std::vector<std::string> &args)
      : DiagnosticEntry(id, args, Severity::Warning) {}
  explicit WarningDiagnosticEntry(DiagIDType id,
                                  std::vector<std::string> &&args)
      : DiagnosticEntry(id, std::move(args), Severity::Warning) {}
};

/// FatalDiagnosticEntry subclass allows to easily create fatal diagnostic
/// entry.
class DLL_A_EXPORT FatalDiagnosticEntry : public DiagnosticEntry {
public:
  FatalDiagnosticEntry() = default;
  explicit FatalDiagnosticEntry(DiagIDType id,
                                const std::vector<std::string> &args)
      : DiagnosticEntry(id, args, Severity::Fatal) {}
  explicit FatalDiagnosticEntry(DiagIDType id, std::vector<std::string> &&args)
      : DiagnosticEntry(id, std::move(args), Severity::Fatal) {}
};

/// VerboseDiagnosticEntry subclass allows to easily create verbose diagnostic
/// entry.
class DLL_A_EXPORT VerboseDiagnosticEntry : public DiagnosticEntry {
public:
  VerboseDiagnosticEntry() = default;
  explicit VerboseDiagnosticEntry(DiagIDType id,
                                  const std::vector<std::string> &args)
      : DiagnosticEntry(id, args, Severity::Verbose) {}
  explicit VerboseDiagnosticEntry(DiagIDType id,
                                  std::vector<std::string> &&args)
      : DiagnosticEntry(id, std::move(args), Severity::Verbose) {}
};

/// NoteDiagnosticEntry subclass allows to easily create note diagnostic
/// entry.
class DLL_A_EXPORT NoteDiagnosticEntry : public DiagnosticEntry {
public:
  NoteDiagnosticEntry() = default;
  explicit NoteDiagnosticEntry(DiagIDType id,
                               const std::vector<std::string> &args)
      : DiagnosticEntry(id, args, Severity::Note) {}
  explicit NoteDiagnosticEntry(DiagIDType id, std::vector<std::string> &&args)
      : DiagnosticEntry(id, std::move(args), Severity::Note) {}
};
} // namespace eld::plugin

#endif
