//===- ELFRelocObjParser.cpp-----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Readers/ELFRelocObjParser.h"
#include "eld/Core/Module.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Fragment/TimingFragment.h"
#include "eld/Input/ELFObjectFile.h"
#include "eld/Object/ObjectLinker.h"
#include "eld/PluginAPI/Expected.h"
#include "eld/Readers/ELFReaderBase.h"
#include "eld/Readers/TimingSection.h"
#include "eld/Readers/TimingSlice.h"
#include "eld/Support/RegisterTimer.h"
#include "eld/Support/Utils.h"
#include "eld/Target/GNULDBackend.h"

using namespace eld;

ELFRelocObjParser::ELFRelocObjParser(Module &module) : m_Module(module) {}

eld::Expected<bool> ELFRelocObjParser::parseFile(InputFile &inputFile,
                                                 bool &ELFOverriddenWithBC) {
  eld::Expected<std::unique_ptr<ELFReaderBase>> expReader =
      ELFReaderBase::Create(m_Module, inputFile);
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expReader);

  std::unique_ptr<ELFReaderBase> ELFReader = std::move(expReader.value());

  auto expCompatibility = ELFReader->isCompatible();
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expCompatibility);
  if (!expCompatibility.value())
    return false;

  ELFReader->recordInputActions();

  bool isPostLTOPhase = m_Module.isPostLTOPhase();
  eld::Expected<bool> expReadSectHeaders = readSectionHeaders(*ELFReader);
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expReadSectHeaders);
  if (!expReadSectHeaders.value())
    return false;

  ELFOverriddenWithBC =
      !isPostLTOPhase &&
      m_Module.getLinker()->getObjLinker()->overrideELFObjectWithBitCode(
          &inputFile);

  if (ELFOverriddenWithBC)
    return true;

  eld::Expected<bool> expReadSections = readSections(*ELFReader);
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expReadSections);
  if (!expReadSections.value())
    return false;

  eld::Expected<bool> expReadSymbols = ELFReader->readSymbols();
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expReadSymbols)
  if (!expReadSymbols.value())
    return false;

  if (isPostLTOPhase)
    ELFReader->setPostLTOCommonSymbolsOrigin();

  eld::Expected<bool> expReadGroups = readGroups(*ELFReader);
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expReadGroups);
  if (!expReadGroups.value())
    return false;

  return true;
}

eld::Expected<bool>
ELFRelocObjParser::readSectionHeaders(ELFReaderBase &ELFReader) {
  return ELFReader.readSectionHeaders();
}

eld::Expected<bool> ELFRelocObjParser::readSections(ELFReaderBase &ELFReader) {
  LinkerConfig &config = m_Module.getConfig();
  eld::RegisterTimer T("Read Sections", "Link Summary",
                       config.options().printTimingStats());
  InputFile *inputFile = ELFReader.getInputFile();
  GNULDBackend &backend = *m_Module.getBackend();

  if (m_Module.getPrinter()->traceFiles())
    config.raise(diag::trace_file) << inputFile->getInput()->decoratedPath();
  ELFObjectFile *EObj = llvm::cast<ELFObjectFile>(inputFile);

  // FIXME: We create section headers (ELFSection) for each section header.
  // We do not need to create section header for group sections that are to
  // be ignored. Should we defer creating ELFSection objects for group sections
  // until we have determined which group sections are to be kept?
  for (Section *section : EObj->getSections()) {
    if (!section || section->isBitcode())
      continue;

    ELFSection *S = llvm::cast<ELFSection>(section);
    backend.mayWarnSection(S);

    switch (S->getKind()) {
    case LDFileFormat::Group: {
      eld::Expected<bool> expReadGroupSection = readGroupSection(ELFReader, S);
      ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expReadGroupSection);
    } break;
    case LDFileFormat::LinkOnce: {
      auto expReadLinkOnceSection = readLinkOnceSection(ELFReader, S);
      ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expReadLinkOnceSection);
    } break;
    case LDFileFormat::Relocation: {
      ASSERT(S->getLink() != nullptr, "");
      size_t linkIndex = S->getLink()->getIndex();
      ELFSection *linkSect = EObj->getELFSection(linkIndex);
      if (!linkSect || linkSect->isIgnore()) {
        // FIXME: Are group sections always before relocation sections in
        // section order sequence? If not, then we can not know here if a
        // section would be set to Ignore later on.

        // Relocation sections of group members should also be part of the
        // group. Thus, if the associated member sections are ignored, the
        // related relocations should be also ignored.
        S->setKind(LDFileFormat::Ignore);
      }
      break;
    }
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
        eld::Expected<bool> expReadCompressedSection =
            ELFReader.readCompressedSection(S);
        ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expReadCompressedSection);
        if (!expReadCompressedSection.value())
          return std::make_unique<plugin::DiagnosticEntry>(
              plugin::DiagnosticEntry(diag::err_cannot_read_section,
                                      {S->name().str()}));
      } else if (!backend.readSection(*inputFile, S)) {
        return std::make_unique<plugin::DiagnosticEntry>(
            plugin::DiagnosticEntry(diag::err_cannot_read_section,
                                    {S->name().str()}));
      }
    } break;
    case LDFileFormat::MergeStr: {
      eld::Expected<bool> expReadMergeStrSect =
          readMergeStrSection(ELFReader, S);
      ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expReadMergeStrSect);
    } break;
    case LDFileFormat::Debug: {
      eld::Expected<bool> expReadDebugSect = readDebugSection(ELFReader, S);
      ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expReadDebugSect);
    } break;
    case LDFileFormat::Timing: {
      eld::Expected<bool> expTimingSection = readTimingSection(ELFReader, S);
      ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expTimingSection);
    } break;
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
    // ignore
    case LDFileFormat::Null:
    case LDFileFormat::NamePool:
    case LDFileFormat::Ignore:
      continue;

    case LDFileFormat::Discard: {
      eld::Expected<bool> expReadDiscardSection =
          readDiscardSection(ELFReader, S);
      ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expReadDiscardSection);
    } break;
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
  }
  EObj->populateDebugSections();
  return true;
}

eld::Expected<bool>
ELFRelocObjParser::readLinkOnceSection(ELFReaderBase &ELFReader,
                                       ELFSection *S) {
  GNULDBackend &backend = *m_Module.getBackend();
  bool isPostLTOPhase = m_Module.isPostLTOPhase();
  InputFile *inputFile = ELFReader.getInputFile();
  LayoutPrinter *printer = m_Module.getLayoutPrinter();
  LinkerConfig &config = m_Module.getConfig();

  Module::GroupSignatureMap &signatures = m_Module.signatureMap();
  bool exist = true;

  llvm::StringRef signatureName =
      Saver.save(S->getSignatureForLinkOnceSection());
  auto R = signatures.find(signatureName);
  if (R == signatures.end()) {
    exist = false;
    ResolveInfo *info = make<ResolveInfo>(signatureName);
    signatures[signatureName] = make<ObjectReader::GroupSignatureInfo>(info, S);
    info->setResolvedOrigin(ELFReader.getInputFile());
  }

  ObjectReader::GroupSignatureInfo *signatureInfo = signatures[signatureName];

  if (isPostLTOPhase && exist) {
    ELFSection *oldSect = signatureInfo->getSection();
    oldSect->setKind(LDFileFormat::Ignore);
    signatureInfo->getInfo()->setResolvedOrigin(ELFReader.getInputFile());
    signatureInfo->setSection(S);
    if (printer)
      printer->recordSection(oldSect, inputFile);
    exist = false;
  }

  if (!exist) {
    if (S->name().starts_with(".gnu.linkonce.wi")) {
      S->setKind(LDFileFormat::Debug);
      if (config.options().stripDebug())
        S->setKind(LDFileFormat::Ignore);
      else {
        if (!backend.readSection(*inputFile, S))
          return std::make_unique<plugin::DiagnosticEntry>(
              plugin::DiagnosticEntry(diag::err_cannot_read_section,
                                      {S->name().str()}));
      }
    } else {
      S->setKind(LDFileFormat::Regular);
      if (!backend.readSection(*inputFile, S)) {
        return std::make_unique<plugin::DiagnosticEntry>(
            plugin::DiagnosticEntry(diag::err_cannot_read_section,
                                    {S->name().str()}));
      }
    }
  } else {
    if (printer)
      printer->recordSection(S, inputFile);
    S->setKind(LDFileFormat::Ignore);
  }
  return true;
}

eld::Expected<bool>
ELFRelocObjParser::readGroupSection(ELFReaderBase &ELFReader, ELFSection *S) {
  InputFile *inputFile = ELFReader.getInputFile();

  eld::Expected<uint32_t> expGroupFlag = ELFReader.getGroupFlag(S);
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expGroupFlag);
  uint32_t groupFlag = expGroupFlag.value();

  // Only GRP_COMDAT flag is supported, in addition to no flags at all.
  if (groupFlag & ~llvm::ELF::GRP_COMDAT)
    return std::make_unique<plugin::DiagnosticEntry>(
        diag::error_group_flags,
        std::vector<std::string>{S->name().str(), utility::toHex(S->getIndex()),
                                 inputFile->getInput()->decoratedPath(),
                                 utility::toHex(groupFlag)});

  eld::Expected<ResolveInfo *> expGroupSignature =
      ELFReader.computeGroupSignature(S);
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expGroupSignature);
  ResolveInfo *groupSignature = expGroupSignature.value();

  bool alreadyExist = false;
  ObjectReader::GroupSignatureInfo *signatureInfo = nullptr;
  if (groupFlag & llvm::ELF::GRP_COMDAT) {
    Module::GroupSignatureMap &signatures = m_Module.signatureMap();
    auto R = signatures.find(groupSignature->getName());
    if (R != signatures.end()) {
      alreadyExist = true;
      signatureInfo = R->second;
    } else {
      signatureInfo = make<ObjectReader::GroupSignatureInfo>(groupSignature, S);
      signatures[groupSignature->getName()] = signatureInfo;
    }
  }

  bool isPostLTOPhase = m_Module.isPostLTOPhase();
  ELFObjectFile *EObj = llvm::cast<ELFObjectFile>(inputFile);
  if (isPostLTOPhase && alreadyExist && EObj->isLTOObject()) {
    InputFile &oldInput = *(signatureInfo->getInfo()->resolvedOrigin());
    // FIXME: Shouldn't oldInput.isBitcode() be an assert instead?
    if (oldInput.isBitcode()) {
      signatureInfo->getInfo()->setResolvedOrigin(inputFile);
      signatureInfo->setSection(S);
      alreadyExist = false;
    }
  }

  bool PartialLinking = m_Module.getConfig().isLinkPartial();
  LayoutPrinter *printer = m_Module.getLayoutPrinter();

  eld::Expected<std::vector<uint32_t>> expIndices =
      ELFReader.getGroupMemberIndices(S);
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expIndices);
  for (uint32_t index : expIndices.value()) {
    ELFSection *groupMemberSect = EObj->getELFSection(index);
    if (!groupMemberSect)
      return std::make_unique<plugin::DiagnosticEntry>(plugin::DiagnosticEntry(
          diag::idx_sect_not_found,
          {std::to_string(index), EObj->getInput()->decoratedPath()}));

    // Discard member sections, if the group is preempted.
    if (alreadyExist) {
      groupMemberSect->setKind(LDFileFormat::Ignore);
      if (printer)
        printer->recordSection(groupMemberSect, EObj);
    } else {
      // Otherwise, record group members, only if linking a relocatable.
      if (PartialLinking)
        S->addSectionsToGroup(groupMemberSect);
    }
  }

  // Group sections must only be added to output image for partial links.
  if (!alreadyExist && PartialLinking) {
    GNULDBackend &backend = *m_Module.getBackend();
    if (!backend.readSection(*inputFile, S))
      return std::make_unique<plugin::DiagnosticEntry>(plugin::DiagnosticEntry(
          diag::err_cannot_read_section, {S->name().str()}));
    EObj->addSectionGroup(S);
  }

  return true;
}

eld::Expected<bool>
ELFRelocObjParser::readMergeStrSection(ELFReaderBase &ELFReader,
                                       ELFSection *S) {
  if (S->isCompressed()) {
    eld::Expected<bool> expReadCompressedSection =
        ELFReader.readCompressedSection(S);
    ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expReadCompressedSection);
    if (!expReadCompressedSection.value())
      return std::make_unique<plugin::DiagnosticEntry>(plugin::DiagnosticEntry(
          diag::err_cannot_read_section, {S->name().str()}));
  }

  LinkerConfig &config = m_Module.getConfig();
  if (config.options().stripDebug() && (S->name().find(".debug") == 0)) {
    S->setKind(LDFileFormat::Ignore);
  } else {
    eld::Expected<bool> expReadMergeStringSection =
        ELFReader.readMergeStringSection(S);
    ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expReadMergeStringSection);
    if (!expReadMergeStringSection.value()) {
      return std::make_unique<plugin::DiagnosticEntry>(plugin::DiagnosticEntry(
          diag::err_cannot_read_section, {S->name().str()}));
    }
  }
  return true;
}

eld::Expected<bool>
ELFRelocObjParser::readDebugSection(ELFReaderBase &ELFReader, ELFSection *S) {
  LinkerConfig &config = m_Module.getConfig();
  GNULDBackend &backend = *m_Module.getBackend();
  InputFile *inputFile = ELFReader.getInputFile();

  if (config.options().stripDebug()) {
    S->setKind(LDFileFormat::Ignore);
  } else {
    // FIXME: Why don't we call GNULDBackend::readSection when S is compressed?
    if (S->isCompressed()) {
      eld::Expected<bool> expReadCompressed =
          ELFReader.readCompressedSection(S);
      ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expReadCompressed);
      if (!expReadCompressed.value())
        return std::make_unique<plugin::DiagnosticEntry>(
            plugin::DiagnosticEntry(diag::err_cannot_read_section,
                                    {S->name().str()}));
    } else if (!backend.readSection(*inputFile, S)) {
      return std::make_unique<plugin::DiagnosticEntry>(plugin::DiagnosticEntry(
          diag::err_cannot_read_section, {S->name().str()}));
    }
  }
  return true;
}

eld::Expected<bool>
ELFRelocObjParser::readTimingSection(ELFReaderBase &ELFReader, ELFSection *S) {
  LinkerConfig &config = m_Module.getConfig();
  InputFile *inputFile = ELFReader.getInputFile();
  ELFObjectFile *EObj = llvm::cast<ELFObjectFile>(inputFile);

  llvm::StringRef TimingSectionData =
      inputFile->getSlice(S->offset(), S->size());
  EObj->setTimingSection(make<TimingSection>(config.getDiagEngine(),
                                             TimingSectionData, S->size(),
                                             S->getFlags(), inputFile));
  TimingSection *TimingSection = EObj->getTimingSection();
  if (config.getPrinter()->isVerbose()) {
    for (const Fragment *TF : TimingSection->getFragmentList()) {
      const TimingSlice *t =
          llvm::dyn_cast<TimingFragment>(TF)->getTimingSlice();
      config.raise(diag::reading_timing_data)
          << t->getModuleName() << std::to_string(t->getBeginningOfTime())
          << std::to_string(t->getDuration())
          << inputFile->getInput()->decoratedPath();
    }
  }
  GNULDBackend &backend = *m_Module.getBackend();
  if (!backend.readSection(*inputFile, S)) {
    return std::make_unique<plugin::DiagnosticEntry>(plugin::DiagnosticEntry(
        diag::err_cannot_read_target_section, {S->name().str()}));
  }
  return true;
}

eld::Expected<bool>
ELFRelocObjParser::readDiscardSection(ELFReaderBase &ELFReader, ELFSection *S) {
  LinkerConfig &config = m_Module.getConfig();
  GNULDBackend &backend = *m_Module.getBackend();
  InputFile *inputFile = ELFReader.getInputFile();

  // Discarded sections will be read, but it will be discarded in the final
  // output.
  config.raise(diag::discarding_section)
      << S->name() << inputFile->getInput()->decoratedPath();
  if (!backend.readSection(*inputFile, S)) {
    return std::make_unique<plugin::DiagnosticEntry>(plugin::DiagnosticEntry(
        diag::err_cannot_read_section, {S->name().str()}));
  }
  if (S->name() == ".note.qc.reloc.section.map")
    config.raise(diag::discard_reloc_section_map)
        << inputFile->getInput()->decoratedPath();
  return true;
}

eld::Expected<bool> ELFRelocObjParser::readGroups(ELFReaderBase &ELFReader) {
  LinkerConfig &config = m_Module.getConfig();
  InputFile *inputFile = ELFReader.getInputFile();

  if (!config.isLinkPartial())
    return true;
  ELFObjectFile *EObj = llvm::cast<ELFObjectFile>(inputFile);
  for (Section *S : EObj->getELFSectionGroupSections()) {
    eld::Expected<bool> expReadOneGroup =
        ELFReader.readOneGroup(llvm::cast<ELFSection>(S));
    ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expReadOneGroup);

    // FIXME: Return an error instead!
    if (!expReadOneGroup.value())
      return false;
  }
  return true;
}

eld::Expected<bool> ELFRelocObjParser::readRelocations(InputFile &inputFile) {
  eld::Expected<std::unique_ptr<ELFReaderBase>> expReader =
      ELFReaderBase::Create(m_Module, inputFile);
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expReader);

  std::unique_ptr<ELFReaderBase> ELFReader = std::move(expReader.value());

  ELFObjectFile *EObj = llvm::cast<ELFObjectFile>(ELFReader->getInputFile());
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
