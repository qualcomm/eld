//===- TargetFragment.h----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
//
//                     The MCLinker Project
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef ELD_FRAGMENT_TARGETFRAGMENT_H
#define ELD_FRAGMENT_TARGETFRAGMENT_H

#include "eld/Fragment/Fragment.h"
#include "llvm/ADT/ArrayRef.h"
#include <string>

namespace eld {
class LinkerConfig;
class ResolveInfo;
class GNULDBackend;

class TargetFragment : public Fragment {
public:
  enum Kind : uint8_t {
    Attributes,
    GNUHash,
    NoteGNUProperty,
    RegionTable,
    SysVHash,
    TargetSpecific,
  };

  TargetFragment(Kind K, ELFSection *O, ResolveInfo *R, uint32_t Align,
                 uint32_t Size = 0);

  virtual ~TargetFragment();

  /// name - name of this stub
  virtual const std::string name() const;

  /// getContent - content of the stub
  virtual llvm::ArrayRef<uint8_t> getContent() const {
    return llvm::ArrayRef<uint8_t>();
  }

  virtual size_t size() const override;

  Kind targetFragmentKind() const { return TargetKind; }

  /// symInfo - ResolveInfo of this GOT.
  ResolveInfo *symInfo() const { return SymInfo; }

  /// ----- modifiers ----- ///
  void setSymInfo(ResolveInfo *PSymInfo);

  // Stub is a kind of Fragment with type of GOT.
  static bool classof(const Fragment *F) {
    return F->getKind() == Fragment::Target;
  }

  static bool classof(const TargetFragment *) { return true; }

  virtual eld::Expected<void> emit(MemoryRegion &Mr, Module &M) override;

  virtual bool updateInfo(GNULDBackend *G) { return false; }

protected:
  ResolveInfo *SymInfo;
  Kind TargetKind;
  size_t ThisSize;
};

} // namespace eld

#endif
