//===- DWARFInfoTest.cpp --------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Config/LinkerConfig.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Diagnostics/DiagnosticInfos.h"
#include "eld/Input/Input.h"
#include "eld/Input/InputFile.h"
#include "eld/PluginAPI/DWARF.h"
#include "eld/PluginAPI/PluginADT.h"
#include <gtest/gtest.h>
#include <memory>
#include <string>

namespace {

TEST(DWARFInfoTest, InvalidObjectInputReportsWarningAndRemainsUsable) {
  eld::DiagnosticEngine DiagEngine(/*useColor=*/false);
  eld::LinkerConfig Config(&DiagEngine);
  DiagEngine.setInfoMap(std::make_unique<eld::DiagnosticInfos>(Config));
  eld::Input Input("invalid-dwarf-input.o", &DiagEngine);
  Input.setResolvedPath("invalid-dwarf-input.o");

  std::string Contents = "not an object file";
  eld::InputFile InputFile(&Input, &DiagEngine);
  InputFile.setContents(Contents);

  eld::plugin::DWARFInfo DWARFInfo{eld::plugin::InputFile(&InputFile)};
  EXPECT_FALSE(DWARFInfo.hasDWARFContext());
  EXPECT_EQ(1U, DiagEngine.getPrinter()->getNumWarnings());
  EXPECT_TRUE(DiagEngine.diagnose());
}

} // namespace
