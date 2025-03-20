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
  explicit ELDDirectory(llvm::StringRef PName, std::string SysRoot);
  virtual ~ELDDirectory();

public:
  bool isInSysroot() const { return BInSysroot; }

  bool getIsFound() const { return IsFound; }

  void setIsFound(bool PIsFound) { IsFound = PIsFound; }

  const std::string &name() const { return Name; }

private:
  std::string Name;
  bool BInSysroot;
  bool IsFound;
};

} // namespace eld

#endif
