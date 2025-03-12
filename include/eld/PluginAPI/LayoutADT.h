//===- LayoutADT.h---------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_PLUGINAPI_LAYOUTADT_H
#define ELD_PLUGINAPI_LAYOUTADT_H

#include "Defines.h"
#include <cstdint>
#include <ctype.h>
#include <string>

namespace eld::plugin {

struct DLL_A_EXPORT LinkerConfig;
struct DLL_A_EXPORT MapHeader;

struct DLL_A_EXPORT MapHeader {
  explicit MapHeader(const plugin::LinkerConfig &Config);
  std::string getVendorName() const;
  std::string getVendorVersion() const;
  std::string getELDVersion() const;
  std::string getLinkTypeString() const;
  // TODO: Add entry symbol when symbol struct is added.

private:
  const LinkerConfig &Config;
};

/// Padding details at section start, fragment and
/// between rules.
struct DLL_A_EXPORT Padding {
  uint64_t startOffset = 0;
  uint64_t size = 0;
  uint64_t fillValue = 0;
  bool isAlignment = false;
};

} // namespace eld::plugin

#endif