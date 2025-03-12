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
  Diagnostic(DiagnosticEngine &pEngine);

  ~Diagnostic();

  DiagnosticEngine::DiagIDType getID() const {
    assert(m_Engine.state().ID.has_value() && "Invalid diagnostic state!");
    return m_Engine.state().ID.value();
  }

  unsigned int getNumArgs() const { return m_Engine.state().numArgs; }

  DiagnosticEngine::ArgumentKind getArgKind(unsigned int pIdx) const {
    assert(pIdx < getNumArgs() && "Argument index is out of range!");
    return (DiagnosticEngine::ArgumentKind)m_Engine.state().ArgumentKinds[pIdx];
  }

  const std::string &getArgStdStr(unsigned int pIdx) const {
    assert(getArgKind(pIdx) == DiagnosticEngine::ak_std_string &&
           "Invalid argument accessor!");
    return m_Engine.state().ArgumentStrs[pIdx];
  }

  const char *getArgCStr(unsigned int pIdx) const {
    assert(getArgKind(pIdx) == DiagnosticEngine::ak_c_string &&
           "Invalid argument accessor!");
    return reinterpret_cast<const char *>(m_Engine.state().ArgumentVals[pIdx]);
  }

  int getArgSInt(unsigned int pIdx) const {
    assert(getArgKind(pIdx) == DiagnosticEngine::ak_sint &&
           "Invalid argument accessor!");
    return (int)m_Engine.state().ArgumentVals[pIdx];
  }

  unsigned int getArgUInt(unsigned int pIdx) const {
    assert(getArgKind(pIdx) == DiagnosticEngine::ak_uint &&
           "Invalid argument accessor!");
    return (unsigned int)m_Engine.state().ArgumentVals[pIdx];
  }

  unsigned long long getArgULongLong(unsigned long long pIdx) const {
    assert(getArgKind(pIdx) == DiagnosticEngine::ak_ulonglong &&
           "Invalid argument accessor!");
    return (unsigned long long)m_Engine.state().ArgumentVals[pIdx];
  }

  bool getArgBool(unsigned int pIdx) const {
    assert(getArgKind(pIdx) == DiagnosticEngine::ak_bool &&
           "Invalid argument accessor!");
    return (bool)m_Engine.state().ArgumentVals[pIdx];
  }

  intptr_t getRawVals(unsigned int pIdx) const {
    assert(getArgKind(pIdx) != DiagnosticEngine::ak_std_string &&
           "Invalid argument accessor!");
    return m_Engine.state().ArgumentVals[pIdx];
  }

  // format - format this diagnostic into string, subsituting the formal
  // arguments. The result is appended at on the pOutStr.
  eld::Expected<void> format(std::string &pOutStr) const;

  // format - format the given formal string, subsituting the formal
  // arguments. The result is appended at on the pOutStr.
  eld::Expected<void> format(const char *pBegin, const char *pEnd,
                             std::string &pOutStr) const;

  /// If the diagnostic is raised by a plugin, then return the name of the
  /// corresponding plugin; Otherwise return an empty string.
  std::string getPluginName() const {
    const Plugin *plugin = m_Engine.state().plugin;
    if (plugin)
      return plugin->getPluginName();
    return "";
  }

private:
  const char *findMatch(char pVal, const char *pBegin, const char *pEnd) const;

private:
  DiagnosticEngine &m_Engine;
};

} // namespace eld

#endif
