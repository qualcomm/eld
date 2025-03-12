//===- Path.inc------------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <limits.h>
#include <stdlib.h>
#include <linux/nfs.h>

std::string Path::getFullPath(const std::string &Path) {
  char ResolvedPath[PATH_MAX + 1];
  char *Result = ::realpath(Path.c_str(), ResolvedPath);
  if (Result)
    return std::string(Result);
  return Path;
}
