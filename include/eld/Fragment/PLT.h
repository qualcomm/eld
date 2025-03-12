//===- PLT.h---------------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_FRAGMENT_PLT_H
#define ELD_FRAGMENT_PLT_H

#include "eld/Fragment/Fragment.h"
#include "eld/Fragment/GOT.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/Support/DataTypes.h"
#include <string>
#include <vector>

namespace eld {

class LinkerConfig;
class ResolveInfo;
class GOT;

class PLT : public Fragment {
public:
  // PLT Types.
  enum PLTType {
    PLT0, /* Lazy Binding PLT */
    PLTN, /* PLTn */
  };

public:
  PLT(PLTType T, GOT *G, ELFSection *O, ResolveInfo *R, uint32_t Align,
      uint32_t Size);

  virtual ~PLT();

  /// name - name of this stub
  virtual const std::string name() const;

  /// getContent - content of the stub
  virtual llvm::ArrayRef<uint8_t> getContent() const = 0;

  virtual size_t size() const override;

  /// symInfo - ResolveInfo of this PLT.
  ResolveInfo *symInfo() const { return m_pSymInfo; }

  /// ----- modifiers ----- ///
  void setSymInfo(ResolveInfo *pSymInfo);

  // Stub is a kind of Fragment with type of PLT.
  static bool classof(const Fragment *F) {
    return F->getKind() == Fragment::Plt;
  }

  static bool classof(const PLT *) { return true; }

  virtual eld::Expected<void> emit(MemoryRegion &mr, Module &M) override;

  // -- PLT Type
  PLTType getType() const { return m_PLTType; }

  // Get the GOT for the PLT.
  GOT *getGOT() const { return m_GOT; }

protected:
  GOT *m_GOT;
  ResolveInfo *m_pSymInfo;
  size_t m_Size;
  PLTType m_PLTType;
};

} // namespace eld

#endif
