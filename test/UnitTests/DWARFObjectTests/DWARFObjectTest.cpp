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
#include "llvm/Support/MemoryBuffer.h"
#include <cstdint>
#include <gtest/gtest.h>
#include <memory>

#ifndef DWARF_OBJECT_FIXTURE_PATH
#error DWARF_OBJECT_FIXTURE_PATH must name the compiled DWARF fixture
#endif
#ifdef DWARF_OBJECT_DWARF5_FIXTURE_PATH
#define HAS_DWARF5_FIXTURE
#endif

namespace {

void checkDWARFContext(
    llvm::StringRef FixturePath, llvm::StringRef DebugInfoSectionName,
    eld::LinkerSectionKind DebugInfoSectionKind = eld::LinkerSectionKind::Debug,
    bool CheckLineTable = true) {
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

  // createDWARFContext checks the sections already parsed by ELD before
  // constructing the context from the original input bytes.
  eld::ELFSection DebugInfoSection(DebugInfoSectionKind,
                                   DebugInfoSectionName.str(), 0, 0, 1,
                                   llvm::ELF::SHT_PROGBITS, 0, nullptr, 0, 0);
  ObjectFile.addSection(&DebugInfoSection);
  ObjectFile.createDWARFContext();

  if (DebugInfoSectionKind == eld::LinkerSectionKind::Ignore ||
      DebugInfoSectionKind == eld::LinkerSectionKind::Discard) {
    EXPECT_EQ(nullptr, ObjectFile.getDWARFContext());
    return;
  }

  ASSERT_NE(nullptr, ObjectFile.getDWARFContext());
  const auto &CompileUnits = ObjectFile.getDWARFContext()->compile_units();
  ASSERT_FALSE(CompileUnits.empty());
  auto CompileUnitIt = CompileUnits.begin();
  llvm::DWARFUnit *Unit = CompileUnitIt->get();

  llvm::DWARFDie CompileUnit = Unit->getUnitDIE(false);
  ASSERT_TRUE(CompileUnit);
  EXPECT_EQ(llvm::dwarf::DW_TAG_compile_unit, CompileUnit.getTag());

  if (!CheckLineTable)
    return;

  const llvm::DWARFDebugLine::LineTable *LineTable =
      ObjectFile.getDWARFContext()->getLineTableForUnit(Unit);
  ASSERT_NE(nullptr, LineTable);
  EXPECT_FALSE(LineTable->Rows.empty());
}

TEST(DWARFObjectTest, ContextReadsCompilationUnitAndLineTable) {
  checkDWARFContext(DWARF_OBJECT_FIXTURE_PATH, ".debug_info");
}

#ifdef DWARF_OBJECT_COMPRESSED_FIXTURE_PATH
TEST(DWARFObjectTest, ContextReadsSHFCompressedSections) {
  checkDWARFContext(DWARF_OBJECT_COMPRESSED_FIXTURE_PATH, ".debug_info");
}
#endif

#ifdef DWARF_OBJECT_GNU_COMPRESSED_FIXTURE_PATH
TEST(DWARFObjectTest, ContextReadsGNUCompressedSections) {
  checkDWARFContext(DWARF_OBJECT_GNU_COMPRESSED_FIXTURE_PATH, ".zdebug_info");
}
#endif

#ifdef HAS_DWARF5_FIXTURE
TEST(DWARFObjectTest, ContextReadsDWARF5CompilationUnit) {
  checkDWARFContext(DWARF_OBJECT_DWARF5_FIXTURE_PATH, ".debug_info",
                    eld::LinkerSectionKind::Debug, /*CheckLineTable=*/false);
}
#endif

TEST(DWARFObjectTest, ContextSkipsIgnoredDebugSections) {
  checkDWARFContext(DWARF_OBJECT_FIXTURE_PATH, ".debug_info",
                    eld::LinkerSectionKind::Ignore);
}

} // namespace
