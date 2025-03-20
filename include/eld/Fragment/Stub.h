//===- Stub.h--------------------------------------------------------------===//
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

#ifndef ELD_FRAGMENT_STUB_H
#define ELD_FRAGMENT_STUB_H

#include "eld/Fragment/Fragment.h"
#include "eld/Readers/Relocation.h"
#include <string>
#include <vector>

namespace eld {

class Module;
class DiagnosticEngine;
class InputFile;
class IRBuilder;
class LinkerConfig;
class Relocation;
class ResolveInfo;

class Stub : public Fragment {
public:
  typedef Relocation::DWord DWord;
  typedef Relocation::SWord SWord;
  typedef Relocation::Type Type;
  class Fixup {
  public:
    Fixup(DWord POffset, SWord PAddend, Type PType)
        : MOffset(POffset), MAddend(PAddend), MType(PType) {}

    ~Fixup() {}

    DWord offset() const { return MOffset; }

    SWord addend() const { return MAddend; }

    Type type() const { return MType; }

  private:
    DWord MOffset;
    SWord MAddend;
    Type MType;
  };

public:
  typedef std::vector<Fixup *> FixupListType;
  typedef FixupListType::iterator fixup_iterator;
  typedef FixupListType::const_iterator const_fixup_iterator;

public:
  Stub();

  virtual ~Stub();

  /// clone - clone function for stub factory to create the corresponding stub
  virtual Stub *clone(InputFile *, Relocation *R, eld::IRBuilder *,
                      DiagnosticEngine *) = 0;

  /// clone - clone function for stub factory to create the corresponding stub
  /// from a fragment instead.
  virtual Stub *clone(InputFile *, Relocation *R, Fragment *Frag,
                      eld::IRBuilder *, DiagnosticEngine *) {
    return nullptr;
  }

  // Check if reloc is in range.
  virtual bool isRelocInRange(const Relocation *PReloc, int64_t TargetAddr,
                              int64_t &Offset, Module &Module) const = 0;

  virtual bool isNeeded(const Relocation *PReloc, int64_t PTargetAddr,
                        Module &Module) const {
    return false;
  }

  virtual bool supportsPIC() const { return false; }

  /// name - name of this stub
  virtual const std::string &name() const = 0;

  /// getContent - content of the stub
  virtual const uint8_t *getContent() const = 0;

  /// size - size of the stub + padding
  virtual size_t size() const override;

  /// symInfo - ResolveInfo of this Stub
  ResolveInfo *symInfo() { return ThisSymInfo; }

  const ResolveInfo *symInfo() const { return ThisSymInfo; }

  /// symValue - initial value for stub's symbol
  virtual uint64_t initSymValue() const { return 0x0; }

  /// getRealAddend - get the real value of addend
  virtual uint32_t getRealAddend(const Relocation &Reloc,
                                 DiagnosticEngine *DiagEngine) const {
    return Reloc.addend();
  }

  ///  -----  Fixup  -----  ///
  fixup_iterator fixupBegin() { return FixupList.begin(); }

  fixup_iterator fixupEnd() { return FixupList.end(); }

  /// ----- modifiers ----- ///
  void setSymInfo(ResolveInfo *CurSymInfo);

  void setSavedSymInfo(ResolveInfo *CurSymInfo) {
    ThisSavedSymInfo = CurSymInfo;
  }

  ResolveInfo *savedSymInfo() const { return ThisSavedSymInfo; }

  // Stub is a kind of Fragment with type of Stub
  static bool classof(const Fragment *F) {
    return F->getKind() == Fragment::Stub;
  }

  static bool classof(const Stub *) { return true; }

  virtual eld::Expected<void> emit(MemoryRegion &Mr, Module &M) override;

  virtual bool isCompatible(Stub *S) const { return true; }

  virtual std::string getStubName(const Relocation &PReloc, bool IsClone,
                                  bool IsSectionRelative,
                                  int64_t NumBranchIsland, int64_t NumClone,
                                  uint32_t RelocAddend,
                                  bool UseOldStyleTrampolineName) const;

  std::string
  getTargetSymbolContextForReloc(const Relocation &Reloc, uint32_t RelocAddend,
                                 bool UseOldStyleTrampolineName) const;

protected:
  /// addFixup - add a fixup for this stub to build a relocation
  void addFixup(DWord POffset, SWord PAddend, Type PType);

  /// addFixup - add a fixup from a existing fixup of the prototype
  void addFixup(const Fixup &PFixup);

private:
  ResolveInfo *ThisSymInfo;
  ResolveInfo *ThisSavedSymInfo; // Saves the symbol for which the trampoline
                                 // was created.
  FixupListType FixupList;

protected:
  size_t ThisSize;
};

} // namespace eld

#endif
