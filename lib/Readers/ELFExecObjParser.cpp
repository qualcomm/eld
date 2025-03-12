//===- ELFExecObjParser.cpp------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#include "eld/Readers/ELFExecObjParser.h"
#include "eld/Core/Module.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Input/InputFile.h"
#include "eld/Object/ObjectLinker.h"
#include "eld/PluginAPI/DiagnosticEntry.h"
#include "eld/Readers/ELFReaderBase.h"
#include "eld/Support/RegisterTimer.h"
#include "eld/Support/Utils.h"
#include "eld/Target/GNULDBackend.h"
#include <memory>
using namespace eld;

ELFExecObjParser::ELFExecObjParser(Module &module) : m_Module(module) {}

eld::Expected<bool> ELFExecObjParser::parseFile(InputFile &inputFile,
                                                bool &ELFOverriddenWithBC) {
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

  bool isPostLTOPhase = m_Module.isPostLTOPhase();
  ELFOverriddenWithBC =
      !isPostLTOPhase &&
      m_Module.getLinker()->getObjLinker()->overrideELFObjectWithBitCode(
          &inputFile);

  if (ELFOverriddenWithBC)
    return true;

  inputFile.setToSkip();

  eld::Expected<void> expReadSections = readSections(*ELFReader);
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expReadSections);

  eld::Expected<bool> expReadSymbols = ELFReader->readSymbols();
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expReadSymbols)
  if (!expReadSymbols.value())
    return false;

  return true;
}

eld::Expected<bool> ELFExecObjParser::parsePatchBase(ELFFileBase &inputFile) {
  eld::Expected<std::unique_ptr<ELFReaderBase>> expReader =
      ELFReaderBase::Create(m_Module, inputFile);
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expReader);
  std::unique_ptr<ELFReaderBase> ELFReader = std::move(expReader.value());
  ELFReader->recordInputActions();
  return readRelocations(inputFile);
}

eld::Expected<void> ELFExecObjParser::readSections(ELFReaderBase &ELFReader) {
  LinkerConfig &config = m_Module.getConfig();
  InputFile *inputFile = ELFReader.getInputFile();
  GNULDBackend &backend = *m_Module.getBackend();
  eld::RegisterTimer T("Read Sections", "Link Summary",
                       config.options().printTimingStats());

  if (m_Module.getPrinter()->traceFiles())
    config.raise(diag::trace_file) << inputFile->getInput()->decoratedPath();

  ObjectFile *ObjFile = llvm::dyn_cast<ObjectFile>(inputFile);
  // handle sections
  for (Section *sect : ObjFile->getSections()) {
    if (!sect)
      continue;

    if (sect->isBitcode())
      continue;

    // Do not support dynamic executables
    if (sect->name() == ".interp") {
      return std::make_unique<plugin::DiagnosticEntry>(plugin::DiagnosticEntry(
          diag::error_unsupported_section_in_executable,
          {sect->name().str(), inputFile->getInput()->decoratedPath()}));
    }

    ELFSection *S = llvm::dyn_cast<eld::ELFSection>(sect);

    switch (S->getType()) {
    case llvm::ELF::SHT_DYNAMIC:
    case llvm::ELF::SHT_DYNSYM:
    case llvm::ELF::SHT_GROUP:
    case llvm::ELF::SHT_HASH:
      return std::make_unique<plugin::DiagnosticEntry>(plugin::DiagnosticEntry(
          diag::error_unsupported_section_in_executable,
          {S->name().str(), inputFile->getInput()->decoratedPath()}));
    default:
      break;
    }

    backend.mayWarnSection(S);

    switch (S->getKind()) {
    /** normal sections **/
    // FIXME: support Version Kind
    case LDFileFormat::Version:
    // FIXME: support GCCExceptTable Kind
    case LDFileFormat::GCCExceptTable:
    /** Fall through **/
    case LDFileFormat::Regular:
    case LDFileFormat::EhFrame:
    case LDFileFormat::Note:
    case LDFileFormat::MetaData: {
      if (S->isCompressed()) {
        if (!ELFReader.readCompressedSection(S))
          return std::make_unique<plugin::DiagnosticEntry>(
              plugin::DiagnosticEntry(diag::err_cannot_read_section,
                                      {S->name().str()}));
      }
      if (!backend.readSection(*inputFile, S))
        return std::make_unique<plugin::DiagnosticEntry>(
            plugin::DiagnosticEntry(diag::err_cannot_read_section,
                                    {S->name().str()}));

      break;
    }
    case LDFileFormat::MergeStr: {
      if (S->isCompressed()) {
        eld::Expected<bool> expReadCompressedSection =
            ELFReader.readCompressedSection(S);
        ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expReadCompressedSection);
        if (!expReadCompressedSection.value())
          return std::make_unique<plugin::DiagnosticEntry>(
              plugin::DiagnosticEntry(diag::err_cannot_read_section,
                                      {S->name().str()}));
      }
      if (config.options().stripDebug() && (S->name().find(".debug") == 0))
        S->setKind(LDFileFormat::Ignore);
      else {
        eld::Expected<bool> expReadMergeStringSect =
            ELFReader.readMergeStringSection(S);
        if (!expReadMergeStringSect.value())
          return std::make_unique<plugin::DiagnosticEntry>(
              plugin::DiagnosticEntry(diag::err_cannot_read_section,
                                      {S->name().str()}));
      }
      break;
    }

    case LDFileFormat::Debug: {
      if (config.options().stripDebug())
        S->setKind(LDFileFormat::Ignore);
      else {
        if (S->isCompressed()) {
          if (!ELFReader.readCompressedSection(S))
            return std::make_unique<plugin::DiagnosticEntry>(
                plugin::DiagnosticEntry(diag::err_cannot_read_section,
                                        {S->name().str()}));
        } else if (!backend.readSection(*inputFile, S))
          return std::make_unique<plugin::DiagnosticEntry>(
              plugin::DiagnosticEntry(diag::err_cannot_read_section,
                                      {S->name().str()}));
      }
      break;
    }

    /** target dependent sections **/
    case LDFileFormat::StackNote:
    case LDFileFormat::GNUProperty:
    case LDFileFormat::Target: {
      if (!backend.readSection(*inputFile, S)) {
        return std::make_unique<plugin::DiagnosticEntry>(
            plugin::DiagnosticEntry(diag::err_cannot_read_target_section,
                                    {S->name().str()}));
      }
      break;
    }

    // ignore sections
    case LDFileFormat::Group:
    case LDFileFormat::LinkOnce:
    case LDFileFormat::Relocation:
    case LDFileFormat::Timing:
    case LDFileFormat::Null:
    case LDFileFormat::NamePool:
    case LDFileFormat::Ignore:
      continue;

    case LDFileFormat::Discard: {
      // Discarded sections will be read, but it will be discarded in the final
      // output.
      config.raise(diag::discarding_section)
          << S->name().str() << inputFile->getInput()->decoratedPath();
      if (!backend.readSection(*inputFile, S)) {
        return std::make_unique<plugin::DiagnosticEntry>(
            plugin::DiagnosticEntry(diag::err_cannot_read_section,
                                    {S->name().str()}));
      }
      break;
    }
    // warning
    case LDFileFormat::EhFrameHdr:
    default: {
      config.raise(diag::warn_illegal_input_section)
          << S->name() << inputFile->getInput()->decoratedPath();
      break;
    }
    }
    if (config.options().isSectionTracingRequested() &&
        config.options().traceSection(S))
      config.raise(diag::read_section)
          << S->getDecoratedName(config.options())
          << inputFile->getInput()->decoratedPath()
          << utility::toHex(S->getAddrAlign()) << utility::toHex(S->size())
          << ELFSection::getELFPermissionsStr(S->getFlags());
  } // end of for all sections
  return {};
}

eld::Expected<bool> ELFExecObjParser::readRelocations(InputFile &inputFile) {
  eld::Expected<std::unique_ptr<ELFReaderBase>> expReader =
      ELFReaderBase::Create(m_Module, inputFile);
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expReader);

  std::unique_ptr<ELFReaderBase> ELFReader = std::move(expReader.value());

  ELFFileBase *EObj = llvm::cast<ELFFileBase>(ELFReader->getInputFile());
  for (ELFSection *S : EObj->getRelocationSections()) {
    if (S->isIgnore() || S->isDiscard())
      continue;
    eld::Expected<bool> expReadRelocSect = ELFReader->readRelocationSection(S);
    ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expReadRelocSect);
    if (!expReadRelocSect.value())
      return false;
  }
  return true;
}
