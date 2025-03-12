//===- INIWriter.h---------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#ifndef ELD_SUPPORT_INIWRITER_H
#define ELD_SUPPORT_INIWRITER_H

#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include <map>
#include <sstream>

namespace eld {

/// \class INISection
/// \brief Represents a particular section within an ini file
class INISection {
public:
  INISection() = default;

  ~INISection() = default;

  /// Add a pair to this section
  /// \param item ini item key
  /// \returns the associated value for item as an std::string
  std::string &operator[](std::string item) {
    if (section.find(item) == section.end())
      section[item] = "";
    return section[item];
  }

  /// \returns layout of this INISection in the form of an std::ostream
  friend std::ostream &operator<<(std::ostream &os, INISection const &m) {
    for (auto pair : m.section)
      os << pair.first << '=' << pair.second << "\n";
    return os;
  }

private:
  std::map<std::string, std::string> section;
};

/// \class INIWriter
/// \brief ini file writer
class INIWriter {
public:
  INIWriter() = default;

  INIWriter(const INIWriter &) = delete;
  INIWriter &operator=(const INIWriter &) = delete;

  ~INIWriter() {
    for (auto &section : ini)
      delete section.second;
  }

  /// Utility function for writing output
  llvm::raw_ostream &outputStream() const {
    if (file) {
      file->flush();
      return *file;
    }
    return llvm::errs();
  }

  /// \returns layout of this ini file in the form of an std::ostream
  friend std::ostream &operator<<(std::ostream &os, INIWriter const &m) {
    for (auto &section : m.ini) {
      os << "[" << section.first().str() << "]\n";
      os << *section.second;
      os << "\n";
    }
    return os;
  }

  /// Write this ini file to disk
  /// \param filename output file name
  std::error_code writeFile(llvm::StringRef filename) {
    if (file == nullptr) {
      std::error_code error;
      file = std::unique_ptr<llvm::raw_fd_ostream>(
          new llvm::raw_fd_ostream(filename, error, llvm::sys::fs::OF_None));
      if (error)
        return error;
    }
    std::stringstream ss;
    ss << *this;
    outputStream() << ss.str();
    outputStream().flush();
    return std::error_code();
  }

  /// create a section in the ini file
  /// \param section name of the section
  /// \returns an INISection
  INISection &operator[](std::string section) {
    if (ini[section] == nullptr)
      ini[section] = new INISection();
    return *ini[section];
  }

private:
  llvm::StringMap<INISection *> ini;
  std::unique_ptr<llvm::raw_fd_ostream> file = nullptr;
};
} // namespace eld

#endif