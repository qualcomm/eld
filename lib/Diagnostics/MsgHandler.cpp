//===- MsgHandler.cpp------------------------------------------------------===//
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
#include "eld/Diagnostics/MsgHandler.h"
#include "eld/Diagnostics/DiagnosticEngine.h"

using namespace eld;

MsgHandler::MsgHandler(DiagnosticEngine &PEngine,
                       std::unique_lock<std::timed_mutex> PLock)
    : DiagEngine(&PEngine), Lock(std::move(PLock)) {}

MsgHandler::~MsgHandler() { emit(); }

bool MsgHandler::emit() {
  if (!DiagEngine)
    return true;
  flushCounts();
  DiagnosticEngine *Engine = DiagEngine;
  DiagEngine = nullptr;
  return Engine->emit(std::move(Lock));
}

void MsgHandler::addString(llvm::StringRef PStr) const {
  if (!DiagEngine)
    return;
  assert(NumArgs < DiagnosticEngine::MaxArguments &&
         "Too many arguments to diagnostic!");
  DiagEngine->state().ArgumentKinds[NumArgs] = DiagnosticEngine::ak_std_string;
  DiagEngine->state().ArgumentStrs[NumArgs++] = PStr.str();
}

void MsgHandler::addString(const std::string &PStr) const {
  if (!DiagEngine)
    return;
  assert(NumArgs < DiagnosticEngine::MaxArguments &&
         "Too many arguments to diagnostic!");
  DiagEngine->state().ArgumentKinds[NumArgs] = DiagnosticEngine::ak_std_string;
  DiagEngine->state().ArgumentStrs[NumArgs++] = PStr;
}

void MsgHandler::addTaggedVal(intptr_t PValue,
                              DiagnosticEngine::ArgumentKind PKind) const {
  if (!DiagEngine)
    return;
  assert(NumArgs < DiagnosticEngine::MaxArguments &&
         "Too many arguments to diagnostic!");
  DiagEngine->state().ArgumentKinds[NumArgs] = PKind;
  DiagEngine->state().ArgumentVals[NumArgs++] = PValue;
}
