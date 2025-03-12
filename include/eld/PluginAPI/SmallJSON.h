//===- SmallJSON.h---------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_SMALL_JSON_H
#define ELD_SMALL_JSON_H

#include "Defines.h"
#include "PluginADT.h"
#include "PluginBase.h"
#include <cstdint>
#include <string>
namespace eld::plugin {

/// This file provides append-only data structures to efficiently build JSON
/// strings

struct DLL_A_EXPORT SmallJSONObject;
struct DLL_A_EXPORT SmallJSONArray;
struct DLL_A_EXPORT SmallJSONValue;

/// A Class representing a value representible in JSON. Includes Objects,
/// Arrays, boolean, floating point, integral, string, and null types
struct DLL_A_EXPORT SmallJSONValue {
public:
  /// Create a SmallJSONValue from a SmallJSONObject
  SmallJSONValue(SmallJSONObject &&Obj);
  /// Create a SmallJSONValue from SmallJSONArray
  SmallJSONValue(SmallJSONArray &&Arr);
  /// Create a SmallJSONValue representing true or false
  SmallJSONValue(bool b);
  /// Create a SmallJSONValue representing a JSON string
  SmallJSONValue(const std::string S);
  /// Create a SmallJSONValue representing a JSON null
  SmallJSONValue(std::nullptr_t null);
  /// Create a SmallJSONValue representing an integer type. Must be
  /// non-narrowing convertible to int64_t
  template <typename T, typename = std::enable_if_t<std::is_integral<T>::value>,
            typename = std::enable_if_t<!std::is_same<T, bool>::value>>
  SmallJSONValue(T I) : m_Data(std::to_string(I)) {}
  /// Create a SmallJSONValue representing a floating point type. Must be
  /// non-narrowing convertible to double
  template <typename T,
            typename = std::enable_if_t<std::is_floating_point<T>::value>,
            double * = nullptr>
  SmallJSONValue(T D) : m_Data(std::to_string(D)){};
  /// Get this JSONValue as a string in JSON format
  /// The string will be as compact as possible without any formatting or
  /// indentation
  const std::string &str();
  /// Get an unsigned integer pointer to raw data of the JSON object
  const uint8_t *getData() const;
  /// Get a memory buffer for containing data
  eld::Expected<plugin::MemoryBuffer>
  getMemoryBuffer(const std::string &BufferName);

private:
  friend struct SmallJSONArray;
  friend struct SmallJSONObject;
  std::string m_Data;
};

/// Represents a JSON Object consisting of key value pairs.
/// This class is append only. Items will appear in the order in which they were
/// inserted
struct DLL_A_EXPORT SmallJSONObject {

public:
  /// Move constructor
  SmallJSONObject(SmallJSONObject &&Other);
  /// Create a JSONObject and reserve InitialSize bytes for its string
  /// representation
  SmallJSONObject(const uint32_t InitialSize = 0);
  // Inserts the key value pair into the string represntation of this object
  // regardless of whether or not Key already exists. It is the caller's
  // responsbility to ensure uniqueness of the Key.
  void insert(const std::string Key, SmallJSONValue &&V);
  /// Insert the closing brace ('}') and complete the string representation of
  /// this object. Every SmallJSONObject must be finish()'ed in order to produce
  /// valid JSON.
  void finish();
  /// Has this Object been finish()'ed?
  bool isFinished() const;
  /// Is this object empty
  bool empty() const;

private:
  friend struct SmallJSONValue;
  bool m_IsFinished = false;
  uint32_t m_Size;
  std::string m_Data;
};

/// An append-only class represeting a heterogenous array of JSON values. Items
/// are stored in insertion order.
struct DLL_A_EXPORT SmallJSONArray {

public:
  /// Move constructor
  SmallJSONArray(SmallJSONArray &&Other);
  /// Create a SmallJSONArray and reserve InitialSize bytes for its string
  /// representation
  SmallJSONArray(const uint32_t InitialSize = 0);
  /// Add a value to the string representing this JSON Array
  void push_back(SmallJSONValue &&V);
  /// Insert the closing bracket (']') and complete the string representation of
  /// this Array. Every SmallJSONArray must be finish()'ed in order to produce
  /// valid JSON.
  void finish();
  /// Has this Array been finish()'ed?
  bool isFinished() const;
  /// Is this object empty?
  bool empty() const;

private:
  friend struct SmallJSONValue;
  bool m_IsFinished = false;
  uint32_t m_Size;
  std::string m_Data;
};

} // namespace eld::plugin

#endif
