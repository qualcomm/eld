//===- GNUPropertyFragment.cpp---------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Fragment/GNUPropertyFragment.h"
#include "eld/Core/Module.h"
#include "eld/Target/GNULDBackend.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/Endian.h"
#include <cstring>

using namespace eld;

//===----------------------------------------------------------------------===//
// GNUPropertyFragment
//===----------------------------------------------------------------------===//

GNUPropertyFragment::GNUPropertyFragment(ELFSection *O,
                                         uint32_t FeatureAndType, bool Is64)
    : TargetFragment(TargetFragment::Kind::NoteGNUProperty, O, nullptr,
                     O->getAddrAlign(), 0),
      FeatureAndType(FeatureAndType), Is64(Is64) {}

GNUPropertyFragment::~GNUPropertyFragment() {}

const std::string GNUPropertyFragment::name() const {
  return "Fragment for GNU property";
}

size_t GNUPropertyFragment::size() const {
  if (!FeatureSet)
    return 0;
  // Nhdr (12) + "GNU\0" (4) + descriptor.
  return 16 + descSize();
}

eld::Expected<void> GNUPropertyFragment::emit(MemoryRegion &mr, Module &M) {
  if (!FeatureSet)
    return {};
  using llvm::support::endian::write32le;
  uint8_t *Buf = mr.begin() + getOffset(M.getConfig().getDiagEngine());
  write32le(Buf, 4);                                     // n_namesz
  write32le(Buf + 4, descSize());                        // n_descsz
  write32le(Buf + 8, llvm::ELF::NT_GNU_PROPERTY_TYPE_0); // n_type
  std::memcpy(Buf + 12, "GNU", 4);                       // name
  write32le(Buf + 16, FeatureAndType);                   // pr_type
  write32le(Buf + 20, 4);                                // pr_datasz
  write32le(Buf + 24, FeatureSet);                       // feature bits
  if (Is64)
    write32le(Buf + 28, 0);                              // pad to 8 bytes
  return {};
}

bool GNUPropertyFragment::updateInfo(GNULDBackend *) { return true; }

void GNUPropertyFragment::dump(llvm::raw_ostream &) {}

bool GNUPropertyFragment::updateInfo(uint32_t Features) {
  FeatureSet |= Features;
  return true;
}
