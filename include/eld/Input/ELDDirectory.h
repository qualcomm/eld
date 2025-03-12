//===- ELDDirectory.h------------------------------------------------------===//
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

#ifndef ELD_INPUT_ELDDIRECTORY_H
#define ELD_INPUT_ELDDIRECTORY_H
#include "eld/Support/FileSystem.h"
#include "llvm/ADT/StringRef.h"
#include <string>

namespace eld {

/** \class ELDDirectory
 *  \brief ELDDirectory is an directory entry for library search.
 *
 */
class ELDDirectory {
public:
  explicit ELDDirectory(llvm::StringRef pName, std::string sysRoot);
  virtual ~ELDDirectory();

public:
  bool isInSysroot() const { return m_bInSysroot; }

  bool getIsFound() const { return m_isFound; }

  void setIsFound(bool isFound) { m_isFound = isFound; }

  const std::string &name() const { return m_Name; }

private:
  std::string m_Name;
  bool m_bInSysroot;
  bool m_isFound;
};

} // namespace eld

#endif
