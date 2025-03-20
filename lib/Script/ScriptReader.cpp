//===- ScriptReader.cpp----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
//
//                     The MCLinker Project
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "eld/Script/ScriptReader.h"
#include "eld/Core/Module.h"
#include "eld/Input/InputBuilder.h"
#include "eld/Input/LinkerScriptFile.h"
#include "eld/Script/ExcludeFiles.h"
#include "eld/Script/ScriptFile.h"
#include "eld/ScriptParser/ScriptParser.h"
#include "eld/Support/MemoryArea.h"
#include "eld/Support/Path.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Process.h"
#include <istream>
#include <sstream>

using namespace eld;

ScriptReader::ScriptReader() : MBTraceScanning(false), MBTraceParsing(false) {
  std::optional<std::string> ScriptDebug =
      llvm::sys::Process::GetEnv("LDSCRIPT_DEBUG");
  if (ScriptDebug && *ScriptDebug == "true") {
    MBTraceScanning = true;
    MBTraceParsing = true;
  }
}

ScriptReader::~ScriptReader() {}

bool ScriptReader::readScript(LinkerConfig &PConfig, ScriptFile &PScriptFile) {
  return readLinkerScriptUsingNewParser(PConfig, PScriptFile);
}

bool ScriptReader::readLinkerScriptUsingNewParser(LinkerConfig &PConfig,
                                                  ScriptFile &PScriptFile) {
  v2::ScriptParser NewScriptParser(PConfig, PScriptFile);
  NewScriptParser.parse();
  return PConfig.getDiagEngine()->diagnose();
}
