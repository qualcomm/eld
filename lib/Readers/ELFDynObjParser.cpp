//===- ELFDynObjParser.cpp-------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Readers/ELFDynObjParser.h"
#include "eld/Core/Module.h"
#include "eld/PluginAPI/DiagnosticEntry.h"
#include "eld/Readers/ELFReader.h"
#include "eld/Readers/ELFReaderBase.h"

using namespace eld;

ELFDynObjParser::ELFDynObjParser(Module &module) : m_Module(module) {}

ELFDynObjParser::~ELFDynObjParser() {}

eld::Expected<uint16_t> ELFDynObjParser::getMachine(InputFile &inputFile) {
  eld::Expected<std::unique_ptr<ELFReaderBase>> expReader =
      ELFReaderBase::Create(m_Module, inputFile);
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expReader);
  std::unique_ptr<ELFReaderBase> ELFReader = std::move(expReader.value());
  return ELFReader->getMachine();
}

eld::Expected<bool> ELFDynObjParser::parseFile(InputFile &inputFile) {
  eld::Expected<std::unique_ptr<ELFReaderBase>> expReader =
      ELFReaderBase::Create(m_Module, inputFile);
  if (!expReader)
    return std::move(expReader.error());

  std::unique_ptr<ELFReaderBase> ELFReader = std::move(expReader.value());

  eld::Expected<bool> expCheckFlags = ELFReader->checkFlags();

  // FIXME: Linker should give some error if checkFlags return false.
  if (expCheckFlags.has_value() && expCheckFlags.value()) {
    LayoutInfo *layoutInfo = m_Module.getLayoutInfo();
    if (layoutInfo) {
      std::string flagStr = ELFReader->getFlagString();
      if (!flagStr.empty()) {
        std::string flag = "[" + std::string(flagStr) + "]";
        layoutInfo->recordInputActions(LayoutInfo::Load, inputFile.getInput(),
                                    flag);
      }
    }
  }

  eld::Expected<bool> expReadHeaders = readSectionHeaders(*ELFReader);
  if (!expReadHeaders.has_value() || !expReadHeaders.value())
    return expReadHeaders;

#ifdef ELD_ENABLE_SYMBOL_VERSIONING
  eld::Expected<void> expReadSections = ELFReader->readSections();
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expReadSections);
#endif

  eld::Expected<bool> expReadSymbols = readSymbols(*ELFReader);
  if (!expReadSymbols || !expReadSymbols.value())
    return expReadSymbols;
  return true;
}

eld::Expected<bool>
ELFDynObjParser::readSectionHeaders(ELFReaderBase &ELFReader) {
  const bool isPostLTOPhase = m_Module.isPostLTOPhase();

  if (isPostLTOPhase)
    return true;

  eld::Expected<bool> expReadSectHdrs = ELFReader.readSectionHeaders();
  return expReadSectHdrs;
}

eld::Expected<bool> ELFDynObjParser::readSymbols(ELFReaderBase &ELFReader) {
  LinkerConfig &config = m_Module.getConfig();
  InputFile *inputFile = ELFReader.getInputFile();
  if (m_Module.getPrinter()->traceFiles())
    config.raise(Diag::trace_file) << inputFile->getInput()->decoratedPath();

  eld::Expected<bool> expCreateSyms = ELFReader.readSymbols();
  if (!expCreateSyms.has_value() || !expCreateSyms.value())
    return expCreateSyms;
  ELFReader.setSymbolsAliasInfo();
  return true;
}
