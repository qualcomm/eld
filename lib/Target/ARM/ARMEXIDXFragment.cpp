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

///   #EXIDX  sec=0x<addr>  sz=0x<size>  n=<npieces>  [<GC>]
///   #P[i]   off=0x<input_offset>  addr=0x<output_addr>  sz=0x<size>  [<GC>]
///   #R      sym=<name>  add=0x<addend>  [<GC>]   (once per reloc in piece)
void EXIDXFragment::dump(llvm::raw_ostream &OS) {
  if (Pieces.empty())
    return;

  ELFSection *S = getOwningSection();
  const bool IsGC = S && (S->isIgnore() || S->isDiscard());
  ELFSection *OutSection = getOutputELFSection();
  const uint64_t SectionAddr = OutSection ? OutSection->addr() : 0;
  const uint64_t FragmentOutputOffset = hasOffset() ? getOffset() : 0;
  const uint64_t FragmentOutputAddr = SectionAddr + FragmentOutputOffset;

  // Fragment header line.
  OS << "#EXIDX";
  OS << "\tsec=0x";
  OS.write_hex(FragmentOutputAddr);
  OS << "\tsz=0x";
  OS.write_hex(size());
  OS << "\tn=" << Pieces.size();
  if (IsGC)
    OS << "\t<GC>";
  OS << "\n";

  for (size_t I = 0; I < Pieces.size(); ++I) {
    const EXIDXPiece &Piece = Pieces[I];

    // Per-piece line: input offset, output address, size.
    OS << "#P[" << I << "]";
    OS << "\toff=0x";
    OS.write_hex(Piece.InputOffset);
    OS << "\taddr=0x";
    OS.write_hex(FragmentOutputAddr + Piece.InputOffset);
    OS << "\tsz=0x";
    OS.write_hex(Piece.Size);
    if (IsGC)
      OS << "\t<GC>";
    OS << "\n";

    if (!S)
      continue;

    // Emit one #R line per relocation whose target falls within this piece.
    // Relocations are stored on the owning section; offsets are in
    // piece-layout space after sortEXIDX translates them.
    for (Relocation *R : S->getRelocations()) {
      if (!R || !R->targetRef() || R->targetRef()->isNull())
        continue;
      if (R->targetRef()->frag() != this)
        continue;

      const uint32_t RelInputOffset = R->targetRef()->offset();
      if (RelInputOffset < Piece.InputOffset ||
          RelInputOffset >= Piece.InputOffset + Piece.Size)
        continue;

      OS << "#R";
      if (ResolveInfo *Sym = R->symInfo())
        OS << "\tsym=" << Sym->name();
      OS << "\tadd=0x";
      OS.write_hex(R->addend());
      if (IsGC)
        OS << "\t<GC>";
      OS << "\n";
    }
  }
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
