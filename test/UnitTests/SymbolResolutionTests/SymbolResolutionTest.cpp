//===- SymbolResolutionTest.cpp-----------------------------------------===//
//
//                     The MCLinker Project
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "SymbolResolutionTest.h"
#include "LinkerWrapper.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Core/LinkerScript.h"
#include "eld/Core/Module.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Fragment/Fragment.h"
#include "eld/Input/BitcodeFile.h"
#include "eld/Input/ELFDynObjectFile.h"
#include "eld/Input/ELFObjectFile.h"
#include "eld/Input/Input.h"
#include "eld/Input/InputFile.h"
#include "eld/Readers/BitcodeReader.h"
#include "eld/Readers/ELFSection.h"
#include "eld/SymbolResolver/IRBuilder.h"
#include "eld/SymbolResolver/ResolveInfo.h"
#include "eld/Target/GNULDBackend.h"
#include "eld/Target/LDFileFormat.h"
#include "eld/Target/TargetInfo.h"
#include "llvm/BinaryFormat/ELF.h"
#include "gtest/gtest.h"

using namespace eld;

SymbolResolutionTest::SymbolResolutionTest() {
  m_DiagEngine = make<DiagnosticEngine>(/*useColor=*/false);
  m_Config = make<LinkerConfig>(m_DiagEngine);
  LinkerScript *LScript = make<LinkerScript>(m_DiagEngine);
  m_Module = make<Module>(*LScript, *m_Config, /*layoutInfo=*/nullptr);
  m_IRBuilder = make<IRBuilder>(*m_Module, *m_Config);
}

SymbolResolutionTest::~SymbolResolutionTest() = default;

void SymbolResolutionTest::SetUp() {}
void SymbolResolutionTest::TearDown() {}

TEST_F(SymbolResolutionTest, StaticSymbolResolution) {
  Input *in1 = make<Input>("a.o", m_DiagEngine);
  InputFile *inFile1 = make<ELFObjectFile>(in1, m_DiagEngine);
  ELFSection *sect1 =
      make<ELFSection>(LDFileFormat::Kind::Regular, ".data.foo",
                       llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE, 0, 0,
                       llvm::ELF::SHT_PROGBITS, 0, nullptr, 0, 0);
  LDSymbol *symFooWeak = m_IRBuilder->addSymbol(
      *inFile1, "foo", ResolveInfo::Type::Function, ResolveInfo::Desc::Define,
      ResolveInfo::Binding::Weak, /*pSize=*/12, /*pValue=*/0x10, sect1,
      ResolveInfo::Visibility::Default, /*isPostLTOPhase=*/false, /*shndx=*/1,
      /*idx=*/1);
  ASSERT_TRUE(symFooWeak != nullptr);
  ASSERT_TRUE(symFooWeak->resolveInfo()->outSymbol() == symFooWeak);
  ASSERT_EQ(symFooWeak->resolveInfo()->outSymbol()->value(),
            static_cast<uint64_t>(0x10));

  Input *in2 = make<Input>("b.o", m_DiagEngine);
  InputFile *inFile2 = make<ELFObjectFile>(in2, m_DiagEngine);
  ELFSection *sect2 =
      make<ELFSection>(LDFileFormat::Kind::Common, ".bss.foo",
                       llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE, 0, 0,
                       llvm::ELF::SHT_NOBITS, 0, nullptr, 0, 0);
  LDSymbol *symFooCommon = m_IRBuilder->addSymbol(
      *inFile2, "foo", ResolveInfo::Type::Function, ResolveInfo::Desc::Common,
      ResolveInfo::Binding::Global, /*pSize=*/10, /*pValue=*/0x0, sect2,
      ResolveInfo::Visibility::Default, /*isPostLTOPhase=*/false, /*shndx=*/1,
      /*idx=*/1);
  ASSERT_TRUE(symFooCommon->resolveInfo()->outSymbol() == symFooCommon);
  ASSERT_TRUE(symFooWeak->resolveInfo()->outSymbol() == symFooCommon);
  ASSERT_TRUE(symFooCommon->resolveInfo() == symFooWeak->resolveInfo());
  ASSERT_EQ(symFooCommon->resolveInfo()->value(), static_cast<uint64_t>(0x0));

  Input *in3 = make<Input>("c.o", m_DiagEngine);
  InputFile *inFile3 = make<ELFObjectFile>(in3, m_DiagEngine);
  ELFSection *sect3 =
      make<ELFSection>(LDFileFormat::Kind::Regular, ".data.foo",
                       llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE, 0, 0,
                       llvm::ELF::SHT_PROGBITS, 0, nullptr, 0, 0);
  LDSymbol *symFooGlobalDef = m_IRBuilder->addSymbol(
      *inFile3, "foo", ResolveInfo::Type::Function, ResolveInfo::Desc::Define,
      ResolveInfo::Binding::Global, /*pSize=*/10, /*pValue=*/0x30, sect3,
      ResolveInfo::Visibility::Default, /*isPostLTOPhase=*/false, /*shndx=*/1,
      /*idx=*/1);
  ASSERT_TRUE(symFooGlobalDef->resolveInfo()->outSymbol() == symFooGlobalDef);
  ASSERT_TRUE(symFooGlobalDef->resolveInfo() == symFooCommon->resolveInfo());
  ASSERT_EQ(symFooGlobalDef->resolveInfo()->value(),
            static_cast<uint64_t>(0x30));
}

TEST_F(SymbolResolutionTest, CommonSymbolResolution) {
  Input *in1 = make<Input>("a.o", m_DiagEngine);
  InputFile *inFile1 = make<ELFObjectFile>(in1, m_DiagEngine);
  ELFSection *sect1 =
      make<ELFSection>(LDFileFormat::Kind::Common, ".bss.foo",
                       llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE, 0, 0,
                       llvm::ELF::SHT_NOBITS, 0, nullptr, 0, 0);
  LDSymbol *symFooCommon50 = m_IRBuilder->addSymbol(
      *inFile1, "foo", ResolveInfo::Type::Function, ResolveInfo::Desc::Common,
      ResolveInfo::Binding::Global, /*pSize=*/50, /*pValue=*/0x0, sect1,
      ResolveInfo::Visibility::Default, /*isPostLTOPhase=*/false, /*shndx=*/1,
      /*idx=*/1);
  ASSERT_TRUE(symFooCommon50 != nullptr);
  ASSERT_TRUE(symFooCommon50->resolveInfo()->outSymbol() == symFooCommon50);

  Input *in2 = make<Input>("b.o", m_DiagEngine);
  InputFile *inFile2 = make<ELFObjectFile>(in2, m_DiagEngine);
  ELFSection *sect2 =
      make<ELFSection>(LDFileFormat::Kind::Common, ".bss.foo",
                       llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE, 0, 0,
                       llvm::ELF::SHT_NOBITS, 0, nullptr, 0, 0);
  LDSymbol *symFooCommon10 = m_IRBuilder->addSymbol(
      *inFile2, "foo", ResolveInfo::Type::Function, ResolveInfo::Desc::Common,
      ResolveInfo::Binding::Global, /*pSize=*/10, /*pValue=*/0x20, sect2,
      ResolveInfo::Visibility::Default, /*isPostLTOPhase=*/false, /*shndx=*/1,
      /*idx=*/1);
  ASSERT_TRUE(symFooCommon10 != nullptr);
  ASSERT_TRUE(symFooCommon10->resolveInfo()->outSymbol() == symFooCommon50);
  ASSERT_TRUE(symFooCommon10->resolveInfo() == symFooCommon50->resolveInfo());

  Input *in3 = make<Input>("c.o", m_DiagEngine);
  InputFile *inFile3 = make<ELFObjectFile>(in3, m_DiagEngine);
  ELFSection *sect3 =
      make<ELFSection>(LDFileFormat::Kind::Common, ".bss.foo",
                       llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE, 0, 0,
                       llvm::ELF::SHT_NOBITS, 0, nullptr, 0, 0);
  LDSymbol *symFooCommon200 = m_IRBuilder->addSymbol(
      *inFile3, "foo", ResolveInfo::Type::Function, ResolveInfo::Desc::Common,
      ResolveInfo::Binding::Global, /*pSize=*/200, /*pValue=*/0x0, sect3,
      ResolveInfo::Visibility::Default, /*isPostLTOPhase=*/false, /*shndx=*/1,
      /*idx=*/1);
  ASSERT_TRUE(symFooCommon200 != nullptr);
  ASSERT_TRUE(symFooCommon200->resolveInfo()->outSymbol() == symFooCommon200);
  ASSERT_TRUE(symFooCommon200->resolveInfo() == symFooCommon10->resolveInfo());
}

TEST_F(SymbolResolutionTest, DynamicSymbolResolution) {
  Input *in1 = make<Input>("a.o", m_DiagEngine);
  InputFile *inFile1 = make<ELFDynObjectFile>(in1, m_DiagEngine);
  ELFSection *sect1 =
      make<ELFSection>(LDFileFormat::Kind::Regular, ".text.foo",
                       llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_EXECINSTR, 0, 0,
                       llvm::ELF::SHT_PROGBITS, 0, nullptr, 0, 0);
  LDSymbol *symFooDyn1 = m_IRBuilder->addSymbol(
      *inFile1, "foo", ResolveInfo::Type::Function, ResolveInfo::Desc::Define,
      ResolveInfo::Binding::Global, /*pSize=*/12, /*pValue=*/0x10, sect1,
      ResolveInfo::Visibility::Default, /*isPostLTOPhase=*/false, /*shndx=*/1,
      /*idx=*/1);
  ASSERT_TRUE(symFooDyn1 != nullptr);
  ASSERT_TRUE(symFooDyn1->resolveInfo()->outSymbol() == nullptr);
  ASSERT_EQ(symFooDyn1->resolveInfo()->value(), static_cast<uint64_t>(0x10));

  Input *in2 = make<Input>("b.o", m_DiagEngine);
  InputFile *inFile2 = make<ELFObjectFile>(in2, m_DiagEngine);
  LDSymbol *symFooUndef = m_IRBuilder->addSymbol(
      *inFile2, "foo", ResolveInfo::Type::Function,
      ResolveInfo::Desc::Undefined, ResolveInfo::Binding::Global, /*pSize=*/10,
      /*pValue=*/0x0, /*pSection=*/nullptr, ResolveInfo::Visibility::Default,
      /*isPostLTOPhase=*/false, /*shndx=*/0,
      /*idx=*/1);
  ASSERT_TRUE(symFooUndef != nullptr);
  ASSERT_TRUE(symFooUndef->resolveInfo()->outSymbol() == symFooDyn1);
  ASSERT_TRUE(symFooUndef->resolveInfo() == symFooDyn1->resolveInfo());
  ASSERT_EQ(symFooUndef->resolveInfo()->value(), static_cast<uint64_t>(0x10));
  ASSERT_TRUE(symFooUndef->resolveInfo()->resolvedOrigin() == inFile1);

  Input *in3 = make<Input>("c.o", m_DiagEngine);
  InputFile *inFile3 = make<ELFDynObjectFile>(in3, m_DiagEngine);
  ELFSection *sect3 =
      make<ELFSection>(LDFileFormat::Kind::Regular, ".text.foo",
                       llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_EXECINSTR, 0, 0,
                       llvm::ELF::SHT_PROGBITS, 0, nullptr, 0, 0);
  LDSymbol *symFooDyn2 = m_IRBuilder->addSymbol(
      *inFile3, "foo", ResolveInfo::Type::Function, ResolveInfo::Desc::Define,
      ResolveInfo::Binding::Global, /*pSize=*/10,
      /*pValue=*/0x30, sect3, ResolveInfo::Visibility::Default,
      /*isPostLTOPhase=*/false, /*shndx=*/1,
      /*idx=*/1);
  ASSERT_TRUE(symFooDyn2->resolveInfo()->outSymbol() == symFooDyn1);
  ASSERT_TRUE(symFooDyn2->resolveInfo() == symFooUndef->resolveInfo());
  ASSERT_EQ(symFooDyn2->resolveInfo()->value(), static_cast<uint64_t>(0x10));
  ASSERT_TRUE(symFooDyn2->resolveInfo()->resolvedOrigin() == inFile1);
}
TEST_F(SymbolResolutionTest, LTOSymbolResolution) {
  Input *in1 = make<Input>("a.o", m_DiagEngine);
  InputFile *inFile1 = make<ELFObjectFile>(in1, m_DiagEngine);
  ELFSection *sect1 =
      make<ELFSection>(LDFileFormat::Kind::Regular, ".text.foo",
                       llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_EXECINSTR, 0, 0,
                       llvm::ELF::SHT_PROGBITS, 0, nullptr, 0, 0);
  LDSymbol *symFooBitcode = m_IRBuilder->addSymbol(
      *inFile1, "foo", ResolveInfo::Type::Function, ResolveInfo::Desc::Define,
      ResolveInfo::Binding::Global, /*pSize=*/12, /*pValue=*/0x10, sect1,
      ResolveInfo::Visibility::Default, /*isPostLTOPhase=*/false, /*shndx=*/1,
      /*idx=*/1);
  ASSERT_TRUE(symFooBitcode != nullptr);
  ASSERT_TRUE(symFooBitcode->resolveInfo()->outSymbol() == symFooBitcode);
  ASSERT_EQ(symFooBitcode->resolveInfo()->value(), static_cast<uint64_t>(0x10));

  symFooBitcode->resolveInfo()->setInBitCode(true);

  Input *in2 = make<Input>("b.o", m_DiagEngine);
  InputFile *inFile2 = make<ELFObjectFile>(in2, m_DiagEngine);
  ELFSection *sect2 =
      make<ELFSection>(LDFileFormat::Kind::Regular, ".text.foo",
                       llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_EXECINSTR, 0, 0,
                       llvm::ELF::SHT_PROGBITS, 0, nullptr, 0, 0);
  LDSymbol *symFooLTOObject = m_IRBuilder->addSymbol(
      *inFile2, "foo", ResolveInfo::Type::Function, ResolveInfo::Desc::Define,
      ResolveInfo::Binding::Global, /*pSize=*/12, /*pValue=*/0x10, sect2,
      ResolveInfo::Visibility::Default, /*isPostLTOPhase=*/true, /*shndx=*/1,
      /*idx=*/1);
  ASSERT_TRUE(symFooLTOObject != nullptr);
  ASSERT_TRUE(symFooLTOObject->resolveInfo()->outSymbol() == symFooLTOObject);
  ASSERT_TRUE(symFooLTOObject->resolveInfo() == symFooBitcode->resolveInfo());
  ASSERT_TRUE(symFooLTOObject->resolveInfo()->isBitCode() == false);
}