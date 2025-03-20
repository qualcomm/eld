//===- Diagnostic.h--------------------------------------------------------===//
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
#ifndef ELD_DIAGNOSTICS_DIAGNOSTIC_H
#define ELD_DIAGNOSTICS_DIAGNOSTIC_H

#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/PluginAPI/Expected.h"
#include "eld/Script/Plugin.h"
#include <optional>
#include <string>

namespace eld {

/** \class Diagnostic
 *  \brief Diagnostic provides current status to DiagnosticPrinters.
 */
class Diagnostic {
public:
  Diagnostic(DiagnosticEngine &PEngine);

  ~Diagnostic();

  DiagnosticEngine::DiagIDType getID() const {
    assert(DiagEngine.state().ID.has_value() && "Invalid diagnostic state!");
    return DiagEngine.state().ID.value();
  }

  unsigned int getNumArgs() const { return DiagEngine.state().NumArgs; }

  DiagnosticEngine::ArgumentKind getArgKind(unsigned int PIdx) const {
    assert(PIdx < getNumArgs() && "Argument index is out of range!");
    return (DiagnosticEngine::ArgumentKind)DiagEngine.state()
        .ArgumentKinds[PIdx];
  }

  const std::string &getArgStdStr(unsigned int PIdx) const {
    assert(getArgKind(PIdx) == DiagnosticEngine::ak_std_string &&
           "Invalid argument accessor!");
    return DiagEngine.state().ArgumentStrs[PIdx];
  }

  const char *getArgCStr(unsigned int PIdx) const {
    assert(getArgKind(PIdx) == DiagnosticEngine::ak_c_string &&
           "Invalid argument accessor!");
    return reinterpret_cast<const char *>(
        DiagEngine.state().ArgumentVals[PIdx]);
  }

  int getArgSInt(unsigned int PIdx) const {
    assert(getArgKind(PIdx) == DiagnosticEngine::ak_sint &&
           "Invalid argument accessor!");
    return (int)DiagEngine.state().ArgumentVals[PIdx];
  }

  unsigned int getArgUInt(unsigned int PIdx) const {
    assert(getArgKind(PIdx) == DiagnosticEngine::ak_uint &&
           "Invalid argument accessor!");
    return (unsigned int)DiagEngine.state().ArgumentVals[PIdx];
  }

  unsigned long long getArgULongLong(unsigned long long PIdx) const {
    assert(getArgKind(PIdx) == DiagnosticEngine::ak_ulonglong &&
           "Invalid argument accessor!");
    return (unsigned long long)DiagEngine.state().ArgumentVals[PIdx];
  }

  bool getArgBool(unsigned int PIdx) const {
    assert(getArgKind(PIdx) == DiagnosticEngine::ak_bool &&
           "Invalid argument accessor!");
    return (bool)DiagEngine.state().ArgumentVals[PIdx];
  }

  intptr_t getRawVals(unsigned int PIdx) const {
    assert(getArgKind(PIdx) != DiagnosticEngine::ak_std_string &&
           "Invalid argument accessor!");
    return DiagEngine.state().ArgumentVals[PIdx];
  }

  // format - format this diagnostic into string, subsituting the formal
  // arguments. The result is appended at on the pOutStr.
  eld::Expected<void> format(std::string &POutStr) const;

  // format - format the given formal string, subsituting the formal
  // arguments. The result is appended at on the pOutStr.
  eld::Expected<void> format(const char *PBegin, const char *PEnd,
                             std::string &POutStr) const;

  /// If the diagnostic is raised by a plugin, then return the name of the
  /// corresponding plugin; Otherwise return an empty string.
  std::string getPluginName() const {
    const Plugin *Plugin = DiagEngine.state().Plugin;
    if (Plugin)
      return Plugin->getPluginName();
    return "";
  }

private:
  const char *findMatch(char PVal, const char *PBegin, const char *PEnd) const;

private:
  DiagnosticEngine &DiagEngine;
};

} // namespace eld

#endif
