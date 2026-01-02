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
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/DataTypes.h"
#include <cstdint>
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
  static ResolveInfo *null();

  // -----  modifiers  ----- //
  /// setRegular - set the source of the file is a regular object
  void setRegular();

  /// setDynamic - set the source of the file is a dynamic object
  void setDynamic();

  /// setSource - set the source of the file
  /// @param pIsDyn is the source from a dynamic object?
  void setSource(bool IsSymbolInDynamicLibrary);

  void setType(uint32_t Type);

  void setDesc(uint32_t Desc);

  void setBinding(uint32_t Binding);

  void setOther(uint32_t Other);

  void setVisibility(Visibility Visibility);

  void setIsSymbol(bool IsSymbol);

  void setReserved(uint32_t Reserved);

  void setSize(SizeType Size) { SymbolSize = Size; }

  void setValue(uint64_t Value, bool IsFinal) {
    if (!IsFinal && isCommon()) {
      if (SymbolValue < Value)
        SymbolValue = Value;
      return;
    }
    SymbolValue = Value;
  }

  void override(const ResolveInfo &FromSymbol, bool CurOverrideOrigin = true);

  void overrideAttributes(const ResolveInfo &PFrom);

  void overrideVisibility(const ResolveInfo &PFrom);

  /// setExportToDyn - set if the symbol should be exported to .dynsym
  void setExportToDyn();
  void clearExportToDyn();

  /// The resolve info sets this flag to bitcode.
  void setInBitCode(bool Flag) {
    if (!Flag)
      ThisBitField &= ~InbitcodeFlag;
    else
      ThisBitField |= InbitcodeFlag;
  }

  // Check if this symbol has to be preserved.
  bool shouldPreserve() const {
    return ((ThisBitField & PreserveMask) == PreserveMask);
  }

  // The resolve info sets this flag to elf.
  void shouldPreserve(bool Preserve) {
    if (Preserve)
      ThisBitField |= PreserveFlag;
    else
      ThisBitField &= ~PreserveFlag;
  }

  void setPatchable() { ThisBitField |= PatchableFlag; }

  bool isPatchable() const {
    return ((ThisBitField & PatchableMask) == PatchableMask);
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
    return ((ThisBitField & InBitcodeMask) == InBitcodeMask);
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

  LDSymbol *outSymbol() { return OutputSymbol; }

  LDSymbol *outSymbol() const { return OutputSymbol; }

  void setOutSymbol(LDSymbol *Sym) { OutputSymbol = Sym; }

  bool isAlias() const { return (SymbolAlias != nullptr); }

  void setAlias(ResolveInfo *Alias) { SymbolAlias = Alias; }

  ResolveInfo *alias() const { return SymbolAlias; }

  SizeType size() const { return SymbolSize; }

  uint64_t value() const { return SymbolValue; }

  const char *name() const { return SymbolName.data(); }

#ifdef ELD_ENABLE_SYMBOL_VERSIONING
  llvm::StringRef getNonVersionedName() const {
    InputFile *origin = resolvedOrigin();
    if (origin && origin->isInternal()) {
      return SymbolName;
    }
    llvm::StringRef::size_type pos = SymbolName.find('@');
    return SymbolName.substr(0, pos);
  }

  llvm::StringRef getVersionName() const {
    auto pos = SymbolName.find_last_of('@');
    if (pos == std::string::npos)
      return "";
    return llvm::StringRef(SymbolName).substr(pos + 1);
  }
#endif

  llvm::StringRef getName() const { return SymbolName; }

  void setName(llvm::StringRef SymName) { SymbolName = SymName; }

  unsigned int nameSize() const { return SymbolName.size(); }

  uint32_t info() const { return (ThisBitField & InfoMask); }

  uint32_t bitfield() const { return ThisBitField; }

  void setResolvedOrigin(InputFile *Input);

  InputFile *resolvedOrigin() const { return SymbolResolvedOrigin; }

  std::string getDecoratedName(bool DoDemangle) const;

  ELFSection *getOwningSection() const;

  std::string getResolvedPath() const;

  bool canBePreemptible() const;

private:
  static const uint32_t GlobalOffset = 0;
  static const uint32_t GlobalMask = 1;

  static const uint32_t DynOffset = 1;
  static const uint32_t DynMask = 1 << DynOffset;

  static const uint32_t DescOffset = 2;
  static const uint32_t DescMask = 0x3 << DescOffset;

  static const uint32_t LocalOffset = 4;
  static const uint32_t LocalMask = 1 << LocalOffset;

  static const uint32_t BindingMask = GlobalMask | LocalMask;

  static const uint32_t VisibilityOffset = 5;
  static const uint32_t VisibilityMask = 0x3 << VisibilityOffset;

  static const uint32_t TypeOffset = 7;
  static const uint32_t TypeMask = 0xF << TypeOffset;

  static const uint32_t SymbolOffset = 11;
  static const uint32_t SymbolMask = 1 << SymbolOffset;

  static const uint32_t ReservedOffset = 12;
  static const uint32_t ReservedMask = 0xF << ReservedOffset;

  static const uint32_t ExportDynOffset = 16;
  static const uint32_t ExportDynMask = 1 << ExportDynOffset;

  static const uint32_t InBitcodeOffset = 17;
  static const uint32_t InBitcodeMask = 1 << InBitcodeOffset;

  static const uint32_t PreserveOffset = 18;
  static const uint32_t PreserveMask = 1 << PreserveOffset;

  // FIXME: offset 19 can be used here!
  static const uint32_t PatchableOffset = 20;
  static const uint32_t PatchableMask = 1 << PatchableOffset;

  static const uint32_t InfoMask = 0xF;

  // Bits are from 0-20.
  static const uint32_t ResolveMask = 0x1FFFFF;

public:
  static const uint32_t GlobalFlag = 0 << GlobalOffset;
  static const uint32_t WeakFlag = 1 << GlobalOffset;
  static const uint32_t RegularFlag = 0 << DynOffset;
  static const uint32_t DynamicFlag = 1 << DynOffset;
  static const uint32_t UndefineFlag = 0 << DescOffset;
  static const uint32_t DefineFlag = 1 << DescOffset;
  static const uint32_t CommonFlag = 2 << DescOffset;
  static const uint32_t LocalFlag = 1 << LocalOffset;
  static const uint32_t AbsoluteFlag = BindingMask;
  static const uint32_t ObjectFlag = Object << TypeOffset;
  static const uint32_t FunctionFlag = Function << TypeOffset;
  static const uint32_t SectionFlag = Section << TypeOffset;
  static const uint32_t FileFlag = File << TypeOffset;
  static const uint32_t StringFlag = 0 << SymbolOffset;
  static const uint32_t SymbolFlag = 1 << SymbolOffset;
  static const uint32_t ExportDynFlag = 1 << ExportDynOffset;
  static const uint32_t InbitcodeFlag = 1 << InBitcodeOffset;
  static const uint32_t PreserveFlag = 1 << PreserveOffset;
  static const uint32_t PatchableFlag = 1 << PatchableOffset;
  ResolveInfo();
  ResolveInfo(llvm::StringRef SymbolName);
  ~ResolveInfo();

private:
  SizeType SymbolSize;
  uint64_t SymbolValue;
  LDSymbol *OutputSymbol;
  uint32_t ThisBitField;
  llvm::StringRef SymbolName;
  ResolveInfo *SymbolAlias;
  InputFile *SymbolResolvedOrigin;
};

} // namespace eld

#endif
