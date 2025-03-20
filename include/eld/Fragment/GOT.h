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
  ResolveInfo *symInfo() const { return ThisSymInfo; }

  /// ----- modifiers ----- ///
  void setSymInfo(ResolveInfo *PSymInfo);

  // Stub is a kind of Fragment with type of GOT.
  static bool classof(const Fragment *F) {
    return F->getKind() == Fragment::Got;
  }

  static bool classof(const GOT *) { return true; }

  virtual eld::Expected<void> emit(MemoryRegion &Mr, Module &M) override;

  void setValueType(ValueType V) { ThisValueType = V; }

  ValueType getValueType() const { return ThisValueType; }

  // -- GOT Type
  GOTType getType() const { return GotType; }

protected:
  ResolveInfo *ThisSymInfo;
  size_t ThisSize;
  ValueType ThisValueType;
  GOTType GotType;
};

} // namespace eld

#endif
