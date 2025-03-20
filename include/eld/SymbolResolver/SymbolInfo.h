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
  SymbolInfo(const InputFile *InputFile, size_t Size,
             ResolveInfo::Binding Binding, ResolveInfo::Type SymType,
             ResolveInfo::Visibility Visibility, ResolveInfo::Desc SymDesc,
             bool IsBitcode);

  enum SymbolBinding { SB_None = 0, Local = 1, Global = 2, Weak = 3 };

  enum SectionIndexKind {
    SIK_None = 0,
    Undef = 1,
    Def = 2,
    Abs = 3,
    Common = 4
  };

  const InputFile *getInputFile() const { return SymbolOrigin; }

  SymbolBinding getSymbolBinding() const {
    return static_cast<SymbolBinding>(SymbolInfoBitfield.SymBinding);
  }

  ResolveInfo::Type getSymbolType() const {
    return static_cast<ResolveInfo::Type>(SymbolInfoBitfield.SymType);
  }

  ResolveInfo::Visibility getSymbolVisibility() const {
    return static_cast<ResolveInfo::Visibility>(
        SymbolInfoBitfield.SymVisibility);
  }

  SectionIndexKind getSymbolSectionIndexKind() const {
    return static_cast<SectionIndexKind>(SymbolInfoBitfield.SymSectIndexKind);
  }

  bool isBitcodeSymbol() const { return SymbolInfoBitfield.IsBitcode; }

  llvm::StringRef getSymbolBindingAsStr() const;

  llvm::StringRef getSymbolTypeAsStr() const;

  llvm::StringRef getSymbolVisibilityAsStr() const;

  llvm::StringRef getSymbolSectionIndexKindAsStr() const;

  size_t getSize() const { return SymbolSize; }

private:
  struct SymbolInfoBitField {
    SymbolInfoBitField()
        : SymBinding(0), SymType(0), SymVisibility(0), SymSectIndexKind(0),
          IsBitcode(0) {}
    unsigned int SymBinding : 2;
    unsigned int SymType : 2;
    unsigned int SymVisibility : 2;
    unsigned int SymSectIndexKind : 3;
    unsigned int IsBitcode : 1;
  };

  void setSymbolBinding(ResolveInfo::Binding Binding);
  void setSymbolType(ResolveInfo::Type SymType);
  void setSymbolVisibility(ResolveInfo::Visibility Visibility);
  void setSymbolSectionIndexKind(ResolveInfo::Binding Binding,
                                 ResolveInfo::Desc SymDesc);
  void setBitcodeAttribute(bool IsBitcode);
  /// Information is stored as follows in this bitfield:
  /// 0b000000000000000000000sssvvttttbb
  /// b: bits used to represent symbol binding.
  /// t: bits used to represent symbol type.
  /// v: bits used to represent symbol visibility.
  /// s: bits used to represent symbol section index kind.
  SymbolInfoBitField SymbolInfoBitfield;
  const InputFile *SymbolOrigin;
  size_t SymbolSize;
};
} // namespace eld

#endif