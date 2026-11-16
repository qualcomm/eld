//===- DiagnosticLocation.h-----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_DIAGNOSTICS_DIAGNOSTICLOCATION_H
#define ELD_DIAGNOSTICS_DIAGNOSTICLOCATION_H

#include <cstdint>
#include <string>

namespace eld {

/// Source location attached to a diagnostic independently of its arguments.
struct DiagnosticLocation {
  std::string FileName;
  uint32_t Line = 0;
  uint32_t Column = 0;

  bool isValid() const { return !FileName.empty() && Line != 0; }
};

} // namespace eld

#endif // ELD_DIAGNOSTICS_DIAGNOSTICLOCATION_H
