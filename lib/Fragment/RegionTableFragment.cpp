//===- RegionTableFragment.cpp---------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Fragment/RegionTableFragment.h"
#include "eld/Target/ELFSegment.h"
#include "eld/Target/ELFSegmentFactory.h"
#include "eld/Target/GNULDBackend.h"
#include "llvm/Object/ELFTypes.h"

using namespace eld;

//===----------------------------------------------------------------------===//
// RegionTableFragment
//===----------------------------------------------------------------------===//
template <class ELFT>
RegionTableFragment<ELFT>::RegionTableFragment(ELFSection *O)
    : TargetFragment(TargetFragment::Kind::RegionTable, O, nullptr, 4, 0),
      NumEntries(3) {}

template <class ELFT> RegionTableFragment<ELFT>::~RegionTableFragment() {}

template <class ELFT>
const std::string RegionTableFragment<ELFT>::name() const {
  return "Fragment for RegionTable";
}

// The RegionTable is organized as a structure. The minimum size of the
// structure is 3 words.
// __region_table_header_size, __region_table_entry_size, __region_table_size
template <class ELFT> size_t RegionTableFragment<ELFT>::size() const {
  return ((NumEntries + (Table.size() * 2)) * sizeof(Elf_Addr));
}

template <class ELFT>
eld::Expected<void> RegionTableFragment<ELFT>::emit(MemoryRegion &mr,
                                                    Module &M) {
  Elf_Addr Size = (Elf_Addr)size();
  auto *P = reinterpret_cast<Elf_Addr *>(mr.begin());
  *P++ = 3 * sizeof(Elf_Addr); // Size of the Header
  *P++ = 2 * sizeof(Elf_Addr); // Start Address, End Address
  *P++ = Size;                 // Size of the RegionTable itself.
  for (auto &R : Table) {
    *P++ = R.first->addr();
    *P++ = R.second->addr() + R.second->size();
  }
  ASSERT(!((uint64_t)(mr.begin()) + (uint64_t)Size - (uint64_t)P),
         "RegionTable inserted more entries than it should!");
  return {};
}

template <class ELFT>
bool RegionTableFragment<ELFT>::updateInfo(GNULDBackend *G) {
  uint32_t NumEntriesInTable = (Table.size() * 2);
  Table.clear();
  const std::vector<ELFSegment *> &Segments = G->elfSegmentTable().segments();
  for (auto &Seg : Segments) {
    OutputSectionEntry *FirstBssInSeg = nullptr;
    OutputSectionEntry *LastBssInSeg = nullptr;
    for (auto &Sec : Seg->sections()) {
      // Continue until we find a non empty BSS section.
      if (!Sec->getSection()->size())
        continue;
      bool IsCurrBss = Sec->getSection()->isBSS();
      if (Sec->prolog().type() == OutputSectDesc::UNINIT)
        IsCurrBss = false;
      if (IsCurrBss && !FirstBssInSeg)
        FirstBssInSeg = Sec;
      if (FirstBssInSeg && !IsCurrBss) {
        Table.push_back(std::make_pair(FirstBssInSeg->getSection(),
                                       LastBssInSeg->getSection()));
        FirstBssInSeg = nullptr;
        LastBssInSeg = nullptr;
      }
      LastBssInSeg = Sec;
    }
    if (FirstBssInSeg)
      Table.push_back(std::make_pair(FirstBssInSeg->getSection(),
                                     LastBssInSeg->getSection()));
  }
  return (NumEntriesInTable != (Table.size() * 2));
}

template <class ELFT>
void RegionTableFragment<ELFT>::dump(llvm::raw_ostream &OS) {
  for (auto &T : Table) {
    OS << "#";
    OS << "\t" << T.first->name() << "\t" << T.second->name();
    OS << "\t[";
    OS << "0x";
    OS.write_hex(T.first->addr());
    OS << ",0x";
    OS.write_hex(T.second->addr());
    OS << "]\n";
  }
}

template class eld::RegionTableFragment<llvm::object::ELF32LE>;
template class eld::RegionTableFragment<llvm::object::ELF32BE>;
template class eld::RegionTableFragment<llvm::object::ELF64LE>;
template class eld::RegionTableFragment<llvm::object::ELF64BE>;
