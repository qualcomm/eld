//===- Version.h-----------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief Defines version macros and version-related utility functions
/// for eld.
///
//===----------------------------------------------------------------------===//

#ifndef ELD_CONFIG_VERSION_H
#define ELD_CONFIG_VERSION_H

#include "eld/Config/Version.inc"
#include "llvm/ADT/StringRef.h"
#include <string>

namespace eld {
/// \brief Retrieves the repository path (e.g., Subversion path) that
/// identifies the particular eld branch, tag, or trunk from which this
/// eld was built.
llvm::StringRef getELDRepositoryPath();
llvm::StringRef getLLVMRepositoryPath();

/// \brief Retrieves the repository revision number (or identifer) from which
/// this eld was built.
llvm::StringRef getELDRevision();
llvm::StringRef getLLVMRevision();

/// \brief Retrieves the full repository version that is an amalgamation of
/// the information in getELDRepositoryPath() and getELDRevision().
std::string getELDRepositoryVersion();
std::string getLLVMRepositoryVersion();

/// \brief Returns true if 'LLVM_REPOSITORY' and 'LLVM_REVISION'
/// macros are defined and have non-empty values.
bool isLLVMRepositoryInfoAvailable();

/// \brief Retrieves a string representing the complete eld version.
llvm::StringRef getELDVersion();
llvm::StringRef getLLVMVersion();

llvm::StringRef getVendorName();
llvm::StringRef getVendorVersion();
} // namespace eld

#endif // ELD_CONFIG_VERSION_H
