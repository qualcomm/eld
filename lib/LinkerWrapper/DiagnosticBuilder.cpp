//===- DiagnosticBuilder.cpp-----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/PluginAPI/DiagnosticBuilder.h"
#include "eld/Diagnostics/MsgHandler.h"
using namespace eld::plugin;

DiagnosticBuilder::DiagnosticBuilder(eld::MsgHandler *msgHandler)
    : m_MsgHandler(msgHandler) {}

DiagnosticBuilder::~DiagnosticBuilder() { delete m_MsgHandler; }

const DiagnosticBuilder &
DiagnosticBuilder::operator<<(const std::string &pStr) const {
  *m_MsgHandler << pStr;
  return *this;
}
const DiagnosticBuilder &DiagnosticBuilder::operator<<(const char *pStr) const {
  *m_MsgHandler << pStr;
  return *this;
}
const DiagnosticBuilder &DiagnosticBuilder::operator<<(int pValue) const {
  *m_MsgHandler << pValue;
  return *this;
}
const DiagnosticBuilder &
DiagnosticBuilder::operator<<(unsigned int pValue) const {
  *m_MsgHandler << pValue;
  return *this;
}
const DiagnosticBuilder &DiagnosticBuilder::operator<<(long pValue) const {
  *m_MsgHandler << pValue;
  return *this;
}
const DiagnosticBuilder &
DiagnosticBuilder::operator<<(unsigned long pValue) const {
  *m_MsgHandler << pValue;
  return *this;
}
const DiagnosticBuilder &
DiagnosticBuilder::operator<<(unsigned long long pValue) const {
  *m_MsgHandler << pValue;
  return *this;
}
const DiagnosticBuilder &DiagnosticBuilder::operator<<(bool pValue) const {
  *m_MsgHandler << pValue;
  return *this;
}
