//===- DiagnosticFrameworkTest.cpp-----------------------------------------===//
//
//                     The MCLinker Project
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "DiagnosticFrameworkTest.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Diagnostics/DiagnosticInfos.h"
#include "eld/Diagnostics/DiagnosticPrinter.h"
#include "eld/PluginAPI/DiagnosticEntry.h"
#include "llvm/Support/raw_ostream.h"
#include "gtest/gtest.h"
#include <memory>

using namespace eld;

DiagnosticFrameworkTest::DiagnosticFrameworkTest() {
  m_DiagEngine = make<DiagnosticEngine>(/*useColor=*/false);
  m_Config = make<LinkerConfig>(m_DiagEngine);
  std::unique_ptr<DiagnosticInfos> diagInfo =
      std::make_unique<DiagnosticInfos>(*m_Config);
  m_DiagEngine->setInfoMap(std::move(diagInfo));
}

DiagnosticFrameworkTest::~DiagnosticFrameworkTest() {}

void DiagnosticFrameworkTest::SetUp() {}

void DiagnosticFrameworkTest::TearDown() {}

TEST_F(DiagnosticFrameworkTest, DiagnosticEntrySeverityTest) {
  eld::plugin::DiagnosticEntry errorNoInputsDE{Diag::err_no_inputs};
  EXPECT_TRUE(errorNoInputsDE.isError());
  EXPECT_FALSE(errorNoInputsDE.isFatal());

  eld::plugin::DiagnosticEntry fatalNoInputsDE{
      Diag::err_no_inputs, {}, eld::plugin::DiagnosticEntry::Severity::Fatal};
  EXPECT_TRUE(fatalNoInputsDE.isFatal());

  eld::plugin::DiagnosticEntry verboseNoInputsDE{
      Diag::err_no_inputs, {}, eld::plugin::DiagnosticEntry::Severity::Verbose};
  EXPECT_TRUE(verboseNoInputsDE.isVerbose());

  eld::plugin::DiagnosticEntry warnIncompatibleOption{
      Diag::warn_incompatible_option};
  EXPECT_TRUE(warnIncompatibleOption.isWarning());

  eld::plugin::DiagnosticEntry noteIncompatibleOption{
      Diag::warn_incompatible_option,
      {},
      eld::plugin::DiagnosticEntry::Severity::Note};
  EXPECT_TRUE(noteIncompatibleOption.isNote());

  eld::plugin::DiagnosticEntry noteELDFlags{Diag::note_eld_flags};
  EXPECT_TRUE(noteELDFlags.isNote());
  EXPECT_FALSE(noteELDFlags.isError());

  eld::plugin::DiagnosticEntry errorEmptyData{eld::plugin::Diagnostic::error_empty_data()};
  EXPECT_TRUE(errorEmptyData.isError());
  EXPECT_FALSE(errorEmptyData.isWarning());
}

TEST_F(DiagnosticFrameworkTest, InvalidDiagnosticsTestRaise) {
  testing::internal::CaptureStderr();
  m_DiagEngine->raise(Diag::err_cannot_find_scriptfile);
  std::string output = testing::internal::GetCapturedStderr();
  EXPECT_EQ(output, "Fatal: Missing argument 0 when reporting diagnostic "
                    "'cannot open %0 file %1'\n");
  EXPECT_TRUE(m_DiagEngine->diagnose() == false);
}

TEST_F(DiagnosticFrameworkTest, InvalidDiagnosticsTestRaiseDiagEntry) {
  testing::internal::CaptureStderr();
  std::unique_ptr<eld::plugin::DiagnosticEntry> de(
      new eld::plugin::DiagnosticEntry(Diag::err_cannot_find_scriptfile, {}));
  m_DiagEngine->raiseDiagEntry(std::move(de));
  std::string output = testing::internal::GetCapturedStderr();
  EXPECT_EQ(output, "Fatal: Missing argument 0 when reporting diagnostic "
                    "'cannot open %0 file %1'\n");
  EXPECT_TRUE(m_DiagEngine->diagnose() == false);
}

TEST_F(DiagnosticFrameworkTest, DiagnosticLocationPrintsSourcePrefix) {
  testing::internal::CaptureStderr();
  m_DiagEngine->raise(Diag::err_no_inputs)
      .setLocation(DiagnosticLocation{"test.c", 7, 0});
  std::string output = testing::internal::GetCapturedStderr();
  EXPECT_EQ(output, "Error: test.c:7: no inputs\n");
}

TEST_F(DiagnosticFrameworkTest, DiagnosticLocationPrintsColumn) {
  testing::internal::CaptureStderr();
  m_DiagEngine->raise(Diag::err_no_inputs)
      .setLocation(DiagnosticLocation{"test.c", 7, 3});
  std::string output = testing::internal::GetCapturedStderr();
  EXPECT_EQ(output, "Error: test.c:7:3: no inputs\n");
}

TEST_F(DiagnosticFrameworkTest, InvalidDiagnosticLocationPrintsNoPrefix) {
  const DiagnosticLocation InvalidLocations[] = {{"test.c", 0, 0}, {"", 7, 0}};
  for (const DiagnosticLocation &Location : InvalidLocations) {
    testing::internal::CaptureStderr();
    m_DiagEngine->getPrinter()->printDiagnostic(
        llvm::raw_ostream::WHITE, "Error", "no inputs", "", &Location);
    std::string output = testing::internal::GetCapturedStderr();
    EXPECT_EQ(output, "Error: no inputs\n");
  }
}

TEST_F(DiagnosticFrameworkTest, DiagnosticLocationIsClearedBetweenMessages) {
  testing::internal::CaptureStderr();
  m_DiagEngine->raise(Diag::err_no_inputs)
      .setLocation(DiagnosticLocation{"first.c", 11, 0});
  m_DiagEngine->raise(Diag::err_no_inputs);
  std::string output = testing::internal::GetCapturedStderr();
  EXPECT_EQ(output, "Error: first.c:11: no inputs\nError: no inputs\n");
}

TEST_F(DiagnosticFrameworkTest, DiagnosticLocationValidity) {
  const DiagnosticLocation Valid{"test.c", 7, 0};
  const DiagnosticLocation MissingFile{"", 7, 0};
  const DiagnosticLocation MissingLine{"test.c", 0, 0};

  EXPECT_TRUE(Valid.isValid());
  EXPECT_FALSE(MissingFile.isValid());
  EXPECT_FALSE(MissingLine.isValid());
}
