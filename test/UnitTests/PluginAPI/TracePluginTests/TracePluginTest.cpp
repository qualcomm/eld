//===- TracePluginTest.cpp -------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
#include "eld/Config/GeneralOptions.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Core/LinkerScript.h"
#include "eld/Core/Module.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/PluginAPI/LinkerWrapper.h"
#include "eld/Script/Plugin.h"
#include "gtest/gtest.h"

using namespace eld;

namespace {

class TracePluginTest : public ::testing::Test {
protected:
  void SetUp() override {
    DiagEngine = make<DiagnosticEngine>(/*useColor=*/false);
    Config = make<LinkerConfig>(DiagEngine);
    Script = make<LinkerScript>(DiagEngine);
    Mod = make<Module>(*Script, *Config, /*layoutInfo=*/nullptr);
  }

  // Constructs a Plugin named `PluginName` along with a standalone
  // LinkerWrapper wrapping it. We construct the LinkerWrapper directly
  // (rather than via Plugin::getLinkerWrapper(), which requires a real
  // dynamically-loaded plugin library to have registered itself first)
  // since LinkerWrapper::isTraced() only needs the Plugin* it was built
  // with.
  eld::Plugin *makePlugin(const std::string &PluginName,
                          plugin::LinkerWrapper **LWOut = nullptr) {
    auto *P = make<eld::Plugin>(plugin::PluginBase::Type::OutputSectionIterator,
                                /*Name=*/PluginName, /*R=*/PluginName,
                                /*O=*/"", /*Stats=*/false,
                                /*DefaultPlugin=*/false, *Mod);
    auto *LW = make<plugin::LinkerWrapper>(P, *Mod);
    if (LWOut)
      *LWOut = LW;
    return P;
  }

  DiagnosticEngine *DiagEngine = nullptr;
  LinkerConfig *Config = nullptr;
  LinkerScript *Script = nullptr;
  Module *Mod = nullptr;
};

TEST_F(TracePluginTest, NotTracedByDefault) {
  plugin::LinkerWrapper *LW = nullptr;
  eld::Plugin *P = makePlugin("MyPlugin", &LW);
  EXPECT_FALSE(P->isTraced());
  EXPECT_FALSE(LW->isTraced());
}

TEST_F(TracePluginTest, UnscopedTraceTracesEveryPlugin) {
  ASSERT_TRUE((bool)Config->options().setTrace("plugin"));

  plugin::LinkerWrapper *LW1 = nullptr, *LW2 = nullptr;
  eld::Plugin *P1 = makePlugin("PluginOne", &LW1);
  eld::Plugin *P2 = makePlugin("PluginTwo", &LW2);
  EXPECT_TRUE(P1->isTraced());
  EXPECT_TRUE(P2->isTraced());
  EXPECT_TRUE(LW1->isTraced());
  EXPECT_TRUE(LW2->isTraced());
}

TEST_F(TracePluginTest, ScopedTraceOnlyMatchesNamedPlugin) {
  ASSERT_TRUE((bool)Config->options().setTrace("plugin=MyPlugin"));

  plugin::LinkerWrapper *MatchingLW = nullptr, *NonMatchingLW = nullptr;
  eld::Plugin *Matching = makePlugin("MyPlugin", &MatchingLW);
  eld::Plugin *NonMatching = makePlugin("OtherPlugin", &NonMatchingLW);
  EXPECT_TRUE(Matching->isTraced());
  EXPECT_TRUE(MatchingLW->isTraced());
  EXPECT_FALSE(NonMatching->isTraced());
  EXPECT_FALSE(NonMatchingLW->isTraced());
}

TEST_F(TracePluginTest, ScopedTraceNameIsARegex) {
  ASSERT_TRUE((bool)Config->options().setTrace("plugin=My.*"));

  eld::Plugin *Matching = makePlugin("MyPlugin");
  eld::Plugin *NonMatching = makePlugin("OtherPlugin");
  EXPECT_TRUE(Matching->isTraced());
  EXPECT_FALSE(NonMatching->isTraced());
}

TEST_F(TracePluginTest, MultipleScopedTracesAreCumulative) {
  ASSERT_TRUE((bool)Config->options().setTrace("plugin=PluginOne"));
  ASSERT_TRUE((bool)Config->options().setTrace("plugin=PluginTwo"));

  eld::Plugin *P1 = makePlugin("PluginOne");
  eld::Plugin *P2 = makePlugin("PluginTwo");
  eld::Plugin *P3 = makePlugin("PluginThree");
  EXPECT_TRUE(P1->isTraced());
  EXPECT_TRUE(P2->isTraced());
  EXPECT_FALSE(P3->isTraced());
}

} // namespace
