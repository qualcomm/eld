//===- Path.h--------------------------------------------------------------===//
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

#ifndef ELD_SUPPORT_PATH_H
#define ELD_SUPPORT_PATH_H

#include "llvm/Support/raw_ostream.h"
#include <functional>
#include <iosfwd>
#include <locale>
#include <string>

namespace eld {
namespace sys {
namespace fs {

#if defined(ELD_ON_WIN32)
const char preferred_separator = '/';
const char separator = '/';
#else
const char preferred_separator = '/';
const char separator = '/';
#endif

const char colon = ':';
const char dot = '.';

/** \class Path
 *  \brief Path provides an abstraction for the path to a file or directory in
 *   the operating system's filesystem.
 */
class Path {
public:
  Path();
  Path(const std::string &s);
  Path(const Path &pCopy);
  virtual ~Path();

  // -----  assignments  ----- //
  Path &assign(const std::string &s);

  //  -----  appends  ----- //
  Path &append(const Path &pPath);

  //  -----  observers  ----- //
  bool empty() const;

  const std::string &native() const { return m_PathName; }
  std::string &native() { return m_PathName; }

  static std::string getFullPath(const std::string &path);

  std::string getFullPath() const { return getFullPath(m_PathName); }

  // -----  decomposition  ----- //
  Path filename() const;
  Path stem() const;
  Path extension() const;

protected:
  std::string m_PathName;
};
} // namespace fs
} // namespace sys
} // namespace eld
#endif
