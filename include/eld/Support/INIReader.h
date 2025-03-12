//===- INIReader.h---------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#ifndef ELD_SUPPORT_INIREADER_H
#define ELD_SUPPORT_INIREADER_H

#include "eld/Input/Input.h"
#include "eld/Input/InputFile.h"
#include "eld/PluginAPI/Expected.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/Support/ErrorOr.h"
#include "llvm/Support/MemoryBuffer.h"
#include <map>
#include <ostream>

namespace eld {

/// \class INIReaderSection
/// \brief Represents a particular section within an ini file
class INIReaderSection {
public:
  INIReaderSection(std::string SectionName) : sectionName(SectionName) {}
  ~INIReaderSection() = default;

  std::string &operator[](std::string item) { // TODO make this private
    if (section.find(item) == section.end())
      section[item] = "";
    return section[item];
  }

  friend std::ostream &operator<<(std::ostream &os, INIReaderSection const &m) {
    for (auto pair : m.section)
      os << pair.first << '=' << pair.second << "\n";
    return os;
  }

  bool containsItem(std::string item) {
    return section.find(item) != section.end();
  }

  void addItem(std::string item, std::string value) { section[item] = value; }

  /// \returns a vector of all key value pairs in this section
  std::vector<std::pair<std::string, std::string>> getItems();

private:
  std::unordered_map<std::string, std::string> section;
  std::string sectionName;
};

/// \class INIReader`
/// \brief An ini file reader
class INIReader {
public:
  enum Kind {
    Comment,
    Error,
    KeyValue,
    None,
    Section,
  };

  INIReader(const std::string &FileName) : INIFileName(FileName) {}

  INIReader(const INIReader &) = delete;
  INIReader &operator=(const INIReader &) = delete;

  INIReader() = default;

  ~INIReader() {
    for (auto &s : INISections)
      delete s.second;
  }

  /// Read the file and return true if successful
  eld::Expected<bool> readINIFile();

  /// \returns The INIReaderSection associated with a section name
  /// \param section The requested ini section
  INIReaderSection &operator[](std::string section) {
    if (!INISections[section])
      INISections[section] = new INIReaderSection(section);
    return *INISections[section];
  }

  /// \return a representation of this ini file in memory, in the form of an
  /// std::ostream for debug purposes
  friend std::ostream &operator<<(std::ostream &os, INIReader &m) {
    for (auto &section : m.INISections) {
      os << section.first().str() << "\n";
      os << (*section.second);
    }
    return os;
  }

  /// \param section section name to check
  /// \returns true if section is found in file otherwise false
  bool containsSection(const std::string &section) const {
    return INISections.find(section) != INISections.end();
  }

  /// \param section section name to check
  /// \param item item name to check
  /// \returns true if a sepcified item is fonund in a given ini section
  /// otherwise false
  bool containsItem(const std::string &section, const std::string &item) {
    return containsSection(section) && INISections[section]->containsItem(item);
  }

  /// \returns vector of all section names in this file
  std::vector<std::string> getSections() {
    std::vector<std::string> sectionNames;
    for (llvm::StringRef s : INISections.keys())
      sectionNames.push_back(s.str());
    return sectionNames;
  }

  /// return true if file is non-empty
  explicit operator bool() const { return !empty(); }

  bool empty() const { return INISections.empty(); }

private:
  Kind getKind(llvm::StringRef S) const;

  /// Add a section
  INIReaderSection *addSection(llvm::StringRef Section);

  /// Add Values
  void addValues(INIReaderSection *S, llvm::StringRef KeyValue);

private:
  std::string INIFileName;
  llvm::StringMap<INIReaderSection *> INISections;
};
} // namespace eld

#endif
