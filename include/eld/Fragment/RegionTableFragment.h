//===- RegionTableFragment.h-----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_FRAGMENT_REGIONTABLEFRAGMENT_H
#define ELD_FRAGMENT_REGIONTABLEFRAGMENT_H

#include "eld/Fragment/TargetFragment.h"
#include "eld/Readers/ELFSection.h"
#include "llvm/ADT/ArrayRef.h"
#include <string>
#include <vector>

namespace eld {
class LinkerConfig;
class GNULDBackend;

template <class ELFT> class RegionTableFragment : public TargetFragment {
public:
  typedef typename ELFT::Addr Elf_Addr;

  RegionTableFragment(ELFSection *O);

  virtual ~RegionTableFragment();

  /// name - name of this stub
  virtual const std::string name() const override;

  virtual size_t size() const override;

  static bool classof(const Fragment *F) {
    return F->getKind() == Fragment::Target;
  }

  static bool classof(const RegionTableFragment *) { return true; }

  virtual eld::Expected<void> emit(MemoryRegion &mr, Module &M) override;

  virtual bool updateInfo(GNULDBackend *G) override;

  virtual void dump(llvm::raw_ostream &OS) override;

protected:
  std::vector<std::pair<ELFSection *, ELFSection *>> Table;
  int NumEntries;
};

} // namespace eld

#endif
