//===- ArchiveParser.cpp---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Readers/ArchiveParser.h"
#include "eld/Core/Module.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Input/ArchiveFile.h"
#include "eld/Input/Input.h"
#include "eld/Input/InputFile.h"
#include "eld/LayoutMap/LayoutInfo.h"
#include "eld/Object/ObjectLinker.h"
#include "eld/PluginAPI/DiagnosticEntry.h"
#include "eld/PluginAPI/Expected.h"
#include "eld/Support/MemoryArea.h"
#include "eld/Support/RegisterTimer.h"
#include "eld/Target/GNULDBackend.h"
#include "llvm-c/Core.h"
#include "llvm/ADT/Hashing.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Object/Archive.h"
#include "llvm/Object/Binary.h"
#include "llvm/Object/ELFObjectFile.h"
#include "llvm/Object/ELFTypes.h"
#include "llvm/Object/Error.h"
#include "llvm/Object/IRObjectFile.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Object/SymbolicFile.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/MemoryBufferRef.h"
#include "llvm/Support/Parallel.h"
#include "llvm/Support/ThreadPool.h"
#include <cstdint>
#include <memory>

using namespace eld;

eld::Expected<uint32_t> ArchiveParser::parseFile(InputFile &inputFile) const {
  ArchiveFile *archiveFile = llvm::cast<ArchiveFile>(&inputFile);
  bool hasAFI = (archiveFile->getArchiveFileInfo() != nullptr);
  if (!hasAFI) {
    archiveFile->initArchiveFileInfo();
  }
  llvm::MemoryBufferRef memRef = inputFile.getInput()->getMemoryBufferRef();
  llvm::Expected<std::unique_ptr<llvm::object::Archive>> expArchiveReader =
      llvm::object::Archive::create(memRef);
  LLVMEXP_RETURN_DIAGENTRY_IF_ERROR(expArchiveReader);
  std::unique_ptr<llvm::object::Archive> archiveReader =
      std::move(*expArchiveReader);
  if (archiveReader->isEmpty())
    return {};
  if (archiveReader->isThin())
    archiveFile->setThinArchive();

  bool isWholeArchive =
      archiveFile->getInput()->getAttribute().isWholeArchive();

  LinkerConfig &config = m_Module.getConfig();
  if (isWholeArchive) {
    if (config.showWholeArchiveWarnings())
      config.raise(Diag::warn_whole_archive_enabled)
          << archiveFile->getInput()->decoratedPath();
    if (archiveFile->shouldSkipFile())
      return 0;
    archiveFile->setToSkip();
  }

  // Add members to the archive if they have not been added yet.
  if (!archiveFile->hasMembers()) {
    if (m_Module.getPrinter()->isVerbose())
      config.raise(Diag::loading_all_members)
          << archiveFile->getInput()->decoratedPath();
    eld::Expected<bool> expAddMembers = addMembers(*archiveReader, archiveFile);
    if (!expAddMembers) {
      m_Module.setFailure(true);
    }
    ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expAddMembers);
  }

  if (config.showArchiveFileWarnings())
    warnRepeatedMembers(*archiveFile);

  if (!hasAFI) {
    eld::Expected<bool> expReadSymTab =
        readSymbolTable(*archiveReader, archiveFile);
    ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expReadSymTab);
  }

  if (isWholeArchive) {
    eld::Expected<uint32_t> expMemCount = includeAllMembers(archiveFile);
    ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expMemCount);
    uint32_t numObjects = expMemCount.value();
    return numObjects;
  }

  // include the needed members in the archive and build up the input tree
  bool willSymResolved = false;
  InputFile *referredSite = nullptr;
  LayoutInfo *layoutInfo = m_Module.getLayoutInfo();
  config.raise(Diag::verbose_performing_archive_symbol_resolution)
      << inputFile.getInput()->decoratedPath();
  do {
    willSymResolved = false;

    for (size_t idx = 0; idx < archiveFile->numOfSymbols(); ++idx) {
      // bypass if we already decided to include this symbol or not
      if (ArchiveFile::Symbol::Unknown != archiveFile->getSymbolStatus(idx))
        continue;

      // check if we should include this defined symbol
      ArchiveFile::Symbol &symbol = *(archiveFile->getSymbolTable()[idx]);
      ArchiveFile::Symbol::SymbolStatus status =
          shouldIncludeSymbol(symbol, &referredSite);
      if (ArchiveFile::Symbol::Include == status) {
        // include the object member from the given offset
        Input *I =
            archiveFile->getLazyLoadMember(archiveFile->getObjFileOffset(idx));
        if (!includeMember(I))
          continue;
        archiveFile->setSymbolStatus(idx, status);
        willSymResolved = true;
        if (layoutInfo && referredSite)
          layoutInfo->recordArchiveMember(
              I, referredSite, &symbol,
              llvm::cast<eld::ObjectFile>(I->getInputFile())
                  ->getSymbol(symbol.Name));
      } // end of if
    } // end of for
  } while (willSymResolved);

  return willSymResolved;
}

eld::Expected<bool>
ArchiveParser::readSymbolTable(const llvm::object::Archive &archiveReader,
                               ArchiveFile *archive) const {
  if (!archiveReader.hasSymbolTable()) {
    m_Module.setFailure(true);
    return std::make_unique<plugin::DiagnosticEntry>(plugin::DiagnosticEntry{
        Diag::err_no_ar_symtab, {archive->getInput()->decoratedPath()}});
  }

  eld::Expected<ArchiveSymbolInfoTable> expSymbolInfoTable =
      computeSymInfoTable(archiveReader, archive);
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expSymbolInfoTable);
  ArchiveSymbolInfoTable symbolInfoTable = expSymbolInfoTable.value();

  for (const llvm::object::Archive::Symbol &sym : archiveReader.symbols()) {
    llvm::Expected<llvm::object::Archive::Child> expMember = sym.getMember();
    LLVMEXP_RETURN_DIAGENTRY_IF_ERROR(expMember);
    const llvm::object::Archive::Child &member = expMember.get();
    auto symIt = symbolInfoTable.find({member.getChildOffset(), sym.getName()});
    ASSERT(symIt != symbolInfoTable.end(),
           "Missing required symbol information!");
    archive->addSymbol(sym.getName().data(), member.getChildOffset(),
                       symIt->second);
  }
  return true;
}

eld::Expected<ArchiveParser::ArchiveSymbolInfoTable>
ArchiveParser::computeSymInfoTable(const llvm::object::Archive &archiveReader,
                                   ArchiveFile *archive) const {
  LinkerConfig &config = m_Module.getConfig();
  ArchiveSymbolInfoTable symbolInfoTable;

  eld::Expected<bool> expAddReqSymbols =
      addReqSymbolsWithPlaceholderValue(archiveReader, symbolInfoTable);
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expAddReqSymbols);

  llvm::Error err = llvm::Error::success();
  for (const llvm::object::Archive::Child &child :
       archiveReader.children(err, /*SkipInternal=*/true)) {
    eld::Expected<bool> expComputeResult = computeSymInfoTableForMember(
        *archive, archiveReader, child, symbolInfoTable);
    ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expComputeResult);
  }
  if (err)
    return std::make_unique<plugin::DiagnosticEntry>(
        config.getDiagEngine()->convertToDiagEntry(std::move(err)));

  return symbolInfoTable;
}

eld::Expected<bool> ArchiveParser::addReqSymbolsWithPlaceholderValue(
    const llvm::object::Archive &archiveReader,
    ArchiveSymbolInfoTable &symbolInfoTable) const {
  for (const llvm::object::Archive::Symbol &sym : archiveReader.symbols()) {
    llvm::Expected<llvm::object::Archive::Child> expMember = sym.getMember();
    LLVMEXP_RETURN_DIAGENTRY_IF_ERROR(expMember);
    const llvm::object::Archive::Child &member = expMember.get();
    llvm::StringRef symName = sym.getName();
    symbolInfoTable[{member.getChildOffset(), symName}] =
        ArchiveFile::Symbol::NoType;
  }
  return true;
}

eld::Expected<bool> ArchiveParser::computeSymInfoTableForMember(
    const ArchiveFile &archiveFile, const llvm::object::Archive &archiveReader,
    const llvm::object::Archive::Child &member,
    ArchiveSymbolInfoTable &symbolInfoTable) const {

  eld::Expected<std::unique_ptr<llvm::object::Binary>> expBinaryObj =
      createMemberReader(archiveFile, archiveReader, member);
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expBinaryObj);

  std::unique_ptr<llvm::object::Binary> binaryObj =
      std::move(expBinaryObj.value());

  // Return true if member is not a binary object.
  // Note: it is not an error if a member is of unsupported type.
  if (!binaryObj)
    return true;

  if (binaryObj->isELF()) {
    ASSERT(llvm::cast<llvm::object::ELFObjectFileBase>(binaryObj.get())
               ->isRelocatableObject(),
           "Only relocatable object files are supported as archive members!");
    if (auto ELFObjFile =
            llvm::dyn_cast<llvm::object::ELFObjectFile<llvm::object::ELF32LE>>(
                binaryObj.get())) {
      eld::Expected<bool> expComputeResult =
          computeSymInfoTableForELFMember(*ELFObjFile, member, symbolInfoTable);
      ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expComputeResult);
    } else if (auto ELFObjFile = llvm::dyn_cast<
                   llvm::object::ELFObjectFile<llvm::object::ELF64LE>>(
                   binaryObj.get())) {
      eld::Expected<bool> expComputeResult =
          computeSymInfoTableForELFMember(*ELFObjFile, member, symbolInfoTable);
      ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expComputeResult);
    } else {
      std::string message =
          binaryObj->getFileName().str() + " has unsupported ELF type!";
      llvm_unreachable(message.c_str());
    }
  } else if (auto IRObj =
                 llvm::dyn_cast<llvm::object::IRObjectFile>(binaryObj.get())) {
    eld::Expected<bool> expComputeResult =
        computeSymInfoTableForIRMember(*IRObj, member, symbolInfoTable);
    ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expComputeResult);
  } else {
    std::string message =
        binaryObj->getFileName().str() + " has unsupported type!";
    llvm_unreachable(message.c_str());
  }
  return true;
}

template <class ELFT>
eld::Expected<bool> ArchiveParser::computeSymInfoTableForELFMember(
    const llvm::object::ELFObjectFile<ELFT> &ELFObjFile,
    const llvm::object::Archive::Child &member,
    ArchiveSymbolInfoTable &symbolInfoTable) const {
  ASSERT(ELFObjFile.isRelocatableObject(),
         "Only relocatable object files are supported as archive members!");
  using Elf_Sym = typename ELFT::Sym;

  for (const llvm::object::ELFSymbolRef &sym : ELFObjFile.symbols()) {
    llvm::Expected<const Elf_Sym *> expRawSym =
        ELFObjFile.getSymbol(sym.getRawDataRefImpl());
    LLVMEXP_RETURN_DIAGENTRY_IF_ERROR(expRawSym);
    const Elf_Sym *rawSym = expRawSym.get();
    if (rawSym->getBinding() != llvm::ELF::STB_LOCAL &&
        rawSym->st_shndx != llvm::ELF::SHN_UNDEF) {
      llvm::Expected<llvm::StringRef> expName = sym.getName();
      LLVMEXP_RETURN_DIAGENTRY_IF_ERROR(expName);
      llvm::StringRef name = expName.get();

      llvm::Expected<llvm::object::SymbolRef::Type> expRefType = sym.getType();
      LLVMEXP_RETURN_DIAGENTRY_IF_ERROR(expRefType);
      llvm::object::SymbolRef::Type reftype = expRefType.get();

      auto iter = symbolInfoTable.find({member.getChildOffset(), name});
      if (iter == symbolInfoTable.end())
        continue;

      GNULDBackend &backend = *m_Module.getBackend();
      ArchiveFile::Symbol::SymbolType &type = iter->second;
      type = ArchiveFile::Symbol::NoType;
      if (rawSym->getBinding() == llvm::ELF::STB_WEAK) {
        type = ArchiveFile::Symbol::Weak;
      } else if (rawSym->st_shndx < llvm::ELF::SHN_LOPROC) {
        if (reftype == llvm::object::SymbolRef::ST_Data)
          type = ArchiveFile::Symbol::DefineData;
        else if (reftype == llvm::object::SymbolRef::ST_Function)
          type = ArchiveFile::Symbol::DefineFunction;
      } else if (rawSym->st_shndx == ResolveInfo::Common ||
                 backend.getSymDesc(rawSym->st_shndx) == ResolveInfo::Common) {
        type = ArchiveFile::Symbol::Common;
      }
    }
  }
  return true;
}

eld::Expected<bool> ArchiveParser::computeSymInfoTableForIRMember(
    const llvm::object::IRObjectFile &IRObj,
    const llvm::object::Archive::Child &member,
    ArchiveSymbolInfoTable &symbolInfoTable) const {
  LinkerConfig &config = m_Module.getConfig();
  for (const llvm::object::BasicSymbolRef &sym : IRObj.symbols()) {
    // FIXME: Should we use auto for LLVM APIs?
    // Because if an LLVM API changes, the return type from
    // llvm::Expected<uint32_t> to llvm::Expected<uint64_t> then
    // the code may become incorrect without us knowing.
    llvm::Expected<uint32_t> expFlags = sym.getFlags();
    LLVMEXP_RETURN_DIAGENTRY_IF_ERROR(expFlags);
    uint32_t flags = expFlags.get();

    std::string name;
    llvm::raw_string_ostream nameOS(name);
    llvm::Error err = sym.printName(nameOS);
    if (err)
      return std::make_unique<plugin::DiagnosticEntry>(
          config.getDiagEngine()->convertToDiagEntry(std::move(err)));
    nameOS.flush();

    auto it = symbolInfoTable.find({member.getChildOffset(), name});
    if (it == symbolInfoTable.end())
      continue;
    ArchiveFile::Symbol::SymbolType &type = it->second;
    if (flags & llvm::object::SymbolRef::SF_Common) {
      type = ArchiveFile::Symbol::Common;
    } else if (flags & llvm::object::SymbolRef::SF_Weak) {
      type = ArchiveFile::Symbol::Weak;
    } else if (flags & llvm::object::SymbolRef::SF_Global) {
      if (flags & llvm::object::SymbolRef::SF_Executable)
        type = ArchiveFile::Symbol::DefineFunction;
      else
        type = ArchiveFile::Symbol::DefineData;
    }
  }
  return true;
}

eld::Expected<bool>
ArchiveParser::addMembers(llvm::object::Archive &archiveReader,
                          ArchiveFile *archive) const {
  LinkerConfig &config = m_Module.getConfig();
  llvm::Error err = llvm::Error::success();
  for (const llvm::object::Archive::Child &member :
       archiveReader.children(err, /*SkipInternal=*/true)) {
    eld::Expected<Input *> expInput =
        createMemberInput(archiveReader, member, archive);
    ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expInput);
    Input *I = expInput.value();
    archive->addLazyLoadMember(member.getChildOffset(), I);
    archive->addMember(I);
    config.raise(Diag::loading_member) << I->decoratedPath();
  }
  if (err)
    return std::make_unique<plugin::DiagnosticEntry>(
        config.getDiagEngine()->convertToDiagEntry(std::move(err)));
  return true;
}

eld::Expected<Input *>
ArchiveParser::createMemberInput(llvm::object::Archive &archiveReader,
                                 const llvm::object::Archive::Child &member,
                                 ArchiveFile *archive) const {
  llvm::Expected<llvm::StringRef> expMemName = member.getName();
  LLVMEXP_RETURN_DIAGENTRY_IF_ERROR(expMemName);
  llvm::StringRef memName = expMemName.get();

  eld::Expected<MemoryArea *> expMemberData =
      getMemberData(*archive, archiveReader, member);
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expMemberData);
  MemoryArea *memberData = expMemberData.value();

  Input *memberInput =
      archive->createMemberInput(memName, memberData, member.getChildOffset());

  if (!memberInput) {
    std::string archiveName = archive->getInput()->decoratedPath();
    return std::make_unique<plugin::DiagnosticEntry>(plugin::DiagnosticEntry{
        Diag::error_create_archive_member_input, {memName.str(), archiveName}});
  }

  return memberInput;
}

eld::Expected<uint32_t>
ArchiveParser::includeAllMembers(ArchiveFile *archive) const {
  LayoutInfo *layoutInfo = m_Module.getLayoutInfo();
  uint32_t IncludeMemberCount = 0;
  for (Input *member : archive->getAllMembers()) {
    if (includeMember(member)) {
      ++IncludeMemberCount;
      if (layoutInfo)
        layoutInfo->recordWholeArchiveMember(member);
    }
  }
  return IncludeMemberCount;
}

eld::Expected<bool> ArchiveParser::includeMember(Input *member) const {
  bool isPostLTOPhase = m_Module.isPostLTOPhase();
  ObjectLinker &objLinker = *m_Module.getLinker()->getObjectLinker();
  if (member->getInputFile()->getKind() == InputFile::BitcodeFileKind &&
      isPostLTOPhase)
    return false;
  if (!objLinker.readAndProcessInput(member, isPostLTOPhase))
    return false;
  static_cast<ArchiveMemberInput *>(member)
      ->getArchiveFile()
      ->incrementLoadedMemberCount();
  return true;
}

ArchiveFile::Symbol::SymbolStatus
ArchiveParser::shouldIncludeSymbol(const ArchiveFile::Symbol &sym,
                                   InputFile **pSite) const {
  bool isPostLTOPhase = m_Module.isPostLTOPhase();
  // TODO: handle symbol version issue and user defined symbols
  const ResolveInfo *info = m_Module.getNamePool().findInfo(sym.Name);
  LayoutInfo *layoutInfo = m_Module.getLayoutInfo();

  if (!info)
    return ArchiveFile::Symbol::Unknown;

  if (!info->isUndef()) {
    if (info->isCommon() && definedSameType(info, sym.Type)) {
      if (layoutInfo) {
        InputFile *oldInput = info->resolvedOrigin();
        if (oldInput) {
          *pSite = oldInput;
        }
      }
      return ArchiveFile::Symbol::Include;
    }
    return ArchiveFile::Symbol::Exclude;
  }

  if (info->isWeak())
    return ArchiveFile::Symbol::Unknown;
  if (layoutInfo) {
    InputFile *oldInput = info->resolvedOrigin();
    if (oldInput) {
      *pSite = oldInput;
    }
  }

  // Dont use a vector to lookup, as the lookup may prove costly.
  // The number of symbols in the extern list is very less, so adding a map
  // doesnot hurt performance.
  if (m_Module.hasSymbolInNeededSet(sym.Name))
    return ArchiveFile::Symbol::Include;

  // If a Bitcode file has a reference to the wrap symbol, then lets use the
  // behavior below, since postLTOPhase may start pulling the symbol from
  // Archive libraries.
  if (isPostLTOPhase && !m_Module.hasWrapReference(sym.Name))
    return ArchiveFile::Symbol::Include;

  // If there is no wrap option specified we default to the behavior (or
  // If there is no wrap option set for this symbol, lets use the default
  // behavior
  if (m_Module.getConfig().options().renameMap().empty() ||
      !m_Module.getConfig().options().renameMap().count(sym.Name))
    return ArchiveFile::Symbol::Include;

  // If there is a wrap option specified for that symbol, we only pull from
  // an archive, if the real symbol is still undefined.
  std::string realSymbol = "__real_" + sym.Name;
  ResolveInfo *RealSymbol = m_Module.getNamePool().findInfo(realSymbol);
  if (RealSymbol && RealSymbol->isUndef())
    return ArchiveFile::Symbol::Include;

  return ArchiveFile::Symbol::Exclude;
}

bool ArchiveParser::definedSameType(
    const ResolveInfo *info, ArchiveFile::Symbol::SymbolType type) const {
  if (type == ArchiveFile::Symbol::DefineData &&
      info->type() == ResolveInfo::Object)
    return true;
  if (type == ArchiveFile::Symbol::DefineFunction &&
      info->type() == ResolveInfo::Function)
    return true;
  return false;
}

eld::Expected<std::unique_ptr<llvm::object::Binary>>
ArchiveParser::createMemberReader(
    const ArchiveFile &archiveFile, const llvm::object::Archive &archiveReader,
    const llvm::object::Archive::Child &member) const {
  LinkerConfig &config = m_Module.getConfig();
  llvm::LLVMContext &context = *llvm::unwrap(LLVMGetGlobalContext());
  Input *archiveMemInput =
      archiveFile.getLazyLoadMember(member.getChildOffset());
  ASSERT(archiveMemInput,
         "Member missing in the " +
             archiveFile.getInput()->decoratedPath(/*showAbsolute=*/false) +
             " archive!");
  llvm::Expected<std::unique_ptr<llvm::object::Binary>> expBinaryObj =
      llvm::object::createBinary(archiveMemInput->getMemoryBufferRef(),
                                 &context);
  DiagnosticEngine &diagEngine = *m_Module.getConfig().getDiagEngine();

  if (expBinaryObj)
    return std::move(expBinaryObj.get());

  llvm::Error E = expBinaryObj.takeError();
  llvm::Error newE = llvm::handleErrors(
      std::move(E),
      [&](std::unique_ptr<llvm::ErrorInfoBase> EIB) -> llvm::Error {
        if (EIB->convertToErrorCode() !=
            llvm::object::object_error::invalid_file_type)
          return std::move(EIB);
        llvm::Expected<llvm::StringRef> expMemName = member.getName();
        if (!expMemName)
          return expMemName.takeError();
        if (config.showArchiveFileWarnings()) {
          llvm::StringRef memName = expMemName.get();
          diagEngine.raise(Diag::warn_unsupported_archive_member)
              << memName << archiveReader.getFileName();
        }
        return llvm::Error::success();
      });
  if (newE)
    return std::make_unique<plugin::DiagnosticEntry>(
        diagEngine.convertToDiagEntry(std::move(newE)));

  return std::unique_ptr<llvm::object::Binary>(nullptr);
}

eld::Expected<MemoryArea *>
ArchiveParser::getMemberData(const ArchiveFile &archiveFile,
                             llvm::object::Archive &archiveReader,
                             const llvm::object::Archive::Child &member) const {
  LinkerConfig &config = m_Module.getConfig();
  llvm::Expected<llvm::StringRef> expMemName = member.getName();
  LLVMEXP_RETURN_DIAGENTRY_IF_ERROR(expMemName);
  llvm::StringRef memName = expMemName.get();
  MemoryArea *memberData = nullptr;
  if (!archiveFile.isThin() || !config.options().hasMappingFile()) {
    llvm::Expected<llvm::MemoryBufferRef> expMemberDataRef =
        member.getMemoryBufferRef();
    LLVMEXP_RETURN_DIAGENTRY_IF_ERROR(expMemberDataRef);
    llvm::MemoryBufferRef memberDataRef = expMemberDataRef.get();

    // Take ownership of thin archive buffers.
    // Thin member buffers are deleted when archiveReader is destroyed.
    if (archiveReader.isThin()) {
      // Currently, the flow of thin buffers allocation/deallocation is weird in
      // llvm archive. A new buffer is created each time
      // llvm::object::Archive::Child::getBuffer is called for a thin archive
      // member. We don't care about thin buffers other than the latest one,
      // and thus they can safely be (automatically) deleted once the if-scope
      // ends.
      std::vector<std::unique_ptr<llvm::MemoryBuffer>> thinBuffers =
          archiveReader.takeThinBuffers();
      memberData = eld::make<MemoryArea>(std::move(thinBuffers.back()));
    } else {
      memberData = eld::make<MemoryArea>(memberDataRef);
    }
  } else {
    std::string memberPath = config.getMappedThinArchiveMember(
        archiveFile.getInput()->decoratedPath(/*showAbsolute=*/false),
        memName.str());
    memberData = make<MemoryArea>(memberPath);
    if (!memberData->Init(config.getDiagEngine())) {
      return std::make_unique<plugin::DiagnosticEntry>(
          Diag::error_failed_to_read_thin_archive_mem,
          std::vector<std::string>{archiveFile.getInput()->decoratedPath(),
                                   memName.str(), memberPath});
    }
  }
  return memberData;
}

void ArchiveParser::warnRepeatedMembers(const ArchiveFile &archiveFile) const {
  LinkerConfig &config = m_Module.getConfig();
  std::unordered_map<uint64_t, size_t> memDataHashToMemIdxMap;
  const std::vector<Input *> &archiveMembers = archiveFile.getAllMembers();
  std::vector<uint64_t> archiveMemberHashes(archiveMembers.size(), 0);
  size_t useThreads = config.options().numThreads() > 1;
  llvm::ThreadPoolInterface *Pool = m_Module.getThreadPool();
  auto computeAndSetHash = [&](size_t i) {
    archiveMemberHashes[i] =
        llvm::hash_value(archiveMembers[i]->getFileContents());
  };
  for (size_t i = 0; i < archiveMembers.size(); ++i) {
    if (useThreads)
      Pool->async(computeAndSetHash, i);
    else
      computeAndSetHash(i);
  }
  if (useThreads)
    Pool->wait();
  for (size_t i = 0; i < archiveMembers.size(); ++i) {
    const ArchiveMemberInput *member =
        llvm::cast<ArchiveMemberInput>(archiveMembers[i]);
    uint64_t hash = archiveMemberHashes[i];
    auto iter = memDataHashToMemIdxMap.find(hash);
    if (iter != memDataHashToMemIdxMap.end()) {
      size_t prevMemIdx = iter->second;
      const ArchiveMemberInput *prevMember =
          llvm::cast<ArchiveMemberInput>(archiveMembers[prevMemIdx]);
      config.raise(Diag::warn_archive_mem_repeated_content)
          << archiveFile.getInput()->decoratedPath() << member->getMemberName()
          << i << prevMember->getMemberName() << prevMemIdx;
    } else {
      memDataHashToMemIdxMap[hash] = i;
    }
  }
}
