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

MsgHandler::MsgHandler(DiagnosticEngine &pEngine,
                       std::unique_lock<std::mutex> lock)
    : m_Engine(pEngine), m_NumArgs(0), m_Lock(std::move(lock)) {}

MsgHandler::~MsgHandler() { emit(); }

bool MsgHandler::emit() {
  flushCounts();
  return m_Engine.emit(std::move(m_Lock));
}

void MsgHandler::addString(llvm::StringRef pStr) const {
  assert(m_NumArgs < DiagnosticEngine::MaxArguments &&
         "Too many arguments to diagnostic!");
  m_Engine.state().ArgumentKinds[m_NumArgs] = DiagnosticEngine::ak_std_string;
  m_Engine.state().ArgumentStrs[m_NumArgs++] = pStr.str();
}

void MsgHandler::addString(const std::string &pStr) const {
  assert(m_NumArgs < DiagnosticEngine::MaxArguments &&
         "Too many arguments to diagnostic!");
  m_Engine.state().ArgumentKinds[m_NumArgs] = DiagnosticEngine::ak_std_string;
  m_Engine.state().ArgumentStrs[m_NumArgs++] = pStr;
}

void MsgHandler::addTaggedVal(intptr_t pValue,
                              DiagnosticEngine::ArgumentKind pKind) const {
  assert(m_NumArgs < DiagnosticEngine::MaxArguments &&
         "Too many arguments to diagnostic!");
  m_Engine.state().ArgumentKinds[m_NumArgs] = pKind;
  m_Engine.state().ArgumentVals[m_NumArgs++] = pValue;
}
