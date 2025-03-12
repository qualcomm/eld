//===- Section.cpp---------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Readers/Section.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Support/MsgHandling.h"
#include "llvm/ADT/StringRef.h"
#include <string>

using namespace eld;

llvm::StringRef Section::getSignatureForLinkOnceSection() const {
  ASSERT(isELF(),
         "Section " + std::string(name()) + " is called with not ELF kind ");
  ASSERT(llvm::dyn_cast<ELFSection>(this)->getKind() == LDFileFormat::LinkOnce,
         "Section " + std::string(name()) + " is not GNU linkonce!");
  // .gnu.linkonce + "." + type + "." + name
  llvm::StringRef sname(
      llvm::StringRef(name()).drop_front(strlen(".gnu.linkonce")));
  return sname.substr(sname.rfind(".") + 1);
}
