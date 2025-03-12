//===- LayoutADT.cpp-------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/PluginAPI/LayoutADT.h"
#include "eld/Config/Version.h"
#include "eld/PluginAPI/PluginADT.h"
#include <filesystem>

using namespace eld;
using namespace eld::plugin;

MapHeader::MapHeader(const plugin::LinkerConfig &Config) : Config(Config) {}

std::string MapHeader::getVendorName() const {
  return eld::getVendorName().str();
}

std::string MapHeader::getVendorVersion() const {
  return eld::getVendorVersion().str();
}

std::string MapHeader::getELDVersion() const {
  return eld::getELDVersion().str();
}

std::string MapHeader::getLinkTypeString() const {
  std::string linkType;
  if (Config.isDynamicLink()) {
    linkType = "Dynamic";
    if (Config.hasBSymbolic())
      linkType += " and Bsymbolic set";
    else
      linkType += " and Bsymbolic not set";
  } else
    linkType = "Static";
  return linkType;
}
