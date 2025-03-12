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
  static FragmentRef *Null();

  static FragmentRef *Discard();

  FragmentRef();

  FragmentRef(Fragment &pFrag, Offset pOffset = 0);

  /// memcpy - copy memory
  /// copy memory from the fragment to the pDesc.
  /// @pDest - the destination address
  /// @pNBytes - copies pNBytes from the fragment[offset()+pOffset]
  /// @pOffset - additional offset.
  ///            the start address offset from fragment[offset()]
  void memcpy(void *pDest, size_t pNBytes, Offset pOffset = 0) const;

  uint32_t getWordAtDest() const {
    uint32_t dest;
    memcpy(&dest, sizeof(dest), 0);
    return dest;
  }

  // -----  observers  ----- //
  bool isNull() const { return (this == Null()); }

  bool isDiscard() const { return this == Discard(); }

  Fragment *frag() const { return m_pFragment; }

  void setFragment(Fragment *frag) { m_pFragment = frag; }

  Offset offset() const { return m_Offset; }

  void setOffset(Offset offset) { m_Offset = offset; }

  Offset getOutputOffset(Module &M) const;

  ELFSection *getOutputELFSection() const;

  OutputSectionEntry *getOutputSection() const;

private:
  Fragment *m_pFragment;
  Offset m_Offset;

  static FragmentRef g_NullFragmentRef;
  static FragmentRef g_DiscardFragmentRef;
};

} // namespace eld

#endif
