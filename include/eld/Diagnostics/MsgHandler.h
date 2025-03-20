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
  MsgHandler(DiagnosticEngine &PEngine, std::unique_lock<std::mutex> Lock);
  ~MsgHandler();

  bool emit();

  void addString(llvm::StringRef PStr) const;

  void addString(const std::string &PStr) const;

  void addTaggedVal(intptr_t PValue,
                    DiagnosticEngine::ArgumentKind PKind) const;

private:
  void flushCounts() { DiagEngine.state().NumArgs = NumArgs; }

private:
  DiagnosticEngine &DiagEngine;
  mutable unsigned int NumArgs;
  std::unique_lock<std::mutex> Lock;
};

inline const MsgHandler &operator<<(const MsgHandler &PHandler,
                                    llvm::StringRef PStr) {
  PHandler.addString(PStr);
  return PHandler;
}

inline const MsgHandler &operator<<(const MsgHandler &PHandler,
                                    const std::string &PStr) {
  PHandler.addString(PStr);
  return PHandler;
}

inline const MsgHandler &operator<<(const MsgHandler &Handler,
                                    const sys::fs::Path &Path) {
  Handler.addString(Path.native());
  return Handler;
}

inline const MsgHandler &operator<<(const MsgHandler &Handler,
                                    const char *Str) {
  Handler.addTaggedVal(reinterpret_cast<intptr_t>(Str),
                       DiagnosticEngine::ak_c_string);
  return Handler;
}

inline const MsgHandler &operator<<(const MsgHandler &PHandler, int PValue) {
  PHandler.addTaggedVal(PValue, DiagnosticEngine::ak_sint);
  return PHandler;
}

inline const MsgHandler &operator<<(const MsgHandler &Handler,
                                    unsigned int Value) {
  Handler.addTaggedVal(Value, DiagnosticEngine::ak_uint);
  return Handler;
}

inline const MsgHandler &operator<<(const MsgHandler &Handler, long Value) {
  Handler.addTaggedVal(Value, DiagnosticEngine::ak_sint);
  return Handler;
}

inline const MsgHandler &operator<<(const MsgHandler &Handler,
                                    unsigned long Value) {
  Handler.addTaggedVal(Value, DiagnosticEngine::ak_ulonglong);
  return Handler;
}

inline const MsgHandler &operator<<(const MsgHandler &Handler,
                                    unsigned long long Value) {
  Handler.addTaggedVal(Value, DiagnosticEngine::ak_ulonglong);
  return Handler;
}

inline const MsgHandler &operator<<(const MsgHandler &Handler, bool Value) {
  Handler.addTaggedVal(Value, DiagnosticEngine::ak_bool);
  return Handler;
}

} // namespace eld

#endif
