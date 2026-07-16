//===- DynStrFragment.h----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_FRAGMENT_DYNSTRFRAGMENT_H
#define ELD_FRAGMENT_DYNSTRFRAGMENT_H

#include "eld/Fragment/Fragment.h"
#include <optional>
#include <string>
#include <unordered_map>

namespace eld {

/// DynStrFragment holds the content of the .dynstr section.
///
/// Strings are appended via addString() and deduplicated. The section starts
/// with a leading null byte so that offset 0 means "no name", matching the
/// ELF string-table convention.
class DynStrFragment : public Fragment {
public:
  explicit DynStrFragment(ELFSection *S);

  static bool classof(const Fragment *F) {
    return F->getKind() == Fragment::Type::DynStr;
  }

  /// Add \p S to the table if not already present. Returns its byte offset.
  std::size_t addString(const std::string &S);

  /// Returns the byte offset of \p S, or nullopt if not present.
  std::optional<std::size_t> getStringOffset(const std::string &S) const;

  size_t size() const override;

  eld::Expected<void> emit(MemoryRegion &Mr, Module &M) override;

private:
  std::string Contents;
  std::unordered_map<std::string, std::size_t> Offsets;
};

} // namespace eld

#endif // ELD_FRAGMENT_DYNSTRFRAGMENT_H
