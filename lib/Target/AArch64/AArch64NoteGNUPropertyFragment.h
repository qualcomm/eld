//===- AArch64NoteGNUPropertyFragment.h------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#ifndef ELD_FRAGMENT_AARCH64_NOTE_GNU_PROPERTY_H
#define ELD_FRAGMENT_AARCH64_NOTE_GNU_PROPERTY_H

#include "eld/Fragment/TargetFragment.h"
#include "eld/Readers/ELFSection.h"
#include "llvm/ADT/SmallVector.h"
#include <string>

namespace eld {

class InputFile;
class LinkerConfig;
class GNULDBackend;

class AArch64NoteGNUPropertyFragment : public TargetFragment {
public:
  AArch64NoteGNUPropertyFragment(ELFSection *O);

  virtual ~AArch64NoteGNUPropertyFragment();

  /// name - name of this stub
  virtual const std::string name() const override;

  virtual size_t size() const override;

  static bool classof(const Fragment *F) {
    return F->getKind() == Fragment::Target;
  }

  static bool classof(const AArch64NoteGNUPropertyFragment *) { return true; }

  virtual eld::Expected<void> emit(MemoryRegion &mr, Module &M) override;

  bool updateInfo(GNULDBackend *G) override;

  virtual void dump(llvm::raw_ostream &OS) override;

  bool updateInfo(uint32_t featureSet);

  void resetFlag(uint32_t F) { featureSet &= ~F; }

protected:
  uint32_t featureSet = 0;
};

} // namespace eld

#endif
