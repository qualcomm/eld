//===- implTest.cpp -------------------------------------------------------===//
//
//                     The MCLinker Project
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#include "eld/Config/LinkerConfig.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Support/FileSystem.h"
#include "eld/SymbolResolver/ResolveInfo.h"
#include "eld/SymbolResolver/StaticResolver.h"
#include <LTOPreserveListTest.h>

using namespace eld;

//===----------------------------------------------------------------------===//
// LTOPreserveListTest
//===----------------------------------------------------------------------===//
// Constructor can do set-up work for all test here.
LTOPreserveListTest::LTOPreserveListTest()
    : m_pResolver(NULL), m_pConfig(NULL) {
  // create testee. modify it if need

  m_pDiagEngine = make<DiagnosticEngine>(/*useColor=*/false);
  m_pResolver = make<StaticResolver>();
  m_pConfig = make<LinkerConfig>(m_pDiagEngine, "hexagon-unknown-elf");
  LinkerScript script(m_pConfig->getDiagEngine());
}

// Destructor can do clean-up work that doesn't throw exceptions here.
LTOPreserveListTest::~LTOPreserveListTest() {}

// SetUp() will be called immediately before each test.
void LTOPreserveListTest::SetUp() {}

// TearDown() will be called immediately after each test.
void LTOPreserveListTest::TearDown() {}

//==========================================================================//
// Testcases
//
TEST_F(LTOPreserveListTest, SimplePreserve) {
  ResolveInfo *old_sym = make<ResolveInfo>("abc");
  ResolveInfo *new_sym = make<ResolveInfo>("abc");

  Input *In1 = make<Input>("a.c", m_pDiagEngine);
  In1->setResolvedPath("a.c");
  old_sym->setResolvedOrigin(make<InputFile>(In1, m_pDiagEngine));
  Input *In2 = make<Input>("b.c", m_pDiagEngine);
  In2->setResolvedPath("b.c");
  new_sym->setResolvedOrigin(make<InputFile>(In2, m_pDiagEngine));

  old_sym->setDesc(ResolveInfo::Define);
  old_sym->setInBitCode(true);

  new_sym->setDesc(ResolveInfo::Undefined);
  new_sym->setInBitCode(false);

  ASSERT_TRUE(eld::ResolveInfo::Undefined == new_sym->desc());
  ASSERT_TRUE(eld::ResolveInfo::Define == old_sym->desc());
  bool override = true;
  bool result =
      m_pResolver->resolve(*old_sym, *new_sym, override, 0x0, m_pConfig, false);
  ASSERT_TRUE(result);
  ASSERT_FALSE(override);
  ASSERT_TRUE(old_sym->shouldPreserve());
}

TEST_F(LTOPreserveListTest, PreserveCommon) {
  ResolveInfo *old_sym = make<ResolveInfo>("abc");
  ResolveInfo *new_sym = make<ResolveInfo>("abc");

  Input *In1 = make<Input>("a.c", m_pDiagEngine);
  In1->setResolvedPath("a.c");
  old_sym->setResolvedOrigin(make<InputFile>(In1, m_pDiagEngine));
  Input *In2 = make<Input>("b.c", m_pDiagEngine);
  In2->setResolvedPath("b.c");
  new_sym->setResolvedOrigin(make<InputFile>(In2, m_pDiagEngine));

  old_sym->setDesc(ResolveInfo::Common);
  old_sym->setInBitCode(true);

  new_sym->setDesc(ResolveInfo::Common);
  new_sym->setInBitCode(false);

  ASSERT_TRUE(eld::ResolveInfo::Common == new_sym->desc());
  ASSERT_TRUE(eld::ResolveInfo::Common == old_sym->desc());
  bool override = true;
  bool result =
      m_pResolver->resolve(*old_sym, *new_sym, override, 0x0, m_pConfig, false);
  ASSERT_TRUE(result);
  ASSERT_FALSE(override);
  ASSERT_TRUE(old_sym->shouldPreserve());
}

TEST_F(LTOPreserveListTest, PreserveWeak) {
  ResolveInfo *old_sym = make<ResolveInfo>("abc");
  ResolveInfo *new_sym = make<ResolveInfo>("abc");

  Input *In1 = make<Input>("a.c", m_pDiagEngine);
  In1->setResolvedPath("a.c");
  old_sym->setResolvedOrigin(make<InputFile>(In1, m_pDiagEngine));
  Input *In2 = make<Input>("b.c", m_pDiagEngine);
  In2->setResolvedPath("b.c");
  new_sym->setResolvedOrigin(make<InputFile>(In2, m_pDiagEngine));

  old_sym->setDesc(ResolveInfo::Define);
  old_sym->setBinding(ResolveInfo::Weak);
  old_sym->setInBitCode(true);

  new_sym->setDesc(ResolveInfo::Undefined);
  new_sym->setInBitCode(false);

  ASSERT_TRUE(eld::ResolveInfo::Define == old_sym->desc());
  ASSERT_TRUE(eld::ResolveInfo::Weak == old_sym->binding());
  ASSERT_TRUE(eld::ResolveInfo::Undefined == new_sym->desc());
  bool override = true;
  bool result =
      m_pResolver->resolve(*old_sym, *new_sym, override, 0x0, m_pConfig, false);
  ASSERT_TRUE(result);
  ASSERT_FALSE(override);
  ASSERT_TRUE(old_sym->shouldPreserve());
}

TEST_F(LTOPreserveListTest, PreserveWithPartition) {
  ResolveInfo *old_sym = make<ResolveInfo>("abc");
  ResolveInfo *new_sym = make<ResolveInfo>("abc");

  Input *In1 = make<Input>("a.c", m_pDiagEngine);
  In1->setResolvedPath("a.c");
  old_sym->setResolvedOrigin(make<InputFile>(In1, m_pDiagEngine));
  Input *In2 = make<Input>("b.c", m_pDiagEngine);
  In2->setResolvedPath("b.c");
  new_sym->setResolvedOrigin(make<InputFile>(In2, m_pDiagEngine));

  old_sym->setDesc(ResolveInfo::Define);
  old_sym->setBinding(ResolveInfo::Global);
  old_sym->setInBitCode(true);

  new_sym->setDesc(ResolveInfo::Undefined);
  new_sym->setInBitCode(false);

  ASSERT_TRUE(eld::ResolveInfo::Define == old_sym->desc());
  ASSERT_TRUE(eld::ResolveInfo::Undefined == new_sym->desc());
  bool override = true;
  bool result =
      m_pResolver->resolve(*old_sym, *new_sym, override, 0x0, m_pConfig, false);
  ASSERT_TRUE(result);
  ASSERT_FALSE(override);
  ASSERT_TRUE(old_sym->shouldPreserve());

  // After LTO, the make<symbols> should override the symbols from bitcode.
  ResolveInfo *new_sym_after_lto_undef = make<ResolveInfo>("abc");
  Input *In3 = make<Input>("lto1.o", m_pDiagEngine);
  In3->setResolvedPath("lto1.o");
  new_sym_after_lto_undef->setResolvedOrigin(make<InputFile>(In3, m_pDiagEngine));
  new_sym_after_lto_undef->setDesc(ResolveInfo::Undefined);
  new_sym_after_lto_undef->setInBitCode(false);

  ResolveInfo *new_sym_after_lto_def = make<ResolveInfo>("abc");
  Input *In4 = make<Input>("lto2.o", m_pDiagEngine);
  In4->setResolvedPath("lto2.o");
  new_sym_after_lto_def->setResolvedOrigin(make<InputFile>(In4, m_pDiagEngine));
  new_sym_after_lto_def->setDesc(ResolveInfo::Define);
  new_sym_after_lto_def->setInBitCode(false);

  m_pResolver->resolve(*old_sym, *new_sym_after_lto_undef, override, 0x0, m_pConfig,
                       true);
  ASSERT_TRUE(old_sym->isBitCode());
}

TEST_F(LTOPreserveListTest, PreserveCommonRef) {
  // Symbol D in a.c as a Defined symbol.
  ResolveInfo *old_sym = make<ResolveInfo>("D");
  // Symbol D in b.c as a common symbol.
  ResolveInfo *new_sym = make<ResolveInfo>("D");

  Input *In1 = make<Input>("a.c", m_pDiagEngine);
  In1->setResolvedPath("a.c");
  old_sym->setResolvedOrigin(make<InputFile>(In1, m_pDiagEngine));
  Input *In2 = make<Input>("b.c", m_pDiagEngine);
  In2->setResolvedPath("b.c");
  new_sym->setResolvedOrigin(make<InputFile>(In2, m_pDiagEngine));

  old_sym->setDesc(ResolveInfo::Define);
  old_sym->setInBitCode(true);

  new_sym->setDesc(ResolveInfo::Common);
  new_sym->setInBitCode(false);

  ASSERT_FALSE(eld::ResolveInfo::Common == old_sym->desc());
  ASSERT_TRUE(eld::ResolveInfo::Common == new_sym->desc());
  bool override = true;
  m_pResolver->resolve(*old_sym, *new_sym, override, 0x0, m_pConfig, false);
  ASSERT_TRUE(old_sym->shouldPreserve());
}
