//===- MsgHandler.h--------------------------------------------------------===//
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
#ifndef ELD_DIAGNOSTICS_MSGHANDLER_H
#define ELD_DIAGNOSTICS_MSGHANDLER_H
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Support/Path.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Twine.h"
#include <mutex>
#include <sstream>
#include <string>

#ifndef NDEBUG
#define ASSERT(condition, message)                                             \
  do {                                                                         \
    if (!(condition)) {                                                        \
      std::stringstream ss;                                                    \
      ss << "Assertion `" #condition "` failed in " << __FILE__ << " line "    \
         << __LINE__ << ": " << message;                                       \
      llvm::report_fatal_error(llvm::Twine(ss.str()));                         \
    }                                                                          \
  } while (false)
#else
#define ASSERT(condition, message)                                             \
  do {                                                                         \
    if (!(condition)) {                                                        \
      std::stringstream ss;                                                    \
      ss << "Assertion `" #condition "` failed with " << ": " << message;      \
      llvm::report_fatal_error(llvm::Twine(ss.str()));                         \
    }                                                                          \
  } while (false)
#endif

namespace eld {

/** \class MsgHandler
 *  \brief MsgHandler controls the timing to output message.
 */
class MsgHandler {
public:
  MsgHandler(DiagnosticEngine &pEngine, std::unique_lock<std::mutex> lock);
  ~MsgHandler();

  bool emit();

  void addString(llvm::StringRef pStr) const;

  void addString(const std::string &pStr) const;

  void addTaggedVal(intptr_t pValue,
                    DiagnosticEngine::ArgumentKind pKind) const;

private:
  void flushCounts() { m_Engine.state().numArgs = m_NumArgs; }

private:
  DiagnosticEngine &m_Engine;
  mutable unsigned int m_NumArgs;
  std::unique_lock<std::mutex> m_Lock;
};

inline const MsgHandler &operator<<(const MsgHandler &pHandler,
                                    llvm::StringRef pStr) {
  pHandler.addString(pStr);
  return pHandler;
}

inline const MsgHandler &operator<<(const MsgHandler &pHandler,
                                    const std::string &pStr) {
  pHandler.addString(pStr);
  return pHandler;
}

inline const MsgHandler &operator<<(const MsgHandler &pHandler,
                                    const sys::fs::Path &pPath) {
  pHandler.addString(pPath.native());
  return pHandler;
}

inline const MsgHandler &operator<<(const MsgHandler &pHandler,
                                    const char *pStr) {
  pHandler.addTaggedVal(reinterpret_cast<intptr_t>(pStr),
                        DiagnosticEngine::ak_c_string);
  return pHandler;
}

inline const MsgHandler &operator<<(const MsgHandler &pHandler, int pValue) {
  pHandler.addTaggedVal(pValue, DiagnosticEngine::ak_sint);
  return pHandler;
}

inline const MsgHandler &operator<<(const MsgHandler &pHandler,
                                    unsigned int pValue) {
  pHandler.addTaggedVal(pValue, DiagnosticEngine::ak_uint);
  return pHandler;
}

inline const MsgHandler &operator<<(const MsgHandler &pHandler, long pValue) {
  pHandler.addTaggedVal(pValue, DiagnosticEngine::ak_sint);
  return pHandler;
}

inline const MsgHandler &operator<<(const MsgHandler &pHandler,
                                    unsigned long pValue) {
  pHandler.addTaggedVal(pValue, DiagnosticEngine::ak_ulonglong);
  return pHandler;
}

inline const MsgHandler &operator<<(const MsgHandler &pHandler,
                                    unsigned long long pValue) {
  pHandler.addTaggedVal(pValue, DiagnosticEngine::ak_ulonglong);
  return pHandler;
}

inline const MsgHandler &operator<<(const MsgHandler &pHandler, bool pValue) {
  pHandler.addTaggedVal(pValue, DiagnosticEngine::ak_bool);
  return pHandler;
}

} // namespace eld

#endif
