//===- AArch64GOT.cpp------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "AArch64LDBackend.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Readers/Relocation.h"
#include "eld/Support/Memory.h"

using namespace eld;

// GOTPLT0
AArch64GOTPLT0 *AArch64GOTPLT0::Create(ELFSection *O, ResolveInfo *R) {
  AArch64GOTPLT0 *G = make<AArch64GOTPLT0>(O, R);

  if (!R)
    return G;

  // Create a relocation and point to the ResolveInfo.
  Relocation *r1 = nullptr;

  r1 = Relocation::Create(llvm::ELF::R_AARCH64_ABS64, 64,
                          make<FragmentRef>(*G, 0), 0);
  r1->setSymInfo(R);

  O->addRelocation(r1);

  return G;
}

AArch64GOTPLTN *AArch64GOTPLTN::Create(ELFSection *O, ResolveInfo *R,
                                       Fragment *PLT) {
  AArch64GOTPLTN *G = make<AArch64GOTPLTN>(O, R);

  // If the symbol is IRELATIVE, the PLT slot contains the relative symbol
  // value. No need to fill the GOT slot with PLT0.
  if (PLT) {
    FragmentRef *PLTFragRef = make<FragmentRef>(*PLT, 0);
    Relocation *r = Relocation::Create(llvm::ELF::R_AARCH64_ABS64, 64,
                                       make<FragmentRef>(*G, 0), 0);
    O->addRelocation(r);
    r->modifyRelocationFragmentRef(PLTFragRef);
  }
  return G;
}

llvm::ArrayRef<uint8_t> AArch64GOT::getContent() const {
  // Convert uint32_t to ArrayRef.
  typedef union {
    uint64_t a;
    uint8_t b[8];
  } C;
  C Content;
  Content.a = 0;
  // If the GOT contents needs to reflect a symbol value, then we use the
  // symbol value.
  if (getValueType() == GOT::SymbolValue)
    Content.a = symInfo()->outSymbol()->value();
  if (getValueType() == GOT::TLSStaticSymbolValue)
    Content.a =
        AArch64LDBackend::getStaticTCBSize() + symInfo()->outSymbol()->value();
  std::memcpy((void *)Value, (void *)&Content.a, sizeof(Value));
  return llvm::ArrayRef(Value);
}
