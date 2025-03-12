//===- SmallJSON.cpp-------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#include "eld/PluginAPI/SmallJSON.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/YAMLParser.h"

using namespace eld;
using namespace eld::plugin;

//------------------------------SmallJSONValue---------------------------------

SmallJSONValue::SmallJSONValue(SmallJSONObject &&Obj) {
  if (!Obj.isFinished())
    Obj.finish();
  m_Data = std::move(Obj.m_Data);
}
SmallJSONValue::SmallJSONValue(SmallJSONArray &&Arr) {
  if (!Arr.isFinished())
    Arr.finish();
  m_Data = std::move(Arr.m_Data);
}
SmallJSONValue::SmallJSONValue(bool b) { m_Data = b ? "true" : "false"; }
SmallJSONValue::SmallJSONValue(const std::string S)
    : m_Data("\"" + llvm::yaml::escape(S, /*EscapePrintable*/ true) + "\"") {}
SmallJSONValue::SmallJSONValue(std::nullptr_t null) : m_Data("\"null\"") {}
const std::string &SmallJSONValue::str() { return m_Data; }
const uint8_t *SmallJSONValue::getData() const {
  return reinterpret_cast<const uint8_t *>(m_Data.data());
}
eld::Expected<plugin::MemoryBuffer>
SmallJSONValue::getMemoryBuffer(const std::string &BufferName) {
  return plugin::MemoryBuffer::getBuffer(BufferName, getData(), m_Data.length(),
                                         true);
}

//------------------------------SmallJSONArray---------------------------------

SmallJSONArray::SmallJSONArray(SmallJSONArray &&Other) {
  this->m_Size = Other.m_Size;
  this->m_Data = std::move(Other.m_Data);
  this->m_IsFinished = Other.m_IsFinished;
}
SmallJSONArray::SmallJSONArray(const uint32_t InitialSize) {
  m_Data = "[";
  m_Data.reserve(InitialSize);
  m_Size = 0;
}
void SmallJSONArray::push_back(SmallJSONValue &&V) {
  if (m_Size)
    m_Data += ",";
  m_Data += V.m_Data;
  ++m_Size;
}

void SmallJSONArray::finish() {
  if (m_IsFinished)
    return;
  m_Data += "]";
  m_IsFinished = true;
}

bool SmallJSONArray::isFinished() const { return m_IsFinished; }

bool SmallJSONArray::empty() const { return m_Size == 0; }

//------------------------------SmallJSONObject--------------------------------

SmallJSONObject::SmallJSONObject(SmallJSONObject &&Other) {
  this->m_Size = Other.m_Size;
  this->m_Data = std::move(Other.m_Data);
  this->m_IsFinished = Other.m_IsFinished;
}

SmallJSONObject::SmallJSONObject(const uint32_t InitialSize) {
  m_Data = "{";
  m_Data.reserve(InitialSize);
  m_Size = 0;
}
void SmallJSONObject::insert(const std::string Key, SmallJSONValue &&V) {
  if (m_Size)
    m_Data += ",";
  m_Data += "\"";
  m_Data += llvm::yaml::escape(Key, /*EscapePrintable*/ true);
  m_Data += "\"";
  m_Data += ":";
  m_Data += V.m_Data;
  ++m_Size;
}

void SmallJSONObject::finish() {
  if (m_IsFinished)
    return;
  m_Data += "}";
  m_IsFinished = true;
}

bool SmallJSONObject::isFinished() const { return m_IsFinished; }

bool SmallJSONObject::empty() const { return m_Size == 0; }