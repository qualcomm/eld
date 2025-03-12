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
  LDSymbol(ResolveInfo *R = nullptr, bool isGC = false);

  ~LDSymbol();

  /// NullSymbol() - This returns a reference to a LDSymbol that represents Null
  /// symbol.
  static LDSymbol *Null();

  // -----  observers  ----- //
  bool isNull() const;

  const char *name() const {
    assert(nullptr != m_pResolveInfo);
    return m_pResolveInfo->name();
  }

  bool hasName() const {
    assert(nullptr != m_pResolveInfo);
    return !str().empty();
  }

  unsigned int nameSize() const {
    assert(nullptr != m_pResolveInfo);
    return m_pResolveInfo->nameSize();
  }

  llvm::StringRef str() const {
    assert(nullptr != m_pResolveInfo);
    return llvm::StringRef(m_pResolveInfo->name(), m_pResolveInfo->nameSize());
  }

  bool isDyn() const {
    assert(nullptr != m_pResolveInfo);
    return m_pResolveInfo->isDyn();
  }

  bool isSection() const {
    assert(nullptr != m_pResolveInfo);
    return m_pResolveInfo->isSection();
  }

  unsigned int type() const {
    assert(nullptr != m_pResolveInfo);
    return m_pResolveInfo->type();
  }
  unsigned int desc() const {
    assert(nullptr != m_pResolveInfo);
    return m_pResolveInfo->desc();
  }
  unsigned int binding() const {
    assert(nullptr != m_pResolveInfo);
    return m_pResolveInfo->binding();
  }

  uint8_t other() const {
    assert(nullptr != m_pResolveInfo);
    return m_pResolveInfo->other();
  }

  uint8_t visibility() const {
    assert(nullptr != m_pResolveInfo);
    return m_pResolveInfo->other();
  }

  ValueType value() const { return m_pResolveInfo->value(); }

  //  This is  method is used only for COMMONS
  // and is effective when GC ios turned on.
  bool shouldIgnore() const { return m_ShouldIgnore; }

  void setShouldIgnore(bool ignore = true) { m_ShouldIgnore = ignore; }

  bool scriptDefined() const { return m_ScriptDefined; }

  void setScriptDefined(bool value = true) { m_ScriptDefined = value; }

  bool scriptValueDefined() const { return m_ScriptValueDefined; }

  void setScriptValueDefined(bool value = true) {
    m_ScriptValueDefined = value;
  }

  const FragmentRef *fragRef() const { return m_pFragRef; }

  FragmentRef *fragRef() { return m_pFragRef; }

  SizeType size() const { return m_pResolveInfo->size(); }

  ResolveInfo *resolveInfo() { return m_pResolveInfo; }

  const ResolveInfo *resolveInfo() const { return m_pResolveInfo; }

  bool hasFragRef() const;

  bool hasFragRefSection() const;

  // -----  modifiers  ----- //
  void setSize(SizeType pSize) {
    assert(nullptr != m_pResolveInfo);
    m_pResolveInfo->setSize(pSize);
  }

  void setValue(ValueType pValue, bool isFinal = true) {
    m_pResolveInfo->setValue(pValue, isFinal);
  }

  void setFragmentRef(FragmentRef *pFragmentRef);

  void setResolveInfo(const ResolveInfo &pInfo);

  // Set the section index to record the symbol
  // from which section it came from
  void setSectionIndex(uint32_t shndx) { m_Shndx = shndx; }

  uint32_t sectionIndex() const { return m_Shndx; }

  uint32_t getSymbolIndex() const { return m_SymIdx; }

  void setSymbolIndex(uint32_t symIdx) { m_SymIdx = symIdx; }

  /// Casting support.
  static bool classof(const LDSymbol *E) { return true; }
  uint64_t getSymbolHash() const;

protected:
  // -----  Symbol's fields  ----- //
  ResolveInfo *m_pResolveInfo;
  FragmentRef *m_pFragRef;
  uint32_t m_Shndx;
  uint32_t m_SymIdx;
  bool m_ScriptDefined;
  bool m_ScriptValueDefined;
  // used for ignore garbage collected Common symbols
  bool m_ShouldIgnore;
};

} // namespace eld

#endif
