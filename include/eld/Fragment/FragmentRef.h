//===- FragmentRef.h-------------------------------------------------------===//
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

#ifndef ELD_FRAGMENT_FRAGMENTREF_H
#define ELD_FRAGMENT_FRAGMENTREF_H

#include "eld/Config/Config.h"
#include "llvm/Support/DataTypes.h"

namespace eld {

class Module;
class DiagnosticEngine;
class ELFSection;
class Fragment;
class Layout;
class OutputSectionEntry;

/** \class FragmentRef
 *  \brief FragmentRef is a reference of a Fragment's contetnt.
 *
 */
class FragmentRef {
public:
  typedef uint64_t Offset; // FIXME: use SizeTraits<T>::Offset

public:
  static FragmentRef *null();

  static FragmentRef *discard();

  FragmentRef();

  FragmentRef(Fragment &PFrag, Offset POffset = 0);

  /// memcpy - copy memory
  /// copy memory from the fragment to the pDesc.
  /// @pDest - the destination address
  /// @pNBytes - copies pNBytes from the fragment[offset()+pOffset]
  /// @pOffset - additional offset.
  ///            the start address offset from fragment[offset()]
  void memcpy(void *PDest, size_t PNBytes, Offset POffset = 0) const;

  uint32_t getWordAtDest() const {
    uint32_t Dest;
    memcpy(&Dest, sizeof(Dest), 0);
    return Dest;
  }

  // -----  observers  ----- //
  bool isNull() const { return (this == null()); }

  bool isDiscard() const { return this == discard(); }

  Fragment *frag() const { return ThisFragment; }

  void setFragment(Fragment *Frag) { ThisFragment = Frag; }

  Offset offset() const { return ThisOffset; }

  void setOffset(Offset Offset) { ThisOffset = Offset; }

  Offset getOutputOffset(Module &M) const;

  ELFSection *getOutputELFSection() const;

  OutputSectionEntry *getOutputSection() const;

private:
  Fragment *ThisFragment;
  Offset ThisOffset;

  static FragmentRef GNullFragmentRef;
  static FragmentRef GDiscardFragmentRef;
};

} // namespace eld

#endif
