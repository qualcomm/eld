//===- LayoutWrapper.h-----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_PLUGINAPI_LAYOUTWRAPPER_H
#define ELD_PLUGINAPI_LAYOUTWRAPPER_H

#include "Defines.h"
#include "LayoutADT.h"
#include <string>
#include <vector>

namespace eld {
class LinkerConfig;
class Module;
} // namespace eld

namespace eld::plugin {

class LinkerWrapper;
struct OutputSection;
/// LayoutWrapper allows plugins to get the link-time information to populate
/// layout data in a map file.
class DLL_A_EXPORT LayoutWrapper {
public:
  explicit LayoutWrapper(const LinkerWrapper &Linker);

  /// Get the map header.
  const MapHeader getMapHeader() const;

  /// Get ABI page size.
  uint32_t getABIPageSize() const;
  /// Get target emulation.
  std::string getTargetEmulation() const;

  /// Get padding details for an output section.
  std::vector<Padding> getPaddings(OutputSection &Section);

  ~LayoutWrapper();

private:
  void recordPadding(std::vector<Padding> &Paddings, uint64_t startOffset,
                     uint64_t size, uint64_t fillValue, bool isAlignment);

private:
  const LinkerWrapper &Linker;
};
} // namespace eld::plugin

#endif
