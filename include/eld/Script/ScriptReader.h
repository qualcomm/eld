//===- ScriptReader.h------------------------------------------------------===//
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
#ifndef ELD_SCRIPT_SCRIPTREADER_H
#define ELD_SCRIPT_SCRIPTREADER_H

#include "eld/Config/LinkerConfig.h"
#include <string>

namespace eld {

class Module;
class ScriptFile;
class Input;
class LinkerConfig;
class LinkerScript;
class GNULDBackend;

class ScriptReader {
public:
  ScriptReader();

  ~ScriptReader();

  void setTraceParsing() { MBTraceParsing = true; }

  void setTraceScanning() { MBTraceScanning = true; }

  /// readScript
  bool readScript(LinkerConfig &PConfig, ScriptFile &PScriptFile);

private:
  bool readLinkerScriptUsingNewParser(LinkerConfig &PConfig,
                                      ScriptFile &PScriptFile);

private:
  bool MBTraceScanning;
  bool MBTraceParsing;
};

} // namespace eld

#endif
