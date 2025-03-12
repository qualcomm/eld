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

ScriptReader::ScriptReader() : m_bTraceScanning(false), m_bTraceParsing(false) {
  std::optional<std::string> ScriptDebug =
      llvm::sys::Process::GetEnv("LDSCRIPT_DEBUG");
  if (ScriptDebug && *ScriptDebug == "true") {
    m_bTraceScanning = true;
    m_bTraceParsing = true;
  }
}

ScriptReader::~ScriptReader() {}

bool ScriptReader::readScript(LinkerConfig &pConfig, ScriptFile &pScriptFile) {
  return readLinkerScriptUsingNewParser(pConfig, pScriptFile);
}

bool ScriptReader::readLinkerScriptUsingNewParser(LinkerConfig &pConfig,
                                                  ScriptFile &pScriptFile) {
  v2::ScriptParser newScriptParser(pConfig, pScriptFile);
  newScriptParser.parse();
  return pConfig.getDiagEngine()->diagnose();
}
