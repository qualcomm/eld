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
    Fixup(DWord pOffset, SWord pAddend, Type pType)
        : m_Offset(pOffset), m_Addend(pAddend), m_Type(pType) {}

    ~Fixup() {}

    DWord offset() const { return m_Offset; }

    SWord addend() const { return m_Addend; }

    Type type() const { return m_Type; }

  private:
    DWord m_Offset;
    SWord m_Addend;
    Type m_Type;
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
  virtual Stub *clone(InputFile *, Relocation *R, Fragment *frag,
                      eld::IRBuilder *, DiagnosticEngine *) {
    return nullptr;
  }

  // Check if reloc is in range.
  virtual bool isRelocInRange(const Relocation *pReloc, int64_t targetAddr,
                              int64_t &Offset, Module &Module) const = 0;

  virtual bool isNeeded(const Relocation *pReloc, int64_t pTargetAddr,
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
  ResolveInfo *symInfo() { return m_pSymInfo; }

  const ResolveInfo *symInfo() const { return m_pSymInfo; }

  /// symValue - initial value for stub's symbol
  virtual uint64_t initSymValue() const { return 0x0; }

  /// getRealAddend - get the real value of addend
  virtual uint32_t getRealAddend(const Relocation &reloc,
                                 DiagnosticEngine *DiagEngine) const {
    return reloc.addend();
  }

  ///  -----  Fixup  -----  ///
  fixup_iterator fixup_begin() { return m_FixupList.begin(); }

  fixup_iterator fixup_end() { return m_FixupList.end(); }

  /// ----- modifiers ----- ///
  void setSymInfo(ResolveInfo *pSymInfo);

  void setSavedSymInfo(ResolveInfo *pSymInfo) { m_pSavedSymInfo = pSymInfo; }

  ResolveInfo *savedSymInfo() const { return m_pSavedSymInfo; }

  // Stub is a kind of Fragment with type of Stub
  static bool classof(const Fragment *F) {
    return F->getKind() == Fragment::Stub;
  }

  static bool classof(const Stub *) { return true; }

  virtual eld::Expected<void> emit(MemoryRegion &mr, Module &M) override;

  virtual bool isCompatible(Stub *S) const { return true; }

  virtual std::string getStubName(const Relocation &pReloc, bool isClone,
                                  bool isSectionRelative,
                                  int64_t numBranchIsland, int64_t numClone,
                                  uint32_t relocAddend,
                                  bool useOldStyleTrampolineName) const;

  std::string
  getTargetSymbolContextForReloc(const Relocation &reloc, uint32_t relocAddend,
                                 bool useOldStyleTrampolineName) const;

protected:
  /// addFixup - add a fixup for this stub to build a relocation
  void addFixup(DWord pOffset, SWord pAddend, Type pType);

  /// addFixup - add a fixup from a existing fixup of the prototype
  void addFixup(const Fixup &pFixup);

private:
  ResolveInfo *m_pSymInfo;
  ResolveInfo *
      m_pSavedSymInfo; // Saves the symbol for which the trampoline was created.
  FixupListType m_FixupList;

protected:
  size_t m_Size;
};

} // namespace eld

#endif
