//===- Path.cpp------------------------------------------------------------===//
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


#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "eld/Support/Path.h"
#include "eld/Config/Config.h"
#include "eld/Support/FileSystem.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Alignment.h"
#include <istream>
#include <locale>
#include <ostream>
#include <string.h>

using namespace eld;
using namespace eld::sys::fs;

//===--------------------------------------------------------------------===//
// Path
//===--------------------------------------------------------------------===//
Path::Path() : m_PathName() {}

Path::Path(const std::string &S) : m_PathName(S) {}

Path::Path(const Path &Copy) : m_PathName(Copy.m_PathName) {}

Path::~Path() {}

/// Replace the current path
/// \param s the string to replace the current pathname
/// \return a pointer to the current Path
Path &Path::assign(const std::string &S) {
  m_PathName.assign(S);
  return *this;
}

/// Append A file or directory name to the current Path
/// \param s The file name to append to the current Path
/// \return A pointer to the current Path
Path &Path::append(const Path &Path) {
  m_PathName.append(Path.native());
  return *this;
}

/// \return True if this Path is empty, false otherwise
bool Path::empty() const { return m_PathName.empty(); }

/// \return Path including the full file name + extension of the
/// file reffered to by this Path
Path Path::filename() const {
  size_t Pos = m_PathName.find_last_of(separator);
  if (Pos != std::string::npos) {
    ++Pos;
    return Path(m_PathName.substr(Pos));
  }
  return Path(*this);
}

/// \return Path of filename without extention
Path Path::stem() const {
  size_t BeginPos = m_PathName.find_last_of(separator) + 1;
  size_t EndPos = m_PathName.find_last_of(dot);
  Path ResultPath(m_PathName.substr(BeginPos, EndPos - BeginPos));
  return ResultPath;
}

/// \return Path containing The file extension of the current file in Path
Path Path::extension() const {
  size_t Pos = m_PathName.find_last_of('.');
  if (Pos == std::string::npos)
    return Path();
  return Path(m_PathName.substr(Pos));
}

#if defined(ELD_ON_MSVC)
#include "Windows/Path.inc"
#endif
#if defined(ELD_ON_UNIX)
#include "Unix/Path.inc"
#endif
