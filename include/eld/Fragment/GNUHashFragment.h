//===- GNUHashFragment.h---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_FRAGMENT_GNUHASHFRAGMENT_H
#define ELD_FRAGMENT_GNUHASHFRAGMENT_H

#include "eld/Fragment/TargetFragment.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/Object/ELF.h"
#include "llvm/Support/DataTypes.h"
#include <string>
#include <vector>

namespace eld {

class LinkerConfig;
class ResolveInfo;

template <class ELFT> class GNUHashFragment : public TargetFragment {
public:
  GNUHashFragment(ELFSection *O, std::vector<ResolveInfo *> &R);

  virtual ~GNUHashFragment();

  /// name - name of this stub
  virtual const std::string name() const override;

  virtual size_t size() const override;

  static bool classof(const Fragment *F) {
    return F->getKind() == Fragment::Target;
  }

  static bool classof(const GNUHashFragment *) { return true; }

  virtual eld::Expected<void> emit(MemoryRegion &mr, Module &M) override;

private:
  unsigned calcNBuckets(unsigned NumHashed) const;

  unsigned calcMaskWords(unsigned NumHashed) const;

  void writeHeader(uint8_t *&Buf);

  void writeBloomFilter(uint8_t *&Buf);

  void writeHashTable(uint8_t *&Buf);

  uint32_t hashGnu(llvm::StringRef Name) const {
    uint32_t H = 5381;
    for (uint8_t C : Name)
      H = (H << 5) + H + C;
    return H;
  }

  // Add symbols to this symbol hash table. Note that this function
  // destructively sort a given vector -- which is needed because
  // GNU-style hash table places some sorting requirements.
  void sortSymbols();

private:
  struct SymbolData {
    ResolveInfo *R;
    uint32_t DynSymIndex;
    uint32_t Hash;
    SymbolData(ResolveInfo *R, uint32_t DynSymIndex, uint32_t Hash)
        : R(R), DynSymIndex(DynSymIndex), Hash(Hash) {}
  };

protected:
  std::vector<ResolveInfo *> &DynamicSymbols; // All Dynamic symbols.
  std::vector<SymbolData *> Symbols;          // Symbols to Hash.
  mutable unsigned NBuckets;
  mutable unsigned MaskWords;
  mutable unsigned Shift2;
};

} // namespace eld

#endif
