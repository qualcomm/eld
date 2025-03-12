//===- ResolveInfo.h-------------------------------------------------------===//
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

#ifndef ELD_SYMBOLRESOLVER_RESOLVEINFO_H
#define ELD_SYMBOLRESOLVER_RESOLVEINFO_H

#include "eld/Input/ELFFileBase.h"
#include "eld/Input/Input.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/DataTypes.h"
#include <string>

namespace eld {

class GNULDBackend;
class InputFile;
class LDSymbol;
class LinkerConfig;

/** \class ResolveInfo
 *  \brief ResolveInfo records the information about how to resolve a symbol.
 *
 *  A symbol must have some `attributes':
 *  - Desc - Defined, Reference, Common
 *  - Binding - Global, Local, Weak
 *  - IsDyn - appear in dynamic objects or regular objects
 *  - Type - what the symbol refers to
 *  - Size  - the size of the symbol point to
 *  - Value - the pointer to another LDSymbol
 *  In order to save the memory and speed up the performance, FragmentLinker
 *  uses a bit field to store all attributes.
 *
 */
class ResolveInfo {
public:
  typedef uint32_t SizeType;

  /** \enum Type
   *  \brief What the symbol stand for
   */
  enum Type {
    NoType = 0,
    Object = 1,
    Function = 2,
    Section = 3,
    File = 4,
    CommonBlock = 5,
    ThreadLocal = 6,
    IndirectFunc = 10,
    LoProc = 13,
    HiProc = 15
  };

  /** \enum Desc
   *  \brief Description of the symbols.
   */
  enum Desc { Undefined = 0, Define = 1, Common = 2, Unused = 3, NoneDesc };

  enum Binding { Global = 0, Weak = 1, Local = 2, Absolute = 3, NoneBinding };

  enum Visibility { Default = 0, Internal = 1, Hidden = 2, Protected = 3 };

public:
  // ResolveInfo for LDSymbol::Null.
  static ResolveInfo *Null();

  // -----  modifiers  ----- //
  /// setRegular - set the source of the file is a regular object
  void setRegular();

  /// setDynamic - set the source of the file is a dynamic object
  void setDynamic();

  /// setSource - set the source of the file
  /// @param pIsDyn is the source from a dynamic object?
  void setSource(bool pIsDyn);

  void setType(uint32_t pType);

  void setDesc(uint32_t pDesc);

  void setBinding(uint32_t pBinding);

  void setOther(uint32_t pOther);

  void setVisibility(Visibility pVisibility);

  void setIsSymbol(bool pIsSymbol);

  void setReserved(uint32_t pReserved);

  void setSize(SizeType pSize) { m_Size = pSize; }

  void setValue(uint64_t pValue, bool isFinal) {
    if (!isFinal && isCommon()) {
      if (m_Value < pValue)
        m_Value = pValue;
      return;
    }
    m_Value = pValue;
  }

  void override(const ResolveInfo &pForm, bool pOverrideOrigin = true);

  void overrideAttributes(const ResolveInfo &pFrom);

  void overrideVisibility(const ResolveInfo &pFrom);

  /// setExportToDyn - set if the symbol should be exported to .dynsym
  void setExportToDyn();
  void clearExportToDyn();

  /// The resolve info sets this flag to bitcode.
  void setInBitCode(bool flag) {
    if (!flag)
      m_BitField &= ~inbitcode_flag;
    else
      m_BitField |= inbitcode_flag;
  }

  // Check if this symbol has to be preserved.
  bool shouldPreserve() const {
    return ((m_BitField & PRESERVE_MASK) == PRESERVE_MASK);
  }

  // The resolve info sets this flag to elf.
  void shouldPreserve(bool preserve) {
    if (preserve)
      m_BitField |= preserve_flag;
    else
      m_BitField &= ~preserve_flag;
  }

  void setPatchable() { m_BitField |= patchable_flag; }

  bool isPatchable() const {
    return ((m_BitField & PATCHABLE_MASK) == PATCHABLE_MASK);
  }

  // -----  observers  ----- //
  bool isNull() const;

  bool isSymbol() const;

  bool isString() const;

  bool isGlobal() const;

  bool isWeak() const;

  bool isWeakUndef() const { return isWeak() && isUndef(); }

  bool isLocal() const;

  bool isAbsolute() const;

  bool isDefine() const;

  bool isUndef() const;

  bool isDyn() const;

  bool isThreadLocal() const { return (type() == ResolveInfo::ThreadLocal); }

  bool isBitCode() const {
    return ((m_BitField & IN_BITCODE_MASK) == IN_BITCODE_MASK);
  }

  bool isFile() const { return (type() == ResolveInfo::File); }

  bool isSection() const { return (type() == ResolveInfo::Section); }

  bool isFunc() const { return (type() == ResolveInfo::Function); }

  bool isObject() const { return (type() == ResolveInfo::Object); }

  bool isNoType() const { return (type() == ResolveInfo::NoType); }

  bool isCommon() const;

  bool isHidden() const;

  bool isProtected() const;

  bool exportToDyn() const;

  uint32_t type() const;

  uint32_t desc() const;

  uint32_t binding() const;

  uint32_t reserved() const;

  uint8_t other() const { return (uint8_t)visibility(); }

  std::string infoAsString() const;

  Visibility visibility() const;

  llvm::StringRef getVisibilityString() const;

  bool hasContextualLabel() const;

  std::string getContextualLabel() const;

  LDSymbol *outSymbol() { return m_SymPtr; }

  LDSymbol *outSymbol() const { return m_SymPtr; }

  void setOutSymbol(LDSymbol *sym) { m_SymPtr = sym; }

  bool isAlias() const { return (m_Alias != nullptr); }

  void setAlias(ResolveInfo *alias) { m_Alias = alias; }

  ResolveInfo *alias() const { return m_Alias; }

  SizeType size() const { return m_Size; }

  uint64_t value() const { return m_Value; }

  const char *name() const { return m_Name.data(); }

  llvm::StringRef getName() const { return m_Name; }

  unsigned int nameSize() const { return m_Name.size(); }

  uint32_t info() const { return (m_BitField & INFO_MASK); }

  uint32_t bitfield() const { return m_BitField; }

  void setResolvedOrigin(InputFile *pInput);

  InputFile *resolvedOrigin() const { return m_pResolvedOrigin; }

  std::string getDecoratedName(bool DoDemangle) const;

  ELFSection *getOwningSection() const;

private:
  static const uint32_t GLOBAL_OFFSET = 0;
  static const uint32_t GLOBAL_MASK = 1;

  static const uint32_t DYN_OFFSET = 1;
  static const uint32_t DYN_MASK = 1 << DYN_OFFSET;

  static const uint32_t DESC_OFFSET = 2;
  static const uint32_t DESC_MASK = 0x3 << DESC_OFFSET;

  static const uint32_t LOCAL_OFFSET = 4;
  static const uint32_t LOCAL_MASK = 1 << LOCAL_OFFSET;

  static const uint32_t BINDING_MASK = GLOBAL_MASK | LOCAL_MASK;

  static const uint32_t VISIBILITY_OFFSET = 5;
  static const uint32_t VISIBILITY_MASK = 0x3 << VISIBILITY_OFFSET;

  static const uint32_t TYPE_OFFSET = 7;
  static const uint32_t TYPE_MASK = 0xF << TYPE_OFFSET;

  static const uint32_t SYMBOL_OFFSET = 11;
  static const uint32_t SYMBOL_MASK = 1 << SYMBOL_OFFSET;

  static const uint32_t RESERVED_OFFSET = 12;
  static const uint32_t RESERVED_MASK = 0xF << RESERVED_OFFSET;

  static const uint32_t EXPORT_DYN_OFFSET = 16;
  static const uint32_t EXPORT_DYN_MASK = 1 << EXPORT_DYN_OFFSET;

  static const uint32_t IN_BITCODE_OFFSET = 17;
  static const uint32_t IN_BITCODE_MASK = 1 << IN_BITCODE_OFFSET;

  static const uint32_t PRESERVE_OFFSET = 18;
  static const uint32_t PRESERVE_MASK = 1 << PRESERVE_OFFSET;

  // FIXME: offset 19 can be used here!
  static const uint32_t PATCHABLE_OFFSET = 20;
  static const uint32_t PATCHABLE_MASK = 1 << PATCHABLE_OFFSET;

  static const uint32_t INFO_MASK = 0xF;

  // Bits are from 0-20.
  static const uint32_t RESOLVE_MASK = 0x1FFFFF;

public:
  static const uint32_t global_flag = 0 << GLOBAL_OFFSET;
  static const uint32_t weak_flag = 1 << GLOBAL_OFFSET;
  static const uint32_t regular_flag = 0 << DYN_OFFSET;
  static const uint32_t dynamic_flag = 1 << DYN_OFFSET;
  static const uint32_t undefine_flag = 0 << DESC_OFFSET;
  static const uint32_t define_flag = 1 << DESC_OFFSET;
  static const uint32_t common_flag = 2 << DESC_OFFSET;
  static const uint32_t local_flag = 1 << LOCAL_OFFSET;
  static const uint32_t absolute_flag = BINDING_MASK;
  static const uint32_t object_flag = Object << TYPE_OFFSET;
  static const uint32_t function_flag = Function << TYPE_OFFSET;
  static const uint32_t section_flag = Section << TYPE_OFFSET;
  static const uint32_t file_flag = File << TYPE_OFFSET;
  static const uint32_t string_flag = 0 << SYMBOL_OFFSET;
  static const uint32_t symbol_flag = 1 << SYMBOL_OFFSET;
  static const uint32_t export_dyn_flag = 1 << EXPORT_DYN_OFFSET;
  static const uint32_t inbitcode_flag = 1 << IN_BITCODE_OFFSET;
  static const uint32_t preserve_flag = 1 << PRESERVE_OFFSET;
  static const uint32_t patchable_flag = 1 << PATCHABLE_OFFSET;
  ResolveInfo();
  ResolveInfo(llvm::StringRef pName);
  ~ResolveInfo();

private:
  SizeType m_Size;
  uint64_t m_Value;
  LDSymbol *m_SymPtr;
  uint32_t m_BitField;
  llvm::StringRef m_Name;
  ResolveInfo *m_Alias;
  InputFile *m_pResolvedOrigin;
};

} // namespace eld

#endif
