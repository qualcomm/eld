//===- SymbolInfo.h--------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_SYMBOLRESOLVER_SYMBOLINFO_H
#define ELD_SYMBOLRESOLVER_SYMBOLINFO_H

#include "eld/Input/InputFile.h"
#include "eld/SymbolResolver/ResolveInfo.h"
#include "llvm/ADT/StringRef.h"

namespace eld {
/// Stores symbol properties.
///
/// This class stores the below-mentioned symbol properties:
/// - Symbol binding
/// - Symbol type
/// - Symbol visibility
/// - Symbol section index kind
/// - Input file
class SymbolInfo {
public:
  SymbolInfo() = default;
  SymbolInfo(const InputFile *inputFile, size_t size,
             ResolveInfo::Binding binding, ResolveInfo::Type symType,
             ResolveInfo::Visibility visibility, ResolveInfo::Desc symDesc,
             bool isBitcode);

  enum SymbolBinding { SB_None = 0, Local = 1, Global = 2, Weak = 3 };

  enum SectionIndexKind {
    SIK_None = 0,
    Undef = 1,
    Def = 2,
    Abs = 3,
    Common = 4
  };

  const InputFile *getInputFile() const { return m_InputFile; }

  SymbolBinding getSymbolBinding() const {
    return static_cast<SymbolBinding>(m_SymbolInfoBitfield.symBinding);
  }

  ResolveInfo::Type getSymbolType() const {
    return static_cast<ResolveInfo::Type>(m_SymbolInfoBitfield.symType);
  }

  ResolveInfo::Visibility getSymbolVisibility() const {
    return static_cast<ResolveInfo::Visibility>(
        m_SymbolInfoBitfield.symVisibility);
  }

  SectionIndexKind getSymbolSectionIndexKind() const {
    return static_cast<SectionIndexKind>(m_SymbolInfoBitfield.symSectIndexKind);
  }

  bool isBitcodeSymbol() const { return m_SymbolInfoBitfield.isBitcode; }

  llvm::StringRef getSymbolBindingAsStr() const;

  llvm::StringRef getSymbolTypeAsStr() const;

  llvm::StringRef getSymbolVisibilityAsStr() const;

  llvm::StringRef getSymbolSectionIndexKindAsStr() const;

  size_t getSize() const { return m_Size; }

private:
  struct SymbolInfoBitField {
    SymbolInfoBitField()
        : symBinding(0), symType(0), symVisibility(0), symSectIndexKind(0),
          isBitcode(0) {}
    unsigned int symBinding : 2;
    unsigned int symType : 2;
    unsigned int symVisibility : 2;
    unsigned int symSectIndexKind : 3;
    unsigned int isBitcode : 1;
  };

  void setSymbolBinding(ResolveInfo::Binding binding);
  void setSymbolType(ResolveInfo::Type symType);
  void setSymbolVisibility(ResolveInfo::Visibility visibility);
  void setSymbolSectionIndexKind(ResolveInfo::Binding binding,
                                 ResolveInfo::Desc symDesc);
  void setBitcodeAttribute(bool isBitcode);
  /// Information is stored as follows in this bitfield:
  /// 0b000000000000000000000sssvvttttbb
  /// b: bits used to represent symbol binding.
  /// t: bits used to represent symbol type.
  /// v: bits used to represent symbol visibility.
  /// s: bits used to represent symbol section index kind.
  SymbolInfoBitField m_SymbolInfoBitfield;
  const InputFile *m_InputFile;
  size_t m_Size;
};
} // namespace eld

#endif