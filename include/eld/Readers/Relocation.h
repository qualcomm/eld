//===- Relocation.h--------------------------------------------------------===//
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
#ifndef ELD_READERS_RELOCATION_H
#define ELD_READERS_RELOCATION_H
#include "eld/Config/Config.h"
#include "eld/Fragment/FragmentRef.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Support/DataTypes.h"
#include <unordered_map>

namespace eld {

class GeneralOptions;
class Module;
class ResolveInfo;
class Relocator;
class LinkerConfig;
class ELFSection;
class ELFSegment;

class Relocation {
  static llvm::DenseMap<const Relocation *, FragmentRef *> m_RelocFragMap;

public:
  typedef uint64_t Address; // FIXME: use SizeTrait<T>::Address instead
  typedef uint64_t DWord;   // FIXME: use SizeTrait<T>::Word instead
  typedef int64_t SWord;    // FIXME: use SizeTrait<T>::SWord instead
  typedef uint32_t Size;
  typedef uint32_t Type;

  Relocation();

  Relocation(Type pType, FragmentRef *pTargetRef, Address pAddend,
             DWord pTargetData);

  Relocation(const Relocator *Relocator, Type pType, FragmentRef *pTargetRef,
             Address pAddend);

  ~Relocation();

public:
  /// Create - produce an empty relocation entry
  static Relocation *Create();

  static Relocation *Create(Type pType, Size pSize, FragmentRef *pFragRef,
                            Address pAddend = 0);

  /// Destroy - destroy a relocation entry
  static void Destroy(Relocation *&pRelocation);

  /// type - relocation type
  Type type() const { return m_Type; }

  /// symValue - S value - the symbol address
  Address symValue(Module &M) const;

  /// symOffset - Offset of the symbol
  Address symOffset(Module &M) const;

  // Return the relocation target section.
  ELFSection *targetSection() const;

  // Return the relocation output section.
  ELFSection *outputSection() const;

  /// addend - A value
  Address addend() const { return m_Addend; }

  /// place - P value - address of the place being relocated
  Address place(Module &M) const;

  /// size - the size of the relocation in bit
  Size size(Relocator &pRelocator) const;

  /// symbol info - binding, type
  ResolveInfo *symInfo() const { return m_pSymInfo; }

  /// target - the target data to relocate
  const DWord &target() const { return m_TargetData; }

  DWord &target() { return m_TargetData; }

  /// targetRef - the reference of the target data
  FragmentRef *targetRef() const { return m_TargetAddress; }

  void setTargetRef(FragmentRef *ref) { m_TargetAddress = ref; }

  size_t getOffset() const;

  /// Returns true if the relocation is successfully applied; Otherwise returns
  /// false.
  bool apply(Relocator &pRelocator);

  std::string getSourcePath(const GeneralOptions &options) const;

  std::string getTargetPath(const GeneralOptions &options) const;

  /// updateAddend - A relocation with a section symbol must update addend
  /// before reading its value.
  void updateAddend(Module &M);

  /// ----- modifiers ----- ///
  void setType(Type pType);

  void setAddend(Address pAddend);

  void setSymInfo(ResolveInfo *pSym);

  void setTargetData(DWord pTargetData);

  void modifyRelocationFragmentRef(FragmentRef *fragRef) {
    if (!fragRef)
      return;
    m_RelocFragMap[this] = fragRef;
  }

  FragmentRef *targetFragRef() const {
    auto it = m_RelocFragMap.find(this);
    if (it == m_RelocFragMap.end())
      return nullptr;
    return it->second;
  }

  static std::string getFragmentPath(ResolveInfo *symInfo, Fragment *frag,
                                     const GeneralOptions &options);

  static std::string getSymbolName(const ResolveInfo *R, bool shouldDemangle);

  static std::string getSectionName(const ELFSection *S,
                                    const GeneralOptions &options);

  ELFSegment *getTargetSegment() const;

  bool shouldUsePLTAddr() const;

  bool isMergeKind() const;

  bool issueOverflow(Relocator &);

private:
  /// m_pSymInfo - resolved symbol info of relocation target symbol
  ResolveInfo *m_pSymInfo;

  /// m_TargetAddress - FragmentRef of the place being relocated
  FragmentRef *m_TargetAddress;

  /// m_Addend - the addend
  Address m_Addend;

  /// m_TargetData - target data of the place being relocated
  DWord m_TargetData;

  /// m_Type - the type of the relocation entries
  Type m_Type;
};

} // namespace eld

#endif
