//===- ELDDWARFObject.h----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_INPUT_ELDDWARFOBJECT_H
#define ELD_INPUT_ELDDWARFOBJECT_H

#include "llvm/DebugInfo/DWARF/DWARFObject.h"
#include "llvm/Support/Error.h"
#include <memory>

namespace eld {

class ELFObjectFile;

namespace detail {

llvm::Expected<std::unique_ptr<const llvm::DWARFObject>>
createELDDWARFObject(const ELFObjectFile &Input);

} // namespace detail
} // namespace eld

#endif // ELD_INPUT_ELDDWARFOBJECT_H
