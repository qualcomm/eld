//===- implTest.cpp -------------------------------------------------------===//
//
//                     The MCLinker Project
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#include "eld/SymbolResolver/StaticResolver.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Support/FileSystem.h"
#include "eld/Support/MsgHandling.h"
#include "eld/SymbolResolver/ResolveInfo.h"
#include <StaticResolverTest.h>

using namespace eld;

//===----------------------------------------------------------------------===//
// StaticResolverTest
//===----------------------------------------------------------------------===//
// Constructor can do set-up work for all test here.
StaticResolverTest::StaticResolverTest() : m_pResolver(NULL), m_pConfig(NULL) {
  // create testee. modify it if need
  m_pResolver = make<StaticResolver>();

  m_pDiagEngine = make<DiagnosticEngine>(/*useColor=*/false);
  m_pConfig = make<LinkerConfig>(m_pDiagEngine, "hexagon-unknown-elf");
  LinkerScript script(m_pConfig->getDiagEngine());
}

// Destructor can do clean-up work that doesn't throw exceptions here.
StaticResolverTest::~StaticResolverTest() {}

// SetUp() will be called immediately before each test.
void StaticResolverTest::SetUp() {}

// TearDown() will be called immediately after each test.
void StaticResolverTest::TearDown() {}

//==========================================================================//
// Testcases
//
TEST_F(StaticResolverTest, MDEF) {
  ResolveInfo *old_sym = make<ResolveInfo>("abc");
  ResolveInfo *new_sym = make<ResolveInfo>("abc");
  Input *In1 = make<Input>("a.o", m_pDiagEngine);
  In1->setResolvedPath("a.o");
  old_sym->setResolvedOrigin(make<InputFile>(In1, m_pDiagEngine));
  Input *In2 = make<Input>("b.so", m_pDiagEngine);
  In2->setResolvedPath("b.so");
  new_sym->setResolvedOrigin(make<InputFile>(In2, m_pDiagEngine));
  new_sym->setDesc(ResolveInfo::Define);
  old_sym->setDesc(ResolveInfo::Define);
  ASSERT_TRUE(eld::ResolveInfo::Define == new_sym->desc());
  ASSERT_TRUE(eld::ResolveInfo::Define == old_sym->desc());
  ASSERT_TRUE(eld::ResolveInfo::DefineFlag == new_sym->info());
  ASSERT_TRUE(eld::ResolveInfo::DefineFlag == old_sym->info());
  bool override = true;
  bool result =
      m_pResolver->resolve(*old_sym, *new_sym, override, 0x0, m_pConfig, false);
  ASSERT_TRUE(result);
  ASSERT_FALSE(override);
}

TEST_F(StaticResolverTest, MDEFD) {
  ResolveInfo *old_sym = make<ResolveInfo>("abc");
  Input *In1 = make<Input>("a.o", m_pDiagEngine);
  In1->setResolvedPath("a.o");
  old_sym->setResolvedOrigin(make<InputFile>(In1, m_pDiagEngine));
  old_sym->setDesc(ResolveInfo::Undefined);

  ResolveInfo *new_sym = make<ResolveInfo>("abc");
  Input *In2 = make<Input>("b.so", m_pDiagEngine);
  In2->setResolvedPath("b.so");
  new_sym->setResolvedOrigin(make<InputFile>(In2, m_pDiagEngine));
  new_sym->setDesc(ResolveInfo::Define);
  new_sym->setSource(true);

  ASSERT_TRUE(eld::ResolveInfo::Undefined == old_sym->desc());
  ASSERT_TRUE(old_sym->isUndef());

  ASSERT_TRUE(eld::ResolveInfo::Define == new_sym->desc());
  ASSERT_TRUE(new_sym->isDefine());
  ASSERT_TRUE(new_sym->isDyn());

  bool override = true;
  bool result =
      m_pResolver->resolve(*old_sym, *new_sym, override, 0x0, m_pConfig, false);
  ASSERT_TRUE(result);
  ASSERT_TRUE(old_sym->isDyn());
  ASSERT_TRUE(old_sym->isDefine());
  ASSERT_TRUE(override);
}

TEST_F(StaticResolverTest, DUND) {
  ResolveInfo *old_sym = make<ResolveInfo>("abc");
  ResolveInfo::Visibility oldV = ResolveInfo::Protected;
  Input *In1 = make<Input>("a.so", m_pDiagEngine);
  In1->setResolvedPath("a.so");
  old_sym->setResolvedOrigin(make<InputFile>(In1, m_pDiagEngine));
  old_sym->setDesc(ResolveInfo::Define);
  old_sym->setSource(true);
  old_sym->setVisibility(oldV);

  ResolveInfo::Visibility newV = ResolveInfo::Default;
  ResolveInfo *new_sym = make<ResolveInfo>("abc");

  Input *In2 = make<Input>("b.o", m_pDiagEngine);
  In2->setResolvedPath("b.o");
  new_sym->setResolvedOrigin(make<InputFile>(In2, m_pDiagEngine));

  new_sym->setDesc(ResolveInfo::Undefined);
  new_sym->setVisibility(newV);

  ASSERT_TRUE(eld::ResolveInfo::Define == old_sym->desc());
  ASSERT_TRUE(old_sym->isDefine());
  ASSERT_TRUE(old_sym->isDyn());

  ASSERT_TRUE(eld::ResolveInfo::Undefined == new_sym->desc());
  ASSERT_TRUE(new_sym->isUndef());

  bool override = true;
  bool result =
      m_pResolver->resolve(*old_sym, *new_sym, override, 0x0, m_pConfig, false);
  ASSERT_TRUE(old_sym->visibility() == newV);
  ASSERT_TRUE(result);
  ASSERT_FALSE(override);
}

TEST_F(StaticResolverTest, VisibilityProtected) {
  ResolveInfo *old_sym = make<ResolveInfo>("abc");
  Input *In1 = make<Input>("a.o", m_pDiagEngine);
  In1->setResolvedPath("a.o");
  old_sym->setResolvedOrigin(make<InputFile>(In1, m_pDiagEngine));
  old_sym->setDesc(ResolveInfo::Undefined);
  old_sym->setVisibility(ResolveInfo::Protected);

  ResolveInfo *new_sym = make<ResolveInfo>("abc");
  Input *In2 = make<Input>("b.so", m_pDiagEngine);
  In2->setResolvedPath("b.so");
  new_sym->setResolvedOrigin(make<InputFile>(In2, m_pDiagEngine));

  new_sym->setDesc(ResolveInfo::Define);
  new_sym->setSource(true);

  ASSERT_TRUE(eld::ResolveInfo::Undefined == old_sym->desc());
  ASSERT_TRUE(old_sym->isUndef());
  ASSERT_TRUE(old_sym->visibility() == ResolveInfo::Protected);

  ASSERT_TRUE(eld::ResolveInfo::Define == new_sym->desc());
  ASSERT_TRUE(new_sym->isDefine());
  ASSERT_TRUE(new_sym->isDyn());

  bool override = true;
  bool result =
      m_pResolver->resolve(*old_sym, *new_sym, override, 0x0, m_pConfig, false);
  ASSERT_TRUE(result);
  ASSERT_TRUE(old_sym->isUndef());
  ASSERT_FALSE(override);
}

TEST_F(StaticResolverTest, VisibilityHidden) {
  ResolveInfo *old_sym = make<ResolveInfo>("abc");
  Input *In1 = make<Input>("a.o", m_pDiagEngine);
  In1->setResolvedPath("a.o");
  old_sym->setResolvedOrigin(make<InputFile>(In1, m_pDiagEngine));
  old_sym->setDesc(ResolveInfo::Undefined);
  old_sym->setVisibility(ResolveInfo::Hidden);

  ResolveInfo *new_sym = make<ResolveInfo>("abc");
  Input *In2 = make<Input>("b.so", m_pDiagEngine);
  In2->setResolvedPath("b.so");
  new_sym->setResolvedOrigin(make<InputFile>(In2, m_pDiagEngine));
  new_sym->setDesc(ResolveInfo::Define);
  new_sym->setSource(true);

  ASSERT_TRUE(eld::ResolveInfo::Undefined == old_sym->desc());
  ASSERT_TRUE(old_sym->isUndef());
  ASSERT_TRUE(old_sym->visibility() == ResolveInfo::Hidden);

  ASSERT_TRUE(eld::ResolveInfo::Define == new_sym->desc());
  ASSERT_TRUE(new_sym->isDefine());
  ASSERT_TRUE(new_sym->isDyn());

  bool override = true;
  bool result =
      m_pResolver->resolve(*old_sym, *new_sym, override, 0x0, m_pConfig, false);
  ASSERT_TRUE(result);
  ASSERT_TRUE(old_sym->isUndef());
  ASSERT_FALSE(override);
}

TEST_F(StaticResolverTest, DynDefAfterDynUndef) {
  ResolveInfo *old_sym = make<ResolveInfo>("abc");
  ResolveInfo *new_sym = make<ResolveInfo>("abc");
  Input *In1 = make<Input>("a.c", m_pDiagEngine);
  In1->setResolvedPath("a.c");
  old_sym->setResolvedOrigin(make<InputFile>(In1, m_pDiagEngine));
  Input *In2 = make<Input>("b.c", m_pDiagEngine);
  In2->setResolvedPath("b.c");
  new_sym->setResolvedOrigin(make<InputFile>(In2, m_pDiagEngine));

  new_sym->setBinding(ResolveInfo::Global);
  old_sym->setBinding(ResolveInfo::Global);
  new_sym->setDesc(ResolveInfo::Undefined);
  old_sym->setDesc(ResolveInfo::Define);
  new_sym->setSource(true);
  old_sym->setSource(true);

  new_sym->setSize(0);

  old_sym->setSize(1);

  ASSERT_TRUE(eld::ResolveInfo::Global == new_sym->binding());
  ASSERT_TRUE(eld::ResolveInfo::Global == old_sym->binding());
  ASSERT_TRUE(eld::ResolveInfo::Undefined == new_sym->desc());
  ASSERT_TRUE(eld::ResolveInfo::Define == old_sym->desc());

  bool override = false;
  bool result =
      m_pResolver->resolve(*old_sym, *new_sym, override, 0x0, m_pConfig, false);
  ASSERT_TRUE(result);
  ASSERT_FALSE(override);
  ASSERT_TRUE(1 == old_sym->size());
}

TEST_F(StaticResolverTest, DynDefAfterDynDef) {
  ResolveInfo *old_sym = make<ResolveInfo>("abc");
  ResolveInfo *new_sym = make<ResolveInfo>("abc");
  Input *In1 = make<Input>("a.c", m_pDiagEngine);
  In1->setResolvedPath("a.c");
  old_sym->setResolvedOrigin(make<InputFile>(In1, m_pDiagEngine));
  Input *In2 = make<Input>("b.c", m_pDiagEngine);
  In2->setResolvedPath("b.c");
  new_sym->setResolvedOrigin(make<InputFile>(In2, m_pDiagEngine));

  new_sym->setBinding(ResolveInfo::Global);
  old_sym->setBinding(ResolveInfo::Global);
  new_sym->setDesc(ResolveInfo::Define);
  old_sym->setDesc(ResolveInfo::Define);
  new_sym->setSource(true);
  old_sym->setSource(true);

  new_sym->setSize(0);

  old_sym->setSize(1);

  ASSERT_TRUE(eld::ResolveInfo::Global == new_sym->binding());
  ASSERT_TRUE(eld::ResolveInfo::Global == old_sym->binding());
  ASSERT_TRUE(eld::ResolveInfo::Define == new_sym->desc());
  ASSERT_TRUE(eld::ResolveInfo::Define == old_sym->desc());

  bool override = false;
  bool result =
      m_pResolver->resolve(*old_sym, *new_sym, override, 0x0, m_pConfig, false);
  ASSERT_TRUE(result);
  ASSERT_FALSE(override);
  ASSERT_TRUE(1 == old_sym->size());
}

TEST_F(StaticResolverTest, DynUndefAfterDynUndef) {
  ResolveInfo *old_sym = make<ResolveInfo>("abc");
  ResolveInfo *new_sym = make<ResolveInfo>("abc");
  Input *In1 = make<Input>("a.c", m_pDiagEngine);
  In1->setResolvedPath("a.c");
  old_sym->setResolvedOrigin(make<InputFile>(In1, m_pDiagEngine));
  Input *In2 = make<Input>("b.c", m_pDiagEngine);
  In2->setResolvedPath("b.c");
  new_sym->setResolvedOrigin(make<InputFile>(In2, m_pDiagEngine));

  new_sym->setBinding(ResolveInfo::Global);
  old_sym->setBinding(ResolveInfo::Global);
  new_sym->setDesc(ResolveInfo::Undefined);
  old_sym->setDesc(ResolveInfo::Undefined);
  new_sym->setSource(true);
  old_sym->setSource(true);

  new_sym->setSize(0);

  old_sym->setSize(1);

  ASSERT_TRUE(eld::ResolveInfo::Global == new_sym->binding());
  ASSERT_TRUE(eld::ResolveInfo::Global == old_sym->binding());
  ASSERT_TRUE(eld::ResolveInfo::Undefined == new_sym->desc());
  ASSERT_TRUE(eld::ResolveInfo::Undefined == old_sym->desc());

  bool override = false;
  bool result =
      m_pResolver->resolve(*old_sym, *new_sym, override, 0x0, m_pConfig, false);
  ASSERT_TRUE(result);
  ASSERT_FALSE(override);
  ASSERT_TRUE(1 == old_sym->size());
}

TEST_F(StaticResolverTest, OverrideWeakByGlobal) {
  ResolveInfo *old_sym = make<ResolveInfo>("abc");
  ResolveInfo *new_sym = make<ResolveInfo>("abc");
  Input *In1 = make<Input>("a.c", m_pDiagEngine);
  In1->setResolvedPath("a.c");
  old_sym->setResolvedOrigin(make<InputFile>(In1, m_pDiagEngine));
  Input *In2 = make<Input>("b.c", m_pDiagEngine);
  In2->setResolvedPath("b.c");
  new_sym->setResolvedOrigin(make<InputFile>(In2, m_pDiagEngine));
  new_sym->setBinding(ResolveInfo::Global);
  old_sym->setBinding(ResolveInfo::Weak);
  new_sym->setSize(0);
  old_sym->setSize(1);

  ASSERT_TRUE(eld::ResolveInfo::Global == new_sym->binding());
  ASSERT_TRUE(eld::ResolveInfo::Weak == old_sym->binding());

  ASSERT_TRUE(eld::ResolveInfo::GlobalFlag == new_sym->info());
  ASSERT_TRUE(eld::ResolveInfo::WeakFlag == old_sym->info());
  bool override = false;
  bool result =
      m_pResolver->resolve(*old_sym, *new_sym, override, 0x0, m_pConfig, false);
  ASSERT_TRUE(result);
  ASSERT_TRUE(override);
  ASSERT_TRUE(0 == old_sym->size());
}

TEST_F(StaticResolverTest, DynWeakAfterDynDef) {

  ResolveInfo *old_sym = make<ResolveInfo>("abc");
  ResolveInfo *new_sym = make<ResolveInfo>("abc");
  Input *In1 = make<Input>("a.c", m_pDiagEngine);
  In1->setResolvedPath("a.c");
  old_sym->setResolvedOrigin(make<InputFile>(In1, m_pDiagEngine));
  Input *In2 = make<Input>("b.c", m_pDiagEngine);
  In2->setResolvedPath("b.c");
  new_sym->setResolvedOrigin(make<InputFile>(In2, m_pDiagEngine));
  old_sym->setBinding(ResolveInfo::Weak);
  new_sym->setBinding(ResolveInfo::Global);

  new_sym->setSource(true);
  old_sym->setSource(true);

  old_sym->setDesc(ResolveInfo::Define);
  new_sym->setDesc(ResolveInfo::Define);

  new_sym->setSize(0);

  old_sym->setSize(1);

  ASSERT_TRUE(eld::ResolveInfo::Weak == old_sym->binding());
  ASSERT_TRUE(eld::ResolveInfo::Global == new_sym->binding());
  ASSERT_TRUE(eld::ResolveInfo::Define == old_sym->desc());
  ASSERT_TRUE(eld::ResolveInfo::Define == new_sym->desc());

  bool override = false;
  bool result =
      m_pResolver->resolve(*old_sym, *new_sym, override, 0x0, m_pConfig, false);
  ASSERT_TRUE(result);
  ASSERT_FALSE(override);
  ASSERT_TRUE(1 == old_sym->size());
}

TEST_F(StaticResolverTest, MarkByBiggerCommon) {

  ResolveInfo *old_sym = make<ResolveInfo>("abc");
  ResolveInfo *new_sym = make<ResolveInfo>("abc");
  Input *In1 = make<Input>("a.c", m_pDiagEngine);
  In1->setResolvedPath("a.c");
  old_sym->setResolvedOrigin(make<InputFile>(In1, m_pDiagEngine));
  Input *In2 = make<Input>("b.c", m_pDiagEngine);
  In2->setResolvedPath("b.c");
  new_sym->setResolvedOrigin(make<InputFile>(In2, m_pDiagEngine));
  new_sym->setDesc(ResolveInfo::Common);
  old_sym->setDesc(ResolveInfo::Common);
  new_sym->setSize(999);
  old_sym->setSize(0);

  ASSERT_TRUE(eld::ResolveInfo::Common == new_sym->desc());
  ASSERT_TRUE(eld::ResolveInfo::Common == old_sym->desc());

  ASSERT_TRUE(eld::ResolveInfo::CommonFlag == new_sym->info());
  ASSERT_TRUE(eld::ResolveInfo::CommonFlag == old_sym->info());
  bool override = true;
  bool result =
      m_pResolver->resolve(*old_sym, *new_sym, override, 0x0, m_pConfig, false);
  ASSERT_TRUE(result);
  ASSERT_TRUE(override);
  ASSERT_TRUE(999 == old_sym->size());
}

TEST_F(StaticResolverTest, OverrideByBiggerCommon) {
  ResolveInfo *old_sym = make<ResolveInfo>("abc");
  ResolveInfo *new_sym = make<ResolveInfo>("abc");
  Input *In1 = make<Input>("a.c", m_pDiagEngine);
  In1->setResolvedPath("a.c");
  old_sym->setResolvedOrigin(make<InputFile>(In1, m_pDiagEngine));
  Input *In2 = make<Input>("b.c", m_pDiagEngine);
  In2->setResolvedPath("b.c");
  new_sym->setResolvedOrigin(make<InputFile>(In2, m_pDiagEngine));
  new_sym->setDesc(ResolveInfo::Common);
  old_sym->setDesc(ResolveInfo::Common);
  old_sym->setBinding(ResolveInfo::Weak);
  new_sym->setSize(999);
  old_sym->setSize(0);

  ASSERT_TRUE(ResolveInfo::Common == new_sym->desc());
  ASSERT_TRUE(ResolveInfo::Common == old_sym->desc());
  ASSERT_TRUE(ResolveInfo::Weak == old_sym->binding());

  ASSERT_TRUE(ResolveInfo::CommonFlag == new_sym->info());
  ASSERT_TRUE((ResolveInfo::WeakFlag | ResolveInfo::CommonFlag) ==
              old_sym->info());

  bool override = false;
  bool result =
      m_pResolver->resolve(*old_sym, *new_sym, override, 0x0, m_pConfig, false);
  ASSERT_TRUE(result);
  ASSERT_TRUE(override);
  ASSERT_TRUE(999 == old_sym->size());
}

TEST_F(StaticResolverTest, OverrideCommonByDefine) {
  ResolveInfo *old_sym = make<ResolveInfo>("abc");
  ResolveInfo *new_sym = make<ResolveInfo>("abc");
  Input *In1 = make<Input>("a.c", m_pDiagEngine);
  In1->setResolvedPath("a.c");
  old_sym->setResolvedOrigin(make<InputFile>(In1, m_pDiagEngine));
  Input *In2 = make<Input>("b.c", m_pDiagEngine);
  In2->setResolvedPath("b.c");
  new_sym->setResolvedOrigin(make<InputFile>(In2, m_pDiagEngine));
  old_sym->setDesc(ResolveInfo::Common);
  old_sym->setSize(0);

  new_sym->setDesc(ResolveInfo::Define);
  new_sym->setSize(999);

  ASSERT_TRUE(ResolveInfo::Define == new_sym->desc());
  ASSERT_TRUE(ResolveInfo::Common == old_sym->desc());

  ASSERT_TRUE(ResolveInfo::DefineFlag == new_sym->info());
  ASSERT_TRUE(ResolveInfo::CommonFlag == old_sym->info());

  bool override = false;
  bool result =
      m_pResolver->resolve(*old_sym, *new_sym, override, 0x0, m_pConfig, false);
  ASSERT_TRUE(result);
  ASSERT_TRUE(override);
  ASSERT_TRUE(999 == old_sym->size());
}

TEST_F(StaticResolverTest, SetUpDesc) {
  ResolveInfo *sym = make<ResolveInfo>("abc");
  Input *In = make<Input>("a.c", m_pDiagEngine);
  In->setResolvedPath("a.c");
  sym->setResolvedOrigin(make<InputFile>(In, m_pDiagEngine));

  sym->setIsSymbol(true);

  //  ASSERT_FALSE( sym->isSymbol() );
  ASSERT_TRUE(sym->isSymbol());
  ASSERT_TRUE(sym->isGlobal());
  ASSERT_FALSE(sym->isWeak());
  ASSERT_FALSE(sym->isLocal());
  ASSERT_FALSE(sym->isDefine());
  ASSERT_TRUE(sym->isUndef());
  ASSERT_FALSE(sym->isDyn());
  ASSERT_FALSE(sym->isCommon());
  ASSERT_TRUE(ResolveInfo::NoType == sym->type());
  ASSERT_TRUE(0 == sym->desc());
  ASSERT_TRUE(0 == sym->binding());
  ASSERT_TRUE(0 == sym->other());

  sym->setIsSymbol(false);
  ASSERT_FALSE(sym->isSymbol());
  //  ASSERT_TRUE( sym->isSymbol() );
  ASSERT_TRUE(sym->isGlobal());
  ASSERT_FALSE(sym->isWeak());
  ASSERT_FALSE(sym->isLocal());
  ASSERT_FALSE(sym->isDefine());
  ASSERT_TRUE(sym->isUndef());
  ASSERT_FALSE(sym->isDyn());
  ASSERT_FALSE(sym->isCommon());
  ASSERT_TRUE(ResolveInfo::NoType == sym->type());
  ASSERT_TRUE(0 == sym->desc());
  ASSERT_TRUE(0 == sym->binding());
  ASSERT_TRUE(0 == sym->other());

  sym->setDesc(ResolveInfo::Define);
  ASSERT_FALSE(sym->isSymbol());
  //  ASSERT_TRUE( sym->isSymbol() );
  ASSERT_TRUE(sym->isGlobal());
  ASSERT_FALSE(sym->isWeak());
  ASSERT_FALSE(sym->isLocal());
  ASSERT_TRUE(sym->isDefine());
  ASSERT_FALSE(sym->isUndef());
  ASSERT_FALSE(sym->isDyn());
  ASSERT_FALSE(sym->isCommon());
  ASSERT_TRUE(ResolveInfo::NoType == sym->type());
  ASSERT_TRUE(ResolveInfo::Define == sym->desc());
  ASSERT_TRUE(0 == sym->binding());
  ASSERT_TRUE(0 == sym->other());

  sym->setDesc(ResolveInfo::Common);
  ASSERT_FALSE(sym->isSymbol());
  //  ASSERT_TRUE( sym->isSymbol() );
  ASSERT_TRUE(sym->isGlobal());
  ASSERT_FALSE(sym->isWeak());
  ASSERT_FALSE(sym->isLocal());
  ASSERT_FALSE(sym->isDyn());
  ASSERT_FALSE(sym->isDefine());
  ASSERT_FALSE(sym->isUndef());
  ASSERT_TRUE(sym->isCommon());
  ASSERT_TRUE(ResolveInfo::NoType == sym->type());
  ASSERT_TRUE(ResolveInfo::Common == sym->desc());
  ASSERT_TRUE(0 == sym->binding());
  ASSERT_TRUE(0 == sym->other());

  sym->setDesc(ResolveInfo::Undefined);
  ASSERT_FALSE(sym->isSymbol());
  ASSERT_TRUE(sym->isGlobal());
  ASSERT_FALSE(sym->isWeak());
  ASSERT_FALSE(sym->isLocal());
  ASSERT_FALSE(sym->isDyn());
  ASSERT_TRUE(sym->isUndef());
  ASSERT_FALSE(sym->isDefine());
  ASSERT_FALSE(sym->isCommon());
  ASSERT_TRUE(ResolveInfo::NoType == sym->type());
  ASSERT_TRUE(0 == sym->desc());
  ASSERT_TRUE(0 == sym->binding());
  ASSERT_TRUE(0 == sym->other());
}

TEST_F(StaticResolverTest, SetUpBinding) {
  ResolveInfo *sym = make<ResolveInfo>("abc");
  Input *In = make<Input>("a.c", m_pDiagEngine);
  In->setResolvedPath("a.c");
  sym->setResolvedOrigin(make<InputFile>(In, m_pDiagEngine));
  sym->setIsSymbol(true);

  //  ASSERT_FALSE( sym->isSymbol() );
  ASSERT_TRUE(sym->isSymbol());
  ASSERT_TRUE(sym->isGlobal());
  ASSERT_FALSE(sym->isWeak());
  ASSERT_FALSE(sym->isLocal());
  ASSERT_FALSE(sym->isDefine());
  ASSERT_TRUE(sym->isUndef());
  ASSERT_FALSE(sym->isDyn());
  ASSERT_FALSE(sym->isCommon());
  ASSERT_TRUE(ResolveInfo::NoType == sym->type());
  ASSERT_TRUE(0 == sym->desc());
  ASSERT_TRUE(0 == sym->binding());
  ASSERT_TRUE(0 == sym->other());

  sym->setBinding(ResolveInfo::Global);
  ASSERT_TRUE(sym->isSymbol());
  ASSERT_TRUE(sym->isGlobal());
  ASSERT_FALSE(sym->isWeak());
  ASSERT_FALSE(sym->isLocal());
  ASSERT_FALSE(sym->isDefine());
  ASSERT_TRUE(sym->isUndef());
  ASSERT_FALSE(sym->isDyn());
  ASSERT_FALSE(sym->isCommon());
  ASSERT_TRUE(ResolveInfo::NoType == sym->type());
  ASSERT_TRUE(0 == sym->desc());
  ASSERT_TRUE(ResolveInfo::Global == sym->binding());
  ASSERT_TRUE(0 == sym->other());

  sym->setBinding(ResolveInfo::Weak);
  ASSERT_TRUE(sym->isSymbol());
  ASSERT_FALSE(sym->isGlobal());
  ASSERT_TRUE(sym->isWeak());
  ASSERT_FALSE(sym->isLocal());
  ASSERT_FALSE(sym->isDyn());
  ASSERT_FALSE(sym->isDefine());
  ASSERT_TRUE(sym->isUndef());
  ASSERT_FALSE(sym->isCommon());
  ASSERT_TRUE(ResolveInfo::NoType == sym->type());
  ASSERT_TRUE(0 == sym->desc());
  ASSERT_TRUE(ResolveInfo::Weak == sym->binding());
  ASSERT_TRUE(0 == sym->other());

  sym->setBinding(ResolveInfo::Local);
  ASSERT_TRUE(sym->isSymbol());
  ASSERT_FALSE(sym->isGlobal());
  ASSERT_FALSE(sym->isWeak());
  ASSERT_TRUE(sym->isLocal());
  ASSERT_FALSE(sym->isDyn());
  ASSERT_FALSE(sym->isDefine());
  ASSERT_TRUE(sym->isUndef());
  ASSERT_FALSE(sym->isCommon());
  ASSERT_TRUE(ResolveInfo::NoType == sym->type());
  ASSERT_TRUE(0 == sym->desc());
  ASSERT_TRUE(ResolveInfo::Local == sym->binding());
  ASSERT_TRUE(0 == sym->other());
}
