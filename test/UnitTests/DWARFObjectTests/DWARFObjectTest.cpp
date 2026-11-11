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
#include "llvm/Support/Error.h"
#include "llvm/Support/MemoryBuffer.h"
#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <optional>

#ifndef DWARF_OBJECT_FIXTURE_PATH
#error DWARF_OBJECT_FIXTURE_PATH must name the compiled DWARF fixture
#endif

namespace {

TEST(DWARFObjectTest, ContextReadsCompilationUnitAndLineTable) {
  auto BufferOrErr = llvm::MemoryBuffer::getFile(DWARF_OBJECT_FIXTURE_PATH);
  ASSERT_TRUE(BufferOrErr) << BufferOrErr.getError().message();

  std::unique_ptr<llvm::MemoryBuffer> Buffer = std::move(*BufferOrErr);
  llvm::StringRef Contents = Buffer->getBuffer();
  ASSERT_GE(Contents.size(), llvm::ELF::EI_NIDENT);

  const auto *Ident = reinterpret_cast<const uint8_t *>(Contents.data());
  if (Ident[llvm::ELF::EI_DATA] != llvm::ELF::ELFDATA2LSB)
    GTEST_SKIP() << "ELDDWARFObject currently supports little-endian ELF only";

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
  eld::Input Input(DWARF_OBJECT_FIXTURE_PATH, &DiagEngine);
  Input.setResolvedPath(DWARF_OBJECT_FIXTURE_PATH);
  eld::MemoryArea Memory(Buffer->getMemBufferRef());
  Input.setMemArea(&Memory);

  eld::ELFObjectFile ObjectFile(&Input, &DiagEngine);
  ObjectFile.setELFKind(Kind);

  // createDWARFContext checks the sections already parsed by ELD before
  // constructing the context from the original input bytes.
  eld::ELFSection DebugInfoSection(eld::LinkerSectionKind::Debug, ".debug_info",
                                   0, 0, 1, llvm::ELF::SHT_PROGBITS, 0, nullptr,
                                   0, 0);
  ObjectFile.addSection(&DebugInfoSection);
  ObjectFile.createDWARFContext();

  ASSERT_NE(nullptr, ObjectFile.getDWARFContext());
  const auto &CompileUnits = ObjectFile.getDWARFContext()->compile_units();
  ASSERT_FALSE(CompileUnits.empty());
  auto CompileUnitIt = CompileUnits.begin();
  llvm::DWARFUnit *Unit = CompileUnitIt->get();

  llvm::DWARFDie CompileUnit = Unit->getUnitDIE(false);
  ASSERT_TRUE(CompileUnit);
  std::optional<llvm::DWARFFormValue> Name =
      CompileUnit.find(llvm::dwarf::DW_AT_name);
  ASSERT_TRUE(Name);
  llvm::Expected<const char *> NameOrErr = Name->getAsCString();
  if (!NameOrErr)
    FAIL() << llvm::toString(NameOrErr.takeError());
  EXPECT_TRUE(llvm::StringRef(*NameOrErr).ends_with("DWARFObjectFixture.cpp"));

  const llvm::DWARFDebugLine::LineTable *LineTable =
      ObjectFile.getDWARFContext()->getLineTableForUnit(Unit);
  ASSERT_NE(nullptr, LineTable);
  EXPECT_FALSE(LineTable->Rows.empty());
}

} // namespace
