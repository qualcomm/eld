//===- FileSystem.h--------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
#ifndef ELD_SUPPORT_FILESYSTEM_H
#define ELD_SUPPORT_FILESYSTEM_H

#include "eld/Config/Config.h"
#include "llvm/ADT/StringRef.h"
#include <string>
#include <system_error>
#include <vector>

namespace eld {
namespace sys {
namespace fs {

std::error_code loadFileContents(llvm::StringRef filePath,
                                 std::vector<std::string> &Lines);

} // namespace fs
} // namespace sys
} // namespace eld

#endif
