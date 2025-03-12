//===- DiagnosticBuilder.h-------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_PLUGINAPI_DIAGNOSTICBUILDER_H
#define ELD_PLUGINAPI_DIAGNOSTICBUILDER_H

#include "Defines.h"
#include <string>
namespace eld {
class MsgHandler;
}
namespace eld::plugin {

/// A light-weight helper class to produce diagnostics.
///
/// This is created by LinkerWrapper::getDiagnosticBuilder, and it allows to
/// add arguments to the diagnostic in a cout style as described below:
///
/// \code
/// Linker->getDiagnosticBuilder(id) << arg1 << arg2
/// \endcode
///
/// DiagnosticBuilder object should always be used as a temporary object.
/// This ensures that the diagnostics are emitted when intended. This is
/// because diagnostics are emitted when the DiagnosicBuilder object is
/// destructed, and temporary objects are destructed when their parent
/// expression is fully evaluated.
/// On the other hand, if DiagnosticBuilder object is stored in a variable,
/// then the diagnostic won't be emitted until the variable goes out of scope.
/// For example:
///
/// \code
/// {
///   auto builder = Linker->getDiagnosticBuilder(id);
///   builder << arg1 << arg2;  // diagnostic would not be emitted at this
///   // ...                    // point.
///   // ...
//  } // diagnostic is emitted at this point.
/// \endcode
class DLL_A_EXPORT DiagnosticBuilder {
public:
  DiagnosticBuilder(eld::MsgHandler *msgHandler);
  /// DiagnosticBuilder should not be copied.
  /// If it is copied, then multiple DiagnosticBuilder object
  /// would represent the same diagnostic. This would lead to
  /// multiple DiagnosticBuilder object trying to emit the same diagnostic.
  DiagnosticBuilder(const DiagnosticBuilder &) = delete;
  DiagnosticBuilder(DiagnosticBuilder &&rhs) noexcept = default;
  ~DiagnosticBuilder();
  /// Add string argument to the diagnostic.
  const DiagnosticBuilder &operator<<(const std::string &pStr) const;
  /// Add C-style string argument to the diagnostic.
  const DiagnosticBuilder &operator<<(const char *pStr) const;
  /// Add integer argument to the diagnostic.
  const DiagnosticBuilder &operator<<(int pValue) const;
  /// Add unsigned int argument to the diagnostic.
  const DiagnosticBuilder &operator<<(unsigned int pValue) const;
  /// Add long int argument to the diagnostic.
  const DiagnosticBuilder &operator<<(long pValue) const;
  /// Add unsigned long argument to the diagnostic.
  const DiagnosticBuilder &operator<<(unsigned long pValue) const;
  /// Add unsigned long long argument to the diagnostic.
  const DiagnosticBuilder &operator<<(unsigned long long pValue) const;
  /// Add bool argument to the diagnostic.
  const DiagnosticBuilder &operator<<(bool pValue) const;

private:
  eld::MsgHandler *m_MsgHandler = nullptr;
};
} // namespace eld::plugin
#endif
