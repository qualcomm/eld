//===- ELFExecObjParser.cpp------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#include "eld/Readers/ELFExecObjParser.h"
#include "eld/Core/Module.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Input/InputFile.h"
#include "eld/PluginAPI/DiagnosticEntry.h"
#include "eld/Readers/ELFReaderBase.h"
#include <memory>
using namespace eld;

ELFExecObjParser::ELFExecObjParser(Module &module) : m_Module(module) {}

eld::Expected<uint16_t> ELFExecObjParser::getMachine(InputFile &inputFile) {
  eld::Expected<std::unique_ptr<ELFReaderBase>> expReader =
      ELFReaderBase::Create(m_Module, inputFile);
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expReader);
  std::unique_ptr<ELFReaderBase> ELFReader = std::move(expReader.value());
  return ELFReader->getMachine();
}

/// Executable inputs are only usable via --just-symbols, which only needs
/// each symbol's name, type, and value.
eld::Expected<bool> ELFExecObjParser::parseFile(InputFile &inputFile) {
  eld::Expected<std::unique_ptr<ELFReaderBase>> expReader =
      ELFReaderBase::Create(m_Module, inputFile);
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expReader);
  std::unique_ptr<ELFReaderBase> ELFReader = std::move(expReader.value());

  eld::Expected<bool> expCompatibility = ELFReader->isCompatible();
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expCompatibility);
  if (!expCompatibility.value())
    return false;

  ELFReader->recordInputActions();

  eld::Expected<bool> expReadSectHeaders = ELFReader->readSectionHeaders();
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expReadSectHeaders);
  if (!expReadSectHeaders.value())
    return false;

  inputFile.setToSkip();

  eld::Expected<bool> expReadSymbols = ELFReader->readSymbols();
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expReadSymbols)
  if (!expReadSymbols.value())
    return false;

  return true;
}
