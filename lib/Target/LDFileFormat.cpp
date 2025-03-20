//===- LDFileFormat.cpp----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
//
//                     The MCLinker Project
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "eld/Target/LDFileFormat.h"
#include "eld/Config/LinkerConfig.h"
#include "llvm/BinaryFormat/ELF.h"

using namespace eld;

LDFileFormat::Kind LDFileFormat::getELFSectionKind(
    uint32_t Flags, uint32_t AddrAlign, uint32_t EntSize, uint32_t Type,
    llvm::StringRef Name, const LinkerConfig &Config) {

  bool IsPartialLink = Config.isLinkPartial();

  if (Flags & llvm::ELF::SHF_EXCLUDE)
    return LDFileFormat::Discard;

  // If we are in partial link mode and see a group section
  // leave the group section as it is, dont treat it as Target
  if (!IsPartialLink || !(Flags & llvm::ELF::SHF_GROUP)) {
    if ((Flags & llvm::ELF::SHF_MASKPROC) || (Flags & llvm::ELF::SHF_MASKOS))
      return LDFileFormat::Target;
  }

  bool IsSectionMergeStrings =
      ((Flags & (llvm::ELF::SHF_MERGE)) && (Flags & (llvm::ELF::SHF_STRINGS)));

  if (IsSectionMergeStrings && AddrAlign == 1 && EntSize == 1 && !IsPartialLink)
    return LDFileFormat::MergeStr;

  if (IsPartialLink && EntSize <= 1) {
    if (IsSectionMergeStrings && Name == ".comment")
      return LDFileFormat::MergeStr;
  }

  if (Name == ".note.gnu.property")
    return LDFileFormat::GNUProperty;
  if (Name == ".hexagon.attributes")
    return LDFileFormat::Target;

  if (Name.starts_with(".note.qc.timing"))
    return LDFileFormat::Timing;
  if (Name.starts_with(".debug") || Name.starts_with(".zdebug") ||
      Name.starts_with(".line") || Name.starts_with(".stab"))
    return LDFileFormat::Debug;
  if (Name.starts_with(".comment"))
    return LDFileFormat::MetaData;
  if (Name.starts_with(".interp") || Name.starts_with(".dynamic"))
    return LDFileFormat::Note;
  if (Name.starts_with(".eh_frame_hdr"))
    return LDFileFormat::EhFrameHdr;
  if (Name.starts_with(".gcc_except_table"))
    return LDFileFormat::GCCExceptTable;
  if (Name.starts_with(".eh_frame"))
    return LDFileFormat::EhFrame;
  if (Name.starts_with(".note.GNU-stack"))
    return LDFileFormat::StackNote;
  if (Name.starts_with(".gnu.linkonce"))
    return LDFileFormat::LinkOnce;

  // Discarded sections. Plugins will still be able to see these sections.
  if (Name == ".note.gnu.build-id")
    return LDFileFormat::Discard;
  if (Name == ".note.qc.reloc.section.map")
    return LDFileFormat::Discard;
  if (Name == ".note.llvm.callgraph")
    return LDFileFormat::Discard;
  if (Name == ".llvm_addrsig")
    return LDFileFormat::Discard;

  switch (Type) {
  case llvm::ELF::SHT_NULL:
    return LDFileFormat::Null;
  case llvm::ELF::SHT_INIT_ARRAY:
  case llvm::ELF::SHT_FINI_ARRAY:
  case llvm::ELF::SHT_PREINIT_ARRAY:
  case llvm::ELF::SHT_PROGBITS:
  case llvm::ELF::SHT_NOBITS:
    return LDFileFormat::Regular;
  case llvm::ELF::SHT_SYMTAB:
  case llvm::ELF::SHT_DYNSYM:
  case llvm::ELF::SHT_STRTAB:
  case llvm::ELF::SHT_HASH:
  case llvm::ELF::SHT_DYNAMIC:
  case llvm::ELF::SHT_SYMTAB_SHNDX:
    return LDFileFormat::NamePool;
  case llvm::ELF::SHT_RELA:
  case llvm::ELF::SHT_REL:
    return LDFileFormat::Relocation;
  case llvm::ELF::SHT_NOTE:
    return LDFileFormat::Note;
  case llvm::ELF::SHT_GROUP:
    return LDFileFormat::Group;
  case llvm::ELF::SHT_GNU_versym:
  case llvm::ELF::SHT_GNU_verdef:
  case llvm::ELF::SHT_GNU_verneed:
    return LDFileFormat::Version;
  case llvm::ELF::SHT_SHLIB:
    return LDFileFormat::Target;
  default:
    if ((Type >= llvm::ELF::SHT_LOPROC && Type <= llvm::ELF::SHT_HIPROC) ||
        (Type >= llvm::ELF::SHT_LOOS && Type <= llvm::ELF::SHT_HIOS) ||
        (Type >= llvm::ELF::SHT_LOUSER && Type <= llvm::ELF::SHT_HIUSER))
      return LDFileFormat::Target;
    Config.raise(Diag::err_unsupported_section) << Name << Type;
    return LDFileFormat::Error;
  }
  return LDFileFormat::MetaData;
}
