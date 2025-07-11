//===- OutputTarWriter.cpp-------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#include "eld/Support/OutputTarWriter.h"
#include "eld/Config/Version.h"
#include "eld/Core/Module.h"
#include "eld/Diagnostics/DiagnosticPrinter.h"
#include "eld/Input/ArchiveMemberInput.h"
#include "eld/Support/MappingFile.h"
#include "eld/Support/Memory.h"
#include "eld/Support/MemoryArea.h"
#include "eld/Support/MsgHandling.h"
#include "eld/Support/ProgressBar.h"
#include "llvm/Support/Compression.h"
#include "llvm/Support/Path.h"
#include <unordered_map>

using namespace eld;

/// Create an OutputTarWriter
/// \param OutputPath where the tarball should be written
/// \param BaseDir base directory
OutputTarWriter::OutputTarWriter(LinkerConfig &Config)
    : m_Config(Config), m_Compress(m_Config.options().getCompressTar()),
      m_Verbose(m_Config.getPrinter()->isVerbose()) {}

bool OutputTarWriter::createOutput() {
  m_OutputPath = m_Config.options().getTarFile();
  std::string OutputFileName = m_Config.options().outputFileName();
  std::string BaseName = std::string(llvm::sys::path::filename(OutputFileName));
  // By default tarFilePath points to the tar file
  tarFilePath = m_Config.options().getTarFile();
  // If we want to compress the files, then let us create a tempoary file.
  if (m_Compress) {
    if (!llvm::compression::zlib::isAvailable()) {
      m_Config.raise(Diag::zlib_not_available);
      return false;
    }
    llvm::SmallString<256> OutputFileName;
    std::error_code EC =
        llvm::sys::fs::createTemporaryFile(BaseName, ".tar", OutputFileName);
    if (EC)
      return false;
    tarFilePath = std::string(OutputFileName);
  }
  llvm::Expected<std::unique_ptr<llvm::TarWriter>> TarOrErr =
      llvm::TarWriter::create(tarFilePath, "reproduce." + BaseName);
  if (!TarOrErr) {
    m_Config.raise(Diag::fail_to_create_tarball)
        << tarFilePath << llvm::toString(TarOrErr.takeError());
    return false;
  }
  Tar = std::move(*TarOrErr);
  return true;
}

/// Add an inputfile to this tarball
/// \param file the file to add to this tarball
void OutputTarWriter::addInputFile(const InputFile *File, bool IsLTOObject) {
  auto *Ipt = File->getInput();
  if (IsLTOObject)
    LTOELFFiles.push_back(Ipt->getName());
  if (File->getMappingFileKind() == MappingFile::Kind::Plugin)
    plugins.insert(Ipt);
  InputMap[File->getMappedPath()] = File;
  if (const ArchiveFile *AF = llvm::dyn_cast<ArchiveFile>(File))
    if (AF->isThinArchive())
      addThinArchiveMembers(AF);
}

std::string OutputTarWriter::getHashAndExtension(const Input *Ipt) const {
  // Returns filename passed to the linker along with the file hash.
  return std::string(
             llvm::sys::path::filename(Ipt->getInputFile()->getMappedPath())) +
         "." + std::to_string(Ipt->getResolvedPathHash());
}

/// Create mapping.ini file of input filepath to its sha2 hash + file extension
/// \returns true if the file was created sucessfully
bool OutputTarWriter::createMappingFile() {
  if (INIFileWriter == nullptr)
    INIFileWriter = std::make_unique<INIWriter>();
  for (const auto &Pair : InputMap) {
    const auto *F = Pair.second;
    auto *Ipt = F->getInput();
    auto Hash = MappingFile::getPathWithDirectory(getHashAndExtension(Ipt),
                                                  F->getMappingFileKind());
    if (plugins.find(Ipt) != plugins.end())
      Hash = "./" + Hash;
    std::string Path = Pair.first().str();
    if (m_Verbose)
      m_Config.raise(Diag::adding_ini_hash_entry) << Path << Hash;
    std::string Category = MappingFile::getMappingCategoryForInputFile(F);
    (*INIFileWriter)[Category][Path] = Hash;
  }

  for (const auto &P : m_StringMap) {
    (*INIFileWriter)["StringMap"][P.first] = P.second;
  }

  if (m_Verbose)
    m_Config.raise(Diag::created_mapping_file)
        << mappingFileName << getMappings();
  return true;
}

void OutputTarWriter::writeFile(llvm::StringRef Name,
                                llvm::StringRef Contents) {
  Tar->append(Name, Contents);
  m_ProgressBar->incrementAndDisplayProgress();
}

bool OutputTarWriter::doCompress() {
  auto Ipt = MemoryArea(tarFilePath);
  Ipt.Init(m_Config.getDiagEngine());
  llvm::SmallVector<uint8_t, 0> Compressed;
  (void)llvm::compression::zlib::compress(
      llvm::arrayRefFromStringRef(Ipt.getContents()), Compressed,
      llvm::compression::zlib::BestSizeCompression);
  std::error_code Error;
  llvm::raw_fd_ostream *File =
      new llvm::raw_fd_ostream(m_OutputPath, Error, llvm::sys::fs::OF_None);
  if (Error) {
    m_Config.raise(Diag::unable_to_write_compressed_tar) << m_OutputPath;
    return false;
  }
  *File << llvm::toStringRef(Compressed).str();
  delete File;
  llvm::sys::fs::remove(tarFilePath);
  return true;
}

/// Append mapping, response, version files to the tar file and write it to
/// disk \returns true if the writes succeed, false otherwise
bool OutputTarWriter::writeOutput(bool ShowProgress) {
  if (!createOutput())
    return false;
  if (INIFileWriter == nullptr)
    return false;
  m_Config.raise(Diag::creating_tarball) << getTarFileName();
  auto Contents = getMappings();
  if (m_Verbose)
    m_Config.raise(Diag::adding_mappingfile_to_tarball) << mappingFileName;
  m_ProgressBar = make<ProgressBar>(InputMap.size() + 3, 80, ShowProgress);
  writeFile(mappingFileName, Contents);
  writeFile(versionFileName, versionContents);
  writeFile(responseFileName, responseContents);
  for (const auto &P : InputMap) {
    auto *Ipt = P.second->getInput();
    writeFile(MappingFile::getPathWithDirectory(getHashAndExtension(Ipt),
                                                P.second->getMappingFileKind()),
              P.second->getContents());
  }
  if (m_Compress)
    return doCompress();
  return true;
}

/// create the version file but do not write append it yet to the tarball
/// \returns true if successful, false otherwise
bool OutputTarWriter::createVersionFile() {
  if (m_Verbose)
    m_Config.raise(Diag::creating_version_file) << versionFileName;
  auto eldVersion = getELDVersion();
  auto LlvmVersion = getLLVMVersion();
  versionContents =
      Saver.save("ELD: " + eldVersion.str() + " LLVM: " + LlvmVersion);
  return !eldVersion.empty();
}

bool OutputTarWriter::createResponseFile(llvm::StringRef Response) {
  responseContents = Saver.save(Response);
  if (m_Verbose)
    m_Config.raise(Diag::created_response_file) << responseFileName << Response;
  return true;
}

llvm::StringRef OutputTarWriter::getVersionFileName() const {
  return versionFileName;
}

llvm::StringRef OutputTarWriter::getTarFileName() const { return tarFilePath; }

llvm::StringRef OutputTarWriter::getMappingFileName() const {
  return mappingFileName;
}

// Why do we take both filename and resolved path as an input here?
void OutputTarWriter::createAndAddFile(std::string Filename,
                                       std::string ResolvedPath,
                                       MappingFile::Kind K, bool IsLTOObject) {
  auto *Ipt =
      make<Input>(ResolvedPath, m_Config.getDiagEngine(), Input::Default);
  if (!Ipt->resolvePath(m_Config))
    return;
  Ipt->setName(Filename);
  auto *IptFile = make<InputFile>(Ipt, m_Config.getDiagEngine());
  IptFile->setMappingFileKind(K);
  IptFile->setContents(!Ipt->getSize() ? "" : Ipt->getFileContents());
  IptFile->setMappedPath(Filename);
  Ipt->setInputFile(IptFile);
  addInputFile(IptFile, IsLTOObject);
}

void OutputTarWriter::createAndAddPlugin(std::string Filename,
                                         std::string ResolvedPath) {
  createAndAddFile(Filename, ResolvedPath, MappingFile::Kind::Plugin,
                   /*isLTO*/ false);
}

void OutputTarWriter::createAndAddConfigFile(std::string Filename,
                                             std::string ResolvedPath) {
  createAndAddFile(Filename, ResolvedPath, MappingFile::Kind::Config,
                   /*isLTO*/ false);
}

void OutputTarWriter::createAndAddScriptFile(std::string Filename,
                                             std::string ResolvedPath) {
  createAndAddFile(Filename, ResolvedPath, MappingFile::LinkerScript,
                   /*isLTO*/ false);
}

void OutputTarWriter::createAndAddCacheFile(std::string Filename) {
  createAndAddFile(Filename, Filename, MappingFile::CacheFile, /*isLTO*/ false);
}

std::string OutputTarWriter::rewritePath(llvm::StringRef S) {
  if (InputMap.find(S) != InputMap.end()) {
    const InputFile *IptFile = InputMap[S];
    return MappingFile::getPathWithDirectory(
        getHashAndExtension(IptFile->getInput()),
        IptFile->getMappingFileKind());
  }
  auto Basename = llvm::sys::path::filename(S);
  return Basename.str();
}

const std::string OutputTarWriter::getMappings() const {
  std::stringstream Mappings;
  Mappings << *INIFileWriter;
  return Mappings.str();
}

void OutputTarWriter::addThinArchiveMembers(const ArchiveFile *AF) {
  ASSERT(AF->isThinArchive(), "Must only be called for thin archives!");
  for (Input *Inp : AF->getAllMembers()) {
    ArchiveMemberInput *Mem = llvm::cast<eld::ArchiveMemberInput>(Inp);
    InputFile *MemFile = Mem->getInputFile();
    MemFile->setMappedPath(Mem->getName());
    MemFile->setMappingFileKind(MappingFile::Kind::ObjectFile);
    std::string MemberKey = AF->getMappedPath() +
                            MappingFile::getThinArchiveMemSeparator() +
                            MemFile->getMappedPath();
    InputMap[MemberKey] = MemFile;
  }
}
