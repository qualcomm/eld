//===- LDSymbol.h----------------------------------------------------------===//
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

#ifndef ELD_SYMBOLRESOLVER_LDSYMBOL_H
#define ELD_SYMBOLRESOLVER_LDSYMBOL_H

#include "eld/SymbolResolver/ResolveInfo.h"

namespace eld {

class FragmentRef;
class Input;
class ELFSection;

class LDSymbol {
public:
  // FIXME: use SizeTrait<32> or SizeTrait<64> instead of big type
  typedef ResolveInfo::SizeType SizeType;
  typedef uint64_t ValueType;

public:
  LDSymbol(ResolveInfo *R = nullptr, bool IsGc = false);

  ~LDSymbol();

  /// NullSymbol() - This returns a reference to a LDSymbol that represents Null
  /// symbol.
  static LDSymbol *null();

  // -----  observers  ----- //
  bool isNull() const;

  const char *name() const {
    assert(nullptr != ThisResolveInfo);
    return ThisResolveInfo->name();
  }

  std::string getNonVersionedName() const {
    assert(ThisResolveInfo != nullptr);
    return ThisResolveInfo->getNonVersionedName();
  }

  bool hasName() const {
    assert(nullptr != ThisResolveInfo);
    return !str().empty();
  }

  unsigned int nameSize() const {
    assert(nullptr != ThisResolveInfo);
    return ThisResolveInfo->nameSize();
  }

  llvm::StringRef str() const {
    assert(nullptr != ThisResolveInfo);
    return llvm::StringRef(ThisResolveInfo->name(),
                           ThisResolveInfo->nameSize());
  }

  bool isDyn() const {
    assert(nullptr != ThisResolveInfo);
    return ThisResolveInfo->isDyn();
  }

  bool isSection() const {
    assert(nullptr != ThisResolveInfo);
    return ThisResolveInfo->isSection();
  }

  unsigned int type() const {
    assert(nullptr != ThisResolveInfo);
    return ThisResolveInfo->type();
  }
  unsigned int desc() const {
    assert(nullptr != ThisResolveInfo);
    return ThisResolveInfo->desc();
  }
  unsigned int binding() const {
    assert(nullptr != ThisResolveInfo);
    return ThisResolveInfo->binding();
  }

  uint8_t other() const {
    assert(nullptr != ThisResolveInfo);
    return ThisResolveInfo->other();
  }

  uint8_t visibility() const {
    assert(nullptr != ThisResolveInfo);
    return ThisResolveInfo->other();
  }

  ValueType value() const { return ThisResolveInfo->value(); }

  //  This is  method is used only for COMMONS
  // and is effective when GC ios turned on.
  bool shouldIgnore() const { return ThisSymbolGarbageCollected; }

  void setShouldIgnore(bool Ignore = true) {
    ThisSymbolGarbageCollected = Ignore;
  }

  bool scriptDefined() const { return ThisSymbolsIsScriptDefined; }

  void setScriptDefined(bool Value = true) {
    ThisSymbolsIsScriptDefined = Value;
  }

  bool scriptValueDefined() const { return ThisSymbolHasScriptValue; }

  void setScriptValueDefined(bool Value = true) {
    ThisSymbolHasScriptValue = Value;
  }

  const FragmentRef *fragRef() const { return ThisFragRef; }

  FragmentRef *fragRef() { return ThisFragRef; }

  SizeType size() const { return ThisResolveInfo->size(); }

  ResolveInfo *resolveInfo() { return ThisResolveInfo; }

  const ResolveInfo *resolveInfo() const { return ThisResolveInfo; }

  bool hasFragRef() const;

  bool hasFragRefSection() const;

  // -----  modifiers  ----- //
  void setSize(SizeType Size) {
    assert(nullptr != ThisResolveInfo);
    ThisResolveInfo->setSize(Size);
  }

  void setValue(ValueType Value, bool IsFinal = true) {
    ThisResolveInfo->setValue(Value, IsFinal);
  }

  void setFragmentRef(FragmentRef *CurFragmentRef);

  void setResolveInfo(const ResolveInfo &CurInfo);

  // Set the section index to record the symbol
  // from which section it came from
  void setSectionIndex(uint32_t Shndx) { ThisShndx = Shndx; }

  uint32_t sectionIndex() const { return ThisShndx; }

  uint32_t getSymbolIndex() const { return ThisSymIdx; }

  void setSymbolIndex(uint32_t SymIdx) { ThisSymIdx = SymIdx; }

  /// Casting support.
  static bool classof(const LDSymbol *E) { return true; }
  uint64_t getSymbolHash() const;

protected:
  // -----  Symbol's fields  ----- //
  ResolveInfo *ThisResolveInfo;
  FragmentRef *ThisFragRef;
  uint32_t ThisShndx;
  uint32_t ThisSymIdx;
  bool ThisSymbolsIsScriptDefined;
  bool ThisSymbolHasScriptValue;
  // used for ignore garbage collected Common symbols
  bool ThisSymbolGarbageCollected;
};

} // namespace eld

#endif
