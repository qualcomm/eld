//===- DWARFObjectTest.cpp ------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Input/ELFObjectFile.h"
#include "eld/Input/Input.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Support/MemoryArea.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/DebugInfo/DWARF/DWARFContext.h"
#include "llvm/DebugInfo/DWARF/DWARFDebugLine.h"
#include "llvm/DebugInfo/DWARF/DWARFUnit.h"
#include "llvm/Object/ELF.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/MemoryBuffer.h"
#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

#ifndef DWARF_OBJECT_FIXTURE_PATH
#error DWARF_OBJECT_FIXTURE_PATH must name the compiled DWARF fixture
#endif
#ifdef DWARF_OBJECT_ARM32_DWARF5_FIXTURE_PATH
#define HAS_ARM32_DWARF5_FIXTURE
#endif

namespace {

template <class ELFT>
bool addDebugSections(eld::ELFObjectFile &ObjectFile,
                      eld::LinkerSectionKind DebugSectionKind,
                      std::vector<std::unique_ptr<eld::ELFSection>> &Sections) {
  auto FileOrErr =
      llvm::object::ELFFile<ELFT>::create(ObjectFile.getContents());
  if (!FileOrErr) {
    ADD_FAILURE() << llvm::toString(FileOrErr.takeError());
    return false;
  }
  auto RawSectionsOrErr = FileOrErr->sections();
  if (!RawSectionsOrErr) {
    ADD_FAILURE() << llvm::toString(RawSectionsOrErr.takeError());
    return false;
  }
  auto SectionNamesOrErr = FileOrErr->getSectionStringTable(*RawSectionsOrErr);
  if (!SectionNamesOrErr) {
    ADD_FAILURE() << llvm::toString(SectionNamesOrErr.takeError());
    return false;
  }

  for (size_t I = 0; I != RawSectionsOrErr->size(); ++I) {
    const typename ELFT::Shdr &RawSection = (*RawSectionsOrErr)[I];
    auto NameOrErr = FileOrErr->getSectionName(RawSection, *SectionNamesOrErr);
    if (!NameOrErr) {
      ADD_FAILURE() << llvm::toString(NameOrErr.takeError());
      return false;
    }
    llvm::StringRef Name = *NameOrErr;
    if (!Name.starts_with(".debug"))
      continue;

    auto Section = std::make_unique<eld::ELFSection>(
        DebugSectionKind, Name.str(), RawSection.sh_flags,
        static_cast<uint32_t>(RawSection.sh_entsize),
        static_cast<uint32_t>(RawSection.sh_addralign), RawSection.sh_type,
        static_cast<uint32_t>(RawSection.sh_info), nullptr, 0,
        RawSection.sh_addr);
    Section->setIndex(static_cast<uint32_t>(I));
    Section->setSize(RawSection.sh_size);
    Section->setOffset(RawSection.sh_offset);
    Section->setInputRange(RawSection.sh_offset, RawSection.sh_size);
    Section->setAddr(RawSection.sh_addr);
    Section->setInputFile(&ObjectFile);
    ObjectFile.addSection(Section.get());
    Sections.push_back(std::move(Section));
  }
  return true;
}

void checkDWARFContext(
    llvm::StringRef FixturePath,
    eld::LinkerSectionKind DebugInfoSectionKind = eld::LinkerSectionKind::Debug,
    bool CheckLineTable = true, bool CheckAddressRange = false) {
  auto BufferOrErr = llvm::MemoryBuffer::getFile(FixturePath);
  ASSERT_TRUE(BufferOrErr) << BufferOrErr.getError().message();

  std::unique_ptr<llvm::MemoryBuffer> Buffer = std::move(*BufferOrErr);
  llvm::StringRef Contents = Buffer->getBuffer();
  ASSERT_GE(Contents.size(), llvm::ELF::EI_NIDENT);

  const auto *Ident = reinterpret_cast<const uint8_t *>(Contents.data());
  ASSERT_EQ(llvm::ELF::ElfMagic[0], Ident[llvm::ELF::EI_MAG0]);
  ASSERT_EQ(llvm::ELF::ElfMagic[1], Ident[llvm::ELF::EI_MAG1]);
  ASSERT_EQ(llvm::ELF::ElfMagic[2], Ident[llvm::ELF::EI_MAG2]);
  ASSERT_EQ(llvm::ELF::ElfMagic[3], Ident[llvm::ELF::EI_MAG3]);

  eld::ObjectFile::ELFKind Kind;
  switch (Ident[llvm::ELF::EI_CLASS]) {
  case llvm::ELF::ELFCLASS32:
    Kind = eld::ObjectFile::ELF32LEKind;
    break;
  case llvm::ELF::ELFCLASS64:
    Kind = eld::ObjectFile::ELF64LEKind;
    break;
  default:
    FAIL() << "fixture is not a supported ELF object";
  }

  eld::DiagnosticEngine DiagEngine(/*useColor=*/false);
  eld::Input Input(FixturePath.str(), &DiagEngine);
  Input.setResolvedPath(FixturePath.str());
  eld::MemoryArea Memory(Buffer->getMemBufferRef());
  Input.setMemArea(&Memory);

  eld::ELFObjectFile ObjectFile(&Input, &DiagEngine);
  ObjectFile.setELFKind(Kind);

  // Build the ELD section objects that the DWARF adapter reads. Their contents
  // are views into the original input buffer, even when the linker will ignore
  // or discard the debug sections.
  std::vector<std::unique_ptr<eld::ELFSection>> DebugSections;
  bool SectionsAdded =
      Kind == eld::ObjectFile::ELF32LEKind
          ? addDebugSections<llvm::object::ELF32LE>(
                ObjectFile, DebugInfoSectionKind, DebugSections)
          : addDebugSections<llvm::object::ELF64LE>(
                ObjectFile, DebugInfoSectionKind, DebugSections);
  ASSERT_TRUE(SectionsAdded);
  ASSERT_FALSE(DebugSections.empty());
  ObjectFile.createDWARFContext();

  ASSERT_NE(nullptr, ObjectFile.getDWARFContext());
  const auto &CompileUnits = ObjectFile.getDWARFContext()->compile_units();
  ASSERT_FALSE(CompileUnits.empty());
  auto CompileUnitIt = CompileUnits.begin();
  llvm::DWARFUnit *Unit = CompileUnitIt->get();

  llvm::DWARFDie CompileUnit = Unit->getUnitDIE(false);
  ASSERT_TRUE(CompileUnit);
  EXPECT_EQ(llvm::dwarf::DW_TAG_compile_unit, CompileUnit.getTag());

  llvm::DWARFContext *Context = ObjectFile.getDWARFContext();
  if (CheckAddressRange) {
    EXPECT_FALSE(Context->getDWARFObj().getAddrSection().Data.empty());
    bool HasFunctionAddressRange = false;
    for (llvm::DWARFDie Child : CompileUnit.children()) {
      if (Child.getTag() != llvm::dwarf::DW_TAG_subprogram)
        continue;
      uint64_t LowPC = 0;
      uint64_t HighPC = 0;
      uint64_t SectionIndex = 0;
      if (!Child.getLowAndHighPC(LowPC, HighPC, SectionIndex))
        continue;
      EXPECT_LT(LowPC, HighPC);
      HasFunctionAddressRange = true;
      break;
    }
    EXPECT_TRUE(HasFunctionAddressRange);
  }

  if (!CheckLineTable)
    return;

  const llvm::DWARFDebugLine::LineTable *LineTable =
      ObjectFile.getDWARFContext()->getLineTableForUnit(Unit);
  ASSERT_NE(nullptr, LineTable);
  EXPECT_FALSE(LineTable->Rows.empty());
}

TEST(DWARFObjectTest, ContextReadsCompilationUnitAndLineTable) {
  checkDWARFContext(DWARF_OBJECT_FIXTURE_PATH);
}

#ifdef HAS_ARM32_DWARF5_FIXTURE
TEST(DWARFObjectTest, ContextReadsARM32DWARF5AddressRange) {
  checkDWARFContext(DWARF_OBJECT_ARM32_DWARF5_FIXTURE_PATH,
                    eld::LinkerSectionKind::Debug, /*CheckLineTable=*/false,
                    /*CheckAddressRange=*/true);
}
#endif

TEST(DWARFObjectTest, ContextReadsIgnoredAndDiscardedDebugSections) {
  checkDWARFContext(DWARF_OBJECT_FIXTURE_PATH, eld::LinkerSectionKind::Ignore);
  checkDWARFContext(DWARF_OBJECT_FIXTURE_PATH, eld::LinkerSectionKind::Discard);
}

} // namespace
