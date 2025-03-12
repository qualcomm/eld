//===- StringFragment.h----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_FRAGMENT_STRINGFRAGMENT_H
#define ELD_FRAGMENT_STRINGFRAGMENT_H

#include "eld/Fragment/Fragment.h"

namespace eld {
class LinkerConfig;
/** \class StringFragment
 *  \brief StringFragment is a kind of Fragment containing strings.
 *  The difference between the RegionFragment and the string fragment is that
 *  the strings are terminated with \0.
 */
class StringFragment : public Fragment {
public:
  StringFragment(std::string str, ELFSection *O = nullptr);

  ~StringFragment();

  static bool classof(const Fragment *F) {
    return F->getKind() == Fragment::String;
  }

  std::string getString() const { return m_String; }

  static bool classof(const StringFragment *) { return true; }

  size_t size() const override;

  virtual eld::Expected<void> emit(MemoryRegion &mr, Module &M) override;

private:
  std::string m_String;
};

} // namespace eld

#endif
