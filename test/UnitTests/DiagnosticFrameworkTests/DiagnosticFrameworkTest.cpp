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
#include "eld/Diagnostics/MsgHandler.h"
#include "eld/PluginAPI/DiagnosticEntry.h"
#include "gtest/gtest.h"
#include <memory>
#include <utility>

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

TEST_F(DiagnosticFrameworkTest, DisabledTraceEmitsNothing) {
  testing::internal::CaptureStderr();
  m_DiagEngine->trace(Diag::trace_file) << "input.o";
  EXPECT_TRUE(testing::internal::GetCapturedStderr().empty());
}

TEST_F(DiagnosticFrameworkTest, EnabledTraceEmits) {
  m_DiagEngine->enableTrace(TraceFiles);
  testing::internal::CaptureStderr();
  m_DiagEngine->trace(Diag::trace_file) << "input.o";
  EXPECT_EQ(testing::internal::GetCapturedStderr(), "Trace: input.o\n");
}

TEST_F(DiagnosticFrameworkTest, KeyedTraceFiltersAccumulate) {
  m_DiagEngine->addTraceFilter(TraceSymbol, "exact");
  m_DiagEngine->addTraceFilter(TraceSymbol, "regex.*");
  testing::internal::CaptureStderr();
  m_DiagEngine->trace(Diag::apply_relocation, "exact") << "exact" << "10";
  m_DiagEngine->trace(Diag::apply_relocation, "regex-key")
      << "regex-key" << "20";
  m_DiagEngine->trace(Diag::apply_relocation, "other") << "other" << "30";
  m_DiagEngine->trace(Diag::apply_relocation) << "keyless" << "40";
  EXPECT_EQ(testing::internal::GetCapturedStderr(),
            "Trace: Symbol exact, application site: 0x10\n"
            "Trace: Symbol regex-key, application site: 0x20\n");
}

TEST_F(DiagnosticFrameworkTest, SectionTraceMatchesKey) {
  m_DiagEngine->addTraceFilter(TraceSection, ".text");
  testing::internal::CaptureStderr();
  m_DiagEngine->trace(Diag::retaining_section, ".text") << ".text" << "input.o";
  m_DiagEngine->trace(Diag::retaining_section, ".data") << ".data" << "input.o";
  EXPECT_EQ(testing::internal::GetCapturedStderr(),
            "Trace: Retaining section .text from file input.o\n");
}

TEST_F(DiagnosticFrameworkTest, TraceMaskUsesAnyCategory) {
  m_DiagEngine->enableTrace(TraceGCLive);
  EXPECT_TRUE(m_DiagEngine->shouldTrace(TraceGC | TraceGCLive));
  EXPECT_FALSE(m_DiagEngine->shouldTrace(TraceGC | TraceSection));
}

TEST_F(DiagnosticFrameworkTest, HasTraceChecksConfiguredCategories) {
  EXPECT_FALSE(m_DiagEngine->hasTrace(TraceSymbol));
  EXPECT_FALSE(m_DiagEngine->shouldTrace(TraceSymbol));

  m_DiagEngine->addTraceFilter(TraceSymbol, "foo");
  EXPECT_TRUE(m_DiagEngine->hasTrace(TraceSymbol));
  EXPECT_FALSE(m_DiagEngine->shouldTrace(TraceSymbol));
  EXPECT_TRUE(m_DiagEngine->shouldTrace(TraceSymbol, "foo"));
  EXPECT_FALSE(m_DiagEngine->shouldTrace(TraceSymbol, "bar"));
}

TEST_F(DiagnosticFrameworkTest, RaiseTraceRemainsUnconditional) {
  testing::internal::CaptureStderr();
  m_DiagEngine->raise(Diag::trace_file) << "input.o";
  EXPECT_EQ(testing::internal::GetCapturedStderr(), "Trace: input.o\n");
}

TEST_F(DiagnosticFrameworkTest, ExplicitEmitIsIdempotent) {
  m_DiagEngine->enableTrace(TraceFiles);
  testing::internal::CaptureStderr();
  {
    MsgHandler Handler = m_DiagEngine->trace(Diag::trace_file);
    Handler << "input.o";
    EXPECT_TRUE(Handler.emit());
  }
  EXPECT_EQ(testing::internal::GetCapturedStderr(), "Trace: input.o\n");
}

TEST_F(DiagnosticFrameworkTest, UnknownTraceOptionWithoutSeparator) {
  EXPECT_FALSE((bool)m_Config->options().setTrace("reloc"));
  EXPECT_FALSE((bool)m_Config->options().setTrace("symbol"));
  EXPECT_FALSE((bool)m_Config->options().setTrace("section"));
  EXPECT_FALSE((bool)m_Config->options().setTrace("symbolic"));
  EXPECT_FALSE((bool)m_Config->options().setTrace("relocations"));
  EXPECT_TRUE((bool)m_Config->options().setTrace("symbol=foo"));
  EXPECT_TRUE((bool)m_Config->options().setTrace("reloc=bar"));
  EXPECT_TRUE((bool)m_Config->options().setTrace("section=.text"));
  EXPECT_TRUE((bool)m_Config->options().setTrace("plugin"));
  EXPECT_TRUE((bool)m_Config->options().setTrace("plugin=myplugin"));
}
