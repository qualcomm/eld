//===- InputFileTest.cpp---------------------------------------------------===//
//
//                     The MCLinker Project
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#include "InputFileTest.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Input/ArchiveFile.h"
#include "eld/Input/BitcodeFile.h"
#include "eld/Input/ELFDynObjectFile.h"
#include "eld/Input/ELFObjectFile.h"
#include "eld/Input/InputFile.h"
#include "eld/Input/InternalInputFile.h"
#include "eld/Input/ObjectFile.h"
#include "eld/Input/SymDefFile.h"
#include "eld/Script/ScriptReader.h"
#include "eld/Support/DynamicLibrary.h"
#include "eld/Support/FileSystem.h"
#include "eld/Support/MsgHandling.h"

using namespace eld;

//===----------------------------------------------------------------------===//
// InputFileTest
//===----------------------------------------------------------------------===//
// Constructor can do set-up work for all test here.
InputFileTest::InputFileTest() : m_pConfig(NULL) {
  m_pDiagEngine = make<DiagnosticEngine>(/*useColor=*/false);
  m_pConfig = make<LinkerConfig>(m_pDiagEngine, "hexagon-unknown-elf");
}

// Destructor can do clean-up work that doesn't throw exceptions here.
InputFileTest::~InputFileTest() {}

// SetUp() will be called immediately before each test.
void InputFileTest::SetUp() {}

// TearDown() will be called immediately after each test.
void InputFileTest::TearDown() {}

//==========================================================================//
// Testcases
//
TEST_F(InputFileTest, CastingFromInputFileToObjectFile) {
  // FIXME: ScriptReader and DynamicLibary references are not needed ideally but
  // are kept to link
  // using GNU linker and needs to be removed in future.
  ScriptReader sc;
  DynamicLibrary::getLibraryName("dummy");
  Input *In1 = make<Input>("a.o", m_pDiagEngine);
  In1->setResolvedPath("a.o");

  // ELF object File to Object File cast should not return NULL.
  InputFile *Obj1 = make<ELFObjectFile>(In1, m_pDiagEngine);
  ObjectFile *O1 = llvm::dyn_cast<ObjectFile>(Obj1);
  EXPECT_TRUE(O1 != nullptr);

  // ELF Dynamic object File to Object File cast should not return NULL.
  InputFile *Obj2 = make<ELFDynObjectFile>(In1, m_pDiagEngine);
  ObjectFile *O2 = llvm::dyn_cast<ObjectFile>(Obj2);
  EXPECT_TRUE(O2 != nullptr);

  // Internal Input File to Object File cast should not return NULL.
  InputFile *Obj3 = make<InternalInputFile>(In1, m_pDiagEngine);
  ObjectFile *O3 = llvm::dyn_cast<ObjectFile>(Obj3);
  EXPECT_TRUE(O3 != nullptr);

  // Bitcode File to Object File cast should not return NULL.
  InputFile *Obj4 = make<BitcodeFile>(In1, m_pDiagEngine);
  ObjectFile *O4 = llvm::dyn_cast<ObjectFile>(Obj4);
  EXPECT_TRUE(O4 != nullptr);

  // Archive File to Object File cast should return NULL.
  InputFile *Obj5 = make<ArchiveFile>(In1, m_pDiagEngine);
  ObjectFile *O5 = llvm::dyn_cast<ObjectFile>(Obj5);
  EXPECT_TRUE(O5 == nullptr);

  // SymDef File to Object File cast should return NULL.
  InputFile *Obj6 = make<SymDefFile>(In1, m_pDiagEngine);
  ObjectFile *O6 = llvm::dyn_cast<ObjectFile>(Obj6);
  EXPECT_TRUE(O6 == nullptr);
}
