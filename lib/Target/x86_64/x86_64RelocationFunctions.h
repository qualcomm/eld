//===- x86_64RelocationFunctions.h-----------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
#ifndef X86_64_RELOCATION_FUNCTIONS_H
#define X86_64_RELOCATION_FUNCTIONS_H

#include "x86_64LLVMExtern.h"
#include "x86_64Relocator.h"

namespace eld {

class Relocation;

struct RelocationDescription;

x86_64Relocator::Result none(Relocation &pEntry, x86_64Relocator &pParent,
                             RelocationDescription &RelocDesc);
x86_64Relocator::Result relocPCREL(Relocation &pEntry, x86_64Relocator &pParent,
                                   RelocationDescription &RelocDesc);
x86_64Relocator::Result relocAbs(Relocation &pEntry, x86_64Relocator &pParent,
                                 RelocationDescription &RelocDesc);
x86_64Relocator::Result unsupport(Relocation &pEntry, x86_64Relocator &pParent,
                                  RelocationDescription &RelocDesc);
x86_64Relocator::Result relocPLT32(Relocation &pEntry, x86_64Relocator &pParent,
                                   RelocationDescription &RelocDesc);
x86_64Relocator::Result relocGOTRelative(Relocation &pEntry,
                                         x86_64Relocator &pParent,
                                         RelocationDescription &RelocDesc);
x86_64Relocator::Result relocTPOFF(Relocation &pEntry, x86_64Relocator &pParent,
                                   RelocationDescription &RelocDesc);
x86_64Relocator::Result relocDTPOFF(Relocation &pEntry,
                                    x86_64Relocator &pParent,
                                    RelocationDescription &RelocDesc);
struct RelocationDescription;

typedef Relocator::Result (*ApplyFunctionType)(
    eld::Relocation &pReloc, eld::x86_64Relocator &pParent,
    RelocationDescription &pRelocDesc);

struct RelocationDescription {
  // The application function for the relocation.
  const ApplyFunctionType func;
  // The Relocation type, this is just kept for convenience when writing new
  // handlers for relocations.
  const unsigned int type;
  // If the user specified, the relocation to be force verified, the relocation
  // is verified for alignment, truncation errors(only for relocations that take
  // in non signed values, signed values are bound to exceed the number of
  // bits).
  bool forceVerify;
};

struct RelocationDescription x86RelocDesc[] = {
    {/*.func = */ none,
     /*.type = */ llvm::ELF::R_X86_64_NONE,
     /*.forceVerify = */ false},
    {/*.func = */ &relocAbs,
     /*.type = */ llvm::ELF::R_X86_64_64,
     /*.forceVerify = */ false},
    {/*.func = */ &relocPCREL,
     /*.type = */ llvm::ELF::R_X86_64_PC32,
     /*.forceVerify = */ false},
    {/*.func = */ none,
     /*.type = */ llvm::ELF::R_X86_64_GOT32,
     /*.forceVerify = */ false},
    {/*.func = */ &relocPLT32,
     /*.type = */ llvm::ELF::R_X86_64_PLT32,
     /*.forceVerify = */ false},
    {/*.func = */ none,
     /*.type = */ llvm::ELF::R_X86_64_COPY,
     /*.forceVerify = */ false},
    {/*.func = */ none,
     /*.type = */ llvm::ELF::R_X86_64_GLOB_DAT,
     /*.forceVerify = */ false},
    {/*.func = */ none,
     /*.type = */ llvm::ELF::R_X86_64_JUMP_SLOT,
     /*.forceVerify = */ false},
    {/*.func = */ none,
     /*.type = */ llvm::ELF::R_X86_64_RELATIVE,
     /*.forceVerify = */ false},
    {/*.func = */ &relocGOTRelative,
     /*.type = */ llvm::ELF::R_X86_64_GOTPCREL,
     /*.forceVerify = */ false},
    {/*.func = */ &relocAbs,
     /*.type = */ llvm::ELF::R_X86_64_32,
     /*.forceVerify = */ false},
    {/*.func = */ &relocAbs,
     /*.type = */ llvm::ELF::R_X86_64_32S,
     /*.forceVerify = */ false},
    {/*.func = */ &relocAbs,
     /*.type = */ llvm::ELF::R_X86_64_16,
     /*.forceVerify = */ false},
    {/*.func = */ &relocPCREL,
     /*.type = */ llvm::ELF::R_X86_64_PC16,
     /*.forceVerify = */ false},
    {/*.func = */ &relocAbs,
     /*.type = */ llvm::ELF::R_X86_64_8,
     /*.forceVerify = */ false},
    {/*.func = */ &relocPCREL,
     /*.type = */ llvm::ELF::R_X86_64_PC8,
     /*.forceVerify = */ false},
    {/*.func = */ none,
     /*.type = */ llvm::ELF::R_X86_64_DTPMOD64,
     /*.forceVerify = */ false},
    {/*.func = */ &relocDTPOFF,
     /*.type = */ llvm::ELF::R_X86_64_DTPOFF64,
     /*.forceVerify = */ false},
    {/*.func = */ &relocTPOFF,
     /*.type = */ llvm::ELF::R_X86_64_TPOFF64,
     /*.forceVerify = */ false},
    {/*.func = */ none,
     /*.type = */ llvm::ELF::R_X86_64_TLSGD,
     /*.forceVerify = */ false},
    {/*.func = */ none,
     /*.type = */ llvm::ELF::R_X86_64_TLSLD,
     /*.forceVerify = */ false},
    {/*.func = */ &relocDTPOFF,
     /*.type = */ llvm::ELF::R_X86_64_DTPOFF32,
     /*.forceVerify = */ false},
    {/*.func = */ &relocGOTRelative,
     /*.type = */ llvm::ELF::R_X86_64_GOTTPOFF,
     /*.forceVerify = */ false},
    {/*.func = */ &relocTPOFF,
     /*.type = */ llvm::ELF::R_X86_64_TPOFF32,
     /*.forceVerify = */ false},
    {/*.func = */ &relocPCREL,
     /*.type = */ llvm::ELF::R_X86_64_PC64,
     /*.forceVerify = */ false},
    {/*.func = */ none, /*.type = */ 25, /*.forceVerify = */ false},
    {/*.func = */ none, /*.type = */ 26, /*.forceVerify = */ false},
    {/*.func = */ none, /*.type = */ 27, /*.forceVerify = */ false},
    {/*.func = */ none, /*.type = */ 28, /*.forceVerify = */ false},
    {/*.func = */ none, /*.type = */ 29, /*.forceVerify = */ false},
    {/*.func = */ none, /*.type = */ 30, /*.forceVerify = */ false},
    {/*.func = */ none, /*.type = */ 31, /*.forceVerify = */ false},
    {/*.func = */ none, /*.type = */ 32, /*.forceVerify = */ false},
    {/*.func = */ none, /*.type = */ 33, /*.forceVerify = */ false},
    {/*.func = */ none, /*.type = */ 34, /*.forceVerify = */ false},
    {/*.func = */ none, /*.type = */ 35, /*.forceVerify = */ false},
    {/*.func = */ none, /*.type = */ 36, /*.forceVerify = */ false},
    {/*.func = */ none, /*.type = */ 37, /*.forceVerify = */ false},
    {/*.func = */ none, /*.type = */ 38, /*.forceVerify = */ false},
    {/*.func = */ none, /*.type = */ 39, /*.forceVerify = */ false},
    {/*.func = */ none, /*.type = */ 40, /*.forceVerify = */ false},
    {/*.func = */ &relocGOTRelative,
     /*.type = */ llvm::ELF::R_X86_64_GOTPCRELX,
     /*.forceVerify = */ false},
    {/*.func = */ &relocGOTRelative,
     /*.type = */ llvm::ELF::R_X86_64_REX_GOTPCRELX,
     /*.forceVerify = */ false},
};

#define x86_64_MAXRELOCS (llvm::ELF::R_X86_64_REX_GOTPCRELX + 1)

} // namespace eld

#endif // X86_64_RELOCATION_FUNCTIONS_H
