//===- Fragment.h----------------------------------------------------------===//
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

#ifndef ELD_FRAGMENT_FRAGMENT_H
#define ELD_FRAGMENT_FRAGMENT_H

#include "eld/PluginAPI/Expected.h"
#include "eld/Support/MemoryRegion.h"
#include "llvm/Support/raw_ostream.h"

namespace eld {

class DiagnosticEngine;
class ELFSection;
class LinkerConfig;
class Module;
class ResolveInfo;

/** \class Fragment
 *  \brief Fragment is the minimun linking unit of MCLinker.
 */
class Fragment {
public:
  /* Null means this fragment is removed */
  enum Type : uint8_t {
    Fillment,
    String,
    Region,
    RegionFragmentEx,
    Target,
    Stub,
    Got,
    OutputSectDataFragType,
    Plt,
    CIE,
    FDE,
    EhFrameHdr,
    Timing,
    Null,
    MergeString,
    BuildID,
    GNUVerDef,
    GNUVerSym,
  };

public:
  Fragment();

  Fragment(Type PKind, ELFSection *CurOwningSection = nullptr,
           uint32_t Align = 1);

  virtual ~Fragment();

  Type getKind() const { return Kind; }

  // Return the owning section that contains this fragment.
  ELFSection *getOwningSection() const { return OwningSection; }

  // Return the output section that this fragment is placed in.
  ELFSection *getOutputELFSection() const;

  uint32_t getOffset(DiagnosticEngine * = nullptr) const;

  /// offset will be adjusted automatically by alignment
  virtual void setOffset(uint32_t POffset);

  static constexpr uint32_t getPaddingValueSize(uint64_t PaddingValue) {
    return (PaddingValue > 0xFFFFFFFF
                ? 8
                : (PaddingValue > 0xFFFF ? 4 : (PaddingValue > 0xFF ? 2 : 1)));
  }

  bool hasOffset() const;

  static bool classof(const Fragment *O) { return true; }

  // size() gets called on the sentinel node too.
  virtual size_t size() const { return 0; }

  virtual bool isZeroSizedFrag() const { return !size(); }

  void setFragmentKind(Type T) { Kind = T; }

  // Get the virtual address of the fragment
  uint64_t getAddr(DiagnosticEngine *DiagEngine) const;

  // Helper functions for getPrevNode and getNextNode.
  Fragment *getPrevNode();
  Fragment *getNextNode();
  llvm::SmallVectorImpl<Fragment *>::iterator getIterator();

  size_t paddingSize() const;

  void setAlignment(size_t Align) {
    if (Align == 0)
      Align = 1;
    Alignment = Align;
  }

  virtual size_t alignment() const { return Alignment; }

  virtual eld::Expected<void> emit(MemoryRegion &Mr, Module &M) = 0;

  uint32_t getNewOffset(uint32_t Offset) const;

  void setOwningSection(ELFSection *O) { this->OwningSection = O; }

  virtual void dump(llvm::raw_ostream &Ostream) {}

  virtual void addSymbol(ResolveInfo *R) {}

  bool isMergeStr() const;

  bool isNull() const { return Kind == Null; }

  bool originatesFromPlugin(const Module &Module) const;

private:
  Fragment(const Fragment &);            // DO NOT IMPLEMENT
  Fragment &operator=(const Fragment &); // DO NOT IMPLEMENT

private:
  uint32_t UnalignedOffset;

protected:
  Type Kind;
  ELFSection *OwningSection;
  size_t Alignment;
};

} // namespace eld

#endif
