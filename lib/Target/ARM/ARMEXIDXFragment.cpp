//===- ARMEXIDXFragment.cpp------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "ARMEXIDXFragment.h"
#include "eld/Diagnostics/MsgHandler.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Readers/Relocation.h"
#include "eld/SymbolResolver/ResolveInfo.h"
#include <cstring>

using namespace eld;

EXIDXPiece EXIDXFragment::getPiece(uint32_t Offset) const {
  ASSERT(getOwningSection(), "EXIDX fragment must have an owning section");
  ASSERT(Offset < getOwningSection()->size(),
         "Exception on .ARM.exidx relocation handling");
  for (const EXIDXPiece &P : Pieces)
    if (P.InputOffset <= Offset && Offset < P.InputOffset + P.Size)
      return P;
  ASSERT(false, "Unable to map relocation to EXIDX piece range");
  return {};
}

uint32_t EXIDXFragment::translateInputOffset(uint32_t InputOffset) const {
  uint32_t OutputOffset = 0;
  for (const EXIDXPiece &P : Pieces) {
    if (P.InputOffset <= InputOffset && InputOffset < P.InputOffset + P.Size)
      return OutputOffset + (InputOffset - P.InputOffset);
    OutputOffset += P.Size;
  }
  ASSERT(false, "Unable to translate EXIDX relocation offset to piece");
  return 0;
}

size_t EXIDXFragment::size() const {
  size_t Total = 0;
  for (const EXIDXPiece &P : Pieces)
    Total += P.Size;
  return Total;
}

eld::Expected<void> EXIDXFragment::emit(MemoryRegion &Mr, Module &) {
  if (Pieces.empty())
    return {};

  llvm::StringRef Region = getRegion();
  uint32_t BaseOffset = getOffset();
  uint32_t OutOffset = 0;
  for (const EXIDXPiece &Piece : Pieces) {
    const uint32_t Begin = Piece.InputOffset;
    const uint32_t PieceSize = Piece.Size;
    ASSERT(Begin + PieceSize <= Region.size(),
           "Invalid EXIDX piece boundaries");
    if (!PieceSize)
      continue;
    std::memcpy(Mr.begin() + BaseOffset + OutOffset, Region.data() + Begin,
                PieceSize);
    OutOffset += PieceSize;
  }
  return {};
}
