//===- GOT.h---------------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_FRAGMENT_GOT_H
#define ELD_FRAGMENT_GOT_H

#include "eld/Fragment/Fragment.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/Support/DataTypes.h"
#include <string>
#include <vector>

namespace eld {

class LinkerConfig;
class ResolveInfo;

class GOT : public Fragment {
public:
  // GOT Types.
  enum GOTType {
    Regular,  /* Regular GOT Slots */
    GOTPLT0,  /* GOT slot for PLT0 */
    GOTPLTN,  /* GOT slot for PLTN */
    TLS_DESC, /* TLS DESC */
    TLS_GD,   /* GD GOT Slots */
    TLS_LD,   /* LD GOT Slots */
    TLS_IE,   /* IE GOT Slots */
    TLS_LE    /* LE GOT Slots */
  };

  enum ValueType { Default, SymbolValue, TLSStaticSymbolValue };

public:
  GOT(GOTType T, ELFSection *O, ResolveInfo *R, uint32_t Align, uint32_t Size);

  virtual ~GOT();

  /// name - name of this stub
  virtual const std::string name() const;

  /// getContent - content of the stub
  virtual llvm::ArrayRef<uint8_t> getContent() const = 0;

  virtual size_t size() const override;

  /// symInfo - ResolveInfo of this GOT.
  ResolveInfo *symInfo() const { return m_pSymInfo; }

  /// ----- modifiers ----- ///
  void setSymInfo(ResolveInfo *pSymInfo);

  // Stub is a kind of Fragment with type of GOT.
  static bool classof(const Fragment *F) {
    return F->getKind() == Fragment::Got;
  }

  static bool classof(const GOT *) { return true; }

  virtual eld::Expected<void> emit(MemoryRegion &mr, Module &M) override;

  void setValueType(ValueType V) { m_ValueType = V; }

  ValueType getValueType() const { return m_ValueType; }

  // -- GOT Type
  GOTType getType() const { return m_GOTType; }

protected:
  ResolveInfo *m_pSymInfo;
  size_t m_Size;
  ValueType m_ValueType;
  GOTType m_GOTType;
};

} // namespace eld

#endif
