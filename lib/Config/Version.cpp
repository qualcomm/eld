//===- Version.cpp---------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
//===----------------------------------------------------------------------===//
//
// This file defines several version-related utility functions for ELD.
//
//===----------------------------------------------------------------------===//

#include "eld/Config/Version.h"
#include "eld/Config/Version.inc"
#include "llvm/Support/VCSRevision.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdlib>
#include <cstring>

using namespace llvm;

/// \brief Helper macro for ELD_VERSION_STRING.
#define ELD_MAKE_VERSION_STRING2(X) #X

/// \brief Helper macro for ELD_VERSION_STRING.
#define ELD_MAKE_VERSION_STRING(X, Y) ELD_MAKE_VERSION_STRING2(X.Y)

/// \brief A string that describes the eld version number, e.g., "1.0".
#define ELD_VERSION_STRING                                                     \
  ELD_MAKE_VERSION_STRING(ELD_VERSION_MAJOR, ELD_VERSION_MINOR)

namespace eld {

StringRef getELDRepositoryPath() {
#ifdef ELD_REPOSITORY_STRING
  return ELD_REPOSITORY_STRING;
#else
  return "";
#endif
}

StringRef getELDRevision() {
#ifdef ELD_REVISION_STRING
  return ELD_REVISION_STRING;
#else
  return "";
#endif
}

StringRef getLLVMRepositoryPath() {
#ifdef LLVM_REPOSITORY
  return LLVM_REPOSITORY;
#else
  return "";
#endif
}

StringRef getLLVMRevision() {
#ifdef LLVM_REVISION
  return LLVM_REVISION;
#else
  return "";
#endif
}

StringRef getVendorName() {
#ifdef LLVM_VENDOR_NAME
  return LLVM_VENDOR_NAME;
#else
  return "";
#endif
}

StringRef getVendorVersion() {
#ifdef LLVM_VENDOR_VERSION
  return LLVM_VENDOR_VERSION;
#else
  return "";
#endif
}

StringRef getELDVersion() {
#ifdef ELD_VERSION_STRING
  return ELD_VERSION_STRING;
#else
  return "";
#endif
}

StringRef getLLVMVersion() {
#ifdef LLVM_VERSION_STRING
  return LLVM_VERSION_STRING;
#else
  return "";
#endif
}

std::string getELDRepositoryVersion() {
  std::string Buf;
  llvm::raw_string_ostream OS(Buf);
  std::string Path = getELDRepositoryPath().str();
  std::string Revision = getELDRevision().str();
  if (!Path.empty() || !Revision.empty()) {
    OS << '(';
    if (!Path.empty())
      OS << Path;
    if (!Revision.empty()) {
      if (!Path.empty())
        OS << ' ';
      OS << Revision;
    }
    OS << ')';
  }
  return OS.str();
}

std::string getLLVMRepositoryVersion() {
  std::string Buf;
  llvm::raw_string_ostream OS(Buf);
  std::string Path = getLLVMRepositoryPath().str();
  std::string Revision = getLLVMRevision().str();
  if (!Path.empty() || !Revision.empty()) {
    OS << '(';
    if (!Path.empty())
      OS << Path;
    if (!Revision.empty()) {
      if (!Path.empty())
        OS << ' ';
      OS << Revision;
    }
    OS << ')';
  }
  return OS.str();
}

bool isLLVMRepositoryInfoAvailable() {
  if (getLLVMRepositoryPath().empty() || getLLVMRepositoryVersion().empty())
    return false;
  return true;
}

} // end namespace eld
