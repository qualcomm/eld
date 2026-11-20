//===- LibReader.cpp-------------------------------------------------------===//
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

#include "eld/Object/LibReader.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Input/ArchiveFile.h"
#include "eld/Input/Input.h"
#include "eld/Input/InputFile.h"
#include "eld/Input/InputTree.h"
#include "eld/Input/LinkerScriptFile.h"
#include "eld/Object/ObjectLinker.h"
#include "eld/Readers/ArchiveParser.h"
#include "eld/Support/MemoryArea.h"
#include "eld/Support/OutputTarWriter.h"
#include "eld/Support/RegisterTimer.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Twine.h"
#include "llvm/BinaryFormat/Magic.h"
#include "llvm/Object/Archive.h"
#include "llvm/Object/ArchiveWriter.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/MemoryBuffer.h"

using namespace eld;

LibReader::LibReader(Module &M, ObjectLinker *ObjLinker)
    : MModule(M), MObjLinker(ObjLinker) {}

struct LibReader::ArchiveMemberList {
  std::vector<llvm::NewArchiveMember> Members;
  bool RequiresRegularArchive = false;
};

static bool appendMember(InputFile *MemberFile, llvm::MemoryBufferRef Buffer,
                         llvm::StringRef Name,
                         std::vector<llvm::NewArchiveMember> &Members,
                         bool &RequiresRegularArchive,
                         std::unique_ptr<llvm::object::Archive> &ParsedArchive,
                         LinkerConfig &Config) {
  assert(MemberFile && "expected a classified --start-lib member");

  auto AppendObject = [&](llvm::MemoryBufferRef Object,
                          llvm::StringRef ObjectName) -> bool {
    llvm::NewArchiveMember Member;

    // NewArchiveMember(MemoryBufferRef) only creates a non-owning wrapper
    // around the original bytes, and MemberName is a non-owning StringRef.
    Member.Buf =
        llvm::MemoryBuffer::getMemBufferCopy(Object.getBuffer(), ObjectName);
    Member.MemberName = Member.Buf->getBufferIdentifier();

    Members.push_back(std::move(Member));
    return true;
  };

  // The top-level input has already been classified into an InputFile.
  if (!llvm::isa<ArchiveFile>(MemberFile)) {
    if (MemberFile->getKind() != InputFile::ELFObjFileKind &&
        !MemberFile->isBitcode()) {
      Config.raise(Diag::error_start_lib_member_type) << Name;
      return false;
    }

    return AppendObject(Buffer, Name);
  }

  auto ArchiveOrErr = llvm::object::Archive::create(Buffer);
  if (!ArchiveOrErr) {
    Config.raise(Diag::error_start_lib_input)
        << Name << llvm::toString(ArchiveOrErr.takeError());
    return false;
  }

  ParsedArchive = std::move(*ArchiveOrErr);
  RequiresRegularArchive = true;

  llvm::Error IterationError = llvm::Error::success();

  for (const auto &Child : ParsedArchive->children(IterationError,
                                                   /*SkipInternal=*/true)) {
    auto ChildName = Child.getName();
    if (!ChildName) {
      Config.raise(Diag::error_start_lib_input)
          << Name
          << llvm::toString(llvm::joinErrors(ChildName.takeError(),
                                             std::move(IterationError)));
      return false;
    }

    const std::string Identity = Name.str() + "(" + ChildName->str() + ")";

    auto ChildBuffer = Child.getMemoryBufferRef();
    if (!ChildBuffer) {
      Config.raise(Diag::error_start_lib_input)
          << Identity
          << llvm::toString(llvm::joinErrors(ChildBuffer.takeError(),
                                             std::move(IterationError)));
      return false;
    }

    // Do not recurse into nested archives. Only relocatable objects and
    // bitcode members are added to the synthetic --start-lib archive.
    const auto ChildMagic = llvm::identify_magic(ChildBuffer->getBuffer());

    if (ChildMagic != llvm::file_magic::elf_relocatable &&
        ChildMagic != llvm::file_magic::bitcode) {
      Config.raise(Diag::error_start_lib_member_type) << Identity;
      return false;
    }

    if (!AppendObject(*ChildBuffer, Identity)) {
      if (IterationError) {
        Config.raise(Diag::error_start_lib_input)
            << Name << llvm::toString(std::move(IterationError));
      }
      return false;
    }
  }

  if (IterationError) {
    Config.raise(Diag::error_start_lib_input)
        << Name << llvm::toString(std::move(IterationError));
    return false;
  }

  return true;
}

LibReader::~LibReader() {}

static std::string getStartLibArchiveName(bool IsThin, uint64_t Id) {
  llvm::StringRef Prefix = IsThin ? "<start-lib-thin:" : "<start-lib:";
  return (llvm::Twine(Prefix) + llvm::Twine(Id) + ">").str();
}

bool LibReader::addArchiveMember(Input *MemberInput, bool IsThin,
                                 LinkerConfig &Config,
                                 ArchiveMemberList &Archive) {
  assert(MemberInput && "expected a valid archive member input");
  InputFile *MemberFile = MemberInput->getInputFile();

  const std::string Name =
      IsThin ? MemberInput->getResolvedPath().getFullPath()
             : MemberInput->getResolvedPath().filename().native();

  // Expand transactionally. Do not modify Archive until every operation
  // required for this input has succeeded.
  std::vector<llvm::NewArchiveMember> Members;
  bool RequiresRegularArchive = false;
  std::unique_ptr<llvm::object::Archive> ParsedArchive;

  if (!appendMember(MemberFile, MemberInput->getMemoryBufferRef(), Name,
                    Members, RequiresRegularArchive, ParsedArchive, Config)) {
    MModule.setFailure(true);
    return false;
  }

  if (MModule.getOutputTarWriter()) {
    if (ParsedArchive && ParsedArchive->isThin()) {
      auto *OriginalArchiveFile = llvm::dyn_cast<ArchiveFile>(MemberFile);

      if (!OriginalArchiveFile) {
        Config.raise(Diag::error_start_lib_input)
            << MemberInput->getResolvedPath().getFullPath()
            << "input was identified as an archive, but its ELD "
               "InputFile is not an ArchiveFile";
        MModule.setFailure(true);
        return false;
      }

      ArchiveParser Parser(MModule);
      eld::Expected<bool> Prepared =
          Parser.prepareMembers(*ParsedArchive, OriginalArchiveFile);

      if (!Prepared) {
        Config.raiseDiagEntry(std::move(Prepared.error()));
        return false;
      }

      if (!Prepared.value()) {
        MModule.setFailure(true);
        return false;
      }
    }

    MappingFile::Kind Kind;

    if (llvm::isa<ArchiveFile>(MemberFile))
      Kind = MappingFile::Kind::Archive;
    else if (MemberFile->isBitcode())
      Kind = MappingFile::Kind::Bitcode;
    else
      Kind = MappingFile::Kind::ObjectFile;

    MemberFile->setMappedPath(MemberInput->getName());
    MemberFile->setMappingFileKind(Kind);

    // For a prepared thin ArchiveFile, this automatically calls
    // OutputTarWriter::addThinArchiveMembers().
    MModule.getOutputTarWriter()->addInputFile(
        MemberFile, /*isLTOObject=*/false);
  }

  Archive.RequiresRegularArchive |= RequiresRegularArchive;

  for (auto &Member : Members)
    Archive.Members.push_back(std::move(Member));

  return true;
}

bool LibReader::readLib(InputBuilder::InputIteratorT &CurNode,
                        InputBuilder &Builder, LinkerConfig &Config,
                        bool IsPostLtoPhase) {
  LayoutInfo *layoutInfo = MModule.getLayoutInfo();
  Attribute LibAttr;
  bool IsThin = false;
  uint64_t LibId = 0;
  bool ReuseArchiveOnly = false;
  MemoryArea *CachedArchiveArea = nullptr;

  if (layoutInfo)
    layoutInfo->recordInputActions(LayoutInfo::StartLib, nullptr);

  ArchiveMemberList Archive;

  // Build archive members from the in-between inputs.
  while ((*CurNode)->kind() != Node::LibEnd) {
    if (const auto *Start = llvm::dyn_cast<LibStart>(*CurNode)) {
      LibAttr = Start->getAttribute();
      IsThin = Start->isThin();
      LibId = Start->getId();
      if (IsPostLtoPhase) {
        std::string ArchiveName = getStartLibArchiveName(IsThin, LibId);
        CachedArchiveArea =
            Input::getMemoryAreaForPath(ArchiveName, Config.getDiagEngine());
        if (CachedArchiveArea)
          ReuseArchiveOnly = true;
      }
      ++CurNode;
      continue;
    }

    FileNode *Node = llvm::dyn_cast<FileNode>(*CurNode);
    if (!Node) {
      ++CurNode;
      continue;
    }

    if (ReuseArchiveOnly) {
      ++CurNode;
      continue;
    }

    Input *Input = Node->getInput();
    if (!Input->resolvePath(Config))
      return false;

    // Classify the input
    if (!Input->getInputFile()) {
      InputFile *File =
          InputFile::create(Input, Config.getDiagEngine());
      if (!File) {
        Config.raise(Diag::err_unrecognized_input_file)
          << Input->getResolvedPath()
          << Config.targets().triple().str();
        MModule.setFailure(true);
        return false;
      }
      Input->setInputFile(File);
    }

    if (Input->getInputFile() &&
        Input->getInputFile()->getKind() == InputFile::GNULinkerScriptKind) {
      auto *LSFile =
        llvm::cast<eld::LinkerScriptFile>(Input->getInputFile());

      if (!MObjLinker->readAndProcessInput(Input, IsPostLtoPhase))
        return false;

      for (auto *N : LSFile->getNodes()) {
        auto *FN = llvm::dyn_cast<FileNode>(N);
        if (!FN)
          continue;

        auto *ScriptInput = FN->getInput();
        if (!ScriptInput->resolvePath(Config))
          return false;

        if (!ScriptInput->getInputFile()) {
          InputFile *File =
              InputFile::create(ScriptInput, Config.getDiagEngine());
          if (!File) {
            Config.raise(Diag::err_unrecognized_input_file)
                << ScriptInput->getResolvedPath()
                << Config.targets().triple().str();
            MModule.setFailure(true);
            return false;
          }

          ScriptInput->setInputFile(File);
        }

        if (!addArchiveMember(ScriptInput, IsThin, Config, Archive))
          return false;
      }
      ++CurNode;
      continue;
    }

    if (!addArchiveMember(Input, IsThin, Config, Archive))
      return false;

    ++CurNode;
  }

  if (layoutInfo)
    layoutInfo->recordInputActions(LayoutInfo::EndLib, nullptr);

  std::string ArchiveName = getStartLibArchiveName(IsThin, LibId);
  MemoryArea *MemArea = CachedArchiveArea;
  if (!MemArea) {
    eld::RegisterTimer T("Build --start-lib archive", "Read all Input files",
                         Config.options().printTimingStats());
    const bool EffectiveThin = IsThin && !Archive.RequiresRegularArchive;
    llvm::Expected<std::unique_ptr<llvm::MemoryBuffer>> ExpArchiveBuf =
        llvm::writeArchiveToBuffer(
            Archive.Members, llvm::SymtabWritingMode::NormalSymtab,
            llvm::object::Archive::K_GNU,
            /*Deterministic=*/true, /*Thin=*/EffectiveThin);
    if (!ExpArchiveBuf) {
      plugin::DiagnosticEntry DiagEntry =
          Config.getDiagEngine()->convertToDiagEntry(ExpArchiveBuf.takeError());
      Config.raiseDiagEntry(
          std::make_unique<plugin::DiagnosticEntry>(std::move(DiagEntry)));
      return false;
    }
    MemArea = make<MemoryArea>(std::move(*ExpArchiveBuf));
    Input::cacheMemoryAreaForPath(ArchiveName, MemArea);
  }

  auto *ArchiveInput =
      make<Input>(ArchiveName, LibAttr, Config.getDiagEngine(), Input::Default);
  ArchiveInput->setResolvedPath(ArchiveName);
  ArchiveInput->setResolvedPathHash(Input::computeFilePathHash(ArchiveName));
  ArchiveInput->setMemberNameHash(Input::computeFilePathHash(ArchiveName));
  ArchiveInput->setMemArea(MemArea);

  bool Success = MObjLinker->readAndProcessInput(ArchiveInput, IsPostLtoPhase);
  if (Success && MModule.getOutputTarWriter() && ArchiveInput->getInputFile()) {
    // Register the generated synthetic archive so the response-file input and
    // mapping remain available during reproduction.
    InputFile *ArchiveFile = ArchiveInput->getInputFile();
    ArchiveFile->setMappedPath(ArchiveName);
    ArchiveFile->setMappingFileKind(MappingFile::Kind::Archive);
    MModule.getOutputTarWriter()->addInputFile(ArchiveFile,
                                               /*isLTOObject=*/false);
  }
  return Success;
}
