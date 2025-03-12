//===- RegionFragmentEx.h--------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_FRAGMENT_REGIONFRAGMENTEX_H
#define ELD_FRAGMENT_REGIONFRAGMENTEX_H

#include "eld/Fragment/Fragment.h"
#include "eld/Readers/Relocation.h"
#include "llvm/ADT/StringRef.h"

namespace eld {
class LinkerConfig;

/** \class RegionFragmentEx
 *  \brief RegionFragmentEx is a kind of Fragment containing input memory region
 *         that has capability of deleting, inserting, replacing and aligning.
 */
class RegionFragmentEx : public Fragment {
public:
  RegionFragmentEx(const char *Data, size_t Sz, ELFSection *O = nullptr,
                   uint32_t Align = 1);

  ~RegionFragmentEx();

  static bool classof(const Fragment *F) {
    return F->getKind() == Fragment::RegionFragmentEx;
  }

  const llvm::StringRef getRegion() const {
    return llvm::StringRef(Data, Size);
  }
  llvm::StringRef getRegion() { return llvm::StringRef(Data, Size); }

  static bool classof(const RegionFragmentEx *) { return true; }

  bool replaceInstruction(uint32_t offset, Relocation *reloc, uint32_t instr,
                          uint8_t size);
  void deleteInstruction(uint32_t offset, uint32_t size);
  void addRequiredNops(uint32_t Offset, uint32_t NumNopsToAdd);

  size_t size() const override;

  virtual eld::Expected<void> emit(MemoryRegion &mr, Module &M) override;

  void copyData(void *pDest, uint32_t pNBytes, uint64_t pOffset) const;

  virtual void addSymbol(ResolveInfo *R) override;

protected:
  std::vector<ResolveInfo *> Symbols;
  const char *Data;
  size_t Size;
};

} // namespace eld

#endif
