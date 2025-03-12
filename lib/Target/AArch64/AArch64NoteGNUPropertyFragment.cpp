//===- AArch64NoteGNUPropertyFragment.cpp----------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "AArch64NoteGNUPropertyFragment.h"
#include "eld/Core/Module.h"
#include "eld/Target/GNULDBackend.h"

using namespace eld;

//===----------------------------------------------------------------------===//
// AArch64NoteGNUPropertyFragment
//===----------------------------------------------------------------------===//

AArch64NoteGNUPropertyFragment::AArch64NoteGNUPropertyFragment(ELFSection *O)
    : TargetFragment(TargetFragment::Kind::NoteGNUProperty, O, nullptr,
                     O->getAddrAlign(), 0) {}

AArch64NoteGNUPropertyFragment::~AArch64NoteGNUPropertyFragment() {}

const std::string AArch64NoteGNUPropertyFragment::name() const {
  return "Fragment for GNU property";
}

size_t AArch64NoteGNUPropertyFragment::size() const {
  if (!featureSet)
    return 0;
  return 0x20;
}

eld::Expected<void> AArch64NoteGNUPropertyFragment::emit(MemoryRegion &mr,
                                                         Module &M) {
  uint8_t *Buf = mr.begin() + getOffset(M.getConfig().getDiagEngine());
  uint32_t featureAndType = llvm::ELF::GNU_PROPERTY_AARCH64_FEATURE_1_AND;
  llvm::support::endian::write32le(Buf, 4);
  llvm::support::endian::write32le(Buf, 4);      // Name size
  llvm::support::endian::write32le(Buf + 4, 16); // Content size
  llvm::support::endian::write32le(Buf + 8,
                                   llvm::ELF::NT_GNU_PROPERTY_TYPE_0); // Type
  std::memcpy(Buf + 12, "GNU", 4);                            // Name string
  llvm::support::endian::write32le(Buf + 16, featureAndType); // Feature type
  llvm::support::endian::write32le(Buf + 20, 4);              // Feature size
  llvm::support::endian::write32le(Buf + 24, featureSet);     // Feature flags
  llvm::support::endian::write32le(Buf + 28, 0);              // Padding
  return {};
}

bool AArch64NoteGNUPropertyFragment::updateInfo(GNULDBackend *G) {
  return true;
}

void AArch64NoteGNUPropertyFragment::dump(llvm::raw_ostream &OS) {}

bool AArch64NoteGNUPropertyFragment::updateInfo(uint32_t features) {
  featureSet |= features;
  return true;
}
